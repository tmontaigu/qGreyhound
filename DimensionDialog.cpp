#include "DimensionDialog.h"

DimensionDialog::DimensionDialog(const std::vector<QString>& names, QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Checkable list in Qt");

	createListWidget(names);
	createOtherWidgets();
	createLayout();
	createConnections();
}

void DimensionDialog::createListWidget(const std::vector<QString>& names) {
	widget = new QListWidget;

	for (const QString& name : names) {
		widget->addItem(name);
	}

	QListWidgetItem* item = 0;
	for (int i = 0; i < widget->count(); ++i) {
		item = widget->item(i);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		
		if (item->text() == "X" || item->text() == "Y" || item->text() == "Z") {
			item->setCheckState(Qt::Checked);
		}
		else {
			item->setCheckState(Qt::Unchecked);
		}
	}
}

void DimensionDialog::createOtherWidgets() {
	viewBox = new QGroupBox(tr("Required components"));
	buttonBox = new QDialogButtonBox;
	saveButton = buttonBox->addButton(QDialogButtonBox::Save);
	closeButton = buttonBox->addButton(QDialogButtonBox::Close);
}

void DimensionDialog::createLayout() {
	QVBoxLayout* viewLayout = new QVBoxLayout;
	viewLayout->addWidget(widget);
	viewBox->setLayout(viewLayout);

	QHBoxLayout* horizontalLayout = new QHBoxLayout;
	horizontalLayout->addWidget(buttonBox);

	QVBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->addWidget(viewBox);
	mainLayout->addLayout(horizontalLayout);

	setLayout(mainLayout);
}

void DimensionDialog::createConnections() {
	QObject::connect(widget, SIGNAL(itemChanged(QListWidgetItem*)),
		this, SLOT(highlightChecked(QListWidgetItem*)));
	QObject::connect(saveButton, SIGNAL(clicked()), this, SLOT(save()));
	QObject::connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
}

void DimensionDialog::highlightChecked(QListWidgetItem *item) {
	if (item->checkState() == Qt::Checked)
		item->setBackgroundColor(QColor("#ffffb2"));
	else
		item->setBackgroundColor(QColor("#ffffff"));
}

void DimensionDialog::save() {

	QFile file("required_components.txt");
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return;

	QTextStream out(&file);
	out << "Required components:" << "\n";

	QListWidgetItem* item = 0;
	for (int i = 0; i < widget->count(); ++i) {
		item = widget->item(i);
		if (item->checkState() == Qt::Checked)
			out << item->text() << "\n";
	}

	QMessageBox::information(this, tr("Checkable list in Qt"),
		tr("Required components were saved."),
		QMessageBox::Ok);
}


std::vector<QString> DimensionDialog::checked_dimensions() {
	QListWidgetItem *item;
	std::vector<QString> checked_items;

	for (int i = 0; i < widget->count(); ++i) {
		item = widget->item(i);
		if (item->checkState() == Qt::Checked) {
			checked_items.push_back(item->text());
		}
	}
	return checked_items;
}