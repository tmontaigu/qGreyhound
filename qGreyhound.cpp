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
#include <QEventLoop>
#include <QtConcurrent>

#include <array>
#include <queue>

#include <GreyhoundReader.hpp>
#include <bounds.hpp>

#include <ccHObject.h>
#include <ccScalarField.h>
#include <ccColorScalesManager.h>

#include "qGreyhound.h"
#include "DimensionDialog.h"
#include "PDALConverter.h"
#include "GreyhoundDownloader.h"
#include "constants.h"

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
		auto *is_ressource = dynamic_cast<ccGreyhoundResource*>(selectedEntities.at(0));
		auto *is_cloud = dynamic_cast<ccGreyhoundCloud*>(selectedEntities.at(0));
		m_download_bounding_box->setEnabled(is_ressource || (is_cloud && is_cloud->state() == ccGreyhoundCloud::IDLE));

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
		m_download_bounding_box->setIcon(QIcon(IconPaths::DownloadIcon));
		connect(m_download_bounding_box, &QAction::triggered, this, &qGreyhound::download_bounding_box);
	}

	return { m_connect_to_resource, m_download_bounding_box };
}

std::vector<QString> ask_for_dimensions(const std::vector<QString>& available_dims)
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
		return {};
	}

	double xmin = ui.xmin->text().toDouble();
	double ymin = ui.ymin->text().toDouble();
	double xmax = ui.xmax->text().toDouble();
	double ymax = ui.ymax->text().toDouble();

	if (xmin > xmax) {
		std::swap(xmin, xmax);
	}

	if (ymin > ymax) {
		std::swap(ymin, ymax);
	}

	return { xmin, ymin, xmax, ymax };
}



void qGreyhound::connect_to_resource() const
{
	auto ok = false;
	const auto text = QInputDialog::getText(
		reinterpret_cast<QWidget*>(m_app->getMainWindow()),
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

	ccGreyhoundResource *resource;
	try {
		resource = new ccGreyhoundResource(url);
	}
	catch (const std::exception& e) {
		m_app->dispToConsole(e.what(), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	m_app->addToDB(resource);
}

void qGreyhound::download_bounding_box() const
{
	assert(m_app);

	const auto& selected_ent = m_app->getSelectedEntities();

	const auto c = dynamic_cast<ccGreyhoundCloud*> (selected_ent.at(0));
	if (c) {
		try
		{
			download_more_dimensions(c);
		}
		catch (const std::exception& e)
		{
			m_app->dispToConsole(QString("[qGreyhound] %1").arg(e.what()), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}
	}

	auto resource = dynamic_cast<ccGreyhoundResource*>(selected_ent.at(0));
	if (!resource) {
		return;
	}

	uint32_t curr_octree_lvl = resource->info().base_depth();
	const auto available_dims(resource->info().available_dim_name());
	const auto requested_dims(ask_for_dimensions(available_dims));
	if (requested_dims.empty()) {
		m_app->dispToConsole("[qGreyhound] no dimensions were selected");
		return;
	}

	Json::Value dims(Json::arrayValue);
	for (const auto& name : requested_dims) {
		dims.append(Json::Value(name.toStdString()));
	}


	auto bounds = ask_for_bbox();
	if (bounds.empty()) {
		m_app->dispToConsole("[qGreyhound] Empty bbox");
		bounds = { 1415593.910970612, 4184732.482818023,1415620.5006109416, 4184752.4613910406, };
	}


	const auto shift = resource->info().bounds_conforming_min();
	PDALConverter converter;
	converter.set_shift(shift);
	pdal::Options opts;
	opts.add("url", resource->url().toString().toStdString());
	opts.add("dims", dims);

	auto cloud = new ccGreyhoundCloud("Cloud (downloading...)");
	cloud->set_state((ccGreyhoundCloud::WAITING_FOR_POINTS));
	// We download the first depth separately here to be able to add it to cc's DB
	{
		pdal::Options q_opts(opts);
		q_opts.add("depth_begin", curr_octree_lvl);
		q_opts.add("depth_end", curr_octree_lvl + 1);
		q_opts.add("bounds", bounds.toJson());

		try {
			download_and_convert_cloud_threaded(cloud, q_opts, converter);
		}
		catch (const std::exception& e) {
			m_app->dispToConsole(QString("[qGreyhound] %1").arg(e.what()), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			return;
		}

		if (cloud->size() == 0) {
			return;
		}

		cloud->set_bbox(bounds);
		cloud->set_origin(resource);
		resource->addChild(cloud);
		m_app->addToDB(cloud, true);
		m_app->updateUI();
		curr_octree_lvl++;
	}

	GreyhoundDownloader downloader(opts, curr_octree_lvl, bounds, converter);
	try {
		QFutureWatcher<void> d;
		QEventLoop loop;
		d.setFuture(QtConcurrent::run([&downloader, cloud]() { downloader.download_to(cloud, GreyhoundDownloader::DEPTH_BY_DEPTH); }));
		QObject::connect(&d, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
		loop.exec();
		d.waitForFinished();
	}
	catch (const std::exception& e) {
		m_app->dispToConsole(QString("[qGreyhound] %1").arg(e.what()), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}
	cloud->setName("Cloud");
	cloud->set_state(ccGreyhoundCloud::IDLE);
	m_app->updateUI();
}


void qGreyhound::download_more_dimensions(ccGreyhoundCloud *cloud) const
{
	if (cloud->state() != ccGreyhoundCloud::State::IDLE)
	{
		m_app->dispToConsole("You have to wait for the current download to finish", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	const auto available(cloud->available_dims());
	std::vector<QString> downloaded;
	std::vector<QString> not_downloaded;


	downloaded.reserve(cloud->getNumberOfScalarFields());

	for (unsigned int i(0); i < cloud->getNumberOfScalarFields(); ++i) {
		downloaded.emplace_back(cloud->getScalarField(i)->getName());
	}

	for (const auto& name : available) {
		if (name == "X" || name == "Y" || name == "Z" || name == "PointId") {
			continue;
		}
		if (std::find(std::begin(downloaded), std::end(downloaded), name) == std::end(downloaded)) {
			not_downloaded.emplace_back(name);
		}
	}

	auto to_be_downloaded = ask_for_dimensions(not_downloaded);
	if (to_be_downloaded.empty()) {
		m_app->dispToConsole("[qGreyhound] no dimensions were selected");
		return;
	}

	Json::Value dims(Json::arrayValue);
	for (const auto& name : to_be_downloaded) {
		dims.append(Json::Value(name.toStdString()));
	}


	PDALConverter converter;
	converter.set_shift(cloud->getGlobalShift());
	pdal::Options opts;
	opts.add("url", cloud->origin()->url().toString().toStdString());
	opts.add("dims", dims);
	opts.add("bounds", cloud->bbox().toJson());

	cloud->set_state(ccGreyhoundCloud::WAITING_FOR_POINTS);
	const auto cloud_name = cloud->getName();
	cloud->setName(cloud_name + " (downloading...)");

	try {
		download_and_convert_cloud_threaded(cloud, opts, converter);
	}
	catch (const std::exception& e) {
		m_app->dispToConsole(QString("[qGreyhound] %1").arg(e.what()), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
	}
	cloud->prepareDisplayForRefresh();
	cloud->redrawDisplay();
	cloud->setName(cloud_name);
	cloud->set_state(ccGreyhoundCloud::IDLE);
	m_app->updateUI();
}

QIcon qGreyhound::getIcon() const
{
	return QIcon(IconPaths::GreyhoundIcon);
}
