/*
 * 文件名: fittingdatadialog.h
 * 文件作用: 拟合数据加载配置窗口头文件
 * 功能描述:
 * 1. 声明 FittingDataSettings 结构体，用于封装用户的选择（列索引、试井类型、初始压力、平滑参数等）。
 * 2. 声明 FittingDataDialog 类，提供从项目或文件加载数据、预览数据、配置列映射的界面。
 * 3. [修改] 支持多文件数据源选择，在“项目数据”模式下可切换不同文件。
 * 4. 包含了文件解析逻辑（CSV, TXT, Excel）。
 */

#ifndef FITTINGDATADIALOG_H
#define FITTINGDATADIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QMap>

namespace Ui {
class FittingDataDialog;
}

// 试井类型枚举
enum WellTestType {
    Test_Drawdown = 0, // 压力降落试井
    Test_Buildup = 1   // 压力恢复试井
};

// 拟合数据加载配置结构体
struct FittingDataSettings {
    bool isFromProject;         // true:从项目加载, false:从文件加载
    QString filePath;           // 外部文件路径 (仅当 isFromProject=false 时有效)
    QString projectFileName;    // [新增] 项目文件名 (仅当 isFromProject=true 时有效)

    int timeColIndex;           // 时间列索引
    int pressureColIndex;       // 压力列索引
    int derivColIndex;          // 导数列索引 (-1 表示自动计算)
    int skipRows;               // 跳过首行数

    WellTestType testType;      // 试井类型 (降落/恢复)
    double initialPressure;     // 地层初始压力 Pi (仅降落试井需要)

    // L-Spacing 参数，用于Bourdet导数计算
    double lSpacing;

    bool enableSmoothing;       // 是否启用平滑
    int smoothingSpan;          // 平滑窗口大小 (奇数)
};

class FittingDataDialog : public QDialog
{
    Q_OBJECT

public:
    // [修改] 构造函数：接收所有项目数据模型的映射表
    explicit FittingDataDialog(const QMap<QString, QStandardItemModel*>& projectModels, QWidget *parent = nullptr);
    ~FittingDataDialog();

    // 获取用户确认后的配置
    FittingDataSettings getSettings() const;

    // 获取当前显示在预览表格中的数据模型
    QStandardItemModel* getPreviewModel() const;

private slots:
    // 数据来源改变时触发 (项目数据 vs 外部文件)
    void onSourceChanged();

    // [新增] 项目文件下拉框选择改变时触发
    void onProjectFileSelectionChanged(int index);

    // 点击浏览按钮时触发
    void onBrowseFile();

    // 导数列选择改变时触发（用于控制平滑选项的启用状态）
    void onDerivColumnChanged(int index);

    // 试井类型改变时触发 (控制初始压力输入框的启用状态)
    void onTestTypeChanged();

    // 启用平滑复选框切换时触发
    void onSmoothingToggled(bool checked);

    // 点击确定按钮时的校验
    void onAccepted();

private:
    Ui::FittingDataDialog *ui;

    // [修改] 存储所有项目数据模型 (Key: 文件名/路径, Value: 模型指针)
    QMap<QString, QStandardItemModel*> m_projectDataMap;

    QStandardItemModel* m_fileModel;    // 外部文件数据临时模型

    // 辅助函数：更新列选择下拉框的内容
    void updateColumnComboBoxes(const QStringList& headers);

    // 辅助函数：解析文本文件 (CSV/TXT)
    bool parseTextFile(const QString& filePath);

    // 辅助函数：解析 Excel 文件
    bool parseExcelFile(const QString& filePath);

    // 辅助函数：获取当前选中的项目数据模型
    QStandardItemModel* getCurrentProjectModel() const;
};

#endif // FITTINGDATADIALOG_H
