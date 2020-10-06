#include "Clip.hpp"

#include <vtkh/filters/CleanGrid.hpp>
#include <vtkh/vtkm_filters/vtkmClip.hpp>
#include <vtkm/ImplicitFunction.h>

namespace vtkh
{

namespace detail
{

class MultiPlane final : public vtkm::ImplicitFunction
{
public:
  MultiPlane() = default;

  VTKM_EXEC_CONT MultiPlane(const Vector points[3],
                            const Vector normals[3],
                            const int num_planes)
  {
    this->SetPlanes(points, normals);
    this->m_num_planes = num_planes;
  }

  VTKM_EXEC void SetPlanes(const Vector points[6], const Vector normals[6])
  {
    for (vtkm::Id index : { 0, 1, 2})
    {
      this->Points[index] = points[index];
    }
    for (vtkm::Id index : { 0, 1, 2})
    {
      this->Normals[index] = normals[index];
    }
    this->Modified();
  }

  VTKM_EXEC void SetPlane(int idx, const Vector& point, const Vector& normal)
  {
    VTKM_ASSERT((idx >= 0) && (idx < 3));
    this->Points[idx] = point;
    this->Normals[idx] = normal;
    this->Modified();
  }

  VTKM_EXEC_CONT void SetNumPlanes(const int &num)
  {
    this->m_num_planes = num;
    this->Modified();
  }

  VTKM_EXEC_CONT void GetPlanes(Vector points[3], Vector normals[3]) const
  {
    for (vtkm::Id index : { 0, 1, 2})
    {
      points[index] = this->Points[index];
    }
    for (vtkm::Id index : { 0, 1, 2})
    {
      normals[index] = this->Normals[index];
    }
  }

  VTKM_EXEC_CONT const Vector* GetPoints() const { return this->Points; }

  VTKM_EXEC_CONT const Vector* GetNormals() const { return this->Normals; }

  VTKM_EXEC_CONT Scalar Value(const Vector& point) const final
  {
    Scalar maxVal = vtkm::NegativeInfinity<Scalar>();
    for (vtkm::Id index = 0; index < this->m_num_planes; ++index)
    {
      const Vector& p = this->Points[index];
      const Vector& n = this->Normals[index];
      const Scalar val = vtkm::Dot(point - p, n);
      maxVal = vtkm::Max(maxVal, val);
    }
    return maxVal;
  }

  VTKM_EXEC_CONT Vector Gradient(const Vector& point) const final
  {
    Scalar maxVal = vtkm::NegativeInfinity<Scalar>();
    vtkm::Id maxValIdx = 0;
    for (vtkm::Id index = 0; index < this->m_num_planes; ++index)
    {
      const Vector& p = this->Points[index];
      const Vector& n = this->Normals[index];
      Scalar val = vtkm::Dot(point - p, n);
      if (val > maxVal)
      {
        maxVal = val;
        maxValIdx = index;
      }
    }
    return this->Normals[maxValIdx];
  }

private:
  Vector Points[6] = { { -0.0f, 0.0f, 0.0f },
                       { 0.0f, 0.0f, 0.0f },
                       { 0.0f, -0.0f, 0.0f }};
  Vector Normals[6] = { { -1.0f, 0.0f, 0.0f },
                        { 1.0f, 0.0f, 0.0f },
                        { 0.0f, 0.0f, 0.0f } };
  int m_num_planes = 3;
};


}// namespace detail

struct Clip::InternalsType
{
  vtkm::cont::ImplicitFunctionHandle m_func;
  InternalsType()
  {}
};

Clip::Clip()
  : m_internals(new InternalsType),
    m_invert(false)
{

}

Clip::~Clip()
{

}

void
Clip::SetInvertClip(bool invert)
{
  m_invert = invert;
}

void
Clip::SetBoxClip(const vtkm::Bounds &clipping_bounds)
{
   auto box =  vtkm::cont::make_ImplicitFunctionHandle(
                 vtkm::Box({ clipping_bounds.X.Min,
                             clipping_bounds.Y.Min,
                             clipping_bounds.Z.Min},
                           { clipping_bounds.X.Max,
                             clipping_bounds.Y.Max,
                             clipping_bounds.Z.Max}));


  m_internals->m_func = box;
}

void
Clip::SetSphereClip(const double center[3], const double radius)
{
  vtkm::Vec<vtkm::FloatDefault,3> vec_center;
  vec_center[0] = center[0];
  vec_center[1] = center[1];
  vec_center[2] = center[2];
  vtkm::FloatDefault r = radius;

  auto sphere = vtkm::cont::make_ImplicitFunctionHandle(vtkm::Sphere(vec_center, r));
  m_internals->m_func = sphere;
}

void
Clip::SetPlaneClip(const double origin[3], const double normal[3])
{
  vtkm::Vec<vtkm::FloatDefault,3> vec_origin;
  vec_origin[0] = origin[0];
  vec_origin[1] = origin[1];
  vec_origin[2] = origin[2];

  vtkm::Vec<vtkm::FloatDefault,3> vec_normal;
  vec_normal[0] = normal[0];
  vec_normal[1] = normal[1];
  vec_normal[2] = normal[2];

  auto plane = vtkm::cont::make_ImplicitFunctionHandle(vtkm::Plane(vec_origin, vec_normal));
  m_internals->m_func = plane;
}

void
Clip::Set2PlaneClip(const double origin1[3],
                    const double normal1[3],
                    const double origin2[3],
                    const double normal2[3])
{
  vtkm::Vec3f plane_points[3];
  plane_points[0][0] = float(origin1[0]);
  plane_points[0][1] = float(origin1[1]);
  plane_points[0][2] = float(origin1[2]);

  plane_points[1][0] = float(origin2[0]);
  plane_points[1][1] = float(origin2[1]);
  plane_points[1][2] = float(origin2[2]);

  plane_points[2][0] = 0.f;
  plane_points[2][1] = 0.f;
  plane_points[2][2] = 0.f;

  vtkm::Vec3f plane_normals[3];
  plane_normals[0][0] = float(normal1[0]);
  plane_normals[0][1] = float(normal1[1]);
  plane_normals[0][2] = float(normal1[2]);

  plane_normals[1][0] = float(normal2[0]);
  plane_normals[1][1] = float(normal2[1]);
  plane_normals[1][2] = float(normal2[2]);

  plane_normals[2][0] = 0.f;
  plane_normals[2][1] = 0.f;
  plane_normals[2][2] = 0.f;

  vtkm::Normalize(plane_normals[0]);
  vtkm::Normalize(plane_normals[1]);

  auto planes
    = vtkm::cont::make_ImplicitFunctionHandle(detail::MultiPlane(plane_points,
                                                                 plane_normals,
                                                                 2));
  m_internals->m_func = planes;
}

void
Clip::Set3PlaneClip(const double origin1[3],
                    const double normal1[3],
                    const double origin2[3],
                    const double normal2[3],
                    const double origin3[3],
                    const double normal3[3])
{
  vtkm::Vec3f plane_points[3];
  plane_points[0][0] = float(origin1[0]);
  plane_points[0][1] = float(origin1[1]);
  plane_points[0][2] = float(origin1[2]);

  plane_points[1][0] = float(origin2[0]);
  plane_points[1][1] = float(origin2[1]);
  plane_points[1][2] = float(origin2[2]);

  plane_points[2][0] = float(origin3[0]);
  plane_points[2][1] = float(origin3[1]);
  plane_points[2][2] = float(origin3[2]);

  vtkm::Vec3f plane_normals[3];
  plane_normals[0][0] = float(normal1[0]);
  plane_normals[0][1] = float(normal1[1]);
  plane_normals[0][2] = float(normal1[2]);

  plane_normals[1][0] = float(normal2[0]);
  plane_normals[1][1] = float(normal2[1]);
  plane_normals[1][2] = float(normal2[2]);

  plane_normals[2][0] = float(normal3[0]);
  plane_normals[2][1] = float(normal3[1]);
  plane_normals[2][2] = float(normal3[2]);

  vtkm::Normalize(plane_normals[0]);
  vtkm::Normalize(plane_normals[1]);
  vtkm::Normalize(plane_normals[2]);

  auto planes
    = vtkm::cont::make_ImplicitFunctionHandle(detail::MultiPlane(plane_points,
                                                                 plane_normals,
                                                                 3));
  m_internals->m_func = planes;
}

void Clip::PreExecute()
{
  Filter::PreExecute();
}

void Clip::PostExecute()
{
  Filter::PostExecute();
}

void Clip::DoExecute()
{

  DataSet data_set;

  const int num_domains = this->m_input->GetNumberOfDomains();
  for(int i = 0; i < num_domains; ++i)
  {
    vtkm::Id domain_id;
    vtkm::cont::DataSet dom;
    this->m_input->GetDomain(i, dom, domain_id);

    vtkh::vtkmClip clipper;

    auto dataset = clipper.Run(dom,
                               m_internals->m_func,
                               m_invert,
                               this->GetFieldSelection());

    data_set.AddDomain(dataset, domain_id);
  }

  CleanGrid cleaner;
  cleaner.SetInput(&data_set);
  cleaner.Update();
  this->m_output = cleaner.GetOutput();
}

std::string
Clip::GetName() const
{
  return "vtkh::Clip";
}

} //  namespace vtkh

#ifdef VTKM_CUDA

// Cuda seems to have a bug where it expects the template class VirtualObjectTransfer
// to be instantiated in a consistent order among all the translation units of an
// executable. Failing to do so results in random crashes and incorrect results.
// We workaroud this issue by explicitly instantiating VirtualObjectTransfer for
// all the implicit functions here.

#include <vtkm/cont/cuda/internal/VirtualObjectTransferCuda.h>

VTKM_EXPLICITLY_INSTANTIATE_TRANSFER(vtkh::detail::MultiPlane);

#endif


