#pragma once


#include <GreyhoundCommon.hpp>

#include <ccPointCloud.h>


class ccGreyhoundResource;

namespace Greyhound = pdal::greyhound;

class ccGreyhoundCloud : public ccPointCloud {
public:
	enum class State
	{
		WaitingForPoints,
		Idle
	};


	explicit ccGreyhoundCloud(const QString& name = QString());

	void set_bbox(const Greyhound::Bounds bbox);
	void set_origin(ccGreyhoundResource *origin);
	void set_state(State state);

	const Greyhound::Bounds& bbox() const;
	std::vector<QString> available_dims() const;
	const ccGreyhoundResource *origin() const;
	State state() const;


private:
	Greyhound::Bounds m_bbox;
	ccGreyhoundResource *m_origin;
	State m_state;
};