#include <ccGreyhoundCloud.h>

#include "ccGreyhoundResource.h"

ccGreyhoundCloud::ccGreyhoundCloud(QString name)
	: ccPointCloud(name)
	, m_bbox()
	, m_origin(nullptr)
{
}

void ccGreyhoundCloud::set_bbox(const Greyhound::Bounds bbox)
{
	m_bbox = bbox;
}

const Greyhound::Bounds & ccGreyhoundCloud::bbox() const
{
	return m_bbox;
}

const std::vector<QString> ccGreyhoundCloud::available_dims() const
{
	if (m_origin) {
		return m_origin->info().available_dim_name();
	}
	return {};
}

void ccGreyhoundCloud::set_origin(ccGreyhoundResource *origin) 
{
	m_origin = origin;
}

const ccGreyhoundResource* ccGreyhoundCloud::origin() const {
	return m_origin;
}

