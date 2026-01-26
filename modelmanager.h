/*
 * modelmanager.h
 * 文件作用: 模型管理类头文件
 * 功能描述:
 * 1. 管理所有试井模型界面 (WT_ModelWidget) 的显示与切换。
 * 2. 管理所有数学模型求解器 (ModelSolver01_06) 的实例与计算。
 * 3. 协调模型计算请求，实现界面与算法的解耦。
 */

#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QStackedWidget>
#include <QPushButton>

// 引入新的界面类和求解器类头文件
#include "wt_modelwidget.h"
#include "modelsolver01-06.h"

class ModelManager : public QObject
{
    Q_OBJECT

public:
    // 使用 Solver 中定义的枚举类型，保持对外接口一致
    using ModelType = ModelSolver01_06::ModelType;
    static const ModelType Model_1 = ModelSolver01_06::Model_1;
    static const ModelType Model_2 = ModelSolver01_06::Model_2;
    static const ModelType Model_3 = ModelSolver01_06::Model_3;
    static const ModelType Model_4 = ModelSolver01_06::Model_4;
    static const ModelType Model_5 = ModelSolver01_06::Model_5;
    static const ModelType Model_6 = ModelSolver01_06::Model_6;

    explicit ModelManager(QWidget* parent = nullptr);
    ~ModelManager();

    // 初始化模型体系（界面+求解器）
    void initializeModels(QWidget* parentWidget);

    // 切换当前显示的模型界面
    void switchToModel(ModelType modelType);

    // 获取模型名称描述
    static QString getModelTypeName(ModelType type);

    // 核心计算接口：代理给对应的 Solver 进行计算 (线程安全，可在拟合线程调用)
    ModelCurveData calculateTheoreticalCurve(ModelType type, const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    // 获取默认参数
    QMap<QString, double> getDefaultParameters(ModelType type);

    // 设置全局计算精度
    void setHighPrecision(bool high);

    // 刷新所有界面模型的参数显示
    void updateAllModelsBasicParameters();

    // 生成对数时间步长 (静态工具)
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

    // 观测数据缓存管理
    void setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d);
    void getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const;
    bool hasObservedData() const;
    void clearCache();

signals:
    void modelSwitched(ModelType newType, ModelType oldType);
    void calculationCompleted(const QString& analysisType, const QMap<QString, double>& results);

private slots:
    void onSelectModelClicked();
    // 接收 Widget 计算完成的信号
    void onWidgetCalculationCompleted(const QString& t, const QMap<QString, double>& r);

private:
    void createMainWidget();
    void connectModelSignals();

private:
    QWidget* m_mainWidget;
    QStackedWidget* m_modelStack;

    // [修改] 界面列表使用 WT_ModelWidget
    QVector<WT_ModelWidget*> m_modelWidgets;

    // [新增] 求解器列表，用于纯数学计算 (与界面分离)
    QVector<ModelSolver01_06*> m_solvers;

    ModelType m_currentModelType;

    QVector<double> m_cachedObsTime;
    QVector<double> m_cachedObsPressure;
    QVector<double> m_cachedObsDerivative;
};

#endif // MODELMANAGER_H
