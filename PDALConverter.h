#pragma once

#include <pdal/pdal.hpp>

#include <ccPointCloud.h>


class PDALConverter {
public:
	PDALConverter() = default;
	void convert(pdal::PointViewPtr, pdal::PointLayoutPtr, ccPointCloud *cloud);
	void set_shift(CCVector3d shift);


private:
	static void convert_rgb(pdal::PointViewPtr, ccPointCloud *out_cloud);
	static void convert_scalar_fields(pdal::PointViewPtr, pdal::PointLayoutPtr, ccPointCloud*);

private:
	CCVector3d m_shift;
};