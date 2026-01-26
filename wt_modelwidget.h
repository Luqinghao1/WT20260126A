/*
 * wt_modelwidget.h
 * 文件作用: 压裂水平井复合页岩油模型界面类头文件 (View/Controller)
 * 功能描述:
 * 1. 管理用户界面，处理参数输入、按钮响应和图表展示。
 * 2. 包含 ModelSolver01_06 实例，调用其进行数学计算。
 * 3. 继承自 QWidget，不再包含复杂的数学算法实现。
 */

#ifndef WT_MODELWIDGET_H
#define WT_MODELWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QColor>
#include <tuple>
#include "chartwidget.h"
#include "modelsolver01-06.h"

namespace Ui {
class WT_ModelWidget;
}

class WT_ModelWidget : public QWidget
{
    Q_OBJECT

public:
    // 使用 Solver 中定义的模型类型
    using ModelType = ModelSolver01_06::ModelType;
    // 使用 Solver 中定义的曲线数据类型
    using ModelCurveData = ::ModelCurveData;

    explicit WT_ModelWidget(ModelType type, QWidget *parent = nullptr);
    ~WT_ModelWidget();

    // 设置高精度模式（转发给 Solver）
    void setHighPrecision(bool high);

    // 直接调用求解器计算（供外部管理器使用，非 UI 交互）
    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    // 获取当前模型名称
    QString getModelName() const;

signals:
    // 计算完成信号，传递模型类型和参数
    void calculationCompleted(const QString& modelType, const QMap<QString, double>& params);

    // 请求模型选择界面的信号
    void requestModelSelection();

public slots:
    void onCalculateClicked();
    void onResetParameters();

    // [逻辑] 响应 L 或 Lf 变化，自动更新 LfD
    void onDependentParamsChanged();

    void onShowPointsToggled(bool checked);
    void onExportData();

private:
    void initUi();
    void initChart();
    void setupConnections();
    void runCalculation(); // UI 触发的计算流程封装

    // 辅助函数
    QVector<double> parseInput(const QString& text);
    void setInputText(QLineEdit* edit, double value);
    void plotCurve(const ModelCurveData& data, const QString& name, QColor color, bool isSensitivity);

private:
    Ui::WT_ModelWidget *ui;
    ModelType m_type;
    ModelSolver01_06* m_solver; // 数学模型求解器实例

    bool m_highPrecision;
    QList<QColor> m_colorList;

    // 缓存计算结果
    QVector<double> res_tD;
    QVector<double> res_pD;
    QVector<double> res_dpD;
};

#endif // WT_MODELWIDGET_H
