#pragma once


#include <GreyhoundCommon.hpp>


#include <ccPointCloud.h>

namespace Greyhound = pdal::greyhound;

class ccGreyhoundCloud : public ccPointCloud {
public:
	ccGreyhoundCloud(QString name = QString());

	void set_bbox(const Greyhound::Bounds bbox);
	const Greyhound::Bounds& bbox() const;


private:
	Greyhound::Bounds m_bbox;
};