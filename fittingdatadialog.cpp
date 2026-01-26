/*
 * 文件名: fittingdatadialog.cpp
 * 文件作用: 拟合数据加载配置窗口实现文件
 * 功能描述:
 * 1. 实现界面逻辑：数据源切换、文件读取与预览。
 * 2. 实现智能列名识别，自动匹配 Time, Pressure 等列。
 * 3. 实现试井类型切换逻辑：降落试井需输入地层压力，恢复试井自动计算。
 * 4. [修改] 适配多文件数据源，实现项目文件切换与预览联动。
 */

#include "fittingdatadialog.h"
#include "ui_fittingdatadialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QTextCodec>
#include <QDebug>
#include <QAxObject>
#include <QDir>
#include <QFileInfo>

// 构造函数
FittingDataDialog::FittingDataDialog(const QMap<QString, QStandardItemModel*>& projectModels, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FittingDataDialog),
    m_projectDataMap(projectModels),
    m_fileModel(new QStandardItemModel(this))
{
    ui->setupUi(this);

    // 初始化项目文件下拉框
    ui->comboProjectFile->clear();
    // 将所有可用的项目文件添加到下拉框
    for(auto it = m_projectDataMap.begin(); it != m_projectDataMap.end(); ++it) {
        // 使用文件名作为显示文本，完整路径或Key作为UserData
        // 这里假设Key就是唯一标识（通常是路径）
        QString key = it.key();
        QFileInfo fi(key);
        QString displayName = fi.fileName().isEmpty() ? key : fi.fileName();
        ui->comboProjectFile->addItem(displayName, key);
    }

    // 连接数据源相关信号槽
    connect(ui->radioProjectData, &QRadioButton::toggled, this, &FittingDataDialog::onSourceChanged);
    connect(ui->radioExternalFile, &QRadioButton::toggled, this, &FittingDataDialog::onSourceChanged);

    // [新增] 连接项目文件切换信号
    connect(ui->comboProjectFile, SIGNAL(currentIndexChanged(int)), this, SLOT(onProjectFileSelectionChanged(int)));

    connect(ui->btnBrowse, &QPushButton::clicked, this, &FittingDataDialog::onBrowseFile);

    // 连接列选择相关信号槽
    connect(ui->comboDerivative, SIGNAL(currentIndexChanged(int)), this, SLOT(onDerivColumnChanged(int)));

    // 连接试井类型切换信号槽 (降落/恢复)
    connect(ui->radioDrawdown, &QRadioButton::toggled, this, &FittingDataDialog::onTestTypeChanged);
    connect(ui->radioBuildup, &QRadioButton::toggled, this, &FittingDataDialog::onTestTypeChanged);

    // 连接平滑复选框
    connect(ui->checkSmoothing, &QCheckBox::toggled, this, &FittingDataDialog::onSmoothingToggled);

    // 重写确定按钮逻辑，先进行校验
    connect(ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &FittingDataDialog::onAccepted);
    // 断开默认的 accepted 信号，由 onAccepted 手动调用 accept()
    disconnect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    // 初始化界面状态
    ui->widgetFileSelect->setVisible(false);

    // 初始化试井类型状态
    onTestTypeChanged();

    // 如果没有项目数据，强制选择外部文件
    if (m_projectDataMap.isEmpty()) {
        ui->radioExternalFile->setChecked(true);
        ui->radioProjectData->setEnabled(false);
        ui->comboProjectFile->setEnabled(false);
    } else {
        // 默认选中第一个项目文件
        ui->radioProjectData->setChecked(true);
        if(ui->comboProjectFile->count() > 0)
            ui->comboProjectFile->setCurrentIndex(0);
        onSourceChanged();
    }
}

FittingDataDialog::~FittingDataDialog()
{
    delete ui;
}

// 校验并确认选择
void FittingDataDialog::onAccepted()
{
    // 基础校验：必须选择时间列和压力列
    if (ui->comboTime->currentIndex() < 0 || ui->comboPressure->currentIndex() < 0) {
        QMessageBox::warning(this, "提示", "请选择时间列和压力列！");
        return;
    }

    // 校验初始压力 (仅当选择降落试井时)
    if (ui->radioDrawdown->isChecked()) {
        if (ui->spinPi->value() <= 0.0001) {
            QMessageBox::warning(this, "提示", "压力降落试井需要输入有效的地层初始压力 (Pi)！");
            return;
        }
    }

    accept();
}

// 获取当前选中的项目数据模型
QStandardItemModel* FittingDataDialog::getCurrentProjectModel() const
{
    QString key = ui->comboProjectFile->currentData().toString();
    if (m_projectDataMap.contains(key)) {
        return m_projectDataMap.value(key);
    }
    return nullptr;
}

// 数据来源切换逻辑
void FittingDataDialog::onSourceChanged()
{
    bool isProject = ui->radioProjectData->isChecked();

    // 控制界面元素的显示/隐藏
    ui->widgetFileSelect->setVisible(!isProject);
    ui->comboProjectFile->setEnabled(isProject);

    QStandardItemModel* targetModel = nullptr;

    if (isProject) {
        targetModel = getCurrentProjectModel();
    } else {
        targetModel = m_fileModel;
    }

    // 清空预览表格
    ui->tablePreview->clear();

    if (targetModel) {
        // 设置表头
        QStringList headers;
        for (int i = 0; i < targetModel->columnCount(); ++i) {
            headers << targetModel->headerData(i, Qt::Horizontal).toString();
        }
        ui->tablePreview->setColumnCount(headers.size());
        ui->tablePreview->setHorizontalHeaderLabels(headers);

        // 设置预览数据（最多50行）
        int rows = qMin(50, targetModel->rowCount());
        ui->tablePreview->setRowCount(rows);
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < targetModel->columnCount(); ++j) {
                QStandardItem* item = targetModel->item(i, j);
                if (item) {
                    ui->tablePreview->setItem(i, j, new QTableWidgetItem(item->text()));
                }
            }
        }

        // 更新列选择下拉框
        updateColumnComboBoxes(headers);
    } else {
        ui->tablePreview->setRowCount(0);
        ui->tablePreview->setColumnCount(0);
        updateColumnComboBoxes(QStringList());
    }
}

// [新增] 项目文件选择改变时的响应
void FittingDataDialog::onProjectFileSelectionChanged(int index)
{
    Q_UNUSED(index);
    // 只有在当前确实选择了“项目数据”模式时才刷新预览
    // 避免在初始化或切换模式时产生不必要的刷新
    if (ui->radioProjectData->isChecked()) {
        onSourceChanged();
    }
}

// 更新下拉框内容
void FittingDataDialog::updateColumnComboBoxes(const QStringList& headers)
{
    ui->comboTime->clear();
    ui->comboPressure->clear();
    ui->comboDerivative->clear();

    // 添加选项
    ui->comboTime->addItems(headers);
    ui->comboPressure->addItems(headers);

    // 导数列特殊处理：第一项为“自动计算”
    ui->comboDerivative->addItem("自动计算 (Bourdet)", -1); // UserData -1
    for(int i=0; i<headers.size(); ++i) {
        ui->comboDerivative->addItem(headers[i], i); // UserData 对应列索引
    }

    // 智能匹配列名
    for (int i = 0; i < headers.size(); ++i) {
        QString h = headers[i].toLower();
        if (h.contains("time") || h.contains("时间") || h.contains("date")) {
            ui->comboTime->setCurrentIndex(i);
        }
        if (h.contains("pressure") || h.contains("压力")) {
            ui->comboPressure->setCurrentIndex(i);
        }
        if (h.contains("deriv") || h.contains("导数")) {
            // 注意 comboDerivative 第0项是自动计算，所以索引要+1
            ui->comboDerivative->setCurrentIndex(i + 1);
        }
    }
}

// 试井类型切换逻辑
void FittingDataDialog::onTestTypeChanged()
{
    bool isDrawdown = ui->radioDrawdown->isChecked();
    // 只有在压力降落试井模式下，才允许用户输入初始地层压力
    ui->spinPi->setEnabled(isDrawdown);
    ui->labelPi->setEnabled(isDrawdown);
    ui->labelUnitPi->setEnabled(isDrawdown);
}

// 浏览文件
void FittingDataDialog::onBrowseFile()
{
    QString path = QFileDialog::getOpenFileName(this, "打开数据文件", "",
                                                "所有支持文件 (*.csv *.txt *.xls *.xlsx);;CSV/文本 (*.csv *.txt);;Excel (*.xls *.xlsx)");
    if (path.isEmpty()) return;

    ui->lineEditFilePath->setText(path);
    m_fileModel->clear();

    bool success = false;
    if (path.endsWith(".xls", Qt::CaseInsensitive) || path.endsWith(".xlsx", Qt::CaseInsensitive)) {
        success = parseExcelFile(path);
    } else {
        success = parseTextFile(path);
    }

    if (success) {
        onSourceChanged();
    } else {
        QMessageBox::warning(this, "错误", "文件解析失败，请检查文件格式。");
    }
}

// 解析文本文件
bool FittingDataDialog::parseTextFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    QByteArray data = file.readAll();
    file.close();

    QString content = codec->toUnicode(data);
    QTextStream in(&content);

    bool headerSet = false;
    int colCount = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QChar sep = ',';
        if (line.contains('\t')) sep = '\t';
        else if (line.contains(';')) sep = ';';
        else if (line.contains(' ')) sep = ' ';

        QStringList parts = line.split(sep, Qt::SkipEmptyParts);
        for(int k=0; k<parts.size(); ++k) {
            QString s = parts[k].trimmed();
            if(s.startsWith('"') && s.endsWith('"')) s = s.mid(1, s.length()-2);
            parts[k] = s;
        }

        if (!headerSet) {
            m_fileModel->setHorizontalHeaderLabels(parts);
            colCount = parts.size();
            headerSet = true;
        } else {
            QList<QStandardItem*> items;
            for (const QString& p : parts) items.append(new QStandardItem(p));
            while(items.size() < colCount) items.append(new QStandardItem(""));
            m_fileModel->appendRow(items);
        }
    }
    return true;
}

// 解析Excel文件
bool FittingDataDialog::parseExcelFile(const QString& filePath)
{
    QAxObject excel("Excel.Application");
    if (excel.isNull()) return false;
    excel.setProperty("Visible", false);
    excel.setProperty("DisplayAlerts", false);

    QAxObject *workbooks = excel.querySubObject("Workbooks");
    if (!workbooks) return false;
    QAxObject *workbook = workbooks->querySubObject("Open(const QString&)", QDir::toNativeSeparators(filePath));
    if (!workbook) { excel.dynamicCall("Quit()"); return false; }

    QAxObject *sheets = workbook->querySubObject("Worksheets");
    QAxObject *sheet = sheets->querySubObject("Item(int)", 1);

    if (sheet) {
        QAxObject *usedRange = sheet->querySubObject("UsedRange");
        if (usedRange) {
            QVariant varData = usedRange->dynamicCall("Value()");
            QList<QList<QVariant>> rowsData;
            if (varData.type() == QVariant::List) {
                QList<QVariant> rows = varData.toList();
                for (const QVariant &row : rows) {
                    if (row.type() == QVariant::List) rowsData.append(row.toList());
                }
            }
            if (!rowsData.isEmpty()) {
                QStringList headers;
                for(const QVariant& v : rowsData.first()) headers << v.toString();
                m_fileModel->setHorizontalHeaderLabels(headers);
                for(int i=1; i<rowsData.size(); ++i) {
                    QList<QStandardItem*> items;
                    for(const QVariant& v : rowsData[i]) items.append(new QStandardItem(v.toString()));
                    m_fileModel->appendRow(items);
                }
            }
            delete usedRange;
        }
        delete sheet;
    }
    workbook->dynamicCall("Close()");
    excel.dynamicCall("Quit()");
    return true;
}

// 导数列变更时逻辑
void FittingDataDialog::onDerivColumnChanged(int index)
{
    // 如果选择了具体的列，可以考虑禁用L-Spacing参数，这里保持始终启用
    Q_UNUSED(index);
}

// 平滑选项切换
void FittingDataDialog::onSmoothingToggled(bool checked)
{
    ui->spinSmoothSpan->setEnabled(checked);
}

// 获取设置结果
FittingDataSettings FittingDataDialog::getSettings() const
{
    FittingDataSettings s;
    s.isFromProject = ui->radioProjectData->isChecked();

    // [新增] 保存选中的项目文件路径
    if (s.isFromProject) {
        s.projectFileName = ui->comboProjectFile->currentData().toString();
    }

    s.filePath = ui->lineEditFilePath->text();

    s.timeColIndex = ui->comboTime->currentIndex();
    s.pressureColIndex = ui->comboPressure->currentIndex();

    // 获取导数列：itemData存储了真实的列索引，-1表示自动
    s.derivColIndex = ui->comboDerivative->currentData().toInt();

    s.skipRows = ui->spinSkipRows->value();

    // 获取试井类型和初始压力
    if (ui->radioDrawdown->isChecked()) {
        s.testType = Test_Drawdown;
        s.initialPressure = ui->spinPi->value();
    } else {
        s.testType = Test_Buildup;
        s.initialPressure = 0.0;
    }

    s.lSpacing = ui->spinLSpacing->value();

    s.enableSmoothing = ui->checkSmoothing->isChecked();
    s.smoothingSpan = ui->spinSmoothSpan->value();

    return s;
}

QStandardItemModel* FittingDataDialog::getPreviewModel() const
{
    return ui->radioProjectData->isChecked() ? getCurrentProjectModel() : m_fileModel;
}
