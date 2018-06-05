#pragma once

#include <ccPointCloud.h>

#include <pdal/pdal.hpp>
#include <GreyhoundCommon.hpp>

#include "PDALConverter.h"

void download_and_convert_cloud(ccPointCloud *cloud, pdal::Options opts, PDALConverter converter = PDALConverter());
void download_and_convert_cloud_threaded(ccPointCloud *cloud, pdal::Options opts, PDALConverter converter = PDALConverter());

struct BoundsDepth
{
	BoundsDepth()
		: b()
		, depth(0)
		, cloud(nullptr)
	{}

	BoundsDepth(const pdal::greyhound::Bounds &b, int depth)
		: b(b)
		, depth(depth)
		, cloud(nullptr)
	{}
	BoundsDepth(const pdal::greyhound::Bounds &b, int depth, ccPointCloud* c)
		: BoundsDepth(b, depth)
	{
		cloud = c;
	}
	pdal::greyhound::Bounds b;
	int depth;
	ccPointCloud *cloud;
};

class GreyhoundDownloader
{
public:
	enum DOWNLOAD_METHOD
	{
		DEPTH_BY_DEPTH,
		QUADTREE,
		OCTREE
	};

public:
	GreyhoundDownloader(pdal::Options opts, uint32_t start_depth, pdal::greyhound::Bounds bounds, PDALConverter converter);
	void download_to(ccPointCloud* cloud, DOWNLOAD_METHOD);


private:
	pdal::Options m_opts;
	uint32_t m_current_depth;
	pdal::greyhound::Bounds m_bounds;
	PDALConverter m_converter;
};