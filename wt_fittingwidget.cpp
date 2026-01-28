/*
 * 文件名: wt_fittingwidget.cpp
 * 文件作用: 试井拟合分析主界面及辅助类的实现文件
 * 功能描述:
 * 1. [辅助类] SamplingSettingsDialog: 提供自定义数据抽样区间的设置界面交互。
 * 2. [主类] FittingWidget:
 * - 界面初始化与图表配置 (QCustomPlot)。
 * - 数据加载与处理 (计算压差、导数)。
 * - 拟合算法实现 (Levenberg-Marquardt)，包含物理约束(内区>外区)和多线程执行。
 * - 绘图逻辑：包含实测数据、理论曲线、以及特定抽样点的高亮显示。
 * - 报告导出：生成包含多坐标系截图、参数分类表、数据表的 Word 兼容格式报告。
 * - 状态管理：保存和恢复拟合进度 (.json)。
 */

#include "wt_fittingwidget.h"
#include "ui_wt_fittingwidget.h"
#include "modelparameter.h"
#include "modelselect.h"
#include "fittingdatadialog.h"
#include "pressurederivativecalculator.h"
#include "pressurederivativecalculator1.h"
#include "paramselectdialog.h"

#include <QtConcurrent>
#include <QMessageBox>
#include <QDebug>
#include <cmath>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QBuffer>
#include <Eigen/Dense>
#include <QFileInfo>
#include <algorithm>

// ============================================================================
// SamplingSettingsDialog 实现 (数据抽样设置对话框)
// ============================================================================

/**
 * @brief SamplingSettingsDialog 构造函数
 * * 初始化抽样设置对话框的界面布局，包括说明文本、启用开关、区间配置表格以及操作按钮。
 * * @param intervals 当前已有的区间列表，用于回显。
 * @param enabled 当前是否启用了自定义抽样。
 * @param dataMinT 数据的最小时间，用于提示和默认区间生成。
 * @param dataMaxT 数据的最大时间。
 * @param parent 父窗口指针。
 */
SamplingSettingsDialog::SamplingSettingsDialog(const QList<SamplingInterval>& intervals, bool enabled,
                                               double dataMinT, double dataMaxT, QWidget *parent)
    : QDialog(parent), m_dataMinT(dataMinT), m_dataMaxT(dataMaxT)
{
    setWindowTitle("数据抽样策略设置");
    resize(600, 450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 1. 顶部说明区域
    QString info = QString("当前数据时间范围: %1 ~ %2 (h)\n\n"
                           "说明: 系统将时间轴按对数空间（如0.1-1, 1-10...）划分，每个区间默认抽取10个点。\n"
                           "您可以手动调整区间范围和点数，重点关注曲线关键变化阶段（如井储、边界）。")
                       .arg(dataMinT).arg(dataMaxT);
    QLabel* lblInfo = new QLabel(info, this);
    lblInfo->setWordWrap(true);
    mainLayout->addWidget(lblInfo);

    // 2. 启用开关
    m_chkEnable = new QCheckBox("启用自定义分段抽样 (若未勾选，则采用系统默认策略：均匀抽取200点)", this);
    m_chkEnable->setChecked(enabled);
    mainLayout->addWidget(m_chkEnable);

    // 3. 设置表格
    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels(QStringList() << "起始时间(h)" << "结束时间(h)" << "抽样点数");
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    mainLayout->addWidget(m_table);

    // 4. 表格操作按钮布局
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* btnAdd = new QPushButton("添加区间", this);
    QPushButton* btnDel = new QPushButton("删除选中行", this);
    QPushButton* btnReset = new QPushButton("重置为对数默认", this);

    btnLayout->addWidget(btnAdd);
    btnLayout->addWidget(btnDel);
    btnLayout->addWidget(btnReset);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // 5. 底部确认/取消按钮布局
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    QPushButton* btnOk = new QPushButton("确定", this);
    QPushButton* btnCancel = new QPushButton("取消", this);
    btnOk->setDefault(true);

    bottomLayout->addWidget(btnOk);
    bottomLayout->addWidget(btnCancel);
    mainLayout->addLayout(bottomLayout);

    // 连接信号槽
    connect(btnAdd, &QPushButton::clicked, this, &SamplingSettingsDialog::onAddRow);
    connect(btnDel, &QPushButton::clicked, this, &SamplingSettingsDialog::onRemoveRow);
    connect(btnReset, &QPushButton::clicked, this, &SamplingSettingsDialog::onResetDefault);
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    // 初始化数据填充
    if (intervals.isEmpty()) {
        onResetDefault(); // 如果传入为空，则生成默认建议
    } else {
        for(const auto& item : intervals) {
            addRow(item.tStart, item.tEnd, item.count);
        }
    }
}

/**
 * @brief 获取配置的区间列表
 * * 遍历表格每一行，提取用户输入的起始时间、结束时间和点数。
 * * 会进行简单的数据有效性检查（结束时间需大于起始时间，点数需大于0）。
 * * @return QList<SamplingInterval> 区间列表
 */
QList<SamplingInterval> SamplingSettingsDialog::getIntervals() const {
    QList<SamplingInterval> list;
    for(int i=0; i<m_table->rowCount(); ++i) {
        SamplingInterval item;
        item.tStart = m_table->item(i, 0)->text().toDouble();
        item.tEnd = m_table->item(i, 1)->text().toDouble();
        item.count = m_table->item(i, 2)->text().toInt();

        if (item.tEnd > item.tStart && item.count > 0) {
            list.append(item);
        }
    }
    return list;
}

/**
 * @brief 获取自定义抽样是否启用
 * @return true 表示启用，false 表示使用默认策略
 */
bool SamplingSettingsDialog::isCustomSamplingEnabled() const {
    return m_chkEnable->isChecked();
}

/**
 * @brief 向表格中添加一行数据
 * @param start 区间起始时间
 * @param end 区间结束时间
 * @param count 抽样点数
 */
void SamplingSettingsDialog::addRow(double start, double end, int count) {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(QString::number(start)));
    m_table->setItem(row, 1, new QTableWidgetItem(QString::number(end)));
    m_table->setItem(row, 2, new QTableWidgetItem(QString::number(count)));
}

/**
 * @brief 添加按钮槽函数
 * * 智能推断下一行的起始时间（衔接上一行结束时间）。
 * * 默认尝试按 10 倍对数跨度生成结束时间。
 */
void SamplingSettingsDialog::onAddRow() {
    double start = m_dataMinT;
    double end = m_dataMaxT;
    if (m_table->rowCount() > 0) {
        bool ok;
        double lastEnd = m_table->item(m_table->rowCount()-1, 1)->text().toDouble(&ok);
        if (ok) start = lastEnd;
    }

    // 寻找下一个 10 的幂次方作为默认结束点
    double safeStart = (start <= 0) ? 1e-4 : start;
    double exponent = std::floor(log10(safeStart));
    double nextPower10 = pow(10, exponent + 1);
    end = nextPower10;

    // 边界检查
    if (end > m_dataMaxT) end = m_dataMaxT;
    if (end <= start) end = start * 10.0;

    addRow(start, end, 10);
}

/**
 * @brief 删除按钮槽函数
 * * 删除当前选中的行，如果未选中则删除最后一行。
 */
void SamplingSettingsDialog::onRemoveRow() {
    int row = m_table->currentRow();
    if (row >= 0) m_table->removeRow(row);
    else if (m_table->rowCount() > 0) m_table->removeRow(m_table->rowCount() - 1);
}

/**
 * @brief 重置按钮槽函数 (核心逻辑)
 * * 按照对数空间（10的幂次方）自动划分区间。
 * * 例如：0.01~0.1, 0.1~1, 1~10 等。
 * * 每个区间默认分配 10 个抽样点。
 */
void SamplingSettingsDialog::onResetDefault() {
    m_table->setRowCount(0);

    double current = m_dataMinT;
    double maxVal = m_dataMaxT;

    if (current <= 1e-6) current = 1e-6; // 防止 log(0)
    if (maxVal <= current) return;

    // 寻找起始点所在的数量级
    double exponent = std::floor(log10(current));
    double nextPower10 = pow(10, exponent + 1);

    while (current < maxVal) {
        double end = nextPower10;

        // 确保不超过最大值
        if (end > maxVal) end = maxVal;

        // 添加区间 (忽略极小误差避免重复)
        if (end > current * 1.000001) {
            addRow(current, end, 10); // 默认每个对数区间抽10个点
        }

        // 准备下一轮
        current = end;
        nextPower10 *= 10.0;

        // 防止浮点数精度问题导致的死循环
        if (std::abs(current - maxVal) < 1e-9) break;
    }
}


// ============================================================================
// FittingWidget 实现 (主界面)
// ============================================================================

/**
 * @brief FittingWidget 构造函数
 * * 初始化主界面、图表组件、参数表格。
 * * 建立信号槽连接，包括数据导出、参数调节联动、拟合控制等。
 */
FittingWidget::FittingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FittingWidget),
    m_modelManager(nullptr),
    m_chartWidget(nullptr),
    m_plot(nullptr),
    m_plotTitle(nullptr),
    m_currentModelType(ModelManager::Model_1),
    m_isFitting(false),
    m_isCustomSamplingEnabled(false) // 初始化时不启用自定义抽样
{
    ui->setupUi(this);

    // 初始化图表组件
    m_chartWidget = new ChartWidget(this);
    ui->plotContainer->layout()->addWidget(m_chartWidget);
    m_plot = m_chartWidget->getPlot();
    m_chartWidget->setTitle("试井解释拟合 (Well Test Fitting)");
    connect(m_chartWidget, &ChartWidget::exportDataTriggered, this, &FittingWidget::onExportCurveData);

    // 设置分割器比例
    QList<int> sizes; sizes << 350 << 650;
    ui->splitter->setSizes(sizes);
    ui->splitter->setCollapsible(0, false);

    // 初始化参数表格管理器
    m_paramChart = new FittingParameterChart(ui->tableParams, this);
    // 滚轮调节参数时实时更新曲线
    connect(m_paramChart, &FittingParameterChart::parameterChangedByWheel, this, [this](){
        updateModelCurve(nullptr);
    });

    setupPlot();

    // 注册元类型以支持跨线程信号
    qRegisterMetaType<QMap<QString,double>>("QMap<QString,double>");
    qRegisterMetaType<ModelManager::ModelType>("ModelManager::ModelType");
    qRegisterMetaType<QVector<double>>("QVector<double>");

    // 连接拟合进度信号
    connect(this, &FittingWidget::sigIterationUpdated, this, &FittingWidget::onIterationUpdate, Qt::QueuedConnection);
    connect(this, &FittingWidget::sigProgress, ui->progressBar, &QProgressBar::setValue);
    connect(&m_watcher, &QFutureWatcher<void>::finished, this, &FittingWidget::onFitFinished);

    // 权重滑块
    connect(ui->sliderWeight, &QSlider::valueChanged, this, &FittingWidget::onSliderWeightChanged);

    // [关键] 连接抽样设置按钮，槽函数名已避免自动连接
    connect(ui->btnSamplingSettings, &QPushButton::clicked, this, &FittingWidget::onOpenSamplingSettings);

    // 初始化权重显示
    ui->sliderWeight->setRange(0, 100);
    ui->sliderWeight->setValue(50);
    onSliderWeightChanged(50);
}

/**
 * @brief 析构函数
 */
FittingWidget::~FittingWidget()
{
    delete ui;
}

/**
 * @brief 设置模型管理器
 * * 注入核心模型计算逻辑的管理者指针，并初始化默认模型。
 */
void FittingWidget::setModelManager(ModelManager *m)
{
    m_modelManager = m;
    m_paramChart->setModelManager(m);
    initializeDefaultModel();
}

/**
 * @brief 设置项目数据模型
 * * 用于在数据加载对话框中提供可选的数据源。
 */
void FittingWidget::setProjectDataModels(const QMap<QString, QStandardItemModel *> &models)
{
    m_dataMap = models;
}

/**
 * @brief 更新基础参数 (占位)
 * * 预留接口，用于同步全局基础参数变化。
 */
void FittingWidget::updateBasicParameters()
{
}

/**
 * @brief 初始化默认模型
 * * 设置初始模型为 Model_1 并重置参数。
 */
void FittingWidget::initializeDefaultModel()
{
    if(!m_modelManager) return;
    m_currentModelType = ModelManager::Model_1;
    ui->btn_modelSelect->setText("当前: " + ModelManager::getModelTypeName(m_currentModelType));
    on_btnResetParams_clicked();
}

/**
 * @brief 初始化绘图区域
 * * 配置坐标轴（双对数坐标）、网格线、图例和交互模式。
 */
void FittingWidget::setupPlot() {
    if (!m_plot) return;

    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_plot->setBackground(Qt::white);
    m_plot->axisRect()->setBackground(Qt::white);

    // 设置对数坐标轴
    QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
    m_plot->xAxis->setScaleType(QCPAxis::stLogarithmic); m_plot->xAxis->setTicker(logTicker);
    m_plot->yAxis->setScaleType(QCPAxis::stLogarithmic); m_plot->yAxis->setTicker(logTicker);

    m_plot->xAxis->setNumberFormat("eb"); m_plot->xAxis->setNumberPrecision(0);
    m_plot->yAxis->setNumberFormat("eb"); m_plot->yAxis->setNumberPrecision(0);

    // 设置字体
    QFont labelFont("Microsoft YaHei", 10, QFont::Bold);
    QFont tickFont("Microsoft YaHei", 9);
    m_plot->xAxis->setLabel("时间 Time (h)");
    m_plot->yAxis->setLabel("压差 & 导数 Delta P & Derivative (MPa)");
    m_plot->xAxis->setLabelFont(labelFont); m_plot->yAxis->setLabelFont(labelFont);
    m_plot->xAxis->setTickLabelFont(tickFont); m_plot->yAxis->setTickLabelFont(tickFont);

    // 设置顶部和右侧坐标轴同步
    m_plot->xAxis2->setVisible(true); m_plot->yAxis2->setVisible(true);
    m_plot->xAxis2->setTickLabels(false); m_plot->yAxis2->setTickLabels(false);
    connect(m_plot->xAxis, SIGNAL(rangeChanged(QCPRange)), m_plot->xAxis2, SLOT(setRange(QCPRange)));
    connect(m_plot->yAxis, SIGNAL(rangeChanged(QCPRange)), m_plot->yAxis2, SLOT(setRange(QCPRange)));
    m_plot->xAxis2->setScaleType(QCPAxis::stLogarithmic); m_plot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    m_plot->xAxis2->setTicker(logTicker); m_plot->yAxis2->setTicker(logTicker);

    // 网格线样式
    m_plot->xAxis->grid()->setVisible(true); m_plot->yAxis->grid()->setVisible(true);
    m_plot->xAxis->grid()->setSubGridVisible(true); m_plot->yAxis->grid()->setSubGridVisible(true);
    m_plot->xAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    m_plot->yAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    m_plot->xAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));
    m_plot->yAxis->grid()->setSubGridPen(QPen(QColor(240, 240, 240), 1, Qt::DotLine));

    // 初始范围
    m_plot->xAxis->setRange(1e-3, 1e3); m_plot->yAxis->setRange(1e-3, 1e2);

    // 添加图层
    m_plot->addGraph(); m_plot->graph(0)->setPen(Qt::NoPen);
    m_plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QColor(0, 100, 0), 6));
    m_plot->graph(0)->setName("实测压差");

    m_plot->addGraph(); m_plot->graph(1)->setPen(Qt::NoPen);
    m_plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssTriangle, Qt::magenta, 6));
    m_plot->graph(1)->setName("实测导数");

    m_plot->addGraph(); m_plot->graph(2)->setPen(QPen(Qt::red, 2));
    m_plot->graph(2)->setName("理论压差");

    m_plot->addGraph(); m_plot->graph(3)->setPen(QPen(Qt::blue, 2));
    m_plot->graph(3)->setName("理论导数");

    m_plot->legend->setVisible(true);
    m_plot->legend->setFont(QFont("Microsoft YaHei", 9));
    m_plot->legend->setBrush(QBrush(QColor(255, 255, 255, 200)));
}

/**
 * @brief 数据加载按钮槽函数
 * * 打开数据选择对话框，选择列并计算压差和导数。
 * * 成功加载后更新界面显示。
 */
void FittingWidget::on_btnLoadData_clicked() {
    FittingDataDialog dlg(m_dataMap, this);
    if (dlg.exec() != QDialog::Accepted) return;

    FittingDataSettings settings = dlg.getSettings();
    QStandardItemModel* sourceModel = dlg.getPreviewModel();

    if (!sourceModel || sourceModel->rowCount() == 0) {
        QMessageBox::warning(this, "警告", "所选数据源为空，无法加载！");
        return;
    }

    QVector<double> rawTime, rawPressureData, finalDeriv;
    int skip = settings.skipRows;
    int rows = sourceModel->rowCount();

    // 提取原始数据
    for (int i = skip; i < rows; ++i) {
        QStandardItem* itemT = sourceModel->item(i, settings.timeColIndex);
        QStandardItem* itemP = sourceModel->item(i, settings.pressureColIndex);

        if (itemT && itemP) {
            bool okT, okP;
            double t = itemT->text().toDouble(&okT);
            double p = itemP->text().toDouble(&okP);

            if (okT && okP && t > 0) {
                rawTime.append(t);
                rawPressureData.append(p);
                // 如果用户指定了导数列，则直接读取
                if (settings.derivColIndex >= 0) {
                    QStandardItem* itemD = sourceModel->item(i, settings.derivColIndex);
                    if (itemD) finalDeriv.append(itemD->text().toDouble());
                    else finalDeriv.append(0.0);
                }
            }
        }
    }

    if (rawTime.isEmpty()) {
        QMessageBox::warning(this, "警告", "未能提取到有效数据。");
        return;
    }

    // 计算压差
    QVector<double> finalDeltaP;
    double p_shutin = rawPressureData.first();

    for (double p : rawPressureData) {
        double deltaP = 0.0;
        if (settings.testType == Test_Drawdown) {
            deltaP = std::abs(settings.initialPressure - p);
        } else {
            deltaP = std::abs(p - p_shutin);
        }
        finalDeltaP.append(deltaP);
    }

    // 计算导数（如果未提供）
    if (settings.derivColIndex == -1) {
        finalDeriv = PressureDerivativeCalculator::calculateBourdetDerivative(rawTime, finalDeltaP, settings.lSpacing);
        if (settings.enableSmoothing) {
            finalDeriv = PressureDerivativeCalculator1::smoothData(finalDeriv, settings.smoothingSpan);
        }
    } else {
        if (settings.enableSmoothing) {
            finalDeriv = PressureDerivativeCalculator1::smoothData(finalDeriv, settings.smoothingSpan);
        }
        if (finalDeriv.size() != rawTime.size()) {
            finalDeriv.resize(rawTime.size());
        }
    }

    setObservedData(rawTime, finalDeltaP, finalDeriv);
    QMessageBox::information(this, "成功", "观测数据已成功加载。");
}

/**
 * @brief 设置观测数据并绘图
 * * 更新内存中的观测数据，并绘制到图表上。
 */
void FittingWidget::setObservedData(const QVector<double>& t, const QVector<double>& deltaP, const QVector<double>& d) {
    m_obsTime = t;
    m_obsDeltaP = deltaP;
    m_obsDerivative = d;

    // 过滤无效点用于绘图
    QVector<double> vt, vp, vd;
    for(int i=0; i<t.size(); ++i) {
        if(t[i]>1e-8 && deltaP[i]>1e-8) {
            vt << t[i];
            vp << deltaP[i];
            if(i < d.size() && d[i] > 1e-8) vd << d[i];
            else vd << 1e-10;
        }
    }

    m_plot->graph(0)->setData(vt, vp);
    m_plot->graph(1)->setData(vt, vd);

    // 自动缩放
    m_plot->rescaleAxes();
    if(m_plot->xAxis->range().lower <= 0) m_plot->xAxis->setRangeLower(1e-3);
    if(m_plot->yAxis->range().lower <= 0) m_plot->yAxis->setRangeLower(1e-3);
    m_plot->replot();
}

/**
 * @brief 权重滑块变更槽函数
 * * 更新界面上的权重数值显示。
 */
void FittingWidget::onSliderWeightChanged(int value)
{
    double wPressure = value / 100.0;
    double wDerivative = 1.0 - wPressure;
    ui->label_ValDerivative->setText(QString("导数权重: %1").arg(wDerivative, 0, 'f', 2));
    ui->label_ValPressure->setText(QString("压差权重: %1").arg(wPressure, 0, 'f', 2));
}

/**
 * @brief 选择拟合参数按钮槽函数
 * * 弹出参数选择对话框，允许用户勾选参与拟合的参数。
 */
void FittingWidget::on_btnSelectParams_clicked()
{
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> currentParams = m_paramChart->getParameters();
    ParamSelectDialog dlg(currentParams, this);
    if(dlg.exec() == QDialog::Accepted) {
        QList<FitParameter> updatedParams = dlg.getUpdatedParams();
        // LfD 始终不作为独立拟合参数
        for(auto& p : updatedParams) {
            if(p.name == "LfD") p.isFit = false;
        }
        m_paramChart->setParameters(updatedParams);
        updateModelCurve();
    }
}

/**
 * @brief 抽样设置按钮槽函数
 * * 打开数据抽样设置对话框。
 * * 成功设置后，立即刷新曲线和误差显示。
 */
void FittingWidget::onOpenSamplingSettings()
{
    if (m_obsTime.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先加载观测数据，以便确定时间范围。");
        return;
    }

    double tMin = m_obsTime.first();
    double tMax = m_obsTime.last();

    SamplingSettingsDialog dlg(m_customIntervals, m_isCustomSamplingEnabled, tMin, tMax, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_customIntervals = dlg.getIntervals();
        m_isCustomSamplingEnabled = dlg.isCustomSamplingEnabled();

        // 设置变更后，更新显示
        updateModelCurve();
    }
}

/**
 * @brief 获取用于拟合计算的抽样数据
 * * 核心函数：根据配置（默认/自定义）从原始大数据中抽取关键点。
 * * 默认策略：数据量>200时，在对数空间均匀抽取200个点。
 * * 自定义策略：在用户指定的每个区间内抽取指定数量的点。
 * * @param srcT/srcP/srcD 源数据
 * @param outT/outP/outD 输出的抽样数据
 */
void FittingWidget::getLogSampledData(const QVector<double>& srcT, const QVector<double>& srcP, const QVector<double>& srcD,
                                      QVector<double>& outT, QVector<double>& outP, QVector<double>& outD)
{
    outT.clear(); outP.clear(); outD.clear();
    if (srcT.isEmpty()) return;

    // 辅助结构体用于排序去重
    struct DataPoint {
        double t, p, d;
        bool operator<(const DataPoint& other) const { return t < other.t; }
        bool operator==(const DataPoint& other) const { return std::abs(t - other.t) < 1e-9; }
    };
    QVector<DataPoint> points;

    // 模式1：默认策略
    if (!m_isCustomSamplingEnabled) {
        int targetCount = 200;
        // 如果数据量很少，直接全量使用
        if (srcT.size() <= targetCount) {
            outT = srcT; outP = srcP; outD = srcD;
            return;
        }

        // 对数空间均匀抽样
        double tMin = srcT.first() <= 1e-10 ? 1e-4 : srcT.first();
        double tMax = srcT.last();
        double logMin = log10(tMin);
        double logMax = log10(tMax);
        double step = (logMax - logMin) / (targetCount - 1);

        int currentIndex = 0;
        for (int i = 0; i < targetCount; ++i) {
            double targetT = pow(10, logMin + i * step);

            // 查找最近点
            double minDiff = 1e30;
            int bestIdx = currentIndex;
            while (currentIndex < srcT.size()) {
                double diff = std::abs(srcT[currentIndex] - targetT);
                if (diff < minDiff) { minDiff = diff; bestIdx = currentIndex; }
                else break;
                currentIndex++;
            }
            currentIndex = bestIdx;

            points.append({srcT[bestIdx],
                           (bestIdx<srcP.size()?srcP[bestIdx]:0.0),
                           (bestIdx<srcD.size()?srcD[bestIdx]:0.0)});
        }
    }
    // 模式2：自定义区间策略
    else {
        if (m_customIntervals.isEmpty()) {
            outT = srcT; outP = srcP; outD = srcD;
            return;
        }

        for (const auto& interval : m_customIntervals) {
            double tStart = interval.tStart;
            double tEnd = interval.tEnd;
            int count = interval.count;

            if (count <= 0) continue;

            // 定位区间索引范围
            auto itStart = std::lower_bound(srcT.begin(), srcT.end(), tStart);
            auto itEnd = std::upper_bound(srcT.begin(), srcT.end(), tEnd);

            int idxStart = std::distance(srcT.begin(), itStart);
            int idxEnd = std::distance(srcT.begin(), itEnd);

            if (idxStart >= srcT.size() || idxStart >= idxEnd) continue;

            // 区间内对数抽样
            double subMin = srcT[idxStart];
            double subMax = srcT[idxEnd - 1];
            if (subMin <= 1e-10) subMin = 1e-4;

            double logMin = log10(subMin);
            double logMax = log10(subMax);
            double step = (count > 1) ? (logMax - logMin) / (count - 1) : 0;

            int subCurrentIdx = idxStart;
            for (int i = 0; i < count; ++i) {
                double targetT = (count == 1) ? subMin : pow(10, logMin + i * step);

                double minDiff = 1e30;
                int bestIdx = subCurrentIdx;

                while (subCurrentIdx < idxEnd) {
                    double diff = std::abs(srcT[subCurrentIdx] - targetT);
                    if (diff < minDiff) { minDiff = diff; bestIdx = subCurrentIdx; }
                    else break;
                    subCurrentIdx++;
                }
                subCurrentIdx = bestIdx;

                if (bestIdx < srcT.size()) {
                    points.append({srcT[bestIdx],
                                   (bestIdx<srcP.size()?srcP[bestIdx]:0.0),
                                   (bestIdx<srcD.size()?srcD[bestIdx]:0.0)});
                }
            }
        }
    }

    // 整理：排序并去重
    std::sort(points.begin(), points.end());
    auto last = std::unique(points.begin(), points.end());
    points.erase(last, points.end());

    for (const auto& p : points) {
        outT.append(p.t);
        outP.append(p.p);
        outD.append(p.d);
    }
}

/**
 * @brief 自动拟合按钮槽函数
 * * 检查状态，启动后台拟合线程。
 */
void FittingWidget::on_btnRunFit_clicked() {
    if(m_isFitting) return;
    if(m_obsTime.isEmpty()) {
        QMessageBox::warning(this,"错误","请先加载观测数据。");
        return;
    }

    m_paramChart->updateParamsFromTable();
    m_isFitting = true;
    m_stopRequested = false;
    ui->btnRunFit->setEnabled(false);

    ModelManager::ModelType modelType = m_currentModelType;
    QList<FitParameter> paramsCopy = m_paramChart->getParameters();
    double w = ui->sliderWeight->value() / 100.0;

    // 启动异步任务
    m_watcher.setFuture(QtConcurrent::run([this, modelType, paramsCopy, w](){
        runOptimizationTask(modelType, paramsCopy, w);
    }));
}

/**
 * @brief 停止按钮槽函数
 * * 设置停止标志位，通知后台线程中断。
 */
void FittingWidget::on_btnStop_clicked() {
    m_stopRequested = true;
}

/**
 * @brief 刷新曲线按钮槽函数
 * * 手动触发曲线更新。
 */
void FittingWidget::on_btnImportModel_clicked() {
    updateModelCurve();
}

/**
 * @brief 重置参数按钮槽函数
 * * 恢复当前模型的默认参数值。
 */
void FittingWidget::on_btnResetParams_clicked() {
    if(!m_modelManager) return;
    m_paramChart->resetParams(m_currentModelType);
    updateModelCurve();
}

/**
 * @brief 模型选择按钮槽函数
 * * 弹出模型选择对话框，切换当前模型。
 */
void FittingWidget::on_btn_modelSelect_clicked() {
    ModelSelect dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();
        QString name = dlg.getSelectedModelName();

        bool found = false;
        ModelManager::ModelType newType = ModelManager::Model_1;

        if (code == "modelwidget1") newType = ModelManager::Model_1;
        else if (code == "modelwidget2") newType = ModelManager::Model_2;
        else if (code == "modelwidget3") newType = ModelManager::Model_3;
        else if (code == "modelwidget4") newType = ModelManager::Model_4;
        else if (code == "modelwidget5") newType = ModelManager::Model_5;
        else if (code == "modelwidget6") newType = ModelManager::Model_6;
        else if (!code.isEmpty()) found = true;

        if (code.startsWith("modelwidget")) found = true;

        if (found) {
            m_paramChart->switchModel(newType);
            m_currentModelType = newType;
            ui->btn_modelSelect->setText("当前: " + name);
            updateModelCurve();
        } else {
            QMessageBox::warning(this, "提示", "所选组合暂无对应的模型。\nCode: " + code);
        }
    }
}

/**
 * @brief 导出参数按钮槽函数
 * * 将当前拟合参数导出为 CSV 或 TXT 文件。
 */
void FittingWidget::on_btnExportData_clicked() {
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString fileName = QFileDialog::getSaveFileName(this, "导出拟合参数", defaultDir + "/FittingParameters.csv", "CSV Files (*.csv);;Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);

    if(fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        file.write("\xEF\xBB\xBF"); // BOM for Excel
        out << QString("参数中文名,参数英文名,拟合值,单位\n");
        for(const auto& param : params) {
            QString htmlSym, uniSym, unitStr, dummyName;
            FittingParameterChart::getParamDisplayInfo(param.name, dummyName, htmlSym, uniSym, unitStr);
            if(unitStr == "无因次" || unitStr == "小数") unitStr = "";
            out << QString("%1,%2,%3,%4\n").arg(param.displayName).arg(uniSym).arg(param.value, 0, 'g', 10).arg(unitStr);
        }
    } else {
        for(const auto& param : params) {
            QString htmlSym, uniSym, unitStr, dummyName;
            FittingParameterChart::getParamDisplayInfo(param.name, dummyName, htmlSym, uniSym, unitStr);
            if(unitStr == "无因次" || unitStr == "小数") unitStr = "";
            QString lineStr = QString("%1 (%2): %3 %4").arg(param.displayName).arg(uniSym).arg(param.value, 0, 'g', 10).arg(unitStr);
            out << lineStr.trimmed() << "\n";
        }
    }
    file.close();
    QMessageBox::information(this, "完成", "参数数据已成功导出。");
}

/**
 * @brief 导出曲线数据槽函数
 * * 导出图表中的数据点（实测+理论）到 CSV。
 */
void FittingWidget::onExportCurveData() {
    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString path = QFileDialog::getSaveFileName(this, "导出拟合曲线数据", defaultDir + "/FittingCurves.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    auto graphObsP = m_plot->graph(0);
    auto graphObsD = m_plot->graph(1);
    auto graphModP = m_plot->graph(2);
    auto graphModD = m_plot->graph(3);

    if (!graphObsP) return;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "Obs_Time,Obs_DP,Obs_Deriv,Model_Time,Model_DP,Model_Deriv\n";

        auto itObsP = graphObsP->data()->begin();
        auto itObsD = graphObsD->data()->begin();

        QCPGraphDataContainer::const_iterator itModP, endModP, itModD;
        bool hasModel = (graphModP != nullptr && graphModD != nullptr);
        if(hasModel) {
            itModP = graphModP->data()->begin();
            endModP = graphModP->data()->end();
            itModD = graphModD->data()->begin();
        }

        auto endObsP = graphObsP->data()->end();

        while (itObsP != endObsP || (hasModel && itModP != endModP)) {
            QStringList line;
            if (itObsP != endObsP) {
                line << QString::number(itObsP->key, 'g', 10);
                line << QString::number(itObsP->value, 'g', 10);
                if (itObsD != graphObsD->data()->end()) {
                    line << QString::number(itObsD->value, 'g', 10);
                    ++itObsD;
                } else {
                    line << "";
                }
                ++itObsP;
            } else {
                line << "" << "" << "";
            }
            if (hasModel && itModP != endModP) {
                line << QString::number(itModP->key, 'g', 10);
                line << QString::number(itModP->value, 'g', 10);
                if (itModD != graphModD->data()->end()) {
                    line << QString::number(itModD->value, 'g', 10);
                    ++itModD;
                } else {
                    line << "";
                }
                ++itModP;
            } else {
                line << "" << "" << "";
            }
            out << line.join(",") << "\n";
        }
        f.close();
        QMessageBox::information(this, "导出成功", "拟合曲线数据已保存。");
    }
}

/**
 * @brief 运行优化任务 (线程入口)
 */
void FittingWidget::runOptimizationTask(ModelManager::ModelType modelType, QList<FitParameter> fitParams, double weight) {
    runLevenbergMarquardtOptimization(modelType, fitParams, weight);
}

/**
 * @brief Levenberg-Marquardt 拟合算法核心实现
 * * 实现了非线性最小二乘拟合。
 * * 集成了数据抽样逻辑（getLogSampledData）以提升大数据量下的性能。
 * * 集成了物理参数约束（内区>外区）以确保结果合理性。
 */
void FittingWidget::runLevenbergMarquardtOptimization(ModelManager::ModelType modelType, QList<FitParameter> params, double weight) {
    if(m_modelManager) m_modelManager->setHighPrecision(false);

    QVector<int> fitIndices;
    for(int i=0; i<params.size(); ++i) {
        if(params[i].isFit && params[i].name != "LfD") fitIndices.append(i);
    }
    int nParams = fitIndices.size();

    if(nParams == 0) {
        QMetaObject::invokeMethod(this, "onFitFinished");
        return;
    }

    // [核心] 使用抽样函数获取拟合用数据点
    QVector<double> fitT, fitP, fitD;
    getLogSampledData(m_obsTime, m_obsDeltaP, m_obsDerivative, fitT, fitP, fitD);

    double lambda = 0.01;
    int maxIter = 50;
    double currentSSE = 1e15;

    QMap<QString, double> currentParamMap;
    for(const auto& p : params) currentParamMap.insert(p.name, p.value);

    // [约束] 初始参数物理约束修正 (内区 > 外区)
    if(currentParamMap.contains("kf") && currentParamMap.contains("km")) {
        if(currentParamMap["kf"] <= currentParamMap["km"]) currentParamMap["kf"] = currentParamMap["km"] * 1.01;
    }
    if(currentParamMap.contains("omega1") && currentParamMap.contains("omega2")) {
        if(currentParamMap["omega1"] <= currentParamMap["omega2"]) currentParamMap["omega1"] = currentParamMap["omega2"] * 1.01;
    }

    // 关联参数计算
    if(currentParamMap.contains("L") && currentParamMap.contains("Lf") && currentParamMap["L"] > 1e-9)
        currentParamMap["LfD"] = currentParamMap["Lf"] / currentParamMap["L"];

    // 计算初始残差
    QVector<double> residuals = calculateResiduals(currentParamMap, modelType, weight, fitT, fitP, fitD);
    currentSSE = calculateSumSquaredError(residuals);

    // 初始迭代显示
    ModelCurveData curve = m_modelManager->calculateTheoreticalCurve(modelType, currentParamMap);
    emit sigIterationUpdated(currentSSE/residuals.size(), currentParamMap, std::get<0>(curve), std::get<1>(curve), std::get<2>(curve));

    // 迭代循环
    for(int iter = 0; iter < maxIter; ++iter) {
        if(m_stopRequested) break;
        if (!residuals.isEmpty() && (currentSSE / residuals.size()) < 3e-3) break; // 精度满足则退出

        emit sigProgress(iter * 100 / maxIter);

        // 计算雅可比矩阵
        QVector<QVector<double>> J = computeJacobian(currentParamMap, residuals, fitIndices, modelType, params, weight, fitT, fitP, fitD);
        int nRes = residuals.size();

        // 计算 Hessian 近似矩阵和梯度向量
        QVector<QVector<double>> H(nParams, QVector<double>(nParams, 0.0));
        QVector<double> g(nParams, 0.0);

        for(int k=0; k<nRes; ++k) {
            for(int i=0; i<nParams; ++i) {
                g[i] += J[k][i] * residuals[k];
                for(int j=0; j<=i; ++j) {
                    H[i][j] += J[k][i] * J[k][j];
                }
            }
        }
        for(int i=0; i<nParams; ++i) {
            for(int j=i+1; j<nParams; ++j) {
                H[i][j] = H[j][i];
            }
        }

        bool stepAccepted = false;
        // 阻尼调节循环
        for(int tryIter=0; tryIter<5; ++tryIter) {
            QVector<QVector<double>> H_lm = H;
            for(int i=0; i<nParams; ++i) {
                H_lm[i][i] += lambda * (1.0 + std::abs(H[i][i]));
            }

            QVector<double> negG(nParams);
            for(int i=0;i<nParams;++i) negG[i] = -g[i];

            QVector<double> delta = solveLinearSystem(H_lm, negG);
            QMap<QString, double> trialMap = currentParamMap;

            // 更新参数
            for(int i=0; i<nParams; ++i) {
                int pIdx = fitIndices[i];
                QString pName = params[pIdx].name;
                double oldVal = currentParamMap[pName];
                bool isLog = (oldVal > 1e-12 && pName != "S" && pName != "nf");
                double newVal;

                if(isLog) newVal = pow(10.0, log10(oldVal) + delta[i]);
                else newVal = oldVal + delta[i];

                newVal = qMax(params[pIdx].min, qMin(newVal, params[pIdx].max));
                trialMap[pName] = newVal;
            }

            if(trialMap.contains("L") && trialMap.contains("Lf") && trialMap["L"] > 1e-9)
                trialMap["LfD"] = trialMap["Lf"] / trialMap["L"];

            // [约束] 试探步的物理约束修正
            if(trialMap.contains("kf") && trialMap.contains("km")) {
                if(trialMap["kf"] <= trialMap["km"]) trialMap["kf"] = trialMap["km"] * 1.01;
            }
            if(trialMap.contains("omega1") && trialMap.contains("omega2")) {
                if(trialMap["omega1"] <= trialMap["omega2"]) trialMap["omega1"] = trialMap["omega2"] * 1.01;
            }

            // 评估新位置
            QVector<double> newRes = calculateResiduals(trialMap, modelType, weight, fitT, fitP, fitD);
            double newSSE = calculateSumSquaredError(newRes);

            if(newSSE < currentSSE) {
                currentSSE = newSSE;
                currentParamMap = trialMap;
                residuals = newRes;
                lambda /= 10.0;
                stepAccepted = true;

                ModelCurveData iterCurve = m_modelManager->calculateTheoreticalCurve(modelType, currentParamMap);
                emit sigIterationUpdated(currentSSE/nRes, currentParamMap, std::get<0>(iterCurve), std::get<1>(iterCurve), std::get<2>(iterCurve));
                break;
            } else {
                lambda *= 10.0;
            }
        }
        if(!stepAccepted && lambda > 1e10) break;
    }

    if(m_modelManager) m_modelManager->setHighPrecision(true);

    if(currentParamMap.contains("L") && currentParamMap.contains("Lf") && currentParamMap["L"] > 1e-9)
        currentParamMap["LfD"] = currentParamMap["Lf"] / currentParamMap["L"];

    ModelCurveData finalCurve = m_modelManager->calculateTheoreticalCurve(modelType, currentParamMap);
    emit sigIterationUpdated(currentSSE/residuals.size(), currentParamMap, std::get<0>(finalCurve), std::get<1>(finalCurve), std::get<2>(finalCurve));

    QMetaObject::invokeMethod(this, "onFitFinished");
}

/**
 * @brief 计算残差向量
 * * 计算理论值与抽样观测值在对数空间的差异。
 * * 考虑了压差和导数的权重。
 */
QVector<double> FittingWidget::calculateResiduals(const QMap<QString, double>& params, ModelManager::ModelType modelType, double weight,
                                                  const QVector<double>& t, const QVector<double>& obsP, const QVector<double>& obsD) {
    if(!m_modelManager || t.isEmpty()) return QVector<double>();

    ModelCurveData res = m_modelManager->calculateTheoreticalCurve(modelType, params, t);
    const QVector<double>& pCal = std::get<1>(res);
    const QVector<double>& dpCal = std::get<2>(res);

    QVector<double> r;
    double wp = weight;
    double wd = 1.0 - weight;

    int count = qMin((int)obsP.size(), (int)pCal.size());
    for(int i=0; i<count; ++i) {
        if(obsP[i] > 1e-10 && pCal[i] > 1e-10)
            r.append( (log(obsP[i]) - log(pCal[i])) * wp );
        else
            r.append(0.0);
    }

    int dCount = qMin((int)obsD.size(), (int)dpCal.size());
    dCount = qMin(dCount, count);
    for(int i=0; i<dCount; ++i) {
        if(obsD[i] > 1e-10 && dpCal[i] > 1e-10)
            r.append( (log(obsD[i]) - log(dpCal[i])) * wd );
        else
            r.append(0.0);
    }
    return r;
}

/**
 * @brief 计算雅可比矩阵
 * * 使用有限差分法计算每个拟合参数对残差的偏导数。
 */
QVector<QVector<double>> FittingWidget::computeJacobian(const QMap<QString, double>& params, const QVector<double>& baseResiduals,
                                                        const QVector<int>& fitIndices, ModelManager::ModelType modelType,
                                                        const QList<FitParameter>& currentFitParams, double weight,
                                                        const QVector<double>& t, const QVector<double>& obsP, const QVector<double>& obsD) {
    int nRes = baseResiduals.size();
    int nParams = fitIndices.size();
    QVector<QVector<double>> J(nRes, QVector<double>(nParams));

    for(int j = 0; j < nParams; ++j) {
        int idx = fitIndices[j];
        QString pName = currentFitParams[idx].name;
        double val = params.value(pName);
        bool isLog = (val > 1e-12 && pName != "S" && pName != "nf");

        double h;
        QMap<QString, double> pPlus = params;
        QMap<QString, double> pMinus = params;

        if(isLog) {
            h = 0.01;
            double valLog = log10(val);
            pPlus[pName] = pow(10.0, valLog + h);
            pMinus[pName] = pow(10.0, valLog - h);
        } else {
            h = 1e-4;
            pPlus[pName] = val + h;
            pMinus[pName] = val - h;
        }

        auto updateDeps = [](QMap<QString,double>& map) {
            if(map.contains("L") && map.contains("Lf") && map["L"] > 1e-9)
                map["LfD"] = map["Lf"] / map["L"];
        };

        if(pName == "L" || pName == "Lf") { updateDeps(pPlus); updateDeps(pMinus); }

        QVector<double> rPlus = calculateResiduals(pPlus, modelType, weight, t, obsP, obsD);
        QVector<double> rMinus = calculateResiduals(pMinus, modelType, weight, t, obsP, obsD);

        if(rPlus.size() == nRes && rMinus.size() == nRes) {
            for(int i=0; i<nRes; ++i) {
                J[i][j] = (rPlus[i] - rMinus[i]) / (2.0 * h);
            }
        }
    }
    return J;
}

/**
 * @brief 求解线性方程组
 * * 使用 Eigen 库求解 (H + lambda*I) * delta = -g。
 */
QVector<double> FittingWidget::solveLinearSystem(const QVector<QVector<double>>& A, const QVector<double>& b) {
    int n = b.size();
    if (n == 0) return QVector<double>();

    Eigen::MatrixXd matA(n, n);
    Eigen::VectorXd vecB(n);

    for (int i = 0; i < n; ++i) {
        vecB(i) = b[i];
        for (int j = 0; j < n; ++j) {
            matA(i, j) = A[i][j];
        }
    }

    Eigen::VectorXd x = matA.ldlt().solve(vecB);

    QVector<double> res(n);
    for (int i = 0; i < n; ++i) res[i] = x(i);
    return res;
}

/**
 * @brief 计算均方误差和 (SSE)
 */
double FittingWidget::calculateSumSquaredError(const QVector<double>& residuals) {
    double sse = 0.0;
    for(double v : residuals) sse += v*v;
    return sse;
}

/**
 * @brief 解析敏感性分析输入字符串
 * * 将逗号分隔的字符串解析为数值向量。
 */
QVector<double> FittingWidget::parseSensitivityValues(const QString& text) {
    QVector<double> values;
    QString cleanText = text;
    cleanText.replace(QChar(0xFF0C), ",");
    QStringList parts = cleanText.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        bool ok;
        double v = part.trimmed().toDouble(&ok);
        if (ok) values.append(v);
    }
    return values;
}

/**
 * @brief 更新模型曲线
 * * 根据当前参数计算理论曲线并更新绘图。
 * * 包含物理参数约束逻辑。
 * * 使用抽样数据计算误差以提升性能。
 * * 支持敏感性分析模式（多条曲线绘制）。
 * @param explicitParams 可选的高精度参数字典。
 */
void FittingWidget::updateModelCurve(const QMap<QString, double>* explicitParams) {
    if(!m_modelManager) {
        QMessageBox::critical(this, "错误", "ModelManager 未初始化！");
        return;
    }
    ui->tableParams->clearFocus();

    QMap<QString, double> baseParams;
    QString sensitivityKey = "";
    QVector<double> sensitivityValues;

    if (explicitParams) {
        baseParams = *explicitParams;
    } else {
        QList<FitParameter> allParams = m_paramChart->getParameters();
        for(const auto& p : allParams) baseParams.insert(p.name, p.value);

        QMap<QString, QString> rawTexts = m_paramChart->getRawParamTexts();
        for(auto it = rawTexts.begin(); it != rawTexts.end(); ++it) {
            QVector<double> vals = parseSensitivityValues(it.value());
            if (!vals.isEmpty()) {
                baseParams.insert(it.key(), vals.first());
                if (vals.size() > 1 && sensitivityKey.isEmpty()) {
                    sensitivityKey = it.key();
                    sensitivityValues = vals;
                }
            } else {
                baseParams.insert(it.key(), 0.0);
            }
        }
    }

    if(baseParams.contains("L") && baseParams.contains("Lf") && baseParams["L"] > 1e-9)
        baseParams["LfD"] = baseParams["Lf"] / baseParams["L"];
    else
        baseParams["LfD"] = 0.0;

    // [约束] 手动调节时也确保 Inner > Outer
    if(baseParams.contains("kf") && baseParams.contains("km")) {
        if(baseParams["kf"] <= baseParams["km"]) baseParams["kf"] = baseParams["km"] * 1.01;
    }
    if(baseParams.contains("omega1") && baseParams.contains("omega2")) {
        if(baseParams["omega1"] <= baseParams["omega2"]) baseParams["omega1"] = baseParams["omega2"] * 1.01;
    }

    ModelManager::ModelType type = m_currentModelType;

    // 生成绘图用的时间序列 (对数均匀)
    QVector<double> targetT;
    if (m_obsTime.size() > 300) {
        double tMin = m_obsTime.first() > 1e-5 ? m_obsTime.first() : 1e-5;
        double tMax = m_obsTime.last();
        targetT = ModelManager::generateLogTimeSteps(300, log10(tMin), log10(tMax));
    } else if (!m_obsTime.isEmpty()) {
        targetT = m_obsTime;
    } else {
        for(double e = -4; e <= 4; e += 0.1) targetT.append(pow(10, e));
    }

    bool isSensitivityMode = !sensitivityKey.isEmpty();
    ui->btnRunFit->setEnabled(!isSensitivityMode);
    if(isSensitivityMode) {
        ui->label_Error->setText(QString("敏感性分析模式: %1 (%2 个值)").arg(sensitivityKey).arg(sensitivityValues.size()));
    }

    for (int i = m_plot->graphCount() - 1; i >= 2; --i) {
        m_plot->removeGraph(i);
    }

    QList<QColor> colors = { Qt::red, Qt::blue, QColor(0,180,0), Qt::magenta, QColor(255,140,0), Qt::cyan, Qt::darkRed, Qt::darkBlue };

    if (isSensitivityMode) {
        for(int i = 0; i < sensitivityValues.size(); ++i) {
            double val = sensitivityValues[i];
            QMap<QString, double> currentParams = baseParams;
            currentParams[sensitivityKey] = val;

            if (sensitivityKey == "L" || sensitivityKey == "Lf") {
                if(currentParams["L"] > 1e-9) currentParams["LfD"] = currentParams["Lf"] / currentParams["L"];
            }
            if(currentParams.contains("kf") && currentParams.contains("km")) {
                if(currentParams["kf"] <= currentParams["km"]) currentParams["kf"] = currentParams["km"] * 1.01;
            }

            ModelCurveData res = m_modelManager->calculateTheoreticalCurve(type, currentParams, targetT);

            QColor c = colors[i % colors.size()];
            QString legendSuffix = QString("%1=%2").arg(sensitivityKey).arg(val);

            plotCurves(std::get<0>(res), std::get<1>(res), std::get<2>(res), true);

            int count = m_plot->graphCount();
            if(count >= 2) {
                m_plot->graph(count-2)->setName("P: " + legendSuffix);
                m_plot->graph(count-2)->setPen(QPen(c, 2, Qt::SolidLine));
                m_plot->graph(count-1)->setName("P': " + legendSuffix);
                m_plot->graph(count-1)->setPen(QPen(c, 2, Qt::DashLine));
            }
        }
        m_plot->replot();
    } else {
        ModelCurveData res = m_modelManager->calculateTheoreticalCurve(type, baseParams, targetT);
        plotCurves(std::get<0>(res), std::get<1>(res), std::get<2>(res), true);

        int count = m_plot->graphCount();
        if(count >= 4) {
            m_plot->graph(2)->setName("理论压差");
            m_plot->graph(2)->setPen(QPen(Qt::red, 2));
            m_plot->graph(3)->setName("理论导数");
            m_plot->graph(3)->setPen(QPen(Qt::blue, 2));
        }

        if (!m_obsTime.isEmpty()) {
            // [关键] 使用统一抽样函数计算误差（确保界面显示的误差与拟合时的一致）
            QVector<double> sampleT, sampleP, sampleD;
            getLogSampledData(m_obsTime, m_obsDeltaP, m_obsDerivative, sampleT, sampleP, sampleD);

            QVector<double> residuals = calculateResiduals(baseParams, type, ui->sliderWeight->value()/100.0, sampleT, sampleP, sampleD);
            double sse = calculateSumSquaredError(residuals);
            ui->label_Error->setText(QString("误差(MSE): %1").arg(sse/residuals.size(), 0, 'e', 3));

            // [修改] 仅当启用了自定义抽样时才绘制抽样点
            if (m_isCustomSamplingEnabled) {
                plotSampledPoints(sampleT, sampleP, sampleD);
            }
        }
        m_plot->replot();
    }
}

/**
 * @brief 拟合迭代更新槽
 * * 在拟合过程中接收线程信号，更新UI显示的参数、误差和曲线。
 */
void FittingWidget::onIterationUpdate(double err, const QMap<QString,double>& p,
                                      const QVector<double>& t, const QVector<double>& p_curve, const QVector<double>& d_curve) {
    ui->label_Error->setText(QString("误差(MSE): %1").arg(err, 0, 'e', 3));

    ui->tableParams->blockSignals(true);
    for(int i=0; i<ui->tableParams->rowCount(); ++i) {
        QString key = ui->tableParams->item(i, 1)->data(Qt::UserRole).toString();
        if(p.contains(key)) {
            double val = p[key];
            ui->tableParams->item(i, 2)->setText(QString::number(val, 'g', 5));
        }
    }
    ui->tableParams->blockSignals(false);

    for (int i = m_plot->graphCount() - 1; i >= 2; --i) {
        m_plot->removeGraph(i);
    }
    plotCurves(t, p_curve, d_curve, true);

    m_plot->graph(2)->setName("理论压差");
    m_plot->graph(2)->setPen(QPen(Qt::red, 2));
    m_plot->graph(3)->setName("理论导数");
    m_plot->graph(3)->setPen(QPen(Qt::blue, 2));

    // [修改] 仅当启用了自定义抽样时才绘制抽样点
    if (!m_obsTime.isEmpty() && m_isCustomSamplingEnabled) {
        QVector<double> st, sp, sd;
        getLogSampledData(m_obsTime, m_obsDeltaP, m_obsDerivative, st, sp, sd);
        plotSampledPoints(st, sp, sd);
    }

    m_plot->replot();
}

/**
 * @brief 拟合完成槽函数
 */
void FittingWidget::onFitFinished() {
    m_isFitting = false;
    ui->btnRunFit->setEnabled(true);
    QMessageBox::information(this, "完成", "拟合完成。");
}

/**
 * @brief 绘制曲线
 * * 过滤无效点并添加到 QCustomPlot 图层。
 */
void FittingWidget::plotCurves(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d, bool isModel) {
    if (!m_plot) return;

    QVector<double> vt, vp, vd;
    for(int i=0; i<t.size(); ++i) {
        if(t[i]>1e-8 && p[i]>1e-8) {
            vt << t[i];
            vp << p[i];
            if(i < d.size() && d[i] > 1e-8) vd << d[i];
            else vd << 1e-10;
        }
    }

    if(isModel) {
        QCPGraph* gP = m_plot->addGraph();
        gP->setData(vt, vp);

        QCPGraph* gD = m_plot->addGraph();
        gD->setData(vt, vd);

        if (m_obsTime.isEmpty() && !vt.isEmpty()) {
            m_plot->rescaleAxes();
            if(m_plot->xAxis->range().lower <= 0) m_plot->xAxis->setRangeLower(1e-3);
            if(m_plot->yAxis->range().lower <= 0) m_plot->yAxis->setRangeLower(1e-3);
        }
    }
}

/**
 * @brief 绘制抽样点
 * * 将参与拟合计算的抽样数据点以实心形状绘制在图表上。
 * * 压差：同颜色（与实测压差一致，深绿色）+ 实心圆 (ssCircle + Brush)。
 * * 导数：同颜色（与实测导数一致，洋红色）+ 实心三角 (ssTriangle + Brush)。
 */
void FittingWidget::plotSampledPoints(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d) {
    if (!m_plot) return;

    // 过滤有效数据（剔除无效值以适应对数坐标）
    QVector<double> vt, vp, vd;
    for(int i=0; i<t.size(); ++i) {
        if(t[i] > 1e-8 && p[i] > 1e-8) {
            vt << t[i];
            vp << p[i];
            if(i < d.size() && d[i] > 1e-8) vd << d[i];
            else vd << 1e-10;
        }
    }

    // 1. 绘制抽样压差 (实心圆，深绿色)
    QCPGraph* gP = m_plot->addGraph();
    gP->setData(vt, vp);
    gP->setPen(Qt::NoPen); // 不连线，只显示散点

    // 设置散点样式：ssCircle (圆) + 深绿色边框 + 深绿色填充
    QCPScatterStyle ssP(QCPScatterStyle::ssCircle, QPen(QColor(0, 100, 0)), QBrush(QColor(0, 100, 0)), 6);
    gP->setScatterStyle(ssP);
    gP->setName("抽样压差");

    // 2. 绘制抽样导数 (实心三角，洋红色)
    QCPGraph* gD = m_plot->addGraph();
    gD->setData(vt, vd);
    gD->setPen(Qt::NoPen); // 不连线

    // 设置散点样式：ssTriangle (三角) + 洋红色边框 + 洋红色填充
    QCPScatterStyle ssD(QCPScatterStyle::ssTriangle, QPen(Qt::magenta), QBrush(Qt::magenta), 6);
    gD->setScatterStyle(ssD);
    gD->setName("抽样导数");
}

/**
 * @brief 导出报告按钮槽函数
 * 1. 井名：优先从 .pwt 项目文件中读取 "wellName" 字段。
 * 2. 格式：严格控制字体（5号，宋体+新罗马），头部信息分5行。
 * 3. 分页：数据表展示前100行以占满前两页，拟合曲线强制从第三页开始。
 */
void FittingWidget::on_btnExportReport_clicked()
{
    // ---------------------------------------------------------
    // 1. 获取井名 (解析 .pwt 文件)
    // ---------------------------------------------------------
    QString wellName = "未命名井";
    QString projectFilePath = ModelParameter::instance()->getProjectFilePath();

    // 尝试读取 pwt 文件提取 wellName
    QFile pwtFile(projectFilePath);
    if (pwtFile.exists() && pwtFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = pwtFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject root = doc.object();
            // 假设 wellName 在根节点或 basicParams 节点下，这里尝试直接查找
            if (root.contains("wellName")) {
                wellName = root["wellName"].toString();
            } else if (root.contains("basicParams")) {
                QJsonObject basic = root["basicParams"].toObject();
                if (basic.contains("wellName")) {
                    wellName = basic["wellName"].toString();
                }
            }
        }
        pwtFile.close();
    }

    // 如果文件读取失败，回退到使用文件名
    if (wellName == "未命名井" || wellName.isEmpty()) {
        QFileInfo fi(projectFilePath);
        wellName = fi.completeBaseName();
    }

    // ---------------------------------------------------------
    // 2. 准备文件路径与参数
    // ---------------------------------------------------------
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QString reportFileName = QString("%1试井解释报告.doc").arg(wellName);
    QString defaultDir = QFileInfo(projectFilePath).absolutePath();
    if(defaultDir.isEmpty() || defaultDir == ".") defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString fileName = QFileDialog::getSaveFileName(this, "导出报告",
                                                    defaultDir + "/" + reportFileName,
                                                    "Word 文档 (*.doc);;HTML 文件 (*.html)");
    if(fileName.isEmpty()) return;

    QFileInfo fileInfo(fileName);
    QString baseName = fileInfo.completeBaseName();
    QString path = fileInfo.path();

    // ---------------------------------------------------------
    // 3. 导出观测数据文件 (Excel/CSV)
    // ---------------------------------------------------------
    QString dataFileName = baseName + "_数据表.csv";
    QString dataFilePath = path + "/" + dataFileName;

    QFile dataFile(dataFilePath);
    if (dataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream outData(&dataFile);
        dataFile.write("\xEF\xBB\xBF"); // BOM
        outData << "序号,时间(h),压差(MPa),压力导数(MPa)\n";
        for(int i = 0; i < m_obsTime.size(); ++i) {
            outData << QString("%1,%2,%3,%4\n")
            .arg(i+1)
                .arg(m_obsTime[i])
                .arg(m_obsDeltaP[i])
                .arg(i < m_obsDerivative.size() ? m_obsDerivative[i] : 0.0);
        }
        dataFile.close();
    }

    // ---------------------------------------------------------
    // 4. 生成截图
    // ---------------------------------------------------------
    QCPRange oldXRange = m_plot->xAxis->range();
    QCPRange oldYRange = m_plot->yAxis->range();

    auto gObsP = m_plot->graph(0); // 实测压差
    auto gObsD = m_plot->graph(1); // 实测导数
    auto gModP = m_plot->graph(2); // 理论压差
    auto gModD = m_plot->graph(3); // 理论导数

    // 隐藏所有抽样点
    for(int i=4; i<m_plot->graphCount(); ++i) m_plot->graph(i)->setVisible(false);

    // --- 图1：标准坐标 (只显实测压差) ---
    if(gObsP) gObsP->setVisible(true);
    if(gObsD) gObsD->setVisible(false);
    if(gModP) gModP->setVisible(false);
    if(gModD) gModD->setVisible(false);

    m_plot->xAxis->setScaleType(QCPAxis::stLinear);
    m_plot->yAxis->setScaleType(QCPAxis::stLinear);
    m_plot->rescaleAxes();
    m_plot->yAxis->scaleRange(1.1); // 避免文字重叠
    m_plot->replot();
    QString imgLinearBase64 = getPlotImageBase64();

    // --- 图2：半对数 (只显实测压差) ---
    m_plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    m_plot->yAxis->setScaleType(QCPAxis::stLinear);
    m_plot->rescaleAxes();
    if(m_plot->xAxis->range().lower <= 0) m_plot->xAxis->setRangeLower(1e-4);
    m_plot->yAxis->scaleRange(1.1);
    m_plot->replot();
    QString imgSemiLogBase64 = getPlotImageBase64();

    // --- 图3：双对数 (全显示) ---
    if(gObsD) gObsD->setVisible(true);
    if(gModP) gModP->setVisible(true);
    if(gModD) gModD->setVisible(true);

    m_plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    m_plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    m_plot->xAxis->setRange(oldXRange);
    m_plot->yAxis->setRange(oldYRange);
    m_plot->replot();
    QString imgLogLogBase64 = getPlotImageBase64();

    // 恢复抽样点可见性
    if (m_isCustomSamplingEnabled) {
        for(int i=4; i<m_plot->graphCount(); ++i) m_plot->graph(i)->setVisible(true);
    }

    // ---------------------------------------------------------
    // 5. 构建 Word 兼容 HTML
    // ---------------------------------------------------------
    QString html = "<html xmlns:o='urn:schemas-microsoft-com:office:office' xmlns:w='urn:schemas-microsoft-com:office:word' xmlns='http://www.w3.org/TR/REC-html40'>";
    html += "<head><meta charset='utf-8'><title>Report</title>";
    html += "";
    html += "<style>";

    // 字体设置：西文 Times New Roman，中文 SimSun (宋体)
    // 字体大小：5号对应 10.5pt
    html += "body { font-family: 'Times New Roman', 'SimSun'; font-size: 10.5pt; }";

    html += "h1 { text-align: center; font-size: 16pt; font-weight: bold; margin: 20px 0; font-family: 'SimSun'; }"; // 标题可稍大
    html += "h2 { font-size: 14pt; font-weight: bold; margin-top: 15px; font-family: 'SimSun'; }";

    // 段落样式：单倍行距或固定行距
    html += "p { margin: 3px 0; line-height: 1.5; }";

    // 表格样式
    html += "table { border-collapse: collapse; width: 100%; margin: 5px 0; font-size: 10.5pt; }";
    html += "th, td { border: 1px solid black; padding: 2px 4px; text-align: center; }";
    html += "th { background-color: #f2f2f2; font-family: 'SimSun'; }";

    html += ".img-box { text-align: center; margin: 10px 0; }";
    html += ".img-cap { font-size: 9pt; font-weight: bold; margin-top: 2px; font-family: 'SimSun'; }";

    // [关键] 分页符样式类
    html += ".page-break { page-break-before: always; }";

    html += "</style></head><body>";

    // --- 标题 ---
    html += QString("<h1>%1试井解释报告</h1>").arg(wellName);

    // --- 头部信息 (每项一行) ---
    QString dateStr = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    QString modelStr = ModelManager::getModelTypeName(m_currentModelType);
    QString mseVal = ui->label_Error->text().remove("误差(MSE): ");

    html += QString("<p><b>井名：</b>%1</p>").arg(wellName);
    html += QString("<p><b>报告日期：</b>%1</p>").arg(dateStr);
    html += QString("<p><b>解释模型：</b>%1</p>").arg(modelStr);
    html += QString("<p><b>数据文件：</b>%1</p>").arg(dataFileName);
    html += QString("<p><b>拟合精度 (MSE)：</b>%1</p>").arg(mseVal);


    // --- 第一部分：数据信息 ---
    html += "<h2>一、数据信息</h2>";
    html += "<table>";
    html += "<tr><th>序号</th><th>时间 (h)</th><th>压差 (MPa)</th><th>压力导数 (MPa)</th></tr>";

    // 导出前50行，通常能占满 Word 的前两页
    int rowCount = qMin(50, (int)m_obsTime.size());
    for(int i=0; i<rowCount; ++i) {
        html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
        .arg(i+1)
            .arg(QString::number(m_obsTime[i], 'f', 4))
            .arg(QString::number(m_obsDeltaP[i], 'f', 4))
            .arg(i < m_obsDerivative.size() ? QString::number(m_obsDerivative[i], 'f', 4) : "-");
    }
    html += "</table>";
    html += QString("<p style='font-size:9pt; color:blue; text-align:right;'>* 注：以上展示前%1行数据，完整数据见附件：<b>%2</b></p>")
                .arg(rowCount).arg(dataFileName);

    // --- 第二部分：拟合曲线 (强制从第三页开始) ---
    // 插入分页符
    html += "<br class='page-break' />";
    html += "<h2>二、拟合曲线</h2>";

    // 图1
    html += "<div class='img-box'>";
    html += QString("<img src='data:image/png;base64,%1' width='500' /><br/>").arg(imgLinearBase64);
    html += "<div class='img-cap'>图1 标准坐标系压力历史图 (实测压差)</div>";
    html += "</div>";

    // 图2
    html += "<div class='img-box'>";
    html += QString("<img src='data:image/png;base64,%1' width='500' /><br/>").arg(imgSemiLogBase64);
    html += "<div class='img-cap'>图2 半对数坐标系压力历史图 (实测压差)</div>";
    html += "</div>";

    // 图3
    html += "<div class='img-box'>";
    html += QString("<img src='data:image/png;base64,%1' width='500' /><br/>").arg(imgLogLogBase64);
    html += "<div class='img-cap'>图3 双对数拟合结果图</div>";
    html += "</div>";

    // --- 参数分离 ---
    QString fitParamRows;
    QString defaultParamRows;
    int idxFit = 1, idxDef = 1;

    for(const auto& p : params) {
        QString chName, symbol, uniSym, unit;
        FittingParameterChart::getParamDisplayInfo(p.name, chName, symbol, uniSym, unit);
        if(unit == "无因次" || unit == "小数") unit = "-";

        QString row = QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                          .arg(p.isFit ? idxFit++ : idxDef++)
                          .arg(chName)
                          .arg(uniSym)
                          .arg(p.value, 0, 'g', 6)
                          .arg(unit);

        if (p.isFit) fitParamRows += row;
        else defaultParamRows += row;
    }

    // --- 第三部分：拟合参数 ---
    html += "<h2>三、拟合参数</h2>";
    if (!fitParamRows.isEmpty()) {
        html += "<table><tr><th width='10%'>序号</th><th width='30%'>参数名称</th><th width='20%'>符号</th><th width='25%'>数值</th><th width='15%'>单位</th></tr>";
        html += fitParamRows;
        html += "</table>";
    } else {
        html += "<p>无拟合参数。</p>";
    }

    // --- 第四部分：默认参数 ---
    html += "<h2>四、默认参数</h2>";
    if (!defaultParamRows.isEmpty()) {
        html += "<table><tr><th width='10%'>序号</th><th width='30%'>参数名称</th><th width='20%'>符号</th><th width='25%'>数值</th><th width='15%'>单位</th></tr>";
        html += defaultParamRows;
        html += "</table>";
    } else {
        html += "<p>无默认参数。</p>";
    }

    html += "<br/><hr/><p style='text-align:center; font-size:9pt; color:#888;'>报告来自PWT压力试井分析系统</p>";
    html += "</body></html>";

    // ---------------------------------------------------------
    // 5. 保存文件
    // ---------------------------------------------------------
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法保存报告文件:\n" + file.errorString());
        return;
    }

    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    file.write("\xEF\xBB\xBF");
    out << html;
    file.close();

    QMessageBox::information(this, "成功", QString("报告及数据已导出！\n\n报告文件: %1\n数据文件: %2").arg(fileName).arg(dataFileName));
}


/**
 * @brief 获取图表截图 Base64
 */
QString FittingWidget::getPlotImageBase64()
{
    if(!m_plot) return "";
    QPixmap pixmap = m_plot->toPixmap(800, 600);
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return QString::fromLatin1(byteArray.toBase64().data());
}

/**
 * @brief 保存结果按钮槽函数
 */
void FittingWidget::on_btnSaveFit_clicked()
{
    emit sigRequestSave();
}

/**
 * @brief 获取当前状态 JSON
 * * 序列化当前界面状态（模型、参数、数据、视图、抽样设置）。
 */
QJsonObject FittingWidget::getJsonState() const
{
    const_cast<FittingWidget*>(this)->m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();

    QJsonObject root;
    root["modelType"] = (int)m_currentModelType;
    root["modelName"] = ModelManager::getModelTypeName(m_currentModelType);
    root["fitWeightVal"] = ui->sliderWeight->value();

    QJsonObject plotRange;
    plotRange["xMin"] = m_plot->xAxis->range().lower;
    plotRange["xMax"] = m_plot->xAxis->range().upper;
    plotRange["yMin"] = m_plot->yAxis->range().lower;
    plotRange["yMax"] = m_plot->yAxis->range().upper;
    root["plotView"] = plotRange;

    QJsonArray paramsArray;
    for(const auto& p : params) {
        QJsonObject pObj;
        pObj["name"] = p.name;
        pObj["value"] = p.value;
        pObj["isFit"] = p.isFit;
        pObj["min"] = p.min;
        pObj["max"] = p.max;
        pObj["isVisible"] = p.isVisible;
        pObj["step"] = p.step;
        paramsArray.append(pObj);
    }
    root["parameters"] = paramsArray;

    QJsonArray timeArr, pressArr, derivArr;
    for(double v : m_obsTime) timeArr.append(v);
    for(double v : m_obsDeltaP) pressArr.append(v);
    for(double v : m_obsDerivative) derivArr.append(v);
    QJsonObject obsData;
    obsData["time"] = timeArr;
    obsData["pressure"] = pressArr;
    obsData["derivative"] = derivArr;
    root["observedData"] = obsData;

    root["useCustomSampling"] = m_isCustomSamplingEnabled;
    QJsonArray intervalArr;
    for(const auto& item : m_customIntervals) {
        QJsonObject obj;
        obj["start"] = item.tStart;
        obj["end"] = item.tEnd;
        obj["count"] = item.count;
        intervalArr.append(obj);
    }
    root["customIntervals"] = intervalArr;

    return root;
}

/**
 * @brief 加载状态 JSON
 * * 反序列化并恢复界面状态。
 */
void FittingWidget::loadFittingState(const QJsonObject& root)
{
    if (root.isEmpty()) return;

    if (root.contains("modelType")) {
        int type = root["modelType"].toInt();
        m_currentModelType = (ModelManager::ModelType)type;
        ui->btn_modelSelect->setText("当前: " + ModelManager::getModelTypeName(m_currentModelType));
    }

    m_paramChart->resetParams(m_currentModelType);

    QMap<QString, double> explicitParamsMap;

    if (root.contains("parameters")) {
        QJsonArray arr = root["parameters"].toArray();
        QList<FitParameter> currentParams = m_paramChart->getParameters();

        for(int i=0; i<arr.size(); ++i) {
            QJsonObject pObj = arr[i].toObject();
            QString name = pObj["name"].toString();

            for(auto& p : currentParams) {
                if(p.name == name) {
                    p.value = pObj["value"].toDouble();
                    p.isFit = pObj["isFit"].toBool();
                    p.min = pObj["min"].toDouble();
                    p.max = pObj["max"].toDouble();
                    if(pObj.contains("isVisible")) {
                        p.isVisible = pObj["isVisible"].toBool();
                    } else {
                        p.isVisible = true;
                    }
                    if(pObj.contains("step")) {
                        p.step = pObj["step"].toDouble();
                    }
                    explicitParamsMap.insert(p.name, p.value);
                    break;
                }
            }
        }
        m_paramChart->setParameters(currentParams);
    }

    if (root.contains("fitWeightVal")) {
        int val = root["fitWeightVal"].toInt();
        ui->sliderWeight->setValue(val);
    }

    if (root.contains("observedData")) {
        QJsonObject obs = root["observedData"].toObject();
        QJsonArray tArr = obs["time"].toArray();
        QJsonArray pArr = obs["pressure"].toArray();
        QJsonArray dArr = obs["derivative"].toArray();

        QVector<double> t, p, d;
        for(auto v : tArr) t.append(v.toDouble());
        for(auto v : pArr) p.append(v.toDouble());
        for(auto v : dArr) d.append(v.toDouble());

        setObservedData(t, p, d);
    }

    if (root.contains("useCustomSampling")) {
        m_isCustomSamplingEnabled = root["useCustomSampling"].toBool();
    }
    if (root.contains("customIntervals")) {
        m_customIntervals.clear();
        QJsonArray arr = root["customIntervals"].toArray();
        for(auto v : arr) {
            QJsonObject obj = v.toObject();
            SamplingInterval item;
            item.tStart = obj["start"].toDouble();
            item.tEnd = obj["end"].toDouble();
            item.count = obj["count"].toInt();
            m_customIntervals.append(item);
        }
    }

    updateModelCurve(&explicitParamsMap);

    if (root.contains("plotView")) {
        QJsonObject range = root["plotView"].toObject();
        if (range.contains("xMin") && range.contains("xMax")) {
            double xMin = range["xMin"].toDouble();
            double xMax = range["xMax"].toDouble();
            double yMin = range["yMin"].toDouble();
            double yMax = range["yMax"].toDouble();
            if (xMax > xMin && yMax > yMin && xMin > 0 && yMin > 0) {
                m_plot->xAxis->setRange(xMin, xMax);
                m_plot->yAxis->setRange(yMin, yMax);
                m_plot->replot();
            }
        }
    }
}
