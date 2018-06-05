#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>

#include "ccGreyhoundResource.h"

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



ccGreyhoundResource::ccGreyhoundResource(QUrl url)
	: ccCustomHObject(QString("[Greyhound] %1").arg(resource_name_from_url(url.toString())))
	, m_url(std::move(url))
	, m_info(greyhound_info(m_url))
{
}

GreyhoundInfo::GreyhoundInfo(QJsonObject info)
	: m_info(info)
{
}

std::vector<QString> GreyhoundInfo::available_dim_name() const
{
	QJsonArray schema = m_info.value("schema").toArray();
	std::vector<QString> dim_names;
	dim_names.reserve(schema.size());
	for (const auto& dimension : schema) {
		QJsonObject dimension_infos = dimension.toObject();
		dim_names.push_back(dimension_infos.value("name").toString());
	}
	return dim_names;
}

int GreyhoundInfo::base_depth() const
{
	return m_info.value("baseDepth").toInt();
}

CCVector3d GreyhoundInfo::offset() const
{
	QJsonArray offset = m_info.value("offset").toArray();
	return { offset.at(0).toDouble(), offset.at(1).toDouble(), offset.at(2).toDouble() };
}

CCVector3d GreyhoundInfo::bounds_conforming_min() const
{
	QJsonArray bounds_conforming = m_info.value("boundsConforming").toArray();
	return { bounds_conforming.at(0).toDouble(), bounds_conforming.at(1).toDouble(), bounds_conforming.at(2).toDouble() };
}

CCVector3d GreyhoundInfo::bounds_min() const
{
	QJsonArray bounds = m_info.value("bounds").toArray();
	return { bounds.at(0).toDouble(), bounds.at(1).toDouble(), bounds.at(2).toDouble() };
}
