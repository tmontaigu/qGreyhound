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

#ifndef Q_GREYHOUND_PLUGIN_HEADER
#define Q_GREYHOUND_PLUGIN_HEADER

#include <ccPointCloud.h>

//qCC
#include "ccStdPluginInterface.h"

#include "GreyhoundResource.h"

/** Replace the 'qGreyhound' string by your own plugin class name
	and then check 'qGreyhound.cpp' for more directions (you
	have to fill-in the blank methods. The most important one is the
	'getActions' method.  This method should return all actions
	(QAction objects). CloudCompare will automatically add them to an
	icon in the plugin toolbar and to an entry in the plugin menu
	(if your plugin returns several actions, CC will create a dedicated
	toolbar and sub-menu). 
	You are responsible to connect these actions to custom slots of your
	plugin.
	Look at the ccStdPluginInterface::m_app attribute to get access to
	most of CC components (database, 3D views, console, etc.).
**/
class qGreyhound : public QObject, public ccStdPluginInterface
{
	Q_OBJECT
	Q_INTERFACES(ccStdPluginInterface)
	Q_PLUGIN_METADATA(IID "cccorp.cloudcompare.plugin.qGreyhound" FILE "info.json")

public:

	//! Default constructor
	explicit qGreyhound(QObject* parent = 0);
	virtual ~qGreyhound() = default;

	//inherited from ccPluginInterface
	virtual QString getName() const override { return "qGreyhound"; }
	virtual QString getDescription() const override { return "Greyhound things in cloudcompare"; }
	virtual QIcon getIcon() const override;

	//inherited from ccStdPluginInterface
	void onNewSelection(const ccHObject::Container& selectedEntities) override;
	virtual QList<QAction*> getActions() override;

protected slots:

	void doAction();
	void getNextOctreeLevel();

protected:

	//! Default action
	/** You can add as many actions as you want in a plugin.
		All actions will correspond to an icon in the dedicated
		toolbar and an entry in the plugin menu.
	**/
	QAction* m_action;
	QAction* m_getNextOctreeLevel;


	//Greyhound related things
	GreyhoundResource m_resource;
	unsigned int m_curr_octree_lvl;
	ccPointCloud *m_cloud;

};

#endif
