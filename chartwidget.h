/*
 * 文件名: chartwidget.h
 * 文件作用: 通用图表组件头文件
 * 功能描述:
 * 1. 封装 MouseZoom (继承自 QCustomPlot)，提供统一的图表展示界面。
 * 2. 管理图表标题 (QCPTextElement)。
 * 3. [新增] 支持开/关井事件线的绘制，且在双坐标系下同时显示。
 * 4. [新增] 支持事件线跟随产量数据横向移动。
 */

#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include <QMenu>
#include <QMap>
#include <QMouseEvent>
#include <QKeyEvent>
#include "mousezoom.h"

namespace Ui {
class ChartWidget;
}

// 标注结构体
struct ChartAnnotation {
    QCPItemText* textItem = nullptr;
    QCPItemLine* arrowItem = nullptr;
};

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    enum ChartMode {
        Mode_Single = 0, // 单一绘图区
        Mode_Stacked     // 上下堆叠绘图区
    };

    explicit ChartWidget(QWidget *parent = nullptr);
    ~ChartWidget();

    // 设置图表标题
    void setTitle(const QString& title);
    // 获取当前标题
    QString title() const;

    MouseZoom* getPlot();
    void setDataModel(QStandardItemModel* model);

    void setChartMode(ChartMode mode);
    ChartMode getChartMode() const;

    QCPAxisRect* getTopRect();
    QCPAxisRect* getBottomRect();

    void clearGraphs();

    // [新增] 清除所有开/关井线
    void clearEventLines();
    // [新增] 添加开/关井线 (type: 0=关井/红, 1=开井/绿)
    void addEventLine(double x, int type);

protected:
    void keyPressEvent(QKeyEvent *event) override;

signals:
    void exportDataTriggered();
    void graphDataModified(QCPGraph* graph);
    void titleChanged(const QString& newTitle);
    void graphsChanged();

private slots:
    void on_btnSavePic_clicked();
    void on_btnExportData_clicked();
    void on_btnSetting_clicked();
    void on_btnReset_clicked();
    void on_btnDrawLine_clicked();

    void onAddAnnotationRequested(QCPItemLine* line);
    void onLineStyleRequested(QCPItemLine* line);
    void onDeleteSelectedRequested();
    void onEditItemRequested(QCPAbstractItem* item);

    // 开/关井线设置槽函数
    void onEventLineSettingsTriggered();

    void onPlotMousePress(QMouseEvent* event);
    void onPlotMouseMove(QMouseEvent* event);
    void onPlotMouseRelease(QMouseEvent* event);
    void onPlotMouseDoubleClick(QMouseEvent* event);

    void onMoveDataXTriggered();
    void onMoveDataYTriggered();

    void onZoomHorizontalTriggered();
    void onZoomVerticalTriggered();
    void onZoomDefaultTriggered();

    void addCharacteristicLine(double slope);
    void addAnnotationToLine(QCPItemLine* line);
    void deleteSelectedItems();

private:
    void initUi();
    void initConnections();
    void setupAxisRect(QCPAxisRect *rect);
    void calculateLinePoints(double slope, double centerX, double centerY,
                             double& x1, double& y1, double& x2, double& y2,
                             bool isLogX, bool isLogY);
    double distToSegment(const QPointF& p, const QPointF& s, const QPointF& e);
    void constrainLinePoint(QCPItemLine* line, bool isMovingStart, double mouseX, double mouseY);
    void updateAnnotationArrow(QCPItemLine* line);
    void refreshTitleElement();
    void exitMoveDataMode();
    void setZoomDragMode(Qt::Orientations orientations);

private:
    Ui::ChartWidget *ui;
    MouseZoom* m_plot;
    QStandardItemModel* m_dataModel;
    QMenu* m_lineMenu;
    QCPTextElement* m_titleElement;

    ChartMode m_chartMode;
    QCPAxisRect* m_topRect;
    QCPAxisRect* m_bottomRect;

    QMap<QCPItemLine*, ChartAnnotation> m_annotations;

    // [新增] 存储事件线列表
    QList<QCPItemLine*> m_eventLines;

    enum InteractionMode {
        Mode_None,
        Mode_Dragging_Line,
        Mode_Dragging_Start,
        Mode_Dragging_End,
        Mode_Dragging_Text,
        Mode_Dragging_ArrowStart,
        Mode_Dragging_ArrowEnd,
        Mode_Moving_Data_X,
        Mode_Moving_Data_Y
    };

    InteractionMode m_interMode;
    QCPItemLine* m_activeLine;
    QCPItemText* m_activeText;
    QCPItemLine* m_activeArrow;
    QPointF m_lastMousePos;

    QCPGraph* m_movingGraph;
    QPoint m_lastMoveDataPos;
};

#endif // CHARTWIDGET_H
