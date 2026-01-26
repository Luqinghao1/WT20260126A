/*
 * 文件名: modelparameter.h
 * 文件作用: 项目参数单例类头文件
 * 功能描述:
 * 1. 管理项目核心数据（孔隙度、粘度等）和文件路径。
 * 2. 负责 _chart.json (图表) 和 _date.json (表格) 的路径生成和存取。
 * 3. 确保项目保存和加载时，数据表格的内容能被正确持久化。
 */

#ifndef MODELPARAMETER_H
#define MODELPARAMETER_H

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>

class ModelParameter : public QObject
{
    Q_OBJECT

public:
    static ModelParameter* instance();

    // ========================================================================
    // 项目文件管理
    // ========================================================================

    // 加载项目文件 (.pwt)
    // 作用：读取主文件配置，并自动寻找同目录下的 _date.json 加载表格数据
    bool loadProject(const QString& filePath);

    // 保存基础参数到 .pwt 文件
    bool saveProject();

    // 关闭项目，清空内存数据
    void closeProject();

    QString getProjectFilePath() const { return m_projectFilePath; }
    QString getProjectPath() const { return m_projectPath; }
    bool hasLoadedProject() const { return m_hasLoaded; }

    // ========================================================================
    // 基础参数存取
    // ========================================================================

    void setParameters(double phi, double h, double mu, double B, double Ct, double q, double rw, const QString& path);

    double getPhi() const { return m_phi; }
    double getH() const { return m_h; }
    double getMu() const { return m_mu; }
    double getB() const { return m_B; }
    double getCt() const { return m_Ct; }
    double getQ() const { return m_q; }
    double getRw() const { return m_rw; }

    // 保存拟合结果
    void saveFittingResult(const QJsonObject& fittingData);
    QJsonObject getFittingResult() const;

    // ========================================================================
    // 独立数据文件存取 (关键修复部分)
    // ========================================================================

    // 保存绘图数据到 "_chart.json"
    void savePlottingData(const QJsonArray& plots);
    QJsonArray getPlottingData() const;

    // 保存表格数据到 "_date.json"
    // DataEditorWidget 调用此函数将表格内容写入磁盘
    void saveTableData(const QJsonArray& tableData);


    // 重置所有项目数据（清空缓存）
    void resetAllData();


    // 获取表格数据
    // DataEditorWidget 加载项目时调用此函数恢复界面
    QJsonArray getTableData() const;

private:
    explicit ModelParameter(QObject* parent = nullptr);
    static ModelParameter* m_instance;

    bool m_hasLoaded;
    QString m_projectPath;
    QString m_projectFilePath;

    // 缓存完整的JSON对象，包含从各个子文件读取的内容
    QJsonObject m_fullProjectData;

    // 基础参数变量
    double m_phi;
    double m_h;
    double m_mu;
    double m_B;
    double m_Ct;
    double m_q;
    double m_rw;

    // 辅助：获取附属文件的绝对路径
    QString getPlottingDataFilePath() const;
    QString getTableDataFilePath() const;
};

#endif // MODELPARAMETER_H
