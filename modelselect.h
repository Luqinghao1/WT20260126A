#ifndef MODELSELECT_H
#define MODELSELECT_H

#include <QDialog>

namespace Ui {
class ModelSelect;
}

/**
 * @brief 模型选择对话框
 * 允许用户通过组合井筒、井、储层和边界条件来选择试井模型。
 */
class ModelSelect : public QDialog
{
    Q_OBJECT

public:
    explicit ModelSelect(QWidget *parent = nullptr);
    ~ModelSelect();

    // 获取选中的模型代码 ID (例如 "modelwidget1")
    QString getSelectedModelCode() const;

    // 获取选中的模型显示名称
    QString getSelectedModelName() const;

private slots:
    // 监听下拉框变化，实时更新模型信息显示
    void onSelectionChanged();

    // 确认按钮点击槽
    void onAccepted();

private:
    Ui::ModelSelect *ui;

    // 当前计算出的模型ID
    QString m_selectedModelCode;
    // 当前模型名称
    QString m_selectedModelName;

    // 初始化下拉框选项内容
    void initOptions();
};

#endif // MODELSELECT_H
