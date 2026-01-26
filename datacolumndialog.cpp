/*
 * 文件名: datacolumndialog.cpp
 * 文件作用: 列定义对话框实现文件
 * 功能描述:
 * 1. 动态生成列配置行（下拉框模式）。
 * 2. 实现了类型与单位的联动逻辑。
 * 3. 样式表优化为白底黑字。
 */

#include "datacolumndialog.h"
#include "ui_datacolumndialog.h"
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDebug>

DataColumnDialog::DataColumnDialog(const QStringList& columnNames,
                                   const QList<ColumnDefinition>& definitions,
                                   QWidget* parent)
    : QDialog(parent), ui(new Ui::DataColumnDialog), m_columnNames(columnNames), m_definitions(definitions)
{
    ui->setupUi(this);
    this->setWindowTitle("列属性定义");
    this->setStyleSheet(getStyleSheet());

    connect(ui->btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->btnPreset, &QPushButton::clicked, this, &DataColumnDialog::onLoadPresetClicked);
    connect(ui->btnReset, &QPushButton::clicked, this, &DataColumnDialog::onResetClicked);

    if (m_definitions.isEmpty()) {
        for (const QString& name : columnNames) {
            ColumnDefinition def; def.name = name; def.type = WellTestColumnType::Custom;
            m_definitions.append(def);
        }
    }
    setupColumnRows();
}

DataColumnDialog::~DataColumnDialog()
{
    delete ui;
}

QString DataColumnDialog::getStyleSheet() const
{
    return "QDialog, QWidget { background-color: #ffffff; color: #000000; }"
           "QLabel { color: #000000; font-size: 14px; }"
           "QComboBox { background-color: #ffffff; color: #000000; border: 1px solid #999999; padding: 2px; }"
           "QComboBox QAbstractItemView { background-color: #ffffff; color: #000000; selection-background-color: #e0e0e0; selection-color: #000000; }"
           "QCheckBox { color: #000000; }"
           "QPushButton { background-color: #f0f0f0; color: #000000; border: 1px solid #999999; padding: 5px 15px; border-radius: 3px; }"
           "QPushButton:hover { background-color: #e0e0e0; }";
}

void DataColumnDialog::setupColumnRows()
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(ui->scrollContent->layout());
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    layout->setSpacing(10);
    layout->setContentsMargins(10, 10, 10, 10);

    // 新增了 "套压" 和 "流压"
    QStringList typeNames = {
        "序号", "日期", "时刻", "时间", "压力", "套压", "流压",
        "温度", "流量", "深度", "粘度", "密度",
        "渗透率", "孔隙度", "井半径", "表皮系数", "距离", "体积", "压降", "自定义"
    };

    // 自定义类型的索引，根据上面的列表长度变化，需重新计算，现在是列表最后一个索引 19
    int customTypeIndex = typeNames.size() - 1;

    for (int i = 0; i < m_columnNames.size(); ++i) {
        QWidget* rowWidget = new QWidget;
        QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(10);

        QLabel* originalNameLabel = new QLabel(QString("列 %1: %2").arg(i+1).arg(m_columnNames[i]));
        originalNameLabel->setFixedWidth(180);
        originalNameLabel->setStyleSheet("font-weight: bold; color: black;");
        rowLayout->addWidget(originalNameLabel);

        QComboBox* typeCombo = new QComboBox;
        typeCombo->addItems(typeNames);
        typeCombo->setFixedWidth(120);

        if (i < m_definitions.size()) {
            typeCombo->setCurrentIndex(static_cast<int>(m_definitions[i].type));
            if (m_definitions[i].type == WellTestColumnType::Custom) {
                typeCombo->setEditable(true);
                QString customName = m_definitions[i].name.split('\\').first();
                if (!customName.isEmpty() && customName != "自定义") {
                    typeCombo->setItemText(customTypeIndex, customName);
                    typeCombo->setCurrentIndex(customTypeIndex);
                }
            } else {
                typeCombo->setEditable(false);
            }
        } else {
            typeCombo->setCurrentIndex(customTypeIndex);
            typeCombo->setEditable(true);
        }

        rowLayout->addWidget(typeCombo);
        m_typeComboBoxes.append(typeCombo);

        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataColumnDialog::onTypeChanged);
        connect(typeCombo, &QComboBox::editTextChanged, this, &DataColumnDialog::onCustomTextChanged);

        QComboBox* unitCombo = new QComboBox;
        unitCombo->setFixedWidth(100);
        rowLayout->addWidget(unitCombo);
        m_unitComboBoxes.append(unitCombo);

        connect(unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataColumnDialog::onUnitChanged);
        connect(unitCombo, &QComboBox::editTextChanged, this, &DataColumnDialog::onCustomTextChanged);

        QCheckBox* requiredCheck = new QCheckBox("必需");
        requiredCheck->setStyleSheet("color: black;");
        if (i < m_definitions.size()) requiredCheck->setChecked(m_definitions[i].isRequired);
        rowLayout->addWidget(requiredCheck);
        m_requiredChecks.append(requiredCheck);

        QLabel* previewLabel = new QLabel;
        previewLabel->setStyleSheet("color: #008000; font-weight: bold;");
        rowLayout->addWidget(previewLabel);
        m_previewLabels.append(previewLabel);

        layout->addWidget(rowWidget);

        WellTestColumnType currentType = static_cast<WellTestColumnType>(typeCombo->currentIndex());
        updateUnitsForType(currentType, unitCombo);

        if (i < m_definitions.size()) {
            QString unit = m_definitions[i].unit;
            int unitIdx = unitCombo->findText(unit);
            if (unitIdx >= 0) unitCombo->setCurrentIndex(unitIdx);
            else if (!unit.isEmpty()) {
                unitCombo->setEditable(true);
                unitCombo->setCurrentText(unit);
            }
        }
        updatePreviewLabel(i);
    }
    layout->addStretch();
}

void DataColumnDialog::onTypeChanged(int index)
{
    QComboBox* senderCombo = qobject_cast<QComboBox*>(sender());
    if (!senderCombo) return;
    int rowIdx = m_typeComboBoxes.indexOf(senderCombo);

    // 自定义类型的索引为 19
    int customTypeIndex = 19;

    if (rowIdx != -1) {
        WellTestColumnType type = static_cast<WellTestColumnType>(index);
        bool isCustomType = (index == customTypeIndex);
        senderCombo->setEditable(isCustomType);
        updateUnitsForType(type, m_unitComboBoxes[rowIdx]);
        updatePreviewLabel(rowIdx);
    }
}

void DataColumnDialog::onUnitChanged(int index)
{
    QComboBox* senderCombo = qobject_cast<QComboBox*>(sender());
    if (!senderCombo) return;
    int rowIdx = m_unitComboBoxes.indexOf(senderCombo);
    if (rowIdx != -1) {
        if (senderCombo->currentText() == "自定义") {
            senderCombo->setEditable(true);
            senderCombo->clearEditText();
        } else {
            senderCombo->setEditable(false);
        }
        updatePreviewLabel(rowIdx);
    }
}

void DataColumnDialog::onCustomTextChanged(const QString& text)
{
    Q_UNUSED(text);
    QWidget* senderWidget = qobject_cast<QWidget*>(sender());
    for(int i=0; i<m_typeComboBoxes.size(); ++i) {
        if (m_typeComboBoxes[i] == senderWidget || m_unitComboBoxes[i] == senderWidget) {
            updatePreviewLabel(i);
            break;
        }
    }
}

void DataColumnDialog::updateUnitsForType(WellTestColumnType type, QComboBox* unitCombo)
{
    unitCombo->blockSignals(true);
    unitCombo->clear();
    unitCombo->setEditable(false);

    switch (type) {
    case WellTestColumnType::SerialNumber: unitCombo->addItems({"-", "自定义"}); break;
    case WellTestColumnType::Date: unitCombo->addItems({"-", "yyyy-MM-dd", "yyyy/MM/dd", "自定义"}); break;
    case WellTestColumnType::TimeOfDay: unitCombo->addItems({"-", "hh:mm:ss", "hh:mm", "自定义"}); break;
    case WellTestColumnType::Time: unitCombo->addItems({"h", "min", "s", "day", "自定义"}); break;

    // 压力、套压、流压、压降共享相同的单位列表
    case WellTestColumnType::Pressure:
    case WellTestColumnType::CasingPressure:
    case WellTestColumnType::BottomHolePressure:
    case WellTestColumnType::PressureDrop: unitCombo->addItems({"MPa", "kPa", "Pa", "psi", "bar", "atm", "自定义"}); break;

    case WellTestColumnType::Temperature: unitCombo->addItems({"°C", "°F", "K", "自定义"}); break;
    case WellTestColumnType::FlowRate: unitCombo->addItems({"m³/d", "m³/h", "L/s", "bbl/d", "自定义"}); break;
    case WellTestColumnType::Depth:
    case WellTestColumnType::Distance: unitCombo->addItems({"m", "ft", "km", "自定义"}); break;
    case WellTestColumnType::Viscosity: unitCombo->addItems({"mPa·s", "cP", "Pa·s", "自定义"}); break;
    case WellTestColumnType::Density: unitCombo->addItems({"kg/m³", "g/cm³", "lb/ft³", "自定义"}); break;
    case WellTestColumnType::Permeability: unitCombo->addItems({"mD", "D", "μm²", "自定义"}); break;
    case WellTestColumnType::Porosity: unitCombo->addItems({"%", "fraction", "自定义"}); break;
    case WellTestColumnType::WellRadius: unitCombo->addItems({"m", "ft", "cm", "in", "自定义"}); break;
    case WellTestColumnType::SkinFactor: unitCombo->addItems({"dimensionless", "自定义"}); break;
    case WellTestColumnType::Volume: unitCombo->addItems({"m³", "L", "bbl", "ft³", "自定义"}); break;
    default: unitCombo->addItems({"-", "自定义"}); break;
    }
    unitCombo->blockSignals(false);
}

void DataColumnDialog::updatePreviewLabel(int index)
{
    QString typeStr = m_typeComboBoxes[index]->currentText();
    QString unitStr = m_unitComboBoxes[index]->currentText();
    if (unitStr == "-" || unitStr.isEmpty() || unitStr == "自定义") {
        m_previewLabels[index]->setText(typeStr);
    } else {
        m_previewLabels[index]->setText(QString("%1\\%2").arg(typeStr).arg(unitStr));
    }
}

void DataColumnDialog::onLoadPresetClicked()
{
    // 自定义索引需更新
    int customIdx = 19;

    for (int i = 0; i < m_columnNames.size(); ++i) {
        QString name = m_columnNames[i].toLower();
        int typeIdx = customIdx; QString unitToSel = "-";

        if (name.contains("序号") || name == "no") { typeIdx = 0; }
        else if (name.contains("日期")) { typeIdx = 1; unitToSel = "yyyy-MM-dd"; }
        else if (name.contains("时刻")) { typeIdx = 2; unitToSel = "hh:mm:ss"; }
        else if (name.contains("时间")) { typeIdx = 3; unitToSel = "h"; }
        else if (name.contains("压力")) { typeIdx = 4; unitToSel = "MPa"; }
        else if (name.contains("流量")) { typeIdx = 8; unitToSel = "m³/d"; } // FlowRate 现在索引是 8
        else if (name.contains("套压")) { typeIdx = 5; unitToSel = "MPa"; } // CasingPressure 索引 5
        else if (name.contains("流压")) { typeIdx = 6; unitToSel = "MPa"; } // BottomHolePressure 索引 6

        m_typeComboBoxes[i]->setCurrentIndex(typeIdx);
        updateUnitsForType(static_cast<WellTestColumnType>(typeIdx), m_unitComboBoxes[i]);
        int uIdx = m_unitComboBoxes[i]->findText(unitToSel);
        if (uIdx >= 0) m_unitComboBoxes[i]->setCurrentIndex(uIdx);
    }
}

void DataColumnDialog::onResetClicked()
{
    int customIdx = 19;
    for (int i = 0; i < m_typeComboBoxes.size(); ++i) {
        m_typeComboBoxes[i]->setCurrentIndex(customIdx);
        m_typeComboBoxes[i]->setEditable(true);
        m_requiredChecks[i]->setChecked(false);
    }
}

QList<ColumnDefinition> DataColumnDialog::getColumnDefinitions() const
{
    QList<ColumnDefinition> result;
    for (int i = 0; i < m_columnNames.size(); ++i) {
        ColumnDefinition def;
        QString typeStr = m_typeComboBoxes[i]->currentText();
        QString unitStr = m_unitComboBoxes[i]->currentText();
        if (unitStr == "-" || unitStr.isEmpty() || unitStr == "自定义") {
            def.name = typeStr; def.unit = "";
        } else {
            def.name = QString("%1\\%2").arg(typeStr).arg(unitStr);
            def.unit = unitStr;
        }
        def.type = static_cast<WellTestColumnType>(m_typeComboBoxes[i]->currentIndex());
        def.isRequired = m_requiredChecks[i]->isChecked();
        result.append(def);
    }
    return result;
}
