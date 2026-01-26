/*
 * modelsolver01-06.h
 * 文件作用: 压裂水平井复合页岩油模型核心计算类头文件
 * 功能描述:
 * 1. 定义模型类型枚举 (ModelType) 和曲线数据类型 (ModelCurveData)。
 * 2. 声明纯数学计算逻辑，包括拉普拉斯变换、贝塞尔函数计算、Stehfest 数值反演等。
 * 3. 不依赖任何 UI 控件，仅负责数据输入与结果输出。
 */

#ifndef MODELSOLVER01_06_H
#define MODELSOLVER01_06_H

#include <QMap>
#include <QVector>
#include <QString>
#include <tuple>
#include <functional>

// 类型定义: <时间, 压力, 导数>
using ModelCurveData = std::tuple<QVector<double>, QVector<double>, QVector<double>>;

class ModelSolver01_06
{
public:
    // 模型类型枚举
    enum ModelType {
        Model_1 = 0, // 无限大 + 变井储
        Model_2,     // 无限大 + 恒定井储
        Model_3,     // 封闭边界 + 变井储
        Model_4,     // 封闭边界 + 恒定井储
        Model_5,     // 定压边界 + 变井储
        Model_6      // 定压边界 + 恒定井储
    };

    // 构造函数
    explicit ModelSolver01_06(ModelType type);
    virtual ~ModelSolver01_06();

    // 设置计算精度
    void setHighPrecision(bool high);

    // 核心计算接口：根据参数和时间序列计算理论曲线
    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    // 获取模型名称（静态辅助函数）
    static QString getModelName(ModelType type);

    // 生成对数时间步长（静态辅助函数，供内部或外部生成时间序列使用）
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

private:
    // 计算无因次压力和导数
    void calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                             std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                             QVector<double>& outPD, QVector<double>& outDeriv);

    // 拉普拉斯空间下的复合模型函数
    double flaplace_composite(double z, const QMap<QString, double>& p);

    // 计算点源解的拉普拉斯变换值
    double PWD_composite(double z, double fs1, double fs2, double M12, double LfD, double rmD, double reD, int nf, const QVector<double>& xwD, ModelType type);

    // 数学辅助函数
    double scaled_besseli(int v, double x);
    double gauss15(std::function<double(double)> f, double a, double b);
    double adaptiveGauss(std::function<double(double)> f, double a, double b, double eps, int depth, int maxDepth);
    double stefestCoefficient(int i, int N);
    double factorial(int n);

private:
    ModelType m_type;       // 当前模型类型
    bool m_highPrecision;   // 高精度计算标志
};

#endif // MODELSOLVER01_06_H
