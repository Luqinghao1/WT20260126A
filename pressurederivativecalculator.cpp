/*
 * 文件名: pressurederivativecalculator.cpp
 * 文件作用: 压力导数计算器实现
 * 功能描述:
 * 1. 实现了基于试井类型的压差计算逻辑 (降落: Pi-P, 恢复: P-Pwf)。
 * 2. 实现了 Bourdet 导数算法。
 * 3. 将计算生成的压差和导数写回数据模型。
 */

#include "pressurederivativecalculator.h"
#include <QStandardItem>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>

PressureDerivativeCalculator::PressureDerivativeCalculator(QObject *parent)
    : QObject(parent)
{
}

PressureDerivativeCalculator::~PressureDerivativeCalculator()
{
}

PressureDerivativeResult PressureDerivativeCalculator::calculatePressureDerivative(
    QStandardItemModel* model, const PressureDerivativeConfig& config)
{
    PressureDerivativeResult result;
    result.success = false;
    // 初始化索引
    result.deltaPColumnIndex = -1;
    result.derivativeColumnIndex = -1;
    result.addedColumnIndex = -1; // 初始化兼容字段
    result.processedRows = 0;

    // 检查数据模型
    if (!model) {
        result.errorMessage = "数据模型不存在";
        return result;
    }

    int rowCount = model->rowCount();
    if (rowCount < 3) {
        result.errorMessage = "数据行数不足（至少需要3行）";
        return result;
    }

    // 检查列索引
    if (config.pressureColumnIndex < 0 || config.pressureColumnIndex >= model->columnCount()) {
        result.errorMessage = "压力列索引无效";
        return result;
    }

    if (config.timeColumnIndex < 0 || config.timeColumnIndex >= model->columnCount()) {
        result.errorMessage = "时间列索引无效";
        return result;
    }

    // 检查L-Spacing参数
    if (config.lSpacing <= 0) {
        result.errorMessage = "L-Spacing参数必须大于0";
        return result;
    }

    emit progressUpdated(10, "正在读取数据...");

    // 读取时间和原始压力数据
    QVector<double> timeData;
    QVector<double> pressureData;
    timeData.reserve(rowCount);
    pressureData.reserve(rowCount);

    for (int row = 0; row < rowCount; ++row) {
        QStandardItem* timeItem = model->item(row, config.timeColumnIndex);
        QStandardItem* pressureItem = model->item(row, config.pressureColumnIndex);

        double timeValue = 0.0;
        double pressureValue = 0.0;

        if (timeItem) timeValue = parseNumericValue(timeItem->text());
        if (pressureItem) pressureValue = parseNumericValue(pressureItem->text());

        // 检查时间值有效性
        if (timeValue < 0) {
            result.errorMessage = QString("检测到无效时间值（行 %1），时间不能为负数").arg(row + 1);
            return result;
        }

        timeData.append(timeValue);
        pressureData.append(pressureValue);
    }

    // --- 步骤 1: 处理时间偏移 (t -> Delta t) ---
    // 双对数曲线要求时间必须 > 0
    double actualTimeOffset = 0.0;
    if (config.autoTimeOffset) {
        double minPositiveTime = -1;
        bool hasZeroTime = false;

        for (double t : timeData) {
            if (t <= 0) hasZeroTime = true;
            else {
                if (minPositiveTime < 0 || t < minPositiveTime) minPositiveTime = t;
            }
        }

        if (hasZeroTime) {
            // 如果有0值，取最小正值的1/10作为偏移，或者使用默认偏移
            if (minPositiveTime > 0) actualTimeOffset = minPositiveTime * 0.1;
            else actualTimeOffset = config.timeOffset;
        }
    } else {
        actualTimeOffset = config.timeOffset;
    }

    QVector<double> adjustedTimeData;
    adjustedTimeData.reserve(rowCount);
    for (double t : timeData) {
        adjustedTimeData.append(t + actualTimeOffset);
    }

    emit progressUpdated(30, "正在计算压差(Delta P)...");

    // --- 步骤 2: 计算压差 (Delta P) ---
    // 根据试井类型选择不同的公式
    QVector<double> deltaPData;
    deltaPData.reserve(rowCount);

    if (config.testType == PressureDerivativeConfig::Drawdown) {
        // 压力降落试井 (Drawdown): Delta P = Pi - P(t)
        // 注意：Pi 由用户输入
        double pi = config.initialPressure;
        for (double p : pressureData) {
            // 理论上降落试井 P < Pi，取差值。如果是异常数据导致 P > Pi，暂时取绝对值以保证双对数图可绘
            double dp = pi - p;
            deltaPData.append(std::abs(dp));
        }
    } else {
        // 压力恢复试井 (Buildup): Delta P = P(t) - Pwf(Delta t=0)
        // 假设数据第一点为关井时刻流压
        double p_shut_in = pressureData.isEmpty() ? 0.0 : pressureData[0];
        for (double p : pressureData) {
            double dp = p - p_shut_in;
            deltaPData.append(std::abs(dp));
        }
    }

    emit progressUpdated(50, "正在计算Bourdet导数...");

    // --- 步骤 3: 计算导数 ---
    QVector<double> derivativeData = calculateBourdetDerivative(adjustedTimeData, deltaPData, config.lSpacing);

    if (derivativeData.size() != rowCount) {
        result.errorMessage = "导数计算结果数量不匹配";
        return result;
    }

    emit progressUpdated(80, "正在写入结果...");

    // --- 步骤 4: 将结果写入模型 ---

    // 4.1 插入压差列 (Delta P)
    // 通常紧跟在原始压力列之后
    int deltaPColIdx = config.pressureColumnIndex + 1;
    model->insertColumn(deltaPColIdx);

    QString deltaPHeader = QString("压差(Delta P)\\%1").arg(config.pressureUnit);
    model->setHorizontalHeaderItem(deltaPColIdx, new QStandardItem(deltaPHeader));

    for (int row = 0; row < rowCount; ++row) {
        QString val = formatValue(deltaPData[row], 6);
        QStandardItem* item = new QStandardItem(val);
        item->setForeground(QBrush(QColor("darkgreen"))); // 绿色文字区分压差
        model->setItem(row, deltaPColIdx, item);
    }
    // 记录压差列索引
    result.deltaPColumnIndex = deltaPColIdx;
    result.deltaPColumnName = deltaPHeader;

    // 4.2 插入导数列 (Derivative)
    // 在压差列之后
    int derivColIdx = deltaPColIdx + 1;
    model->insertColumn(derivColIdx);

    QString derivHeader = QString("压力导数\\%1").arg(config.pressureUnit);
    model->setHorizontalHeaderItem(derivColIdx, new QStandardItem(derivHeader));

    for (int row = 0; row < rowCount; ++row) {
        QString val = formatValue(derivativeData[row], 6);
        QStandardItem* item = new QStandardItem(val);
        item->setForeground(QBrush(QColor("#1565C0"))); // 蓝色文字区分导数
        model->setItem(row, derivColIdx, item);
        result.processedRows++;
    }

    // 记录导数列索引
    result.derivativeColumnIndex = derivColIdx;
    result.derivativeColumnName = derivHeader;

    // --- 关键修正：为了兼容旧代码，填充旧字段 ---
    // 旧代码通常只关心计算出的那个“导数”列
    result.addedColumnIndex = derivColIdx;
    result.columnName = derivHeader;

    emit progressUpdated(100, "计算完成");

    result.success = true;
    emit calculationCompleted(result);

    return result;
}

// 静态方法实现：Bourdet 导数核心算法
QVector<double> PressureDerivativeCalculator::calculateBourdetDerivative(
    const QVector<double>& timeData,
    const QVector<double>& pressureDropData,
    double lSpacing)
{
    QVector<double> derivativeData;
    int n = timeData.size();
    derivativeData.reserve(n);

    if (n == 0) return derivativeData;

    for (int i = 0; i < n; ++i) {
        double derivative = 0.0;
        double ti = timeData[i];
        double pi = pressureDropData[i];

        // 寻找左侧点j：ln(ti) - ln(tj) ≥ L
        int leftIndex = findLeftPoint(timeData, i, lSpacing);

        // 寻找右侧点k：ln(tk) - ln(ti) ≥ L
        int rightIndex = findRightPoint(timeData, i, lSpacing);

        // 1. 如果找到左右两个点，使用加权平均法 (Bourdet Standard)
        if (leftIndex >= 0 && rightIndex >= 0) {
            double tj = timeData[leftIndex];
            double pj = pressureDropData[leftIndex];
            double tk = timeData[rightIndex];
            double pk = pressureDropData[rightIndex];

            // 计算对数差值
            double deltaXL = std::log(ti) - std::log(tj);
            double deltaXR = std::log(tk) - std::log(ti);

            // 计算左导数和右导数
            double mL = calculateDerivativeValue(ti, tj, pi, pj);
            double mR = calculateDerivativeValue(tk, ti, pk, pi);

            // 加权平均公式
            if (deltaXL + deltaXR > 1e-12) {
                derivative = (mL * deltaXR + mR * deltaXL) / (deltaXL + deltaXR);
            } else {
                derivative = 0.0;
            }
        }
        // 2. 边界情况：只找到左侧点 (曲线末端)
        else if (leftIndex >= 0 && rightIndex < 0) {
            double tj = timeData[leftIndex];
            double pj = pressureDropData[leftIndex];
            derivative = calculateDerivativeValue(ti, tj, pi, pj);
        }
        // 3. 边界情况：只找到右侧点 (曲线开端)
        else if (leftIndex < 0 && rightIndex >= 0) {
            double tk = timeData[rightIndex];
            double pk = pressureDropData[rightIndex];
            derivative = calculateDerivativeValue(tk, ti, pk, pi);
        }
        // 4. L-Spacing 范围内点不足
        else {
            // 使用简单的相邻点差分作为保底
            if (i > 0) {
                double t_prev = timeData[i-1];
                double p_prev = pressureDropData[i-1];
                derivative = calculateDerivativeValue(ti, t_prev, pi, p_prev);
            } else if (i < n - 1) {
                double t_next = timeData[i+1];
                double p_next = pressureDropData[i+1];
                derivative = calculateDerivativeValue(t_next, ti, p_next, pi);
            } else {
                derivative = 0.0;
            }
        }

        // 导数结果取绝对值（双对数图要求正值）
        derivativeData.append(std::abs(derivative));
    }

    return derivativeData;
}

int PressureDerivativeCalculator::findLeftPoint(const QVector<double>& timeData, int currentIndex, double lSpacing)
{
    if (currentIndex <= 0 || timeData.isEmpty()) return -1;
    double ti = timeData[currentIndex];
    if (ti <= 0) return -1;
    double lnTi = std::log(ti);

    for (int j = currentIndex - 1; j >= 0; --j) {
        double tj = timeData[j];
        if (tj <= 0) continue;
        double lnTj = std::log(tj);
        if ((lnTi - lnTj) >= lSpacing) return j;
    }
    return -1;
}

int PressureDerivativeCalculator::findRightPoint(const QVector<double>& timeData, int currentIndex, double lSpacing)
{
    int n = timeData.size();
    if (currentIndex >= n - 1 || timeData.isEmpty()) return -1;
    double ti = timeData[currentIndex];
    if (ti <= 0) return -1;
    double lnTi = std::log(ti);

    for (int k = currentIndex + 1; k < n; ++k) {
        double tk = timeData[k];
        if (tk <= 0) continue;
        double lnTk = std::log(tk);
        if ((lnTk - lnTi) >= lSpacing) return k;
    }
    return -1;
}

double PressureDerivativeCalculator::calculateDerivativeValue(double t1, double t2, double p1, double p2)
{
    if (t1 <= 0 || t2 <= 0) return 0.0;
    double lnT1 = std::log(t1);
    double lnT2 = std::log(t2);
    double deltaLnT = lnT1 - lnT2;

    if (std::abs(deltaLnT) < 1e-10) return 0.0;
    return (p1 - p2) / deltaLnT;
}

PressureDerivativeConfig PressureDerivativeCalculator::autoDetectColumns(QStandardItemModel* model)
{
    PressureDerivativeConfig config;
    if (!model) return config;
    config.pressureColumnIndex = findPressureColumn(model);
    config.timeColumnIndex = findTimeColumn(model);
    return config;
}

int PressureDerivativeCalculator::findPressureColumn(QStandardItemModel* model)
{
    if (!model) return -1;
    QStringList pressureKeywords = {"压力", "pressure", "pres", "P\\", "压力\\"};
    for (int col = 0; col < model->columnCount(); ++col) {
        QStandardItem* headerItem = model->horizontalHeaderItem(col);
        if (headerItem) {
            QString headerText = headerItem->text();
            for (const QString& keyword : pressureKeywords) {
                if (headerText.contains(keyword, Qt::CaseInsensitive)) {
                    if (!headerText.contains("压降") && !headerText.contains("导数") && !headerText.contains("Delta")) {
                        return col;
                    }
                }
            }
        }
    }
    return -1;
}

int PressureDerivativeCalculator::findTimeColumn(QStandardItemModel* model)
{
    if (!model) return -1;
    QStringList timeKeywords = {"时间", "time", "t\\", "小时", "hour", "min", "sec"};
    for (int col = 0; col < model->columnCount(); ++col) {
        QStandardItem* headerItem = model->horizontalHeaderItem(col);
        if (headerItem) {
            QString headerText = headerItem->text();
            for (const QString& keyword : timeKeywords) {
                if (headerText.contains(keyword, Qt::CaseInsensitive)) {
                    return col;
                }
            }
        }
    }
    return -1;
}

double PressureDerivativeCalculator::parseNumericValue(const QString& str)
{
    if (str.isEmpty()) return 0.0;
    QString cleanStr = str.trimmed();
    bool ok;
    double value = cleanStr.toDouble(&ok);
    if (ok) return value;
    cleanStr.remove(QRegularExpression("[a-zA-Z%\\s]+$"));
    value = cleanStr.toDouble(&ok);
    return ok ? value : 0.0;
}

QString PressureDerivativeCalculator::formatValue(double value, int precision)
{
    if (std::isnan(value) || std::isinf(value)) return "0";
    return QString::number(value, 'g', precision);
}
