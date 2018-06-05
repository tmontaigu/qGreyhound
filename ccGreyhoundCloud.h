#pragma once


#include <GreyhoundCommon.hpp>

#include <ccPointCloud.h>


class ccGreyhoundResource;

namespace Greyhound = pdal::greyhound;

class ccGreyhoundCloud : public ccPointCloud {
public:
	ccGreyhoundCloud(QString name = QString());

	void set_bbox(const Greyhound::Bounds bbox);
	void set_origin(ccGreyhoundResource *origin);

	const Greyhound::Bounds& bbox() const;
	const std::vector<QString> available_dims() const;
	const ccGreyhoundResource *origin() const;


private:
	Greyhound::Bounds m_bbox;
	ccGreyhoundResource *m_origin;
};