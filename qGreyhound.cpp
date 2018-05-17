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

#include "ui_bbox_form.h"

qGreyhound::qGreyhound(QObject* parent/*=0*/)
	: QObject(parent)
	, ccStdPluginInterface(":/CC/plugin/qGreyhound/info.json")
	, m_action(nullptr)
	, m_cloud(nullptr)
	, m_resource()
{
}

//This method should enable or disable each plugin action
//depending on the currently selected entities ('selectedEntities').
//For example: if none of the selected entities is a cloud, and your
//plugin deals only with clouds, call 'm_action->setEnabled(false)'
void qGreyhound::onNewSelection(const ccHObject::Container& selectedEntities)
{
	//if (m_action)
	//	m_action->setEnabled(!selectedEntities.empty());
}


//This method returns all 'actions' of your plugin.
//It will be called only once, when plugin is loaded.
QList<QAction*> qGreyhound::getActions()
{
	//default action (if it has not been already created, it's the moment to do it)
	if (!m_action)
	{
		//here we use the default plugin name, description and icon,
		//but each action can have its own!
		m_action = new QAction("Download box", this);
		m_action->setToolTip(getDescription());
		m_action->setIcon(getIcon());
		connect(m_action, SIGNAL(triggered()), this, SLOT(doAction()));
	}

	return QList<QAction*> { m_action };
}


int convert_view_to_cloud(pdal::PointViewPtr view, pdal::PointLayoutPtr layout, ccPointCloud *cloud)
{
	if (!cloud) {
		return -1;
	}

	if (!cloud->reserve(cloud->size() + view->size())) {
		return -1;
	}

	CCVector3d bbmin{
		std::numeric_limits<double>::max(),
		std::numeric_limits<double>::max(),
		std::numeric_limits<double>::max()
	};

	for (size_t i = 0; i < view->size(); ++i) {
		bbmin[0] = std::fmin(bbmin[0], view->getFieldAs<double>(pdal::Dimension::Id::X, i));
		bbmin[1] = std::fmin(bbmin[1], view->getFieldAs<double>(pdal::Dimension::Id::Y, i));
		bbmin[2] = std::fmin(bbmin[2], view->getFieldAs<double>(pdal::Dimension::Id::Z, i));
	}

	for (size_t i = 0; i < view->size(); ++i) {
		cloud->addPoint(CCVector3(
			view->getFieldAs<double>(pdal::Dimension::Id::X, i) - bbmin[0],
			view->getFieldAs<double>(pdal::Dimension::Id::Y, i) - bbmin[1],
			view->getFieldAs<double>(pdal::Dimension::Id::Z, i) - bbmin[2]
		));
	}

	pdal::Dimension::IdList color_ids{ pdal::Dimension::Id::Red, pdal::Dimension::Id::Green, pdal::Dimension::Id::Blue };
	bool has_all_colors = true;
	for (auto color_id : color_ids) {
		if (!view->hasDim(color_id)) {
			has_all_colors = false;
			break;
		}
	}

	if (has_all_colors) {
		if (!cloud->reserveTheRGBTable()) {
			ccLog::Error("Failed to allocate memory for the colors.");
		}
		
		std::array<uint16_t, 3> rgb_16 { 0,0,0 };
		std::array<ColorCompType, 3> rgb { 0,0,0 };
		for (size_t i = 0; i < view->size(); ++i) {
			rgb_16[0] = view->getFieldAs<uint16_t>(pdal::Dimension::Id::Red, i);
			rgb_16[1] = view->getFieldAs<uint16_t>(pdal::Dimension::Id::Green, i);
			rgb_16[2] = view->getFieldAs<uint16_t>(pdal::Dimension::Id::Blue, i);

			//TODO: Some resources may not have colors coded on the 16 bits
			// thus not requiring the shift
			rgb[0] = static_cast<ColorCompType>(rgb_16[0] >> 8);
			rgb[1] = static_cast<ColorCompType>(rgb_16[1] >> 8);
			rgb[2] = static_cast<ColorCompType>(rgb_16[2] >> 8);
			cloud->addRGBColor(rgb.data());
		}
		cloud->showColors(true);
	}


	for (pdal::Dimension::Id id : view->dims()) {
		if (id == pdal::Dimension::Id::X ||
			id == pdal::Dimension::Id::Y ||
			id == pdal::Dimension::Id::Z) {
			continue;
		}
		ccScalarField *sf = new ccScalarField(layout->dimName(id).c_str());
		sf->reserve(view->size());

		for (size_t i = 0; i < view->size(); ++i) {
			ScalarType value = view->getFieldAs<ScalarType>(id, i);
			sf->addElement(value);

		}

		sf->computeMinAndMax();

		int sf_index = cloud->addScalarField(sf);
		if (id == pdal::Dimension::Id::Intensity) {
			sf->setColorScale(ccColorScalesManager::GetDefaultScale(ccColorScalesManager::GREY));
			cloud->setCurrentDisplayedScalarField(sf_index);
			cloud->showSF(true);
		}

		sf->link();
	}

	return view->size();
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

void qGreyhound::doAction()
{
	//m_app should have already been initialized by CC when plugin is loaded!
	//(--> pure internal check)
	assert(m_app);
	if (!m_app) {
		return;
	}

	bool ok = false;
	QString text = QInputDialog::getText(
		(QWidget*)m_app->getMainWindow(), 
		tr("QInputDialog::getText()"),
		tr("Greyhound ressource url"), 
		QLineEdit::Normal,
		"http://<url>:<port>/resource/<resource_name>",
		&ok
	);

	if (!ok) {
		m_app->dispToConsole("[qGreyhound] canceled by user");
		return;
	}

	if (text.isEmpty()) {
		m_app->dispToConsole("You have to enter an url", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	const QUrl url(text);

	if (!url.isValid()) {
		m_app->dispToConsole(QString("The Url '%1' doesn't look valid").arg(text), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	m_resource.set_url(url);
	QJsonObject infos;

	try {
		infos = m_resource.info_query();
	}
	catch (const GreyhoundExc& e) {
		m_app->dispToConsole(e.message(), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	QJsonArray schema = infos.value("schema").toArray();
	std::vector<QString> available_dims;
	for (const auto dim : schema) {
		QJsonObject dim_infos = dim.toObject();
		available_dims.push_back(dim_infos.value("name").toString());
	}

	Json::Value dims(Json::arrayValue);
	std::vector<QString> requested_dims(std::move(ask_for_dimensions(available_dims)));
	for (const QString& name : requested_dims) {
		dims.append(Json::Value(name.toStdString()));
	}


	//pdal::greyhound::Bounds bounds(1415593.910970612, 4184752.4613910406, 1415620.5006109416, 4184732.482818023);
	pdal::greyhound::Bounds bounds = ask_for_bbox();
	if (bounds.empty()) {
		m_app->dispToConsole("Empty bbox");
		return;
	}


	uint32_t curr_octree_lvl = infos.value("baseDepth").toInt();

	m_cloud = new ccPointCloud("Greyhound");
	m_app->addToDB(m_cloud);


	while (true) {
		std::exception_ptr eptr(nullptr);
		pdal::PointTable table;
		pdal::PointViewSet view_set;
		pdal::PointViewPtr view_ptr;

		pdal::Options opts;
		pdal::GreyhoundReader reader;
		opts.add("url", text.toStdString());
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
		ccPointCloud *new_level = new ccPointCloud();
		convert_view_to_cloud(view_ptr, table.layout(), new_level);
		m_cloud->append(new_level, m_cloud->size());
		m_cloud->prepareDisplayForRefresh();
		m_cloud->refreshDisplay();
		m_app->updateUI();

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
