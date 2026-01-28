/*
 * modelsolver01-06.cpp
 * 文件作用: 压裂水平井复合页岩油模型核心计算类实现
 * 功能描述:
 * 1. 实现6种不同边界和井储条件组合的页岩油数学模型解。
 * 2. 包含 Stehfest 数值反演算法、自适应高斯积分、Bessel 函数调用等核心算法。
 * 3. 实现了数据处理和物理量到无因次量的转换逻辑。
 * 4. [修改] 强制在计算中执行 LfD = Lf / L 的约束逻辑，确保物理意义一致。
 */

#include "modelsolver01-06.h"
#include "pressurederivativecalculator.h"

#include <Eigen/Dense>
#include <boost/math/special_functions/bessel.hpp>
#include <cmath>
#include <algorithm>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 构造函数
ModelSolver01_06::ModelSolver01_06(ModelType type)
    : m_type(type)
    , m_highPrecision(true)
{
}

// 析构函数
ModelSolver01_06::~ModelSolver01_06()
{
}

// 设置精度
void ModelSolver01_06::setHighPrecision(bool high)
{
    m_highPrecision = high;
}

// 获取模型名称
QString ModelSolver01_06::getModelName(ModelType type)
{
    switch(type) {
    case Model_1: return "模型1: 变井储+无限大边界";
    case Model_2: return "模型2: 恒定井储+无限大边界";
    case Model_3: return "模型3: 变井储+封闭边界";
    case Model_4: return "模型4: 恒定井储+封闭边界";
    case Model_5: return "模型5: 变井储+定压边界";
    case Model_6: return "模型6: 恒定井储+定压边界";
    default: return "未知模型";
    }
}

// 生成对数时间步长
QVector<double> ModelSolver01_06::generateLogTimeSteps(int count, double startExp, double endExp)
{
    QVector<double> t;
    if (count <= 0) return t;
    t.reserve(count);
    for (int i = 0; i < count; ++i) {
        double exponent = startExp + (endExp - startExp) * i / (count - 1);
        t.append(pow(10.0, exponent));
    }
    return t;
}

// 核心计算函数
ModelCurveData ModelSolver01_06::calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime)
{
    // 1. 准备时间序列
    QVector<double> tPoints = providedTime;
    if (tPoints.isEmpty()) {
        tPoints = generateLogTimeSteps(100, -3.0, 3.0);
    }

    // 2. 提取物理参数
    double phi = params.value("phi", 0.05);
    double mu = params.value("mu", 0.5);
    double B = params.value("B", 1.05);
    double Ct = params.value("Ct", 5e-4);
    double q = params.value("q", 5.0);
    double h = params.value("h", 20.0);
    double kf = params.value("kf", 1e-3);
    double L = params.value("L", 1000.0);

    // 3. 计算无因次时间 tD
    // 注意：这里的系数 14.4 是基于特定单位制的工程常数
    // 公式: tD = C * k * t / (phi * mu * Ct * L^2)
    double td_coeff = 14.4 * kf / (phi * mu * Ct * pow(L, 2));

    QVector<double> tD_vec;
    tD_vec.reserve(tPoints.size());
    for(double t : tPoints) {
        double val = td_coeff * t;
        tD_vec.append(val);
    }

    // 4. 计算无因次压力和导数
    QVector<double> PD_vec, Deriv_vec;
    auto func = std::bind(&ModelSolver01_06::flaplace_composite, this, std::placeholders::_1, std::placeholders::_2);
    calculatePDandDeriv(tD_vec, params, func, PD_vec, Deriv_vec);

    // 5. 将无因次量转换为物理量 (压差 dp)
    // dp = 1.842e-3 * q * mu * B / (k * h) * pD
    double p_coeff = 1.842e-3 * q * mu * B / (kf * h);

    QVector<double> finalP(tPoints.size()), finalDP(tPoints.size());

    for(int i=0; i<tPoints.size(); ++i) {
        finalP[i] = p_coeff * PD_vec[i];
        finalDP[i] = p_coeff * Deriv_vec[i];
    }

    return std::make_tuple(tPoints, finalP, finalDP);
}

// Stehfest 数值反演计算 PD 和导数
void ModelSolver01_06::calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                                           std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                                           QVector<double>& outPD, QVector<double>& outDeriv)
{
    int numPoints = tD.size();
    outPD.resize(numPoints);
    outDeriv.resize(numPoints);

    int N_param = (int)params.value("N", 4);
    int N = m_highPrecision ? N_param : 4;
    if (N % 2 != 0) N = 4;
    double ln2 = log(2.0);

    double gamaD = params.value("gamaD", 0.0);

    for (int k = 0; k < numPoints; ++k) {
        double t = tD[k];
        if (t <= 1e-12) { outPD[k] = 0; continue; }

        double pd_val = 0.0;
        for (int m = 1; m <= N; ++m) {
            double z = m * ln2 / t;
            double pf = laplaceFunc(z, params);
            if (std::isnan(pf) || std::isinf(pf)) pf = 0.0;
            pd_val += stefestCoefficient(m, N) * pf;
        }
        outPD[k] = pd_val * ln2 / t;

        // 考虑压敏效应修正
        if (std::abs(gamaD) > 1e-9) {
            double arg = 1.0 - gamaD * outPD[k];
            if (arg > 1e-12) {
                outPD[k] = -1.0 / gamaD * std::log(arg);
            }
        }
    }

    // 计算导数 (Bourdet 导数)
    if (numPoints > 2) {
        outDeriv = PressureDerivativeCalculator::calculateBourdetDerivative(tD, outPD, 0.1);
    } else {
        outDeriv.fill(0.0);
    }
}

// 拉普拉斯空间下的复合模型总函数 (包含井储和表皮)
double ModelSolver01_06::flaplace_composite(double z, const QMap<QString, double>& p) {
    double kf = p.value("kf");
    double km = p.value("km");

    // 强制计算无因次缝长 LfD = Lf / L
    // 即使传入了参数 map，也优先使用 Lf 和 L 计算 LfD，确保数据一致性
    double L = p.value("L");
    double Lf = p.value("Lf");
    double LfD = 0.0;
    if (L > 1e-9) {
        LfD = Lf / L;
    } else {
        LfD = p.value("LfD"); // 如果 L 无效，回退到参数值
    }

    double rmD = p.value("rmD");
    double reD = p.value("reD", 0.0);
    double omga1 = p.value("omega1");
    double omga2 = p.value("omega2");
    double remda1 = p.value("lambda1");
    int nf = (int)p.value("nf", 4);
    if(nf < 1) nf = 1;

    double M12 = kf / km;

    // 生成裂缝位置 xwD
    QVector<double> xwD;
    if (nf == 1) {
        xwD.append(0.0);
    } else {
        double start = -0.9;
        double end = 0.9;
        double step = (end - start) / (nf - 1);
        for(int i=0; i<nf; ++i) xwD.append(start + i * step);
    }

    double temp = omga2;
    double fs1 = omga1 + remda1 * temp / (remda1 + z * temp);
    double fs2 = M12 * temp;

    // 计算不含井储的拉普拉斯空间压力
    double pf = PWD_composite(z, fs1, fs2, M12, LfD, rmD, reD, nf, xwD, m_type);

    // 加入井储和表皮效应
    bool hasStorage = (m_type == Model_1 || m_type == Model_3 || m_type == Model_5);
    if (hasStorage) {
        double CD = p.value("cD", 0.0);
        double S = p.value("S", 0.0);
        if (CD > 1e-12 || std::abs(S) > 1e-12) {
            pf = (z * pf + S) / (z + CD * z * z * (z * pf + S));
        }
    }

    return pf;
}

// 核心点源解叠加计算
double ModelSolver01_06::PWD_composite(double z, double fs1, double fs2, double M12, double LfD, double rmD, double reD, int nf, const QVector<double>& xwD, ModelType type) {
    using namespace boost::math;
    QVector<double> ywD(nf, 0.0); // 假设裂缝在y方向无偏移
    double gama1 = sqrt(z * fs1);
    double gama2 = sqrt(z * fs2);
    double arg_g2_rm = gama2 * rmD;
    double arg_g1_rm = gama1 * rmD;

    double k0_g2 = cyl_bessel_k(0, arg_g2_rm);
    double k1_g2 = cyl_bessel_k(1, arg_g2_rm);
    double k0_g1 = cyl_bessel_k(0, arg_g1_rm);
    double k1_g1 = cyl_bessel_k(1, arg_g1_rm);

    double term_mAB_i0 = 0.0;
    double term_mAB_i1 = 0.0;

    bool isInfinite = (type == Model_1 || type == Model_2);
    bool isClosed = (type == Model_3 || type == Model_4);
    bool isConstP = (type == Model_5 || type == Model_6);

    // 边界条件处理
    if (!isInfinite) {
        // 防止 reD 无效导致 Bessel K0(0) = Inf
        if (reD <= 1e-5) return 0.0;

        double arg_re = gama2 * reD;
        double i1_re_s = scaled_besseli(1, arg_re);
        double i0_re_s = scaled_besseli(0, arg_re);
        double k1_re = cyl_bessel_k(1, arg_re);
        double k0_re = cyl_bessel_k(0, arg_re);
        double i0_g2_s = scaled_besseli(0, arg_g2_rm);
        double i1_g2_s = scaled_besseli(1, arg_g2_rm);

        if (isClosed) {
            if (i1_re_s > 1e-100) {
                term_mAB_i0 = (k1_re / i1_re_s) * i0_g2_s * std::exp(arg_g2_rm - arg_re);
                term_mAB_i1 = (k1_re / i1_re_s) * i1_g2_s * std::exp(arg_g2_rm - arg_re);
            }
        } else if (isConstP) {
            if (i0_re_s > 1e-100) {
                term_mAB_i0 = -(k0_re / i0_re_s) * i0_g2_s * std::exp(arg_g2_rm - arg_re);
                term_mAB_i1 = -(k0_re / i0_re_s) * i1_g2_s * std::exp(arg_g2_rm - arg_re);
            }
        }
    }

    double term1 = term_mAB_i0 + k0_g2;
    double term2 = term_mAB_i1 - k1_g2;

    double Acup = M12 * gama1 * k1_g1 * term1 + gama2 * k0_g1 * term2;

    double i1_g1_s = scaled_besseli(1, arg_g1_rm);
    double i0_g1_s = scaled_besseli(0, arg_g1_rm);

    double Acdown_scaled = M12 * gama1 * i1_g1_s * term1 - gama2 * i0_g1_s * term2;

    if (std::abs(Acdown_scaled) < 1e-100) Acdown_scaled = 1e-100;

    double Ac_prefactor = Acup / Acdown_scaled;

    // 建立线性方程组求解裂缝各段流量分布
    int size = nf + 1;
    Eigen::MatrixXd A_mat(size, size);
    Eigen::VectorXd b_vec(size);
    b_vec.setZero();
    b_vec(nf) = 1.0; // 定产条件

    for (int i = 0; i < nf; ++i) {
        for (int j = 0; j < nf; ++j) {
            auto integrand = [&](double a) -> double {
                double dist = std::sqrt(std::pow(xwD[i] - xwD[j] - a, 2) + std::pow(ywD[i] - ywD[j], 2));
                double arg_dist = gama1 * dist;
                if (arg_dist < 1e-10) arg_dist = 1e-10;

                double term2 = 0.0;
                double exponent = arg_dist - arg_g1_rm;
                if (exponent > -700.0) {
                    term2 = Ac_prefactor * scaled_besseli(0, arg_dist) * std::exp(exponent);
                }
                return cyl_bessel_k(0, arg_dist) + term2;
            };
            // 沿裂缝积分
            double val = adaptiveGauss(integrand, -LfD, LfD, 1e-5, 0, 10);
            A_mat(i, j) = z * val / (M12 * z * 2 * LfD);
        }
    }
    // 补充方程：各裂缝压力相等，流量和为1
    for (int i = 0; i < nf; ++i) {
        A_mat(i, nf) = -1.0;
        A_mat(nf, i) = z;
    }
    A_mat(nf, nf) = 0.0;

    return A_mat.fullPivLu().solve(b_vec)(nf);
}

double ModelSolver01_06::scaled_besseli(int v, double x) {
    if (x < 0) x = -x;
    if (x > 600.0) return 1.0 / std::sqrt(2.0 * M_PI * x);
    return boost::math::cyl_bessel_i(v, x) * std::exp(-x);
}

double ModelSolver01_06::gauss15(std::function<double(double)> f, double a, double b) {
    static const double X[] = { 0.0, 0.201194, 0.394151, 0.570972, 0.724418, 0.848207, 0.937299, 0.987993 };
    static const double W[] = { 0.202578, 0.198431, 0.186161, 0.166269, 0.139571, 0.107159, 0.070366, 0.030753 };
    double h = 0.5 * (b - a); double c = 0.5 * (a + b); double s = W[0] * f(c);
    for (int i = 1; i < 8; ++i) { double dx = h * X[i]; s += W[i] * (f(c - dx) + f(c + dx)); }
    return s * h;
}

double ModelSolver01_06::adaptiveGauss(std::function<double(double)> f, double a, double b, double eps, int depth, int maxDepth) {
    double c = (a + b) / 2.0; double v1 = gauss15(f, a, b); double v2 = gauss15(f, a, c) + gauss15(f, c, b);
    if (depth >= maxDepth || std::abs(v1 - v2) < 1e-10 * std::abs(v2) + eps) return v2;
    return adaptiveGauss(f, a, c, eps/2, depth+1, maxDepth) + adaptiveGauss(f, c, b, eps/2, depth+1, maxDepth);
}

double ModelSolver01_06::stefestCoefficient(int i, int N) {
    double s = 0.0; int k1 = (i + 1) / 2; int k2 = std::min(i, N / 2);
    for (int k = k1; k <= k2; ++k) {
        double num = pow(k, N / 2.0) * factorial(2 * k);
        double den = factorial(N / 2 - k) * factorial(k) * factorial(k - 1) * factorial(i - k) * factorial(2 * k - i);
        if(den!=0) s += num/den;
    }
    return ((i + N / 2) % 2 == 0 ? 1.0 : -1.0) * s;
}

double ModelSolver01_06::factorial(int n) {
    if(n<=1)return 1;
    double r=1;
    for(int i=2;i<=n;++i) r*=i;
    return r;
}

