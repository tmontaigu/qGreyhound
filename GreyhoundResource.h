#pragma once

#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QException>
#include <QJsonObject>

#include <ccCustomObject.h>


class qGreyhoundResource : public ccCustomHObject
{
public:
	qGreyhoundResource(QUrl url);
	bool isSerializable() const override { return false; };
	static QString DefautMetaDataClassName() { return "qGreyHoundResource";  };
	static QString DefaultMetaDataPluginName() { return "qGreyhound"; };

	QUrl url() const { return m_url; }
	const QJsonObject& infos() const { return m_infos; }

private:
	QUrl m_url;
	QJsonObject m_infos;
};



QString resource_name_from_url(const QString& url);
QJsonObject greyhound_info(QUrl url);