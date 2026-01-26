/*
 * 文件名: paramselectdialog.h
 * 文件作用: 拟合参数选择对话框头文件
 * 功能描述:
 * 1. 声明 ParamSelectDialog 类，用于配置模型的拟合参数。
 * 2. 声明 eventFilter 用于拦截输入框的滚轮事件，防止误操作。
 */

#ifndef PARAMSELECTDIALOG_H
#define PARAMSELECTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QEvent>
#include "fittingparameterchart.h"

namespace Ui {
class ParamSelectDialog;
}

class ParamSelectDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造函数：传入当前参数列表
    explicit ParamSelectDialog(const QList<FitParameter>& params, QWidget *parent = nullptr);
    ~ParamSelectDialog();

    // 获取用户修改后的参数列表
    QList<FitParameter> getUpdatedParams() const;

protected:
    // 事件过滤器，用于屏蔽输入框的鼠标滚轮事件
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::ParamSelectDialog *ui;

    // 暂存的参数列表副本
    QList<FitParameter> m_params;

    // 初始化表格视图
    void initTable();
    // 收集数据
    void collectData();

private slots:
    void onConfirm();
    void onCancel();
};

#endif // PARAMSELECTDIALOG_H
