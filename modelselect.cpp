#include "modelselect.h"
#include "ui_modelselect.h"
#include <QDialogButtonBox>
#include <QPushButton>
#include <QDebug>

ModelSelect::ModelSelect(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ModelSelect)
{
    ui->setupUi(this);

    // 设置全局样式为黑色，防止在某些暗色主题下看不清文字
    this->setStyleSheet("QWidget { color: black; font-family: Arial; }");

    initOptions();

    // 连接所有下拉框信号到更新槽，确保任意选项改变都能实时触发逻辑判断
    connect(ui->comboWellbore, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboWell, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboReservoir, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboBoundary, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));

    // 修复"需要点击两次"的问题：覆盖默认的 accept 行为，改用自定义的 onAccepted 槽
    disconnect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ModelSelect::onAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 初始化界面状态
    onSelectionChanged();
}

ModelSelect::~ModelSelect()
{
    delete ui;
}

void ModelSelect::initOptions()
{
    // 清空现有选项，防止重复添加
    ui->comboWellbore->clear();
    ui->comboWell->clear();
    ui->comboReservoir->clear();
    ui->comboBoundary->clear();

    // 1. 井筒模型 (Wellbore)
    // Changing: 变井储 (需要 cD, S 参数)
    // Constant: 恒定井储 (通常指无井储或恒定流量，不需要 cD, S)
    ui->comboWellbore->addItem("变井储", "Changing");
    ui->comboWellbore->addItem("恒定井储", "Constant");

    // 2. 井模型 (Well)
    ui->comboWell->addItem("垂直井", "Vertical");
    ui->comboWell->addItem("水平井", "Horizontal");
    ui->comboWell->addItem("压裂垂直井", "FracVertical");
    ui->comboWell->addItem("压裂水平井", "FracHorizontal");

    // 3. 储层模型 (Reservoir)
    ui->comboReservoir->addItem("均质油藏", "Homogeneous");
    ui->comboReservoir->addItem("双重孔隙介质油藏", "DualPorosity");
    ui->comboReservoir->addItem("复合油藏", "Composite");

    // 4. 边界条件 (Boundary)
    ui->comboBoundary->addItem("无穷大外边界", "Infinite");
    ui->comboBoundary->addItem("封闭边界", "Closed");
    ui->comboBoundary->addItem("定压边界", "ConstantPressure");

    // 默认选中一组有效值 (例如模型1：变井储+压裂水平+复合+无穷大)
    ui->comboWellbore->setCurrentIndex(0); // 变井储
    ui->comboWell->setCurrentIndex(3);     // 压裂水平井
    ui->comboReservoir->setCurrentIndex(2); // 复合油藏
    ui->comboBoundary->setCurrentIndex(0); // 无穷大
}

void ModelSelect::onSelectionChanged()
{
    // 获取四个维度的当前数据
    QString wb = ui->comboWellbore->currentData().toString();
    QString we = ui->comboWell->currentData().toString();
    QString res = ui->comboReservoir->currentData().toString();
    QString bnd = ui->comboBoundary->currentData().toString();

    // 初始化状态
    bool isValid = false;
    m_selectedModelCode = "";
    m_selectedModelName = "";

    // === 核心判断逻辑 ===
    // 基础条件必须满足：压裂水平井 + 复合油藏
    if (we == "FracHorizontal" && res == "Composite") {

        // 分支1：无穷大外边界
        if (bnd == "Infinite") {
            if (wb == "Changing") {
                m_selectedModelCode = "modelwidget1";
                m_selectedModelName = "压裂水平井复合页岩油模型1";
                isValid = true;
            } else if (wb == "Constant") {
                m_selectedModelCode = "modelwidget2";
                m_selectedModelName = "压裂水平井复合页岩油模型2";
                isValid = true;
            }
        }
        // 分支2：封闭边界
        else if (bnd == "Closed") {
            if (wb == "Changing") {
                m_selectedModelCode = "modelwidget3";
                m_selectedModelName = "压裂水平井复合页岩油模型3";
                isValid = true;
            } else if (wb == "Constant") {
                m_selectedModelCode = "modelwidget4";
                m_selectedModelName = "压裂水平井复合页岩油模型4";
                isValid = true;
            }
        }
        // 分支3：定压边界
        else if (bnd == "ConstantPressure") {
            if (wb == "Changing") {
                m_selectedModelCode = "modelwidget5";
                m_selectedModelName = "压裂水平井复合页岩油模型5";
                isValid = true;
            } else if (wb == "Constant") {
                m_selectedModelCode = "modelwidget6";
                m_selectedModelName = "压裂水平井复合页岩油模型6";
                isValid = true;
            }
        }
    }

    // 更新界面显示
    if (isValid) {
        ui->label_ModelName->setText(m_selectedModelName);
        ui->label_FileName->setText("代码文件: " + m_selectedModelCode);
        ui->label_ModelName->setStyleSheet("color: black; font-weight: bold; font-size: 14px;");
    } else {
        ui->label_ModelName->setText("该组合暂无已实现模型");
        ui->label_FileName->setText("代码文件: 无");
        ui->label_ModelName->setStyleSheet("color: red; font-weight: normal;");
    }

    // 文件名标签也强制黑色
    ui->label_FileName->setStyleSheet("color: black; font-style: italic;");

    // 根据是否有效，控制确定按钮，防止用户选择无效模型
    QPushButton* okBtn = ui->buttonBox->button(QDialogButtonBox::Ok);
    if(okBtn) okBtn->setEnabled(isValid);
}

void ModelSelect::onAccepted()
{
    // 再次检查有效性（防御性编程）
    if (!m_selectedModelCode.isEmpty()) {
        accept();
    }
}

QString ModelSelect::getSelectedModelCode() const
{
    return m_selectedModelCode;
}

QString ModelSelect::getSelectedModelName() const
{
    return m_selectedModelName;
}
