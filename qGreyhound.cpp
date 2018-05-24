//##########################################################################
//#                                                                        #
//#                       CLOUDCOMPARE PLUGIN: qGreyhound                  #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                       COPYRIGHT: Thomas Montaigu                       #
//#                                                                        #
//##########################################################################


// Qt
#include <QtGui>
#include <QInputDialog>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDataStream>
#include <QEventLoop>
#include <QtConcurrent>

#include <array>
#include <queue>
#include <cmath>

#include <GreyhoundReader.hpp>
#include <bounds.hpp>

#include <ccHObject.h>
#include <ccScalarField.h>
#include <ccColorScalesManager.h>
#include <ccProgressDialog.h>

#include "qGreyhound.h"
#include "DimensionDialog.h"
#include "PDALConverter.h"
#include "GreyhoundDownloader.h"

#include "ui_bbox_form.h"

qGreyhound::qGreyhound(QObject* parent/*=0*/)
	: QObject(parent)
	, ccStdPluginInterface(":/CC/plugin/qGreyhound/info.json")
	, m_download_bounding_box(nullptr)
	, m_connect_to_resource(nullptr)
{
}

void qGreyhound::onNewSelection(const ccHObject::Container& selectedEntities)
{
	if (selectedEntities.size() == 1) {
		qGreyhoundResource *r = dynamic_cast<qGreyhoundResource*>(selectedEntities.at(0));
		m_download_bounding_box->setEnabled(r != nullptr);
	}
	else {
		m_download_bounding_box->setEnabled(false);
	}
}


QList<QAction*> qGreyhound::getActions()
{
	if (!m_connect_to_resource) {
		m_connect_to_resource = new QAction("Connect to resource", this);
		m_connect_to_resource->setToolTip("Connect to a Greyhound resource");
		m_connect_to_resource->setIcon(getIcon());
		connect(m_connect_to_resource, &QAction::triggered, this, &qGreyhound::connect_to_resource);
	}

	if (!m_download_bounding_box) {
		m_download_bounding_box = new QAction("Download Bbox", this);
		m_download_bounding_box->setToolTip("Download points in a bounding box from a resource");
		m_download_bounding_box->setIcon(QIcon(":/CC/plugin/qGreyhound/dl_icon.png"));
		connect(m_download_bounding_box, &QAction::triggered, this, &qGreyhound::download_bounding_box);
	}

	return QList<QAction*> { m_download_bounding_box, m_connect_to_resource };
}

std::vector<QString> ask_for_dimensions(std::vector<QString> available_dims) 
{
	QEventLoop loop;
	DimensionDialog dm(available_dims);
	dm.show();
	QObject::connect(&dm, &DimensionDialog::finished, &loop, &QEventLoop::quit);
	loop.exec();
	return dm.checked_dimensions();
}

pdal::greyhound::Bounds ask_for_bbox()
{
	QEventLoop loop;
	Ui::BboxDialog ui;
	QDialog d;
	ui.setupUi(&d);
	QObject::connect(&d, &QDialog::finished, &loop, &QEventLoop::quit);
	d.show();
	loop.exec();

	if (ui.xmin->text().isEmpty() ||
		ui.ymin->text().isEmpty() ||
		ui.xmax->text().isEmpty() ||
		ui.ymax->text().isEmpty())
	{
		return pdal::greyhound::Bounds();
	}
	return pdal::greyhound::Bounds(
			ui.xmin->text().toDouble(),
			ui.ymin->text().toDouble(),
			ui.xmax->text().toDouble(),
			ui.ymax->text().toDouble()
		);
}



void qGreyhound::connect_to_resource()
{
	bool ok = false;
	QString text = QInputDialog::getText(
		(QWidget*)m_app->getMainWindow(), 
		tr("Connect to Greyhound"),
		tr("Greyhound ressource url"), 
		QLineEdit::Normal,
		"http://<url>:<port>/resource/<resource_name>",
		&ok
	);

	if (!ok) {
		m_app->dispToConsole("[qGreyhound] canceled by user");
		return;
	}

	const QUrl url(text);

	if (!url.isValid()) {
		m_app->dispToConsole(QString("The Url '%1' it not valid").arg(text), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	qGreyhoundResource *resource(nullptr);

	try {
		resource = new qGreyhoundResource(url);
	}
	catch (const std::exception& e) {
		m_app->dispToConsole(e.what(), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	m_app->addToDB(resource);
}

void qGreyhound::download_bounding_box()
{
	assert(m_app);
	
	const auto& selected_ent = m_app->getSelectedEntities();
	qGreyhoundResource *r = dynamic_cast<qGreyhoundResource*>(selected_ent.at(0));
	if (!r) {
		return;
	}

	uint32_t start_level = r->info().base_depth();
	uint32_t curr_octree_lvl = start_level;
	std::vector<QString> available_dims(std::move(r->info().available_dim_name()));
	std::vector<QString> requested_dims(std::move(ask_for_dimensions(available_dims)));

	Json::Value dims(Json::arrayValue);
	for (const QString& name : requested_dims) {
		dims.append(Json::Value(name.toStdString()));
	}


	pdal::greyhound::Bounds bounds(1415593.910970612, 4184752.4613910406, 1415620.5006109416, 4184732.482818023);
	//pdal::greyhound::Bounds bounds = ask_for_bbox();
	//if (bounds.empty()) {
	//	m_app->dispToConsole("Empty bbox");
	//	return;
	//}
		

	CCVector3d shift = r->info().bounds_conforming_min();
	PDALConverter converter;
	converter.set_shift(shift);
	pdal::Options opts;
	opts.add("url", r->url().toString().toStdString());
	opts.add("dims", dims);

	ccPointCloud *cloud = nullptr;
	while (true)
	{
		pdal::Options q_opts(opts);
		q_opts.add("depth_begin", curr_octree_lvl);
		q_opts.add("depth_end", curr_octree_lvl + 1);
		q_opts.add("bounds", bounds.toJson());

		std::unique_ptr<ccPointCloud> downloaded_cloud(nullptr);
		try {
			downloaded_cloud = download_and_convert_cloud_threaded(q_opts, converter);
		}
		catch (const std::exception& e) {
			m_app->dispToConsole(QString("[qGreyhound] %1").arg(e.what()), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}

		unsigned int nb_pts_received = downloaded_cloud->size();

		if (curr_octree_lvl == start_level) {
			cloud = downloaded_cloud.release();
			cloud->setName("Base");
			r->addChild(cloud);
			m_app->addToDB(cloud, true);
		}
		else {
			cloud->append(downloaded_cloud.release(), cloud->size());
			cloud->prepareDisplayForRefresh();
			cloud->refreshDisplay();
		}
		m_app->updateUI();
		curr_octree_lvl++;

		if (nb_pts_received > MAX_PTS_QUERY) {
			break;
		}
	}

	GreyhoundDownloader downloader(opts, curr_octree_lvl, bounds, converter);
	try {
		QFutureWatcher<void> d;
		QEventLoop loop;
		d.setFuture(QtConcurrent::run([&downloader, cloud]() { downloader.download_to(cloud, GreyhoundDownloader::QUADTREE, []() {}); }));
		QObject::connect(&d, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
		loop.exec();
		d.waitForFinished();
	}
	catch (const std::exception& e) {
		m_app->dispToConsole(QString("[qGreyhound] %1").arg(e.what()), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}
}


QIcon qGreyhound::getIcon() const
{
	return QIcon(":/CC/plugin/qGreyhound/icon.png");
}
