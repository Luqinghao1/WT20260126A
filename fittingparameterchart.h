/*
 * 文件名: fittingparameterchart.h
 * 文件作用: 拟合参数图表管理类头文件
 * 功能描述:
 * 1. 定义拟合参数结构体 FitParameter。
 * 2. 管理拟合界面参数表格的显示、交互与逻辑。
 * 3. 实现参数的默认选择逻辑：根据试井模型类型，自动勾选需要拟合的核心参数。
 * 4. 实现鼠标滚轮调节参数功能，并增加防抖动和边界限制保护。
 */

#ifndef FITTINGPARAMETERCHART_H
#define FITTINGPARAMETERCHART_H

#include <QObject>
#include <QTableWidget>
#include <QList>
#include <QMap>
#include <QEvent>
#include <QTimer>
#include "modelmanager.h"

// 定义拟合参数结构体
struct FitParameter {
    QString name;           // 参数内部标识 (如 "kf")
    QString displayName;    // 参数显示名称 (如 "内区渗透率")
    double value = 0.0;     // 当前值
    bool isFit = false;     // 是否参与拟合
    double min = 0.0;       // 最小值限制
    double max = 100.0;     // 最大值限制
    bool isVisible = true;  // 是否在表格中显示
    double step = 0.1;      // 滚轮调节步长
};

class FittingParameterChart : public QObject
{
    Q_OBJECT
public:
    explicit FittingParameterChart(QTableWidget* parentTable, QObject *parent = nullptr);

    // 设置模型管理器
    void setModelManager(ModelManager* m);

    // 根据模型类型重置参数（设置默认值、可见性及默认拟合勾选）
    void resetParams(ModelManager::ModelType type);

    // 获取/设置参数列表
    QList<FitParameter> getParameters() const;
    void setParameters(const QList<FitParameter>& params);

    // 切换模型（保留共有参数值）
    void switchModel(ModelManager::ModelType newType);

    // 从UI表格读取参数到内部结构
    void updateParamsFromTable();

    // 获取原始文本（用于敏感性分析）
    QMap<QString, QString> getRawParamTexts() const;

    // 刷新表格显示
    void refreshParamTable();

    // 静态辅助：获取参数显示信息
    static void getParamDisplayInfo(const QString& name, QString& chName, QString& symbol, QString& uniSymbol, QString& unit);

signals:
    // 当参数通过滚轮改变且稳定后（防抖）发出此信号
    void parameterChangedByWheel();

protected:
    // 事件过滤器，用于拦截滚轮事件
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // 表格内容变化槽（处理 LfD 联动）
    void onTableItemChanged(QTableWidgetItem *item);

    // 滚轮操作防抖定时器超时槽
    void onWheelDebounceTimeout();

private:
    QTableWidget* m_table;
    ModelManager* m_modelManager;
    QList<FitParameter> m_params;

    // 滚轮防抖定时器
    QTimer* m_wheelTimer;

    // 辅助：向表格添加一行
    void addRowToTable(const FitParameter& p, int& serialNo, bool highlight);

    // 辅助：根据模型类型获取默认需要拟合的参数列表
    QStringList getDefaultFitKeys(ModelManager::ModelType type);
};

#endif // FITTINGPARAMETERCHART_H
