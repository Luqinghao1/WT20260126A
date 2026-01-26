/*
 * 文件名: modelparameter.cpp
 * 文件作用: 项目参数单例类实现文件
 * 功能描述:
 * 1. 实现项目数据的加载与保存。
 * 2. [关键] loadProject 时强制读取 _date.json 到 m_fullProjectData["table_data"]，解决数据丢失问题。
 */

#include "modelparameter.h"
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDebug>

ModelParameter* ModelParameter::m_instance = nullptr;

ModelParameter::ModelParameter(QObject* parent) : QObject(parent), m_hasLoaded(false)
{
    m_phi = 0.05; m_h = 20.0; m_mu = 0.5; m_B = 1.05; m_Ct = 5e-4; m_q = 50.0; m_rw = 0.1;
}

ModelParameter* ModelParameter::instance()
{
    if (!m_instance) m_instance = new ModelParameter();
    return m_instance;
}

void ModelParameter::setParameters(double phi, double h, double mu, double B, double Ct, double q, double rw, const QString& path)
{
    m_phi = phi; m_h = h; m_mu = mu; m_B = B; m_Ct = Ct; m_q = q; m_rw = rw;
    m_projectFilePath = path;

    QFileInfo fi(path);
    m_projectPath = fi.isFile() ? fi.absolutePath() : path;
    m_hasLoaded = true;

    if (m_fullProjectData.isEmpty()) {
        QJsonObject reservoir;
        reservoir["porosity"] = m_phi;
        reservoir["thickness"] = m_h;
        reservoir["wellRadius"] = m_rw;
        reservoir["productionRate"] = m_q;
        QJsonObject pvt;
        pvt["viscosity"] = m_mu;
        pvt["volumeFactor"] = m_B;
        pvt["compressibility"] = m_Ct;
        m_fullProjectData["reservoir"] = reservoir;
        m_fullProjectData["pvt"] = pvt;
    }
}

// 构造图表数据路径: 原文件名 + "_chart.json"
QString ModelParameter::getPlottingDataFilePath() const
{
    if (m_projectFilePath.isEmpty()) return QString();
    QFileInfo fi(m_projectFilePath);
    QString baseName = fi.completeBaseName();
    return fi.absolutePath() + "/" + baseName + "_chart.json";
}

// 构造表格数据路径: 原文件名 + "_date.json"
QString ModelParameter::getTableDataFilePath() const
{
    if (m_projectFilePath.isEmpty()) return QString();
    QFileInfo fi(m_projectFilePath);
    QString baseName = fi.completeBaseName();
    return fi.absolutePath() + "/" + baseName + "_date.json";
}

bool ModelParameter::loadProject(const QString& filePath)
{
    // 1. 加载主项目文件 (.pwt)
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开项目文件:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;

    m_fullProjectData = doc.object();

    // 解析基础物理参数
    if (m_fullProjectData.contains("reservoir")) {
        QJsonObject res = m_fullProjectData["reservoir"].toObject();
        m_q = res["productionRate"].toDouble(50.0);
        m_phi = res["porosity"].toDouble(0.05);
        m_h = res["thickness"].toDouble(20.0);
        m_rw = res["wellRadius"].toDouble(0.1);
    }
    if (m_fullProjectData.contains("pvt")) {
        QJsonObject pvt = m_fullProjectData["pvt"].toObject();
        m_Ct = pvt["compressibility"].toDouble(5e-4);
        m_mu = pvt["viscosity"].toDouble(0.5);
        m_B = pvt["volumeFactor"].toDouble(1.05);
    }

    m_projectFilePath = filePath;
    m_projectPath = QFileInfo(filePath).absolutePath();
    m_hasLoaded = true;

    // 2. 加载图表数据 (_chart.json)
    QString chartPath = getPlottingDataFilePath();
    QFile chartFile(chartPath);
    if (chartFile.exists() && chartFile.open(QIODevice::ReadOnly)) {
        QJsonDocument d = QJsonDocument::fromJson(chartFile.readAll());
        if (!d.isNull() && d.isObject()) {
            QJsonObject obj = d.object();
            if (obj.contains("plotting_data")) {
                m_fullProjectData["plotting_data"] = obj["plotting_data"];
            }
        }
        chartFile.close();
    }

    // 3. [关键修复] 加载表格数据 (_date.json)
    // 必须确保这里的逻辑与 DataEditorWidget::onSave 对应
    QString datePath = getTableDataFilePath();
    QFile dateFile(datePath);
    if (dateFile.exists() && dateFile.open(QIODevice::ReadOnly)) {
        QJsonDocument d = QJsonDocument::fromJson(dateFile.readAll());
        if (!d.isNull() && d.isObject()) {
            QJsonObject obj = d.object();
            if (obj.contains("table_data")) {
                // 将读取到的数组存入内存，供 DataEditorWidget::loadFromProjectData 获取
                m_fullProjectData["table_data"] = obj["table_data"];
                qDebug() << "成功加载表格数据文件:" << datePath << "数据量:" << obj["table_data"].toArray().size();
            }
        } else {
            qDebug() << "表格数据文件解析失败:" << datePath;
        }
        dateFile.close();
    } else {
        qDebug() << "未找到表格数据文件:" << datePath;
        // 如果文件不存在，务必清除内存中的旧数据，防止显示错误
        m_fullProjectData.remove("table_data");
    }

    return true;
}

bool ModelParameter::saveProject()
{
    if (!m_hasLoaded || m_projectFilePath.isEmpty()) return false;

    // 更新参数到内存对象
    QJsonObject reservoir;
    if(m_fullProjectData.contains("reservoir")) reservoir = m_fullProjectData["reservoir"].toObject();
    reservoir["porosity"] = m_phi;
    reservoir["thickness"] = m_h;
    reservoir["wellRadius"] = m_rw;
    reservoir["productionRate"] = m_q;
    m_fullProjectData["reservoir"] = reservoir;

    QJsonObject pvt;
    if(m_fullProjectData.contains("pvt")) pvt = m_fullProjectData["pvt"].toObject();
    pvt["viscosity"] = m_mu;
    pvt["volumeFactor"] = m_B;
    pvt["compressibility"] = m_Ct;
    m_fullProjectData["pvt"] = pvt;

    // 保存 .pwt 主文件时，剔除大数据块，只保留配置
    QJsonObject dataToWrite = m_fullProjectData;
    dataToWrite.remove("plotting_data");
    dataToWrite.remove("table_data");

    QFile file(m_projectFilePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(dataToWrite).toJson());
    file.close();

    return true;
}

void ModelParameter::closeProject()
{
    m_hasLoaded = false;
    m_projectPath.clear();
    m_projectFilePath.clear();
    m_fullProjectData = QJsonObject();
    m_phi=0.05; m_h=20.0; m_mu=0.5; m_B=1.05; m_Ct=5e-4; m_q=50.0; m_rw=0.1;
}

void ModelParameter::saveFittingResult(const QJsonObject& fittingData)
{
    if (m_projectFilePath.isEmpty()) return;
    m_fullProjectData["fitting"] = fittingData;

    QFile file(m_projectFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject dataToWrite = m_fullProjectData;
        dataToWrite.remove("plotting_data");
        dataToWrite.remove("table_data");
        file.write(QJsonDocument(dataToWrite).toJson());
        file.close();
    }
}

QJsonObject ModelParameter::getFittingResult() const
{
    return m_fullProjectData.value("fitting").toObject();
}

void ModelParameter::savePlottingData(const QJsonArray& plots)
{
    if (m_projectFilePath.isEmpty()) return;

    m_fullProjectData["plotting_data"] = plots;

    QString dataFilePath = getPlottingDataFilePath();
    QJsonObject dataObj;
    dataObj["plotting_data"] = plots;

    QFile file(dataFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(dataObj).toJson());
        file.close();
    }
}

QJsonArray ModelParameter::getPlottingData() const
{
    return m_fullProjectData.value("plotting_data").toArray();
}

// 保存表格数据
void ModelParameter::saveTableData(const QJsonArray& tableData)
{
    if (m_projectFilePath.isEmpty()) return;

    // 1. 更新内存缓存
    m_fullProjectData["table_data"] = tableData;

    // 2. 写入独立文件 _date.json
    QString dataFilePath = getTableDataFilePath();
    QJsonObject dataObj;
    dataObj["table_data"] = tableData;

    QFile file(dataFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(dataObj).toJson());
        file.close();
        qDebug() << "表格数据已保存至:" << dataFilePath << "条目数:" << tableData.size();
    } else {
        qDebug() << "表格数据保存失败:" << dataFilePath;
    }
}


// [新增] 实现重置逻辑
void ModelParameter::resetAllData()
{
    // 1. 重置基础物理参数为默认值
    m_phi = 0.05;
    m_h = 20.0;
    m_mu = 0.5;
    m_B = 1.05;
    m_Ct = 5e-4;
    m_q = 50.0;
    m_rw = 0.1;

    // 2. 清空项目路径信息
    m_hasLoaded = false;
    m_projectPath.clear();
    m_projectFilePath.clear();

    // 3. [关键] 清空核心数据存储对象
    // 你的代码中，表格数据、绘图数据、拟合数据全都在这个对象里
    m_fullProjectData = QJsonObject();

    qDebug() << "ModelParameter: 所有全局数据缓存已清空 (m_fullProjectData 已重置)。";
}

// 获取表格数据
QJsonArray ModelParameter::getTableData() const
{
    // 直接从内存缓存中读取（loadProject 时已填充）
    return m_fullProjectData.value("table_data").toArray();
}
