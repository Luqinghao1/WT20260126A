/*
 * 文件名: wt_fittingwidget.h
 * 文件作用: 试井拟合分析主界面类的头文件
 * 功能描述:
 * 1. 声明 FittingWidget 类，用于单次试井分析的拟合操作。
 * 2. 包含 ChartWidget 绘图容器、参数表 FittingParameterChart。
 * 3. 声明加载/保存状态、算法拟合、敏感性分析绘图等核心功能。
 */

#ifndef WT_FITTINGWIDGET_H
#define WT_FITTINGWIDGET_H

#include <QWidget>
#include <QFutureWatcher>
#include <QMap>
#include <QVector>
#include <QJsonObject>
#include "modelmanager.h"
#include "fittingparameterchart.h"
#include "chartwidget.h"
#include "mousezoom.h"

namespace Ui {
class FittingWidget;
}

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
    // 拟合进度信号
    void sigIterationUpdated(double error, QMap<QString,double> params, QVector<double> t, QVector<double> p, QVector<double> d);
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

    // 自动拟合控制
    void on_btnRunFit_clicked();
    void on_btnStop_clicked();
    void onFitFinished();

    // 导出功能
    void on_btnExportData_clicked();
    void on_btnExportReport_clicked();
    void onExportCurveData();
    void on_btnImportModel_clicked();
    void on_btnSaveFit_clicked();

    // [修改] 更新模型曲线
    // 增加 explicitParams 参数：如果传入非空指针，则强制使用该参数计算曲线（避免从表格读取字符串导致的精度丢失）
    void updateModelCurve(const QMap<QString, double>* explicitParams = nullptr);

    // 拟合迭代更新槽
    void onIterationUpdate(double err, const QMap<QString,double>& p, const QVector<double>& t, const QVector<double>& p_curve, const QVector<double>& d_curve);

private:
    Ui::FittingWidget *ui;
    ModelManager* m_modelManager;
    ChartWidget* m_chartWidget;
    MouseZoom* m_plot;
    QCPTextElement* m_plotTitle;
    FittingParameterChart* m_paramChart;

    QMap<QString, QStandardItemModel*> m_dataMap;
    ModelManager::ModelType m_currentModelType;

    // 观测数据
    QVector<double> m_obsTime;
    QVector<double> m_obsDeltaP;
    QVector<double> m_obsDerivative;

    // 拟合控制
    bool m_isFitting;
    bool m_stopRequested;
    QFutureWatcher<void> m_watcher;

    void setupPlot();
    void initializeDefaultModel();
    void plotCurves(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d, bool isModel);

    // 拟合算法相关
    void runOptimizationTask(ModelManager::ModelType modelType, QList<FitParameter> fitParams, double weight);
    void runLevenbergMarquardtOptimization(ModelManager::ModelType modelType, QList<FitParameter> params, double weight);
    QVector<double> calculateResiduals(const QMap<QString, double>& params, ModelManager::ModelType modelType, double weight);
    QVector<QVector<double>> computeJacobian(const QMap<QString, double>& params, const QVector<double>& baseResiduals, const QVector<int>& fitIndices, ModelManager::ModelType modelType, const QList<FitParameter>& currentFitParams, double weight);
    QVector<double> solveLinearSystem(const QVector<QVector<double>>& A, const QVector<double>& b);
    double calculateSumSquaredError(const QVector<double>& residuals);

    // 辅助解析逗号分隔的数值
    QVector<double> parseSensitivityValues(const QString& text);
};

#endif // WT_FITTINGWIDGET_H
