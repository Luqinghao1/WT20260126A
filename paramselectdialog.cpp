/*
 * 文件名: paramselectdialog.cpp
 * 文件作用: 参数选择配置对话框的具体实现
 * 功能描述:
 * 1. 初始化对话框表格，生成参数配置项。
 * 2. [恢复] “滚轮步长”精度恢复为6位小数，以便设置精细的调节步长。
 * 3. [保留] 安装事件过滤器，禁止所有数值输入框响应鼠标滚轮。
 * 4. [保留] collectData 增加空指针检查，防止闪退。
 */

#include "paramselectdialog.h"
#include "ui_paramselectdialog.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QDebug>

ParamSelectDialog::ParamSelectDialog(const QList<FitParameter> &params, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParamSelectDialog),
    m_params(params)
{
    ui->setupUi(this);

    this->setWindowTitle("拟合参数配置");

    // 连接信号槽
    connect(ui->btnOk, &QPushButton::clicked, this, &ParamSelectDialog::onConfirm);
    connect(ui->btnCancel, &QPushButton::clicked, this, &ParamSelectDialog::onCancel);

    ui->btnCancel->setAutoDefault(false);

    // 初始化表格
    initTable();
}

ParamSelectDialog::~ParamSelectDialog()
{
    delete ui;
}

// 事件过滤器：拦截 SpinBox 的滚轮事件
bool ParamSelectDialog::eventFilter(QObject *obj, QEvent *event)
{
    // 如果是滚轮事件，且目标是 QDoubleSpinBox (或其子控件)，则拦截（不处理，返回 true）
    if (event->type() == QEvent::Wheel) {
        if (qobject_cast<QAbstractSpinBox*>(obj)) {
            return true; // 拦截事件，禁止滚轮调节数值
        }
    }
    return QDialog::eventFilter(obj, event);
}

void ParamSelectDialog::initTable()
{
    QStringList headers;
    headers << "显示" << "参数名称" << "当前数值" << "单位" << "拟合变量" << "下限" << "上限" << "滚轮步长";
    ui->tableWidget->setColumnCount(headers.size());
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    ui->tableWidget->setRowCount(m_params.size());

    // 复选框样式
    QString checkBoxStyle =
        "QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #cccccc; border-radius: 3px; background-color: white; }"
        "QCheckBox::indicator:checked { background-color: #0078d7; border-color: #0078d7; }"
        "QCheckBox::indicator:hover { border-color: #0078d7; }";

    for(int i = 0; i < m_params.size(); ++i) {
        const FitParameter& p = m_params[i];

        // 0. 显示勾选框
        QWidget* pWidgetVis = new QWidget();
        QHBoxLayout* pLayoutVis = new QHBoxLayout(pWidgetVis);
        QCheckBox* chkVis = new QCheckBox();
        chkVis->setChecked(p.isVisible);
        chkVis->setStyleSheet(checkBoxStyle);
        pLayoutVis->addWidget(chkVis);
        pLayoutVis->setAlignment(Qt::AlignCenter);
        pLayoutVis->setContentsMargins(0,0,0,0);
        ui->tableWidget->setCellWidget(i, 0, pWidgetVis);

        // 1. 参数名称 (只读)
        QString displayNameFull = QString("%1 (%2)").arg(p.displayName).arg(p.name);
        QTableWidgetItem* nameItem = new QTableWidgetItem(displayNameFull);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, p.name);
        ui->tableWidget->setItem(i, 1, nameItem);

        // 2. 数值 (禁止滚轮)
        QDoubleSpinBox* spinVal = new QDoubleSpinBox();
        spinVal->setRange(-9e9, 9e9);
        spinVal->setDecimals(6);
        spinVal->setValue(p.value);
        spinVal->setFrame(false);
        spinVal->installEventFilter(this); // 安装过滤器
        ui->tableWidget->setCellWidget(i, 2, spinVal);

        // 3. 单位
        QString dummy, dummy2, dummy3, unitStr;
        FittingParameterChart::getParamDisplayInfo(p.name, dummy, dummy2, dummy3, unitStr);
        if(unitStr == "无因次" || unitStr == "小数") unitStr = "-";
        QTableWidgetItem* unitItem = new QTableWidgetItem(unitStr);
        unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 3, unitItem);

        // 4. 拟合勾选框
        QWidget* pWidgetFit = new QWidget();
        QHBoxLayout* pLayoutFit = new QHBoxLayout(pWidgetFit);
        QCheckBox* chkFit = new QCheckBox();
        chkFit->setChecked(p.isFit);
        chkFit->setStyleSheet(checkBoxStyle);
        pLayoutFit->addWidget(chkFit);
        pLayoutFit->setAlignment(Qt::AlignCenter);
        pLayoutFit->setContentsMargins(0,0,0,0);
        ui->tableWidget->setCellWidget(i, 4, pWidgetFit);

        // 联动逻辑
        connect(chkFit, &QCheckBox::checkStateChanged, [chkVis](Qt::CheckState state){
            if (state == Qt::Checked) {
                chkVis->setChecked(true);
                chkVis->setEnabled(false);
                chkVis->setStyleSheet("QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #ccc; border-radius: 3px; background-color: #e0e0e0; } "
                                      "QCheckBox::indicator:checked { background-color: #80bbeb; border-color: #80bbeb; }");
            } else {
                chkVis->setEnabled(true);
                chkVis->setStyleSheet(
                    "QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #cccccc; border-radius: 3px; background-color: white; }"
                    "QCheckBox::indicator:checked { background-color: #0078d7; border-color: #0078d7; }"
                    "QCheckBox::indicator:hover { border-color: #0078d7; }"
                    );
            }
        });

        if (p.isFit) {
            chkVis->setChecked(true);
            chkVis->setEnabled(false);
            chkVis->setStyleSheet("QCheckBox::indicator { width: 20px; height: 20px; border: 1px solid #ccc; border-radius: 3px; background-color: #e0e0e0; } "
                                  "QCheckBox::indicator:checked { background-color: #80bbeb; border-color: #80bbeb; }");
        }

        // 5. 下限 (禁止滚轮)
        QDoubleSpinBox* spinMin = new QDoubleSpinBox();
        spinMin->setRange(-9e9, 9e9);
        spinMin->setDecimals(6);
        spinMin->setValue(p.min);
        spinMin->setFrame(false);
        spinMin->installEventFilter(this); // 安装过滤器
        ui->tableWidget->setCellWidget(i, 5, spinMin);

        // 6. 上限 (禁止滚轮)
        QDoubleSpinBox* spinMax = new QDoubleSpinBox();
        spinMax->setRange(-9e9, 9e9);
        spinMax->setDecimals(6);
        spinMax->setValue(p.max);
        spinMax->setFrame(false);
        spinMax->installEventFilter(this); // 安装过滤器
        ui->tableWidget->setCellWidget(i, 6, spinMax);

        // 7. 滚轮步长 (恢复为6位小数，禁止滚轮)
        QDoubleSpinBox* spinStep = new QDoubleSpinBox();
        spinStep->setRange(0.0, 10000.0); // 步长需大于等于0
        spinStep->setDecimals(6);         // [修改] 恢复6位小数精度
        spinStep->setValue(p.step);
        spinStep->setFrame(false);
        spinStep->installEventFilter(this); // 安装过滤器
        ui->tableWidget->setCellWidget(i, 7, spinStep);
    }

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void ParamSelectDialog::collectData()
{
    // [保留修复] 增加健壮性检查，防止 nullptr 导致的 Crash
    for(int i = 0; i < ui->tableWidget->rowCount(); ++i) {
        if(i >= m_params.size()) break;

        // 0. 显示
        QWidget* wVis = ui->tableWidget->cellWidget(i, 0);
        if (wVis) {
            QCheckBox* chkVis = wVis->findChild<QCheckBox*>();
            if(chkVis) m_params[i].isVisible = chkVis->isChecked();
        }

        // 2. 数值
        QDoubleSpinBox* spinVal = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 2));
        if(spinVal) m_params[i].value = spinVal->value();

        // 4. 拟合选择
        QWidget* wFit = ui->tableWidget->cellWidget(i, 4);
        if (wFit) {
            QCheckBox* chkFit = wFit->findChild<QCheckBox*>();
            if(chkFit) m_params[i].isFit = chkFit->isChecked();
        }

        // 5. 下限
        QDoubleSpinBox* spinMin = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 5));
        if(spinMin) m_params[i].min = spinMin->value();

        // 6. 上限
        QDoubleSpinBox* spinMax = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 6));
        if(spinMax) m_params[i].max = spinMax->value();

        // 7. 步长
        QDoubleSpinBox* spinStep = qobject_cast<QDoubleSpinBox*>(ui->tableWidget->cellWidget(i, 7));
        if(spinStep) m_params[i].step = spinStep->value();
    }
}

QList<FitParameter> ParamSelectDialog::getUpdatedParams() const
{
    return m_params;
}

void ParamSelectDialog::onConfirm()
{
    collectData();
    accept();
}

void ParamSelectDialog::onCancel()
{
    reject();
}
