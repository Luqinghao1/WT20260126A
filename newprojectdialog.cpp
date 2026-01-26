#include "newprojectdialog.h"
#include "ui_newprojectdialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QJsonDocument>
#include <QDebug>
#include <QDate>
#include <QStandardPaths>
#include <QStyle>

NewProjectDialog::NewProjectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewProjectDialog)
{
    ui->setupUi(this);

    // 设置窗口图标 (使用系统标准文件图标)
    this->setWindowIcon(style()->standardIcon(QStyle::SP_FileIcon));

    // 加载样式 (包含日期控件颜色修复)
    loadModernStyle();

    initDefaultValues();

    // 信号连接
    connect(ui->btnBrowse, &QPushButton::clicked, this, &NewProjectDialog::on_btnBrowse_clicked);
    connect(ui->comboUnits, SIGNAL(currentIndexChanged(int)), this, SLOT(on_comboUnits_currentIndexChanged(int)));
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &NewProjectDialog::on_btnOk_clicked);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &NewProjectDialog::on_btnCancel_clicked);
}

NewProjectDialog::~NewProjectDialog()
{
    delete ui;
}

void NewProjectDialog::loadModernStyle()
{
    QString style = R"(
        /* 全局设置 */
        QDialog {
            background-color: #ffffff;
            color: #000000;
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 10pt;
        }

        QLabel {
            color: #333333;
            font-weight: normal;
            padding: 2px;
        }

        /* 重点修复：显式包含 QDateTimeEdit
           确保文字颜色为黑色，背景为白色
        */
        QLineEdit, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QComboBox {
            background-color: #ffffff;
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 6px;
            color: #000000; /* 强制黑色文字 */
            selection-background-color: #0078d7;
            selection-color: white;
        }

        /* 聚焦状态 */
        QLineEdit:focus, QDoubleSpinBox:focus, QDateEdit:focus, QDateTimeEdit:focus, QComboBox:focus {
            border: 1px solid #0078d7;
            background-color: #fbfbfb;
        }

        /* 针对 QDateTimeEdit 内部的 QLineEdit (双重保险) */
        QDateTimeEdit QLineEdit {
            color: #000000;
            background-color: #ffffff;
        }

        QTextEdit {
            border: 1px solid #cccccc;
            border-radius: 4px;
            padding: 5px;
            background-color: white;
            color: #000000;
        }

        /* 下拉箭头样式 */
        QComboBox::drop-down, QDateEdit::drop-down, QDateTimeEdit::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left-width: 0px;
        }
        QComboBox::down-arrow, QDateEdit::down-arrow, QDateTimeEdit::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid #666;
            margin-top: 2px;
            margin-right: 2px;
        }

        /* 下拉列表视图 */
        QComboBox QAbstractItemView {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #cccccc;
            selection-background-color: #0078d7;
            selection-color: white;
        }

        /* -------------------------------------------
           日历控件样式 (QCalendarWidget)
           ------------------------------------------- */
        QCalendarWidget QWidget {
            color: #000000;
            background-color: #ffffff;
            alternate-background-color: #f9f9f9;
        }
        QCalendarWidget QWidget#qt_calendar_navigationbar {
            background-color: #ffffff;
            border-bottom: 1px solid #cccccc;
        }
        QCalendarWidget QToolButton {
            color: #000000;
            background-color: transparent;
            icon-size: 20px;
            border: none;
            font-weight: bold;
        }
        QCalendarWidget QToolButton:hover {
            background-color: #e0e0e0;
            border-radius: 4px;
        }
        QCalendarWidget QSpinBox {
            color: #000000;
            background-color: #ffffff;
            selection-background-color: #0078d7;
            selection-color: white;
        }
        QCalendarWidget QTableView {
            background-color: #ffffff;
            color: #000000;
            selection-background-color: #0078d7;
            selection-color: #ffffff;
            gridline-color: #e0e0e0;
        }
        /* ------------------------------------------- */

        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #dcdcdc;
            border-radius: 4px;
            color: #000000;
            padding: 6px 16px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
            border-color: #c0c0c0;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }

        /* Tab Widget */
        QTabWidget::pane {
            border: 1px solid #e0e0e0;
            background: #ffffff;
            border-radius: 4px;
            top: -1px;
        }
        QTabBar::tab {
            background: #f9f9f9;
            border: 1px solid #e0e0e0;
            padding: 8px 20px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            color: #555555;
        }
        QTabBar::tab:selected {
            background: #ffffff;
            border-bottom-color: #ffffff;
            color: #0078d7;
            font-weight: bold;
        }

        QGroupBox {
            font-weight: bold;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 10px;
            color: #000000;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 5px;
            color: #0078d7;
        }
    )";
    this->setStyleSheet(style);
}

void NewProjectDialog::initDefaultValues()
{
    // Page 1: 基本信息
    ui->editProjectName->setText("Project_01");
    ui->editOilField->setText("ShaleOilField");
    ui->editWell->setText("Well-01");
    ui->editEngineer->setText("Admin");
    ui->dateEdit->setDateTime(QDateTime::currentDateTime());

    // 默认路径：优先 D 盘
    QString defaultPath = "D:/";
    QDir dDir(defaultPath);
    if (dDir.exists()) {
        ui->editPath->setText(defaultPath);
    } else {
        ui->editPath->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    }

    // Page 2: 油藏与井
    ui->comboTestType->setCurrentIndex(1); // 压力恢复
    ui->spinQ->setValue(50.0);
    ui->spinPhi->setValue(0.05);
    ui->spinH->setValue(20.0);
    ui->spinRw->setValue(0.1);

    // Page 3: PVT
    ui->comboUnits->setCurrentIndex((int)ProjectUnitType::Metric_SI);
    ui->spinCt->setValue(5.0e-4);
    ui->spinMu->setValue(0.5);
    ui->spinB->setValue(1.2);

    updateUnitLabels(ProjectUnitType::Metric_SI);
}

ProjectData NewProjectDialog::getProjectData() const
{
    return m_projectData;
}

void NewProjectDialog::on_btnBrowse_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择项目存储位置",
                                                    ui->editPath->text(),
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        ui->editPath->setText(dir);
    }
}

void NewProjectDialog::on_comboUnits_currentIndexChanged(int index)
{
    ProjectUnitType newSystem = (ProjectUnitType)index;
    ProjectUnitType oldSystem = (newSystem == ProjectUnitType::Metric_SI) ? ProjectUnitType::Field_Unit : ProjectUnitType::Metric_SI;

    convertValues(oldSystem, newSystem);
    updateUnitLabels(newSystem);
}

void NewProjectDialog::updateUnitLabels(ProjectUnitType unit)
{
    if (unit == ProjectUnitType::Metric_SI) {
        ui->label_unit_q->setText("m³/d");
        ui->label_unit_h->setText("m");
        ui->label_unit_rw->setText("m");
        ui->label_unit_Ct->setText("MPa⁻¹");
        ui->label_unit_mu->setText("mPa·s");
        ui->label_unit_B->setText("m³/m³");
    } else {
        ui->label_unit_q->setText("STB/d");
        ui->label_unit_h->setText("ft");
        ui->label_unit_rw->setText("ft");
        ui->label_unit_Ct->setText("psi⁻¹");
        ui->label_unit_mu->setText("cp");
        ui->label_unit_B->setText("RB/STB");
    }
}

void NewProjectDialog::convertValues(ProjectUnitType from, ProjectUnitType to)
{
    if (from == to) return;

    // 转换因子
    const double M_TO_FT = 3.28084;
    const double MPA_TO_PSI = 145.038;
    const double M3D_TO_STBD = 6.2898;

    double h = ui->spinH->value();
    double rw = ui->spinRw->value();
    double ct = ui->spinCt->value();
    double q = ui->spinQ->value();

    if (from == ProjectUnitType::Metric_SI && to == ProjectUnitType::Field_Unit) {
        // SI -> Field
        ui->spinH->setValue(h * M_TO_FT);
        ui->spinRw->setValue(rw * M_TO_FT);
        ui->spinCt->setValue(ct / MPA_TO_PSI);
        ui->spinQ->setValue(q * M3D_TO_STBD);
    }
    else if (from == ProjectUnitType::Field_Unit && to == ProjectUnitType::Metric_SI) {
        // Field -> SI
        ui->spinH->setValue(h / M_TO_FT);
        ui->spinRw->setValue(rw / M_TO_FT);
        ui->spinCt->setValue(ct * MPA_TO_PSI);
        ui->spinQ->setValue(q / M3D_TO_STBD);
    }
}

void NewProjectDialog::on_btnOk_clicked()
{
    if (ui->editProjectName->text().isEmpty() || ui->editOilField->text().isEmpty() || ui->editWell->text().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "项目名称、油田名称和井名不能为空！");
        return;
    }
    if (ui->editPath->text().isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请选择存储位置！");
        return;
    }

    if (createProjectStructure()) {
        QDialog::accept();
    }
}

void NewProjectDialog::on_btnCancel_clicked()
{
    QDialog::reject();
}

bool NewProjectDialog::createProjectStructure()
{
    // 文件夹命名: 油田-井名
    QString folderName = QString("%1-%2").arg(ui->editOilField->text().trimmed()).arg(ui->editWell->text().trimmed());
    QDir baseDir(ui->editPath->text());

    QString projectDirPath = baseDir.filePath(folderName);

    if (!baseDir.exists(folderName)) {
        if (!baseDir.mkpath(folderName)) {
            QMessageBox::critical(this, "错误", "无法创建项目文件夹，请检查路径权限。");
            return false;
        }
    }

    m_projectData.projectName = ui->editProjectName->text().trimmed();
    m_projectData.oilFieldName = ui->editOilField->text().trimmed();
    m_projectData.wellName = ui->editWell->text().trimmed();
    m_projectData.engineer = ui->editEngineer->text().trimmed();
    m_projectData.comments = ui->textComment->toPlainText();
    m_projectData.projectPath = projectDirPath;
    m_projectData.testType = ui->comboTestType->currentIndex();
    m_projectData.testDate = ui->dateEdit->dateTime();
    m_projectData.currentUnitSystem = (ProjectUnitType)ui->comboUnits->currentIndex();

    m_projectData.productionRate = ui->spinQ->value();
    m_projectData.porosity = ui->spinPhi->value();
    m_projectData.thickness = ui->spinH->value();
    m_projectData.wellRadius = ui->spinRw->value();

    m_projectData.compressibility = ui->spinCt->value();
    m_projectData.viscosity = ui->spinMu->value();
    m_projectData.volumeFactor = ui->spinB->value();

    // [修改] 文件后缀名改为 .pwt
    QString fileName = m_projectData.projectName + ".pwt";
    m_projectData.fullFilePath = QDir(projectDirPath).filePath(fileName);

    saveProjectFile(m_projectData.fullFilePath, m_projectData);

    return true;
}

void NewProjectDialog::saveProjectFile(const QString &filePath, const ProjectData &data)
{
    QJsonObject root;
    root["projectName"] = data.projectName;
    root["oilField"] = data.oilFieldName;
    root["wellName"] = data.wellName;
    root["engineer"] = data.engineer;
    root["createdDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["testType"] = data.testType;

    QJsonObject reservoir;
    reservoir["unitSystem"] = (data.currentUnitSystem == ProjectUnitType::Metric_SI ? "Metric" : "Field");
    reservoir["productionRate"] = data.productionRate;
    reservoir["porosity"] = data.porosity;
    reservoir["thickness"] = data.thickness;
    reservoir["wellRadius"] = data.wellRadius;

    QJsonObject pvt;
    pvt["compressibility"] = data.compressibility;
    pvt["viscosity"] = data.viscosity;
    pvt["volumeFactor"] = data.volumeFactor;

    root["reservoir"] = reservoir;
    root["pvt"] = pvt;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

