#pragma once

#include <QDialogButtonBox>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

class DimensionDialog : public QDialog
{
	Q_OBJECT

public:
	explicit DimensionDialog(const std::vector<QString>& names, QWidget *parent = 0);
	std::vector<QString> checked_dimensions();

public slots:
	void highlightChecked(QListWidgetItem* item);

private:
	QListWidget * widget;
	QDialogButtonBox* buttonBox;
	QGroupBox* viewBox;
	QPushButton* saveButton;
	QPushButton* closeButton;

	void createListWidget(const std::vector<QString>& names);
	void createOtherWidgets();
	void createLayout();
	void createConnections();
};