#include <ccGreyhoundCloud.h>

#include "ccGreyhoundResource.h"

ccGreyhoundCloud::ccGreyhoundCloud(const QString& name)
	: ccPointCloud(name)
	, m_origin(nullptr)
	, m_state(ccGreyhoundCloud::State::Idle)
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

std::vector<QString> ccGreyhoundCloud::available_dims() const
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

void ccGreyhoundCloud::set_state(const State state)
{
	m_state = state;
}

const ccGreyhoundResource* ccGreyhoundCloud::origin() const {
	return m_origin;
}

ccGreyhoundCloud::State ccGreyhoundCloud::state() const
{
	return m_state;
}

