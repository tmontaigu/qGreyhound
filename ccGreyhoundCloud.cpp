#include <ccGreyhoundCloud.h>


ccGreyhoundCloud::ccGreyhoundCloud(QString name)
	: ccPointCloud(name)
	, m_bbox()
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
