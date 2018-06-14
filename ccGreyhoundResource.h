#pragma once

#include <QNetworkReply>
#include <QJsonObject>
#include <QIcon>

#include <ccCustomObject.h>

#include "constants.h"

class GreyhoundInfo
{
public:
	explicit GreyhoundInfo(const QJsonObject& info); 
	int base_depth() const;
	std::vector<QString> available_dim_name() const;
	CCVector3d offset() const;
	CCVector3d bounds_conforming_min() const;
	CCVector3d bounds_min() const;
	QString srs() const;

private:
	QJsonObject m_info;
};


class ccGreyhoundResource : public ccCustomHObject
{
public:
	explicit ccGreyhoundResource(QUrl url);
	bool isSerializable() const override { return false; };
	static QString DefautMetaDataClassName() { return "qGreyHoundResource"; };
	static QString DefaultMetaDataPluginName() { return "qGreyhound"; };
	QIcon getIcon() const override { return QIcon(IconPaths::GreyhoundIcon); };

	QUrl url() const { return m_url; }
	const GreyhoundInfo& info() const { return m_info; }

private:
	QUrl m_url;
	GreyhoundInfo m_info;
};


QString resource_name_from_url(const QString& url);
QJsonObject greyhound_info(const QUrl& url);