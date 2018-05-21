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

GreyhoundResource::GreyhoundResource()
	: m_base_url()
	, m_reply(nullptr)
{
}


GreyhoundResource::~GreyhoundResource()
{
}

void GreyhoundResource::set_url(QUrl url) {
	m_base_url = url;
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
