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




class GreyhoundExc : public QException

{

public:
	GreyhoundExc(const QString& msg) : m_msg(msg) {}

	virtual const char* what() const noexcept { return m_msg.toStdString().c_str(); }
	const QString& message() const { return m_msg; }

	void raise() const { throw *this; }
	GreyhoundExc *clone() const { return new GreyhoundExc(*this); }
	
private:
	QString m_msg;
};

QString resource_name_from_url(const QString& url);
QJsonObject greyhound_info(QUrl url);