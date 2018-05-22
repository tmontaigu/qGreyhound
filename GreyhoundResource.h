#pragma once

#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QException>
#include <QJsonObject>

#include <ccCustomObject.h>

class GreyhoundInfo
{
public:
	GreyhoundInfo(QJsonObject info); 
	int base_depth() const;
	std::vector<QString> available_dim_name() const;

private:
	QJsonObject m_info;

};


class qGreyhoundResource : public ccCustomHObject
{
public:
	qGreyhoundResource(QUrl url);
	bool isSerializable() const override { return false; };
	static QString DefautMetaDataClassName() { return "qGreyHoundResource"; };
	static QString DefaultMetaDataPluginName() { return "qGreyhound"; };

	QUrl url() const { return m_url; }
	const GreyhoundInfo& info() const { return m_info; }

private:
	QUrl m_url;
	GreyhoundInfo m_info;
};


QString resource_name_from_url(const QString& url);
QJsonObject greyhound_info(QUrl url);