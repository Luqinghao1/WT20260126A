/*
 * 文件名: wt_fittingwidget.h
 * 文件作用: 试井拟合分析主界面及辅助设置对话框的头文件
 * 功能描述:
 * 1. 声明 SamplingInterval 结构体与 SamplingSettingsDialog 类，用于管理自定义数据抽样区间。
 * 2. 声明 FittingWidget 类，作为试井拟合的主交互界面。
 * 3. 包含 ChartWidget 绘图容器、参数表 FittingParameterChart。
 * 4. 声明加载/保存状态、算法拟合、敏感性分析绘图等核心功能。
 * 5. 声明基于数据抽样优化的拟合逻辑，支持大数据量下的高效计算。
 * 6. [新增] 声明 plotSampledPoints 函数，用于在图中可视化显示参与拟合的抽样点。
 */

#ifndef WT_FITTINGWIDGET_H
#define WT_FITTINGWIDGET_H

#include <QWidget>
#include <QFutureWatcher>
#include <QMap>
#include <QVector>
#include <QJsonObject>
#include <QDialog>
#include <QTableWidget>
#include <QCheckBox>

#include "modelmanager.h"
#include "fittingparameterchart.h"
#include "chartwidget.h"
#include "mousezoom.h"

namespace Ui {
class FittingWidget;
}

// ============================================================================
// 辅助结构体与对话框类：用于数据抽样设置
// ============================================================================

// 抽样区间结构体
struct SamplingInterval {
    double tStart; // 起始时间
    double tEnd;   // 结束时间
    int count;     // 该区间内的抽样点数
};

// 抽样设置对话框类
class SamplingSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param intervals 当前已有的区间列表
     * @param enabled 当前是否启用自定义抽样
     * @param dataMinT 数据的最小时间（用于提示或默认值）
     * @param dataMaxT 数据的最大时间
     * @param parent 父窗口指针
     */
    explicit SamplingSettingsDialog(const QList<SamplingInterval>& intervals, bool enabled,
                                    double dataMinT, double dataMaxT, QWidget *parent = nullptr);

    // 获取设置后的区间列表
    QList<SamplingInterval> getIntervals() const;
    // 获取是否启用自定义抽样
    bool isCustomSamplingEnabled() const;

private slots:
    void onAddRow();      // 添加一行
    void onRemoveRow();   // 删除选中行
    void onResetDefault();// 重置为默认区间 (按对数空间)

private:
    QTableWidget* m_table; // 表格控件
    QCheckBox* m_chkEnable;// 启用开关
    double m_dataMinT;     // 数据最小时间
    double m_dataMaxT;     // 数据最大时间

    // 辅助函数：向表格添加一行
    void addRow(double start, double end, int count);
};

// ============================================================================
// 主界面类：FittingWidget
// ============================================================================

class FittingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FittingWidget(QWidget *parent = nullptr);
    ~FittingWidget();

    // 设置模型管理器
    void setModelManager(ModelManager* m);

    // 设置项目数据模型集合（用于加载数据对话框）
    void setProjectDataModels(const QMap<QString, QStandardItemModel*>& models);

    // 设置观测数据
    void setObservedData(const QVector<double>& t, const QVector<double>& deltaP, const QVector<double>& d);

    // 初始化/重置基本参数
    void updateBasicParameters();

    // 获取当前状态（用于保存项目）
    QJsonObject getJsonState() const;
    // 加载状态
    void loadFittingState(const QJsonObject& root);

    // 获取截图
    QString getPlotImageBase64();

signals:
    // 拟合进度信号：更新误差、参数、曲线数据
    void sigIterationUpdated(double error, QMap<QString,double> params, QVector<double> t, QVector<double> p, QVector<double> d);
    // 进度条信号
    void sigProgress(int percent);
    // 请求父页面保存
    void sigRequestSave();

private slots:
    // 数据加载
    void on_btnLoadData_clicked();

    // 参数调节与重置
    void on_btnSelectParams_clicked();
    void on_btnResetParams_clicked();
    void on_btn_modelSelect_clicked();

    // 权重调节
    void onSliderWeightChanged(int value);

    // 拟合控制
    void on_btnRunFit_clicked();
    void on_btnStop_clicked();
    void onFitFinished();

    // 数据抽样设置按钮槽函数
    void onOpenSamplingSettings();

    // 导出功能
    void on_btnExportData_clicked();
    void on_btnExportReport_clicked();
    void onExportCurveData();
    void on_btnImportModel_clicked();
    void on_btnSaveFit_clicked();

    // 更新模型曲线 (explicitParams 用于防止文本转换精度丢失)
    void updateModelCurve(const QMap<QString, double>* explicitParams = nullptr);

    // 拟合迭代更新槽 (线程安全)
    void onIterationUpdate(double err, const QMap<QString,double>& p, const QVector<double>& t, const QVector<double>& p_curve, const QVector<double>& d_curve);

private:
    Ui::FittingWidget *ui;
    ModelManager* m_modelManager;
    ChartWidget* m_chartWidget;
    MouseZoom* m_plot;
    QCPTextElement* m_plotTitle;
    FittingParameterChart* m_paramChart; // 参数表管理类

    QMap<QString, QStandardItemModel*> m_dataMap;
    ModelManager::ModelType m_currentModelType;

    // 观测数据
    QVector<double> m_obsTime;
    QVector<double> m_obsDeltaP;
    QVector<double> m_obsDerivative;

    // 拟合控制
    bool m_isFitting;
    bool m_stopRequested;
    QFutureWatcher<void> m_watcher; // 异步任务监视器

    // 抽样设置相关变量
    bool m_isCustomSamplingEnabled;           // 是否启用自定义抽样
    QList<SamplingInterval> m_customIntervals;// 自定义抽样区间列表

    // 内部初始化函数
    void setupPlot();
    void initializeDefaultModel();
    void plotCurves(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d, bool isModel);

    // [新增] 绘制抽样点：用于在图中显示实际参与拟合的数据点
    void plotSampledPoints(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d);

    // 拟合算法相关
    void runOptimizationTask(ModelManager::ModelType modelType, QList<FitParameter> fitParams, double weight);
    void runLevenbergMarquardtOptimization(ModelManager::ModelType modelType, QList<FitParameter> params, double weight);

    // 计算残差向量 (支持传入抽样后的数据)
    QVector<double> calculateResiduals(const QMap<QString, double>& params, ModelManager::ModelType modelType, double weight,
                                       const QVector<double>& t, const QVector<double>& obsP, const QVector<double>& obsD);

    // 计算雅可比矩阵 (支持传入抽样后的数据)
    QVector<QVector<double>> computeJacobian(const QMap<QString, double>& params, const QVector<double>& baseResiduals,
                                             const QVector<int>& fitIndices, ModelManager::ModelType modelType,
                                             const QList<FitParameter>& currentFitParams, double weight,
                                             const QVector<double>& t, const QVector<double>& obsP, const QVector<double>& obsD);

    QVector<double> solveLinearSystem(const QVector<QVector<double>>& A, const QVector<double>& b);
    double calculateSumSquaredError(const QVector<double>& residuals);

    // 辅助解析敏感性分析输入
    QVector<double> parseSensitivityValues(const QString& text);

    // 抽样函数：根据设置（默认或自定义）获取用于拟合计算的数据点
    void getLogSampledData(const QVector<double>& srcT, const QVector<double>& srcP, const QVector<double>& srcD,
                           QVector<double>& outT, QVector<double>& outP, QVector<double>& outD);
};

#endif // WT_FITTINGWIDGET_H
