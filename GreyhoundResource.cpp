#include "GreyhoundResource.h"


#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>


QString resource_name_from_url(const QString& url) 
{
	QStringList splits = url.split('/');
	return splits.at(splits.size() - 1);
}

QJsonObject greyhound_info(QUrl url)
{
	QNetworkAccessManager qnam;
	QNetworkRequest request;
	QNetworkReply *reply(nullptr);

	const QUrl info_url(url.toString() + "/info");
	request.setUrl(info_url);
	reply = qnam.get(request);

	QEventLoop loop;
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error() != QNetworkReply::NoError) {
		throw std::runtime_error(reply->errorString().toStdString());
	}

	QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
	
	if (!document.isObject()) {
		throw std::runtime_error("Received info is not a proper json object");
	}
	return document.object();
}



qGreyhoundResource::qGreyhoundResource(QUrl url)
	: ccCustomHObject(QString("[Greyhound] %1").arg(resource_name_from_url(url.toString())))
	, m_url(std::move(url))
	, m_infos(std::move(greyhound_info(m_url)))
{
}
