/*
 * modelmanager.cpp
 * 文件作用: 模型管理类实现文件
 * 功能描述:
 * 1. 实例化并管理 6 个 WT_ModelWidget (用于界面显示)。
 * 2. 实例化并管理 6 个 ModelSolver01_06 (用于后台计算)。
 * 3. 处理模型选择逻辑，分发计算任务。
 */

#include "modelmanager.h"
#include "modelselect.h"
#include "modelparameter.h"
#include "wt_modelwidget.h"
#include "modelsolver01-06.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QDebug>
#include <cmath>

ModelManager::ModelManager(QWidget* parent)
    : QObject(parent), m_mainWidget(nullptr), m_modelStack(nullptr)
    , m_currentModelType(Model_1)
{
}

ModelManager::~ModelManager()
{
    // 清理求解器内存 (Widget 由 Qt 父子对象机制自动清理)
    qDeleteAll(m_solvers);
    m_solvers.clear();
}

void ModelManager::initializeModels(QWidget* parentWidget)
{
    if (!parentWidget) return;
    createMainWidget();

    m_modelStack = new QStackedWidget(m_mainWidget);

    m_modelWidgets.clear();
    m_solvers.clear();

    // 循环创建 6 组界面和求解器
    // 使用 ModelSolver01_06::Model_x 枚举
    using MT = ModelSolver01_06::ModelType;
    QList<MT> types = { MT::Model_1, MT::Model_2, MT::Model_3, MT::Model_4, MT::Model_5, MT::Model_6 };

    for(MT type : types) {
        // 1. 创建界面对象，用于显示和交互
        WT_ModelWidget* widget = new WT_ModelWidget(type, m_modelStack);
        m_modelWidgets.append(widget);
        m_modelStack->addWidget(widget);

        // 连接子界面的模型选择请求信号
        connect(widget, &WT_ModelWidget::requestModelSelection, this, &ModelManager::onSelectModelClicked);

        // 2. 创建独立的求解器对象，用于后台/拟合计算
        ModelSolver01_06* solver = new ModelSolver01_06(type);
        m_solvers.append(solver);
    }

    m_mainWidget->layout()->addWidget(m_modelStack);
    connectModelSignals();

    switchToModel(Model_1);

    if (parentWidget->layout()) parentWidget->layout()->addWidget(m_mainWidget);
    else {
        QVBoxLayout* layout = new QVBoxLayout(parentWidget);
        layout->addWidget(m_mainWidget);
        parentWidget->setLayout(layout);
    }
}

void ModelManager::createMainWidget()
{
    m_mainWidget = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(m_mainWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0); // 无边距
    mainLayout->setSpacing(0);
    m_mainWidget->setLayout(mainLayout);
}

void ModelManager::connectModelSignals()
{
    for(WT_ModelWidget* w : m_modelWidgets) {
        connect(w, &WT_ModelWidget::calculationCompleted, this, &ModelManager::onWidgetCalculationCompleted);
    }
}

void ModelManager::switchToModel(ModelType modelType)
{
    if (!m_modelStack) return;
    ModelType old = m_currentModelType;
    m_currentModelType = modelType;
    int index = (int)modelType;

    if (index >= 0 && index < m_modelWidgets.size()) {
        m_modelStack->setCurrentIndex(index);
    }

    emit modelSwitched(modelType, old);
}

void ModelManager::onSelectModelClicked()
{
    ModelSelect dlg(m_mainWidget);
    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();
        // 保持字符串映射逻辑不变
        if (code == "modelwidget1") switchToModel(Model_1);
        else if (code == "modelwidget2") switchToModel(Model_2);
        else if (code == "modelwidget3") switchToModel(Model_3);
        else if (code == "modelwidget4") switchToModel(Model_4);
        else if (code == "modelwidget5") switchToModel(Model_5);
        else if (code == "modelwidget6") switchToModel(Model_6);
        else {
            qDebug() << "未知的模型代码: " << code;
        }
    }
}

QString ModelManager::getModelTypeName(ModelType type)
{
    return ModelSolver01_06::getModelName(type);
}

void ModelManager::onWidgetCalculationCompleted(const QString &t, const QMap<QString, double> &r) {
    emit calculationCompleted(t, r);
}

void ModelManager::setHighPrecision(bool high) {
    // 1. 设置界面里的求解器精度
    for(WT_ModelWidget* w : m_modelWidgets) {
        w->setHighPrecision(high);
    }
    // 2. 设置后台求解器精度 (关键：拟合时使用的是这里的求解器)
    for(ModelSolver01_06* s : m_solvers) {
        s->setHighPrecision(high);
    }
}

void ModelManager::updateAllModelsBasicParameters()
{
    for(WT_ModelWidget* w : m_modelWidgets) {
        QMetaObject::invokeMethod(w, "onResetParameters");
    }
    qDebug() << "所有模型的参数已从全局项目设置中刷新。";
}

QMap<QString, double> ModelManager::getDefaultParameters(ModelType type)
{
    QMap<QString, double> p;
    ModelParameter* mp = ModelParameter::instance();

    p.insert("phi", mp->getPhi());
    p.insert("h", mp->getH());
    p.insert("mu", mp->getMu());
    p.insert("B", mp->getB());
    p.insert("Ct", mp->getCt());
    p.insert("q", mp->getQ());

    p.insert("nf", 4.0);
    p.insert("kf", 1e-3);
    p.insert("km", 1e-4);
    p.insert("L", 1000.0);
    p.insert("Lf", 100.0);
    p.insert("LfD", 0.1);
    p.insert("rmD", 4.0);
    p.insert("omega1", 0.4);
    p.insert("omega2", 0.08);
    p.insert("lambda1", 1e-3);
    p.insert("gamaD", 0.02);

    if (type == Model_1 || type == Model_3 || type == Model_5) {
        p.insert("cD", 0.01);
        p.insert("S", 1.0);
    } else {
        p.insert("cD", 0.0);
        p.insert("S", 0.0);
    }

    if (type == Model_3 || type == Model_4 || type == Model_5 || type == Model_6) {
        p.insert("reD", 10.0);
    }

    return p;
}

// [核心修改] 使用独立的 Solver 进行计算，不再调用 Widget 方法
ModelCurveData ModelManager::calculateTheoreticalCurve(ModelType type, const QMap<QString, double>& params, const QVector<double>& providedTime)
{
    int index = (int)type;
    // 使用 m_solvers 而不是 m_modelWidgets
    if (index >= 0 && index < m_solvers.size()) {
        return m_solvers[index]->calculateTheoreticalCurve(params, providedTime);
    }
    return ModelCurveData();
}

QVector<double> ModelManager::generateLogTimeSteps(int count, double startExp, double endExp) {
    // 委托给 Solver 的静态方法
    return ModelSolver01_06::generateLogTimeSteps(count, startExp, endExp);
}

void ModelManager::setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d)
{
    m_cachedObsTime = t;
    m_cachedObsPressure = p;
    m_cachedObsDerivative = d;
}

void ModelManager::getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const
{
    t = m_cachedObsTime;
    p = m_cachedObsPressure;
    d = m_cachedObsDerivative;
}

void ModelManager::clearCache()
{
    m_cachedObsTime.clear();
    m_cachedObsPressure.clear();
    m_cachedObsDerivative.clear();
}

bool ModelManager::hasObservedData() const
{
    return !m_cachedObsTime.isEmpty();
}
