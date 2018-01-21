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
#include "GreyhoundResource.h"

// Qt
#include <QtGui>
#include <QInputDialog>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <QDataStream>

// CloudCompare
#include <ccPointCloud.h>

#include <array>


//Default constructor: should mainly be used to initialize
//actions (pointers) and other members
qGreyhound::qGreyhound(QObject* parent/*=0*/)
	: QObject(parent)
	, m_action(0)
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
void qGreyhound::getActions(QActionGroup& group)
{
	//default action (if it has not been already created, it's the moment to do it)
	if (!m_action)
	{
		//here we use the default plugin name, description and icon,
		//but each action can have its own!
		m_action = new QAction(getName(),this);
		m_action->setToolTip(getDescription());
		m_action->setIcon(getIcon());
		//connect appropriate signal
		connect(m_action, SIGNAL(triggered()), this, SLOT(doAction()));
	}

	group.addAction(m_action);
}

void info_querry(const QUrl& resource_url)
{
	QUrl info_url(resource_url.toString() + "/info");

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

	GreyhoundResource r(url);
	QJsonObject infos;
	QJsonObject count;

	try {
		infos = r.info_query();
	}
	catch (const GreyhoundExc& e) {
		m_app->dispToConsole(e.message(), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	int base_depth = infos.value("baseDepth").toInt();
	m_app->dispToConsole(QString("baseDepth: %1").arg(base_depth), ccMainAppInterface::STD_CONSOLE_MESSAGE);

	m_app->dispToConsole(QString("Asking for the point count of the resource"), ccMainAppInterface::STD_CONSOLE_MESSAGE);
	try {
		count = r.count_query();
	}
	catch (const GreyhoundExc& e) {
		m_app->dispToConsole(e.message(), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	uint64_t num_pts = count.value("points").toInt();
	m_app->dispToConsole(QString("[qGreyhound] NumberOfPoints: %1").arg(num_pts), ccMainAppInterface::STD_CONSOLE_MESSAGE);


	m_app->dispToConsole(QString("Downloading points of cloud (octree lvl 8)"), ccMainAppInterface::STD_CONSOLE_MESSAGE);
	QByteArray points_data;
	try {
		//TODO Add Params as QJsonObject
		points_data = r.read_query();
	}
	catch (const GreyhoundExc& e) {
		m_app->dispToConsole(e.message(), ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}
	

	m_app->dispToConsole(QString("[qGreyhound] Recieved %1 bytes").arg(points_data.size()), ccMainAppInterface::STD_CONSOLE_MESSAGE);

	QByteArray data;
	for (size_t i = points_data.size() - 4; i < points_data.size(); ++i) {
		data.append(points_data.at(i));
	}
	
	uint32_t num_pts_recieved = *(uint32_t*) data.constData();
	m_app->dispToConsole(QString("[qGreyhound] Recieved %1 points").arg(num_pts_recieved), ccMainAppInterface::STD_CONSOLE_MESSAGE);

	ccPointCloud *cloud = new ccPointCloud("Greyhound");
	if (!cloud->reserve(num_pts_recieved)) {
		delete cloud;
		m_app->dispToConsole("Not enough memory!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return; 
	}

	QDataStream stream(&points_data, QIODevice::OpenModeFlag::ReadOnly);
	std::array<char, sizeof(double) * 3> buffer;
	double x = 0.0, y = 0.0, z = 0.0;

	for (size_t i = 0; i < num_pts_recieved; ++i) {
	
		stream.readRawData(buffer.data(), buffer.size());
		double *xyz = (double*) buffer.data();
		x = xyz[0];
		y = xyz[1];
		z = xyz[2];

		CCVector3d point(x, y, z);
		cloud->addPoint(CCVector3::fromArray(point.u));
	}
	m_app->dispToConsole(QString("[qGreyhound] We got a cloud compare cloud with %1 points").arg(cloud->size()), ccMainAppInterface::STD_CONSOLE_MESSAGE);

	m_app->addToDB(cloud);
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
