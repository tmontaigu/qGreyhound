#include "GreyhoundResource.h"


#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>

#include <QEventLoop>


GreyhoundResource::GreyhoundResource(QUrl url)
	: m_base_url(url)
	, m_reply(nullptr)
{
}


GreyhoundResource::~GreyhoundResource()
{
}

QJsonObject GreyhoundResource::info_query()
{
	QEventLoop loop;
	const QUrl info_url(m_base_url.toString() + "/info");
	m_request.setUrl(info_url);
	m_reply = m_qnam.get(m_request);
	QObject::connect(m_reply, SIGNAL(finished()), &loop, SLOT(quit()));
	loop.exec();

	if (m_reply->error() != QNetworkReply::NoError) {
		throw GreyhoundExc(m_reply->errorString());
	}

	QJsonDocument document = QJsonDocument::fromJson(m_reply->readAll());
	
	if (!document.isObject()) {
		throw GreyhoundExc("Recieved info is not a proper json object");
	}
	return document.object();
}

// Nice copy pasta
QJsonObject GreyhoundResource::count_query()
{
	QEventLoop loop;
	const QUrl info_url(m_base_url.toString() + "/count");
	m_request.setUrl(info_url);
	m_reply = m_qnam.get(m_request);
	QObject::connect(m_reply, SIGNAL(finished()), &loop, SLOT(quit()));
	loop.exec();

	if (m_reply->error() != QNetworkReply::NoError) {
		throw GreyhoundExc(m_reply->errorString());
	}

	QJsonDocument document = QJsonDocument::fromJson(m_reply->readAll());
	
	if (!document.isObject()) {
		throw GreyhoundExc("Recieved info is not a proper json object");
	}
	return document.object();
}

QByteArray GreyhoundResource::read_query()
{
	QEventLoop loop;
	QUrl read_url(m_base_url.toString() + "/read");
	QUrlQuery options;
	options.addQueryItem("depth", "8");
	options.addQueryItem("schema", "[{\"name\":\"X\",\"type\":\"floating\",\"size\":\"8\"},{\"name\":\"Y\",\"type\":\"floating\",\"size\":\"8\"},{\"name\":\"Z\",\"type\":\"floating\",\"size\":\"8\"}]");
	read_url.setQuery(options);

	m_request.setUrl(read_url);
	m_reply = m_qnam.get(m_request);
	QObject::connect(m_reply, SIGNAL(finished()), &loop, SLOT(quit()));
	loop.exec();

	if (m_reply->error() != QNetworkReply::NoError) {
		throw GreyhoundExc(m_reply->errorString());
	}

	return m_reply->readAll();
}
