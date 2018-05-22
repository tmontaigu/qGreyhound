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
		m_download_bounding_box->setIcon(getIcon());
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
	//m_app should have already been initialized by CC when plugin is loaded!
	//(--> pure internal check)
	assert(m_app);
	
	const auto& selected_ent = m_app->getSelectedEntities();
	qGreyhoundResource *r = dynamic_cast<qGreyhoundResource*>(selected_ent.at(0));
	if (!r) {
		return;
	}

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


		
	uint32_t curr_octree_lvl = r->info().base_depth();

	ccPointCloud *cloud = new ccPointCloud("Greyhound");
	m_app->addToDB(cloud);
	r->addChild(cloud);

	while (true) {
		std::exception_ptr eptr(nullptr);
		pdal::PointTable table;
		pdal::PointViewSet view_set;
		pdal::PointViewPtr view_ptr;

		pdal::Options opts;
		pdal::GreyhoundReader reader;
		opts.add("url", r->url().toString().toStdString());
		opts.add("depth_begin", curr_octree_lvl);
		opts.add("depth_end", curr_octree_lvl + 1);
		opts.add("bounds", bounds.toJson());
		opts.add("dims", dims);

		reader.addOptions(opts);

		auto pdal_download = [&]() {
			try {
				reader.prepare(table);
				view_set = reader.execute(table);
			}
			catch (...) {
				eptr = std::current_exception();
			}
		};


		QFutureWatcher<void> downloader;
		QEventLoop loop;
		downloader.setFuture(QtConcurrent::run(pdal_download));
		QObject::connect(&downloader, SIGNAL(finished()), &loop, SLOT(quit()));
		loop.exec();
		downloader.waitForFinished();

		try {
			if (eptr) {
				std::rethrow_exception(eptr);
			}
		}
		catch (const std::exception& e) {
			m_app->dispToConsole(QString("[qGreyhound] %1").arg(e.what()), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}
		view_ptr = *view_set.begin();
		if (view_ptr->size() == 0) {
			break;
		}
		m_app->dispToConsole(QString("[qGreyhound] We got a cloud compare cloud with %1 points").arg(view_ptr->size()));
		PDALConverter converter;
		auto cloud_ptr = converter.convert(view_ptr, table.layout());
		if (!cloud_ptr) {
			m_app->dispToConsole(QString("[qGreyhound] Something went wrong when converting the cloud"));
			return;
		}

		// In case the user deletes the cloud from the DB Tree
		// while data is still being added
		if (cloud) {
			cloud->append(cloud_ptr.release(), cloud->size());
			cloud->prepareDisplayForRefresh();
			cloud->refreshDisplay();
			m_app->updateUI();
		}
		else {
			m_app->dispToConsole("[qGreyhound] User deleted cloud, data downloading stops...");
			return;
		}
		curr_octree_lvl++;
	}
}


//This method should return the plugin icon (it will be used mainly
//if your plugin as several actions in which case CC will create a
//dedicated sub-menu entry with this icon.
QIcon qGreyhound::getIcon() const
{
	//open qGreyhound.qrc (text file), update the "prefix" and the
	//icon(s) filename(s). Then save it with the right name (yourPlugin.qrc).
	//(eventually, remove the original qGreyhound.qrc file!)
	return QIcon(":/CC/plugin/qGreyhound/icon.png");
}
