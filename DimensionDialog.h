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
	explicit DimensionDialog(const std::vector<QString>& names, QWidget *parent = nullptr);
	std::vector<QString> checked_dimensions();

public slots:
	void highlightChecked(QListWidgetItem* item);

private:
	QListWidget * m_widget{};
	QDialogButtonBox* m_button_box{};
	QGroupBox* m_view_box{};
	QPushButton* m_save_button{};
	QPushButton* m_close_button{};

	void createListWidget(const std::vector<QString>& names);
	void createOtherWidgets();
	void createLayout();
	void createConnections() const;
};