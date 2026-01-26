/*
 * main.cpp
 * 文件作用: 应用程序入口
 * 功能描述:
 * 1. 初始化 QApplication 应用程序对象
 * 2. 设置应用程序的全局窗口图标 (PWT.png)
 * 3. [新增] 内置自定义翻译器 (ChineseTranslator)，实现全局标准按钮("OK", "Cancel"等)的汉化。
 * 4. [核心修改] 定义极其详尽的全局样式表 (StyleSheet)：
 * - 统一所有控件为“白底黑字”风格，杜绝看不清的情况。
 * - [优化] 全新设计的 QSpinBox：圆形按钮、极简边框。
 * - [优化] 全新设计的 QComboBox：6px圆角、36px高度、现代化下拉效果。
 * 5. 设置全局调色板以适配不同系统主题。
 * 6. 启动主窗口。
 */

#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QMessageBox>
#include <QFileDialog>
#include <QIcon>
#include <QTranslator>

// ========================================================================
// 自定义翻译器类：用于全局汉化标准按钮
// ========================================================================
class ChineseTranslator : public QTranslator
{
public:
    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1) const override
    {
        Q_UNUSED(context)
        Q_UNUSED(disambiguation)
        Q_UNUSED(n)

        QString source = QString::fromLatin1(sourceText);

        // 常见标准按钮文本汉化
        if (source == "OK" || source == "&OK") {
            return QStringLiteral("确定");
        } else if (source == "Cancel" || source == "&Cancel") {
            return QStringLiteral("取消");
        } else if (source == "Yes" || source == "&Yes") {
            return QStringLiteral("是");
        } else if (source == "No" || source == "&No") {
            return QStringLiteral("否");
        } else if (source == "Save" || source == "&Save") {
            return QStringLiteral("保存");
        } else if (source == "Discard" || source == "&Discard" || source == "Don't Save") {
            return QStringLiteral("不保存");
        } else if (source == "Apply" || source == "&Apply") {
            return QStringLiteral("应用");
        } else if (source == "Reset" || source == "&Reset") {
            return QStringLiteral("重置");
        } else if (source == "Close" || source == "&Close") {
            return QStringLiteral("关闭");
        } else if (source == "Help" || source == "&Help") {
            return QStringLiteral("帮助");
        }

        // 其他未匹配文本返回空，交由 Qt 默认处理
        return QString();
    }
};

int main(int argc, char *argv[])
{
// 解决 HighDpiScaling 在 Qt6 中已废弃的警告
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);

    // [新增] 加载自定义翻译器，解决标准按钮(QDialogButtonBox)中文显示问题
    ChineseTranslator translator;
    app.installTranslator(&translator);

    // 设置软件全局图标
    app.setWindowIcon(QIcon(":/new/prefix1/Resource/PWT.png"));

    // ========================================================================
    // 全局样式表定义 (Global StyleSheet)
    // ========================================================================
    QString styleSheet = R"(
        /* -------------------------------------------------------
           1. 全局基础设置
        ------------------------------------------------------- */
        QWidget {
            color: #333333; /* 深灰色字体，比纯黑更柔和现代 */
            font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;
            font-size: 14px;
            selection-background-color: #e5f3ff; /* 选中项背景：极浅蓝 */
            selection-color: #333333;            /* 选中项文字：深灰 */
            outline: none; /* 去除焦点虚线框 */
        }

        /* -------------------------------------------------------
           2. 输入类控件 (LineEdit, TextEdit)
        ------------------------------------------------------- */
        QLineEdit, QTextEdit, QPlainTextEdit {
            border: 1px solid #dcdcdc; /* 极细边框 */
            border-radius: 4px;
            padding: 6px 8px;
            background-color: white;
            color: #333333;
            min-height: 20px;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #0078d7; /* 聚焦蓝框 */
            background-color: #ffffff;
        }
        QLineEdit:read-only {
            background-color: #f7f7f7; /* 浅灰底 */
            color: #888888;
            border-color: #e0e0e0;
        }

        /* -------------------------------------------------------
           3. 数值输入框 (SpinBox) - 极简现代风 + 圆形按钮
        ------------------------------------------------------- */
        QAbstractSpinBox {
            border: 1px solid #dcdcdc;
            border-radius: 4px;
            padding-top: 6px;
            padding-bottom: 6px;
            padding-left: 8px;
            padding-right: 30px; /* 右侧留出按钮空间 */
            background-color: white; /* 纯白背景 */
            color: #333333;
            min-height: 24px; /* 整体高度 */
        }
        QAbstractSpinBox:focus {
            border: 1px solid #0078d7;
        }
        QAbstractSpinBox:hover {
            border: 1px solid #b0b0b0;
        }

        /* 上下按钮区域 */
        QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
            subcontrol-origin: border;
            width: 20px;  /* 按钮宽度 */
            height: 14px; /* 按钮高度的一半，紧凑布局 */
            border: none;
            background: transparent; /* 透明背景，靠图标显示 */
            margin-right: 4px;
        }

        QAbstractSpinBox::up-button {
            subcontrol-position: top right;
            margin-top: 4px; /* 顶部间距 */
        }
        QAbstractSpinBox::down-button {
            subcontrol-position: bottom right;
            margin-bottom: 4px; /* 底部间距 */
        }

        /* 按钮悬停态：圆形微背景 */
        QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover {
            background-color: #f0f0f0;
            border-radius: 2px; /* 轻微圆角 */
        }

        /* 按钮按下态 */
        QAbstractSpinBox::up-button:pressed, QAbstractSpinBox::down-button:pressed {
            background-color: #e0e0e0;
        }

        /* 箭头图标 (利用Qt自带绘制，颜色设为深灰) */
        QAbstractSpinBox::up-arrow, QAbstractSpinBox::down-arrow {
            width: 8px;
            height: 8px;
            image: none; /* 清除可能存在的图片 */
        }
        /* 若系统未自动绘制箭头，可考虑用 border hack 绘制，但在 Qt Style Sheet 中
           通常 Qt 会自动绘制默认箭头。为了更美观，可以依赖系统默认的深色箭头。 */
        QAbstractSpinBox::up-arrow:disabled, QAbstractSpinBox::up-arrow:off {
           background: none;
        }


        /* -------------------------------------------------------
           4. 下拉选择框 (ComboBox) - 现代化圆角设计
        ------------------------------------------------------- */
        QComboBox {
            border: 1px solid #dcdcdc;
            border-radius: 6px; /* 6px 圆角 */
            padding-left: 10px;
            padding-right: 10px; /* 给文字留空 */
            background-color: white;
            color: #333333;
            min-height: 36px; /* 增加高度，约 36-40px */
            font-size: 14px;
        }

        /* 悬停状态 */
        QComboBox:hover {
            border: 1px solid #a0a0a0;
            background-color: #fcfcfc;
        }

        /* 聚焦状态 */
        QComboBox:on { /* 弹出时 */
            border: 1px solid #0078d7;
            border-bottom-left-radius: 0px;
            border-bottom-right-radius: 0px;
        }

        /* 下拉箭头区域 */
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 30px; /* 箭头区域宽度 */
            border-left: none; /* 去除分隔线，更现代 */
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
        }

        /* 下拉箭头图标 styling */
        QComboBox::down-arrow {
            width: 10px;
            height: 10px;
            /* 这里使用 Qt 默认绘制的箭头，通常够用。
               若需自定义 SVG，需 resource 文件支持。
               此处通过 border 颜色控制其可见性。 */
            border: none;
            background: none;
            color: #555555;
        }

        /* 弹出的下拉列表 */
        QComboBox QAbstractItemView {
            border: 1px solid #0078d7;
            border-top: none;
            background-color: white;
            color: #333333;
            selection-background-color: #f0f8ff; /* 悬停/选中背景：淡爱丽丝蓝 */
            selection-color: #0078d7;            /* 选中文字：品牌蓝 */
            outline: none;
            border-bottom-left-radius: 6px;
            border-bottom-right-radius: 6px;
            padding: 4px;
        }

        /* 下拉列表项 */
        QComboBox::item {
            height: 32px; /* 列表项高度 */
            padding-left: 8px;
        }

        /* -------------------------------------------------------
           5. 标签页 (TabWidget)
        ------------------------------------------------------- */
        QTabWidget::pane {
            border: 1px solid #dcdcdc;
            background-color: white;
            top: -1px;
        }
        QTabBar::tab {
            background: #f5f5f5;
            border: 1px solid #dcdcdc;
            padding: 8px 20px;
            margin-right: 2px;
            color: #555555;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            min-width: 80px;
        }
        QTabBar::tab:selected {
            background: white;
            border-bottom-color: white;
            color: #0078d7;
            font-weight: bold;
        }
        QTabBar::tab:hover:!selected {
            background: #eef6ff;
            color: #0078d7;
        }

        /* -------------------------------------------------------
           6. 分组框 (GroupBox)
        ------------------------------------------------------- */
        QGroupBox {
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            margin-top: 24px; /* 标题空间 */
            background-color: white;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 5px;
            left: 10px;
            color: #333333;
            font-weight: bold;
            background-color: transparent;
        }

        /* -------------------------------------------------------
           7. 列表、表格 (TableView, ListWidget)
        ------------------------------------------------------- */
        QTableView, QListWidget, QTreeWidget {
            background-color: white;
            alternate-background-color: #fafafa; /* 极淡灰 */
            gridline-color: #eeeeee;
            color: #333333;
            border: 1px solid #dcdcdc;
            selection-background-color: #e5f3ff;
            selection-color: black;
        }
        QHeaderView::section {
            background-color: #f9f9f9;
            color: #333333;
            padding: 6px;
            border: none;
            border-bottom: 1px solid #dcdcdc;
            border-right: 1px solid #eeeeee;
            font-weight: bold;
        }

        /* -------------------------------------------------------
           8. 滚动条 (ScrollBar) - 细长现代
        ------------------------------------------------------- */
        QScrollBar:vertical {
            border: none;
            background: #f0f0f0;
            width: 8px;
            margin: 0px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #cdcdcd;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #a0a0a0;
        }
        QScrollBar:horizontal {
            border: none;
            background: #f0f0f0;
            height: 8px;
            margin: 0px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background: #cdcdcd;
            min-width: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #a0a0a0;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            height: 0px; width: 0px;
        }

        /* -------------------------------------------------------
           9. 菜单与工具栏
        ------------------------------------------------------- */
        QMenuBar {
            background-color: #f9f9f9;
            color: #333333;
            border-bottom: 1px solid #e0e0e0;
        }
        QMenuBar::item:selected {
            background-color: #e5f3ff;
            color: #000000;
        }
        QMenu {
            background-color: white;
            border: 1px solid #dcdcdc;
            color: #333333;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #e5f3ff;
            color: black;
        }
        QToolBar {
            background: #ffffff;
            border-bottom: 1px solid #e0e0e0;
            spacing: 6px;
            padding: 4px;
        }

        /* -------------------------------------------------------
           10. 提示框 (ToolTip)
        ------------------------------------------------------- */
        QToolTip {
            border: 1px solid #dcdcdc;
            background-color: white;
            color: #333333;
            padding: 4px;
            opacity: 230;
        }

        /* -------------------------------------------------------
           11. 按钮与弹窗 (灰底黑字原则)
        ------------------------------------------------------- */
        QMessageBox, QDialog, QFileDialog {
            background-color: white;
        }
        QMessageBox QLabel {
            color: #333333;
        }

        /* [方案一] 默认弹窗按钮：灰底黑字，圆角 */
        QMessageBox QPushButton, QDialog QPushButton, QFileDialog QPushButton {
            background-color: #f5f5f5;
            color: #333333;
            border: 1px solid #dcdcdc;
            border-radius: 4px;
            padding: 6px 18px;
            min-width: 80px;
            min-height: 24px;
        }
        QMessageBox QPushButton:hover, QDialog QPushButton:hover, QFileDialog QPushButton:hover {
            background-color: #e8e8e8;
            border: 1px solid #c0c0c0;
        }
        QMessageBox QPushButton:pressed, QDialog QPushButton:pressed, QFileDialog QPushButton:pressed {
            background-color: #dcdcdc;
        }

        /* [方案二] 功能按钮自定义样式 (btnType) */

        /* Primary: 主要操作 - 蓝色实心 */
        QPushButton[btnType="primary"] {
            background-color: #0078d7;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 18px;
            font-weight: bold;
        }
        QPushButton[btnType="primary"]:hover {
            background-color: #1084e0;
        }
        QPushButton[btnType="primary"]:pressed {
            background-color: #006cc1;
        }

        /* Danger: 危险操作 - 红色实心 */
        QPushButton[btnType="danger"] {
            background-color: #d9534f;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 18px;
        }
        QPushButton[btnType="danger"]:hover {
            background-color: #e0605c;
        }
        QPushButton[btnType="danger"]:pressed {
            background-color: #ac2925;
        }

        /* Secondary: 次要操作 - 灰底黑字 */
        QPushButton[btnType="secondary"] {
            background-color: #f5f5f5;
            color: #333333;
            border: 1px solid #dcdcdc;
            border-radius: 4px;
            padding: 6px 18px;
        }
        QPushButton[btnType="secondary"]:hover {
            background-color: #e8e8e8;
        }
    )";

    app.setStyleSheet(styleSheet);

    // 设置全局调色板 (双重保险，防止原生控件未被 CSS 覆盖时的颜色异常)
    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, Qt::white);
    palette.setColor(QPalette::WindowText, QColor(51, 51, 51));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor(250, 250, 250));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, QColor(51, 51, 51));
    palette.setColor(QPalette::Text, QColor(51, 51, 51));
    palette.setColor(QPalette::Button, QColor(245, 245, 245));
    palette.setColor(QPalette::ButtonText, QColor(51, 51, 51));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(0, 120, 215));
    palette.setColor(QPalette::Highlight, QColor(229, 243, 255)); // 浅蓝高亮
    palette.setColor(QPalette::HighlightedText, Qt::black);

    QApplication::setPalette(palette);

    MainWindow w;
    w.show();

    return app.exec();
}
