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
//#                             COPYRIGHT: XXX                             #
//#                                                                        #
//##########################################################################

#include "qGreyhound.h"

// Qt
#include <QtGui>
#include <QInputDialog>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <QDataStream>

#include <array>

#include <GreyhoundReader.hpp>
#include <bounds.hpp>
#include <cmath>

#include <ccHObject.h>

qGreyhound::qGreyhound(QObject* parent/*=0*/)
	: QObject(parent)
	, ccStdPluginInterface(":/CC/plugin/qGreyhound/info.json")
	, m_action(nullptr)
	, m_getNextOctreeLevel(nullptr)
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
		//connect appropriate signal
		connect(m_action, SIGNAL(triggered()), this, SLOT(doAction()));
	}

	if (!m_getNextOctreeLevel)
	{
		m_getNextOctreeLevel = new QAction("Get next octree level", this);
		connect(m_getNextOctreeLevel, SIGNAL(triggered()), this, SLOT(getNextOctreeLevel()));
	}

	return QList<QAction*> { m_action, m_getNextOctreeLevel };
}


int convert_view_to_cloud(pdal::PointViewPtr view, ccPointCloud *cloud)
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

	return view->size();
}


//This is an example of an action's slot called when the corresponding action
//is triggered (i.e. the corresponding icon or menu entry is clicked in CC's
//main interface). You can access to most of CC components (database,
//3D views, console, etc.) via the 'm_app' attribute (ccMainAppInterface
//object).
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
	QEventLoop loop;
	QNetworkAccessManager qnam;
	QNetworkRequest req;
	QNetworkReply *reply = nullptr;

	if (!url.isValid()) {
		m_app->dispToConsole(QString("The Url '%1' doesn't look valid").arg(text), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	m_resource.set_url(url);
	QJsonObject infos;
	QJsonObject count;

	try {
		infos = m_resource.info_query();
	}
	catch (const GreyhoundExc& e) {
		m_app->dispToConsole(e.message(), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	m_curr_octree_lvl = infos.value("baseDepth").toInt();
	m_app->dispToConsole(QString("baseDepth: %1").arg(m_curr_octree_lvl), ccMainAppInterface::STD_CONSOLE_MESSAGE);
	m_app->dispToConsole(QString("Downloading points of cloud (octree lvl %1)").arg(m_curr_octree_lvl), ccMainAppInterface::STD_CONSOLE_MESSAGE);
	
	pdal::greyhound::Bounds bounds(1415593.910970612, 4184752.4613910406, 1415620.5006109416, 4184732.482818023);
	m_app->dispToConsole(QString("AREA: %1").arg(bounds.area()), ccMainAppInterface::STD_CONSOLE_MESSAGE);
		
	pdal::GreyhoundReader reader;
	pdal::Options opts;
	opts.add("url", text.toStdString());
	//opts.add("depth_begin", m_curr_octree_lvl);
	//opts.add("depth_end", m_curr_octree_lvl + 1);
	opts.add("bounds", bounds.toJson());

	Json::Value dims(Json::arrayValue);
	dims.append(Json::Value("X"));
	dims.append(Json::Value("Y"));
	dims.append(Json::Value("Z"));
	opts.add("dims", dims);

	reader.addOptions(opts);

	pdal::PointTable table;
	pdal::PointViewPtr view_ptr;

	try {
		reader.prepare(table);
		m_app->dispToConsole(QString("PrepareTable"));
	}
	catch (const std::exception &e) {
		m_app->dispToConsole(QString("%1").arg(e.what()));
		return;
	}

	try {
		pdal::PointViewSet view_set = reader.execute(table);
		view_ptr = *view_set.begin();
		m_app->dispToConsole(QString("[qGreyhound] We got a cloud compare cloud with %1 points").arg(view_ptr->size()), ccMainAppInterface::STD_CONSOLE_MESSAGE);
	}
	catch (const std::exception &e) {
		m_app->dispToConsole(e.what(), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	m_cloud = new ccPointCloud("Greyhound");
	convert_view_to_cloud(view_ptr, m_cloud);
	m_app->addToDB(m_cloud);
}

void qGreyhound::getNextOctreeLevel()
{
	QByteArray points_data;
	try {
		QUrlQuery options;
		options.addQueryItem("depth", QString::number(++m_curr_octree_lvl));
		points_data = m_resource.read_query(options);
	}
	catch (const GreyhoundExc& e) {
		m_app->dispToConsole(e.message(), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	ccPointCloud *new_level = new ccPointCloud();
	//int num_pts_recieved = add_data_to_cloud(new_level, points_data);
	m_cloud->append(new_level, m_cloud->size());
	m_cloud->prepareDisplayForRefresh();
	m_cloud->refreshDisplay();
	m_app->updateUI();
}


//This method should return the plugin icon (it will be used mainly
//if your plugin as several actions in which case CC will create a
//dedicated sub-menu entry with this icon.
QIcon qGreyhound::getIcon() const
{
	//open qGreyhound.qrc (text file), update the "prefix" and the
	//icon(s) filename(s). Then save it with the right name (yourPlugin.qrc).
	//(eventually, remove the original qGreyhound.qrc file!)
	return QIcon(":/CC/plugin/qGreyHound/icon.png");
}
