/*
 * 文件名: wt_datawidget.h
 * 文件作用: 数据编辑器主窗口头文件
 * 功能描述:
 * 1. 定义数据编辑器的主界面类 WT_DataWidget。
 * 2. 负责管理多个 DataSingleSheet 实例，支持多文件同时打开。
 * 3. 协调顶部工具栏与当前活动页签的交互。
 * 4. 负责将所有页签数据同步保存到项目文件中。
 * 5. [保留优化] 提供了 getAllDataModels 接口，支持多文件数据传递。
 */

#ifndef WT_DATAWIDGET_H
#define WT_DATAWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include <QJsonArray>
#include <QMap>
#include "datasinglesheet.h" // 包含单页类

namespace Ui {
class WT_DataWidget;
}

class WT_DataWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WT_DataWidget(QWidget *parent = nullptr);
    ~WT_DataWidget();

    // 清空所有数据
    void clearAllData();

    // 从项目参数恢复数据
    void loadFromProjectData();

    // 获取当前活动页的模型（兼容旧接口）
    QStandardItemModel* getDataModel() const;

    // [保留功能] 获取所有已打开文件的数据模型 (用于多文件绘图/拟合选择)
    QMap<QString, QStandardItemModel*> getAllDataModels() const;

    // 加载指定文件数据
    void loadData(const QString& filePath, const QString& fileType = "auto");

    // 获取当前文件名
    QString getCurrentFileName() const;

    // 是否有数据
    bool hasData() const;

signals:
    void dataChanged();
    void fileChanged(const QString& filePath, const QString& fileType);

private slots:
    // 文件操作
    void onOpenFile();
    void onSave();
    void onExportExcel();

    // 工具栏操作（分发给当前页签）
    void onDefineColumns();
    void onTimeConvert();
    void onPressureDropCalc();
    void onCalcPwf();
    void onHighlightErrors();

    // 状态
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void onSheetDataChanged();

private:
    Ui::WT_DataWidget *ui;

    void initUI();
    void setupConnections();
    void updateButtonsState();

    // 辅助函数：创建新页签
    void createNewTab(const QString& filePath, const DataImportSettings& settings);
    // 辅助函数：获取当前活动页签
    DataSingleSheet* currentSheet() const;
};

#endif // WT_DATAWIDGET_H
