#include <pdal/pdal.hpp>

#include <ccPointCloud.h>


class PDALConverter {
public:
	PDALConverter() = default;
	std::unique_ptr<ccPointCloud> convert(pdal::PointViewPtr, pdal::PointLayoutPtr);
	void set_shift(CCVector3d shift);


private:
	void convert_rgb(pdal::PointViewPtr, ccPointCloud *out_cloud);
	void convert_scalar_fields(pdal::PointViewPtr, pdal::PointLayoutPtr, ccPointCloud*);

private:
	CCVector3d m_shift;
};