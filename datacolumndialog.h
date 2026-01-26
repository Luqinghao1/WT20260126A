/*
 * 文件名: datacolumndialog.h
 * 文件作用: 列定义对话框头文件
 * 功能描述:
 * 1. 定义列属性设置界面类 DataColumnDialog。
 * 2. 采用下拉框(ComboBox)交互模式。
 */

#ifndef DATACOLUMNDIALOG_H
#define DATACOLUMNDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QList>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include "wt_datawidget.h"

namespace Ui {
class DataColumnDialog;
}

class DataColumnDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataColumnDialog(const QStringList& columnNames,
                              const QList<ColumnDefinition>& definitions = QList<ColumnDefinition>(),
                              QWidget* parent = nullptr);

    ~DataColumnDialog();

    QList<ColumnDefinition> getColumnDefinitions() const;

private slots:
    void onTypeChanged(int index);
    void onUnitChanged(int index);
    void onCustomTextChanged(const QString& text);
    void onLoadPresetClicked();
    void onResetClicked();

private:
    Ui::DataColumnDialog *ui;

    void setupColumnRows();
    void updateUnitsForType(WellTestColumnType type, QComboBox* unitCombo);
    void updatePreviewLabel(int index);
    QString getStyleSheet() const;

    QStringList m_columnNames;
    QList<ColumnDefinition> m_definitions;

    QList<QComboBox*> m_typeComboBoxes;
    QList<QComboBox*> m_unitComboBoxes;
    QList<QCheckBox*> m_requiredChecks;
    QList<QLabel*> m_previewLabels;
};

#endif // DATACOLUMNDIALOG_H
