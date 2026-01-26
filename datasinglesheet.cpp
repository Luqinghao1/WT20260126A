/*
 * 文件名: datasinglesheet.cpp
 * 文件作用: 单个数据表页签类实现文件
 * 功能描述:
 * 1. 管理数据表格的核心逻辑，包括界面初始化、模型(Model)设置。
 * 2. 实现多种格式数据的加载功能：
 * - loadExcelFile: 支持 .xlsx (基于 QXlsx) 和 .xls (基于 QAxObject) 格式。
 * - loadTextFile: 支持 .csv、.txt 等文本格式，支持自定义编码、分隔符、起始行和表头行。
 * 3. 实现表格的交互功能：
 * - 右键菜单 (插入/删除/隐藏行列、排序、分列、合并单元格)。
 * - Ctrl + 滚轮缩放表格字体。
 * 4. 集成数据处理与计算接口 (通过调用外部计算类)：
 * - 定义列属性 (onDefineColumns)。
 * - 时间转换 (onTimeConvert)。
 * - 压降计算 (onPressureDropCalc)。
 * - 井底流压计算 (onCalcPwf)。
 * - 错误高亮检查 (onHighlightErrors)。
 * 5. 实现数据的导出 (Excel) 和 序列化保存 (JSON)。
 * 6. 强制应用统一的 UI 样式，确保弹窗按钮清晰可见。
 */

#include "datasinglesheet.h"
#include "ui_datasinglesheet.h"
#include "datacolumndialog.h"
#include "datacalculate.h"
#include "dataimportdialog.h"

// 引入 QXlsx 头文件
#include "xlsxdocument.h"
#include "xlsxchartsheet.h"
#include "xlsxcellrange.h"
#include "xlsxformat.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QTextCodec>
#include <QLineEdit>
#include <QEvent>
#include <QAxObject>
#include <QDir>
#include <QDateTime>
#include <QRadioButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QWheelEvent>

// ============================================================================
// [辅助函数] 强制应用“灰底黑字”的按钮样式
// 作用：解决部分系统下 Qt 默认弹窗按钮背景偏白导致文字看不清的问题
// ============================================================================
static void applySheetDialogStyle(QWidget* dialog) {
    if (!dialog) return;
    // 强制设置背景色为白色，文字为黑色，按钮为浅灰色带边框
    QString qss = "QWidget { color: black; background-color: white; font-family: 'Microsoft YaHei'; }"
                  "QPushButton { "
                  "   background-color: #f0f0f0; "  // 浅灰背景
                  "   color: black; "               // 黑色文字
                  "   border: 1px solid #bfbfbf; "  // 灰色边框
                  "   border-radius: 3px; "
                  "   padding: 5px 15px; "
                  "   min-width: 70px; "
                  "}"
                  "QPushButton:hover { background-color: #e0e0e0; }"
                  "QPushButton:pressed { background-color: #d0d0d0; }"
                  "QLabel { color: black; }"
                  "QLineEdit { color: black; background-color: white; border: 1px solid #ccc; }"
                  "QGroupBox { color: black; border: 1px solid #ccc; margin-top: 20px; }"
                  "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 3px; }";
    dialog->setStyleSheet(qss);
}

// [辅助函数] 显示带统一样式的消息提示框
static void showStyledMessage(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text) {
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);
    msgBox.setIcon(icon);
    msgBox.addButton(QMessageBox::Ok);
    applySheetDialogStyle(&msgBox);
    msgBox.exec();
}

// ============================================================================
// [内部类] InternalSplitDialog
// 作用：提供数据分列功能的配置对话框（选择分隔符）
// ============================================================================
class InternalSplitDialog : public QDialog
{
public:
    explicit InternalSplitDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("数据分列");
        resize(300, 200);
        // 应用统一样式
        applySheetDialogStyle(this);

        QVBoxLayout* layout = new QVBoxLayout(this);
        QGroupBox* group = new QGroupBox("选择分隔符");
        QVBoxLayout* gLayout = new QVBoxLayout(group);

        btnGroup = new QButtonGroup(this);

        radioSpace = new QRadioButton("空格 (Space)"); radioSpace->setChecked(true);
        radioTab = new QRadioButton("制表符 (Tab)");
        radioT = new QRadioButton("字母 'T' (日期时间)");
        radioCustom = new QRadioButton("自定义:");
        editCustom = new QLineEdit(); editCustom->setEnabled(false);

        btnGroup->addButton(radioSpace);
        btnGroup->addButton(radioTab);
        btnGroup->addButton(radioT);
        btnGroup->addButton(radioCustom);

        gLayout->addWidget(radioSpace);
        gLayout->addWidget(radioTab);
        gLayout->addWidget(radioT);

        QHBoxLayout* hLayout = new QHBoxLayout;
        hLayout->addWidget(radioCustom);
        hLayout->addWidget(editCustom);
        gLayout->addLayout(hLayout);

        layout->addWidget(group);

        QHBoxLayout* btnLayout = new QHBoxLayout;
        QPushButton* btnOk = new QPushButton("确定");
        QPushButton* btnCancel = new QPushButton("取消");
        btnLayout->addStretch();
        btnLayout->addWidget(btnOk);
        btnLayout->addWidget(btnCancel);
        layout->addLayout(btnLayout);

        connect(radioCustom, &QRadioButton::toggled, editCustom, &QLineEdit::setEnabled);
        connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    // 获取用户选择的分隔符字符串
    QString getSeparator() const {
        if (radioSpace->isChecked()) return " ";
        if (radioTab->isChecked()) return "\t";
        if (radioT->isChecked()) return "T";
        if (radioCustom->isChecked()) return editCustom->text();
        return " ";
    }

private:
    QButtonGroup* btnGroup;
    QRadioButton *radioSpace, *radioTab, *radioT, *radioCustom;
    QLineEdit *editCustom;
};

// ============================================================================
// [内部类] NoContextMenuDelegate & EditorEventFilter
// 作用：拦截 QTableView 编辑器内的默认右键菜单，防止与自定义右键菜单冲突
// ============================================================================
class EditorEventFilter : public QObject {
public:
    EditorEventFilter(QObject *parent) : QObject(parent) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::ContextMenu) return true; // 屏蔽默认右键菜单
        return QObject::eventFilter(obj, event);
    }
};

QWidget *NoContextMenuDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const
{
    QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
    if (editor) editor->installEventFilter(new EditorEventFilter(editor));
    return editor;
}

// ============================================================================
// [类实现] DataSingleSheet
// ============================================================================

DataSingleSheet::DataSingleSheet(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataSingleSheet),
    m_dataModel(new QStandardItemModel(this)),
    m_proxyModel(new QSortFilterProxyModel(this)),
    m_undoStack(new QUndoStack(this))
{
    ui->setupUi(this);
    initUI();
    setupModel();

    // 连接右键菜单信号
    connect(ui->dataTableView, &QTableView::customContextMenuRequested, this, &DataSingleSheet::onCustomContextMenu);
    // 连接模型数据变更信号
    connect(m_dataModel, &QStandardItemModel::itemChanged, this, &DataSingleSheet::onModelDataChanged);

    // 安装事件过滤器以捕获表格视图的滚轮事件（用于缩放）
    ui->dataTableView->viewport()->installEventFilter(this);
}

DataSingleSheet::~DataSingleSheet()
{
    delete ui;
}

// 初始化界面控件属性
void DataSingleSheet::initUI()
{
    ui->dataTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    // 设置自定义代理以处理编辑器事件
    ui->dataTableView->setItemDelegate(new NoContextMenuDelegate(this));
}

// 初始化数据模型与代理模型
void DataSingleSheet::setupModel()
{
    m_proxyModel->setSourceModel(m_dataModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive); // 过滤不区分大小写
    ui->dataTableView->setModel(m_proxyModel);
    ui->dataTableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->dataTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
}

// 事件过滤器实现：Ctrl + 滚轮 缩放表格字体
bool DataSingleSheet::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->dataTableView->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            int delta = wheelEvent->angleDelta().y();
            if (delta == 0) return false;

            QFont font = ui->dataTableView->font();
            int fontSize = font.pointSize();

            // 根据滚轮方向调整字号
            if (delta > 0) {
                fontSize += 1;
            } else {
                fontSize -= 1;
            }

            // 限制字号范围
            if (fontSize < 5) fontSize = 5;
            if (fontSize > 30) fontSize = 30;

            font.setPointSize(fontSize);
            ui->dataTableView->setFont(font);
            // 调整行高以适应新字体
            ui->dataTableView->resizeRowsToContents();

            return true; // 事件已处理
        }
    }
    return QWidget::eventFilter(obj, event);
}

// 设置表格过滤文本
void DataSingleSheet::setFilterText(const QString& text)
{
    m_proxyModel->setFilterWildcard(text);
}

// 加载数据总入口
bool DataSingleSheet::loadData(const QString& filePath, const DataImportSettings& settings)
{
    m_filePath = filePath;
    m_dataModel->clear();
    m_columnDefinitions.clear();

    if (settings.isExcel) {
        return loadExcelFile(filePath, settings);
    } else {
        return loadTextFile(filePath, settings);
    }
}

// 加载 Excel 文件 (.xlsx 或 .xls)
bool DataSingleSheet::loadExcelFile(const QString& path, const DataImportSettings& settings)
{
    // 分支1：处理 .xlsx 文件 (使用 QXlsx 库)
    if(path.endsWith(".xlsx", Qt::CaseInsensitive)) {
        QXlsx::Document xlsx(path);
        if(!xlsx.load()) {
            showStyledMessage(this, QMessageBox::Critical, "错误", "无法加载 .xlsx 文件");
            return false;
        }

        // 确保选中第一个工作表
        if(xlsx.currentWorksheet()==nullptr && !xlsx.sheetNames().isEmpty())
            xlsx.selectSheet(xlsx.sheetNames().first());

        int maxRow = xlsx.dimension().lastRow();
        int maxCol = xlsx.dimension().lastColumn();
        if(maxRow < 1 || maxCol < 1) return true; // 空表

        for(int r = 1; r <= maxRow; ++r) {
            // 跳过不需要的行：既不是表头行，也不在数据起始行之后
            if(r < settings.startRow && !(settings.useHeader && r == settings.headerRow)) continue;

            QStringList fields;
            for(int c = 1; c <= maxCol; ++c) {
                auto cell = xlsx.cellAt(r, c);
                if(cell) {
                    if(cell->isDateTime())
                        fields.append(cell->readValue().toDateTime().toString("yyyy-MM-dd hh:mm:ss"));
                    else
                        fields.append(cell->value().toString());
                } else {
                    fields.append("");
                }
            }

            // 设置表头
            if(settings.useHeader && r == settings.headerRow) {
                m_dataModel->setHorizontalHeaderLabels(fields);
                for(auto h : fields) {
                    ColumnDefinition d;
                    d.name = h;
                    m_columnDefinitions.append(d);
                }
            }
            // 添加数据行
            else if(r >= settings.startRow) {
                QList<QStandardItem*> items;
                for(auto f : fields) items.append(new QStandardItem(f));
                m_dataModel->appendRow(items);
            }
        }
        return true;
    }
    // 分支2：处理 .xls 文件 (使用 QAxObject / OLE 自动化)
    else {
        QAxObject excel("Excel.Application");
        if(excel.isNull()) return false;

        excel.setProperty("Visible", false);
        excel.setProperty("DisplayAlerts", false);

        QAxObject* workbooks = excel.querySubObject("Workbooks");
        if (!workbooks) return false;

        QAxObject* wb = workbooks->querySubObject("Open(const QString&)", QDir::toNativeSeparators(path));
        if(!wb) { excel.dynamicCall("Quit()"); return false; }

        QAxObject* sheets = wb->querySubObject("Worksheets");
        QAxObject* sheet = sheets->querySubObject("Item(int)", 1); // 这里的索引从1开始

        if(sheet) {
            QAxObject* ur = sheet->querySubObject("UsedRange");
            if(ur) {
                // 一次性读取所有数据以提高性能
                QVariant val = ur->dynamicCall("Value()");
                QList<QList<QVariant>> data;

                // 转换 QVariant 为二维列表
                if(val.typeId() == QMetaType::QVariantList) {
                    for(auto r : val.toList()) {
                        if(r.typeId() == QMetaType::QVariantList)
                            data.append(r.toList());
                    }
                }

                // 遍历处理数据
                for(int i = 0; i < data.size(); ++i) {
                    // Excel 行号从1开始，列表索引从0开始，需要转换匹配
                    int currentRow = i + 1;

                    if(currentRow < settings.startRow && !(settings.useHeader && currentRow == settings.headerRow)) continue;

                    QStringList fields;
                    for(auto c : data[i]) {
                        if(c.typeId() == QMetaType::QDateTime)
                            fields.append(c.toDateTime().toString("yyyy-MM-dd hh:mm:ss"));
                        else if(c.typeId() == QMetaType::QDate)
                            fields.append(c.toDate().toString("yyyy-MM-dd"));
                        else
                            fields.append(c.toString());
                    }

                    if(settings.useHeader && currentRow == settings.headerRow) {
                        m_dataModel->setHorizontalHeaderLabels(fields);
                        for(auto h : fields) {
                            ColumnDefinition d; d.name = h; m_columnDefinitions.append(d);
                        }
                    }
                    else if(currentRow >= settings.startRow) {
                        QList<QStandardItem*> items;
                        for(auto f : fields) items.append(new QStandardItem(f));
                        m_dataModel->appendRow(items);
                    }
                }
                delete ur;
            }
            delete sheet;
        }
        wb->dynamicCall("Close()"); delete wb; delete workbooks; excel.dynamicCall("Quit()");
        return true;
    }
}

// 加载文本文件 (.csv, .txt) - 已修正逻辑以支持配置参数
bool DataSingleSheet::loadTextFile(const QString& path, const DataImportSettings& settings)
{
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&f);

    // 1. 设置编码
    if(settings.encoding.startsWith("GBK")) {
        in.setEncoding(QStringConverter::System); // 兼容中文系统编码
    } else if (settings.encoding.startsWith("ISO")) {
        in.setEncoding(QStringConverter::Latin1);
    } else {
        in.setEncoding(QStringConverter::Utf8);
    }

    // 2. 确定分隔符
    QChar separator = ','; // 默认逗号
    if (settings.separator.contains("Tab")) separator = '\t';
    else if (settings.separator.contains("Space")) separator = ' ';
    else if (settings.separator.contains("Semicolon")) separator = ';';
    else if (settings.separator.contains("Comma")) separator = ',';
    else if (settings.separator.contains("Auto")) {
        // 自动识别：读取首行判断逗号和制表符的数量
        qint64 originalPos = in.pos();
        QString firstLine = in.readLine();
        if (firstLine.count('\t') > firstLine.count(',')) {
            separator = '\t';
        }
        in.seek(originalPos); // 恢复流位置
    }

    // 3. 逐行读取并解析
    int lineIdx = 0; // 当前处理的行号（逻辑行号，从1开始计数）

    while(!in.atEnd()) {
        QString line = in.readLine();

        // 如果是空行，且不是表头行，通常可以选择跳过，或者作为空数据行
        // 这里为了稳健，暂不强制跳过空行，取决于数据本身的质量

        lineIdx++; // 行号增加

        // 检查是否在需要处理的范围内
        // 规则：必须 >= 起始行，或者 == 表头行（如果启用了表头）
        bool isHeader = (settings.useHeader && lineIdx == settings.headerRow);
        bool isData = (lineIdx >= settings.startRow);

        if (!isHeader && !isData) {
            continue; // 跳过无关行
        }

        // 分割字符串
        QStringList parts = line.split(separator);

        // 处理 CSV 引号 (例如 "text, with, comma")
        // 与预览界面逻辑保持一致：去除首尾的双引号
        for (int i = 0; i < parts.size(); ++i) {
            QString p = parts[i].trimmed();
            if (p.startsWith('"') && p.endsWith('"') && p.length() >= 2) {
                p = p.mid(1, p.length() - 2);
            }
            parts[i] = p;
        }

        // 分支处理：表头 vs 数据
        if (isHeader) {
            // 设置表头
            m_dataModel->setHorizontalHeaderLabels(parts);

            // 更新列定义结构
            m_columnDefinitions.clear();
            for (const QString& h : parts) {
                ColumnDefinition d;
                d.name = h;
                m_columnDefinitions.append(d);
            }
        } else if (isData) {
            // 添加数据行
            QList<QStandardItem*> items;
            for(const QString& p : parts) {
                items.append(new QStandardItem(p.trimmed()));
            }
            m_dataModel->appendRow(items);
        }
    }

    f.close();
    return true;
}

// 导出为 Excel 文件
void DataSingleSheet::onExportExcel()
{
    QString path = QFileDialog::getSaveFileName(this, "导出 Excel", "", "Excel 文件 (*.xlsx)");
    if (path.isEmpty()) return;

    QXlsx::Document xlsx;
    // 设置表头样式
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setFillPattern(QXlsx::Format::PatternSolid);
    headerFormat.setPatternBackgroundColor(QColor(240, 240, 240));
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    headerFormat.setBorderStyle(QXlsx::Format::BorderThin);

    int colCount = m_dataModel->columnCount();
    int rowCount = m_dataModel->rowCount();

    // 写入表头
    for (int col = 0; col < colCount; ++col) {
        QString header = m_dataModel->headerData(col, Qt::Horizontal).toString();
        xlsx.write(1, col + 1, header, headerFormat);
        // 如果列被隐藏，Excel中也隐藏
        if (ui->dataTableView->isColumnHidden(col)) xlsx.setColumnHidden(col + 1, true);
    }

    // 写入数据
    for (int row = 0; row < rowCount; ++row) {
        // 如果行被隐藏，Excel中也隐藏
        if (ui->dataTableView->isRowHidden(row)) xlsx.setRowHidden(row + 2, true);

        for (int col = 0; col < colCount; ++col) {
            QStandardItem* item = m_dataModel->item(row, col);
            if (!item) continue;

            QVariant value = item->data(Qt::DisplayRole);
            QString strVal = value.toString();
            QXlsx::Format cellFormat;

            // 尝试写入数值以保持 Excel 计算功能
            if (strVal.startsWith("=")) {
                xlsx.write(row + 2, col + 1, strVal, cellFormat); // 公式
            } else {
                bool ok;
                double dVal = value.toDouble(&ok);
                if (ok && !strVal.isEmpty()) {
                    xlsx.write(row + 2, col + 1, dVal, cellFormat); // 数值
                } else {
                    xlsx.write(row + 2, col + 1, strVal, cellFormat); // 文本
                }
            }
        }
    }

    if (xlsx.saveAs(path))
        showStyledMessage(this, QMessageBox::Information, "成功", "数据已成功导出！");
    else
        showStyledMessage(this, QMessageBox::Warning, "失败", "导出失败，请检查文件是否被占用。");
}

// 显示自定义右键菜单
void DataSingleSheet::onCustomContextMenu(const QPoint& pos) {
    QMenu menu(this);
    // 设置菜单样式
    menu.setStyleSheet("QMenu { background-color: #FFFFFF; border: 1px solid #CCCCCC; padding: 4px; } "
                       "QMenu::item { padding: 6px 24px; color: #333333; } "
                       "QMenu::item:selected { background-color: #E6F7FF; color: #000000; }");

    // 行操作子菜单
    QMenu* rowMenu = menu.addMenu("行操作");
    rowMenu->addAction("在上方插入行", [=](){ onAddRow(1); });
    rowMenu->addAction("在下方插入行", [=](){ onAddRow(2); });
    rowMenu->addAction("删除选中行", this, &DataSingleSheet::onDeleteRow);
    rowMenu->addSeparator();
    rowMenu->addAction("隐藏选中行", this, &DataSingleSheet::onHideRow);
    rowMenu->addAction("显示所有行", this, &DataSingleSheet::onShowAllRows);

    // 列操作子菜单
    QMenu* colMenu = menu.addMenu("列操作");
    colMenu->addAction("在左侧插入列", [=](){ onAddCol(1); });
    colMenu->addAction("在右侧插入列", [=](){ onAddCol(2); });
    colMenu->addAction("删除选中列", this, &DataSingleSheet::onDeleteCol);
    colMenu->addSeparator();
    colMenu->addAction("隐藏选中列", this, &DataSingleSheet::onHideCol);
    colMenu->addAction("显示所有列", this, &DataSingleSheet::onShowAllCols);

    menu.addSeparator();

    // 数据处理菜单
    QMenu* dataMenu = menu.addMenu("数据处理");
    dataMenu->addAction("升序排列 (A-Z)", this, &DataSingleSheet::onSortAscending);
    dataMenu->addAction("降序排列 (Z-A)", this, &DataSingleSheet::onSortDescending);
    dataMenu->addAction("数据分列...", this, &DataSingleSheet::onSplitColumn);

    // 选中多个单元格时显示合并选项
    if (ui->dataTableView->selectionModel()->selectedIndexes().size() > 1) {
        menu.addSeparator();
        menu.addAction("合并单元格", this, &DataSingleSheet::onMergeCells);
        menu.addAction("取消合并", this, &DataSingleSheet::onUnmergeCells);
    }

    menu.exec(ui->dataTableView->mapToGlobal(pos));
}

// 行列操作的具体实现槽函数
void DataSingleSheet::onHideRow() {
    QModelIndexList s = ui->dataTableView->selectionModel()->selectedRows();
    if(s.isEmpty()) {
        QModelIndex i = ui->dataTableView->currentIndex();
        if(i.isValid()) ui->dataTableView->setRowHidden(i.row(),true);
    } else {
        for(auto i : s) ui->dataTableView->setRowHidden(i.row(),true);
    }
}
void DataSingleSheet::onShowAllRows() { for(int i=0; i<m_dataModel->rowCount(); ++i) ui->dataTableView->setRowHidden(i, false); }
void DataSingleSheet::onHideCol() {
    QModelIndexList s = ui->dataTableView->selectionModel()->selectedColumns();
    if(s.isEmpty()) {
        QModelIndex i = ui->dataTableView->currentIndex();
        if(i.isValid()) ui->dataTableView->setColumnHidden(i.column(),true);
    } else {
        for(auto i : s) ui->dataTableView->setColumnHidden(i.column(),true);
    }
}
void DataSingleSheet::onShowAllCols() { for(int i=0; i<m_dataModel->columnCount(); ++i) ui->dataTableView->setColumnHidden(i, false); }

// 合并单元格
void DataSingleSheet::onMergeCells() {
    auto s = ui->dataTableView->selectionModel()->selectedIndexes();
    if(s.isEmpty()) return;
    int r1 = 2147483647, r2 = -1, c1 = 2147483647, c2 = -1;
    // 计算选区的矩形范围
    for(auto i : s){
        r1 = qMin(r1, i.row()); r2 = qMax(r2, i.row());
        c1 = qMin(c1, i.column()); c2 = qMax(c2, i.column());
    }
    ui->dataTableView->setSpan(r1, c1, r2-r1+1, c2-c1+1);
}

void DataSingleSheet::onUnmergeCells() {
    auto i = ui->dataTableView->currentIndex();
    if(i.isValid()) ui->dataTableView->setSpan(i.row(), i.column(), 1, 1);
}

void DataSingleSheet::onSortAscending() {
    if(ui->dataTableView->currentIndex().isValid())
        m_proxyModel->sort(ui->dataTableView->currentIndex().column(), Qt::AscendingOrder);
}
void DataSingleSheet::onSortDescending() {
    if(ui->dataTableView->currentIndex().isValid())
        m_proxyModel->sort(ui->dataTableView->currentIndex().column(), Qt::DescendingOrder);
}

// 插入行
void DataSingleSheet::onAddRow(int m) {
    int r = m_dataModel->rowCount();
    QModelIndex i = ui->dataTableView->currentIndex();
    if(i.isValid()){
        int sr = m_proxyModel->mapToSource(i).row();
        r = (m == 1) ? sr : sr + 1;
    }
    QList<QStandardItem*> l;
    for(int k=0; k<m_dataModel->columnCount(); ++k) l << new QStandardItem("");
    m_dataModel->insertRow(r, l);
}

// 删除行
void DataSingleSheet::onDeleteRow() {
    auto s = ui->dataTableView->selectionModel()->selectedRows();
    if(s.isEmpty()){
        auto i = ui->dataTableView->currentIndex();
        if(i.isValid()) m_dataModel->removeRow(m_proxyModel->mapToSource(i).row());
    } else {
        QList<int> rs;
        for(auto i : s) rs << m_proxyModel->mapToSource(i).row();
        // 从大到小排序，防止删除时索引偏移
        std::sort(rs.begin(), rs.end(), std::greater<int>());
        auto l = std::unique(rs.begin(), rs.end());
        rs.erase(l, rs.end());
        for(int r : rs) m_dataModel->removeRow(r);
    }
}

// 插入列
void DataSingleSheet::onAddCol(int m) {
    int c = m_dataModel->columnCount();
    QModelIndex i = ui->dataTableView->currentIndex();
    if(i.isValid()){
        int sc = m_proxyModel->mapToSource(i).column();
        c = (m == 1) ? sc : sc + 1;
    }
    m_dataModel->insertColumn(c);

    // 同步更新列定义
    ColumnDefinition d; d.name = "新列";
    if(c < m_columnDefinitions.size()) m_columnDefinitions.insert(c, d);
    else m_columnDefinitions.append(d);
    m_dataModel->setHeaderData(c, Qt::Horizontal, "新列");
}

// 删除列
void DataSingleSheet::onDeleteCol() {
    auto s = ui->dataTableView->selectionModel()->selectedColumns();
    if(s.isEmpty()){
        auto i = ui->dataTableView->currentIndex();
        if(i.isValid()){
            int c = m_proxyModel->mapToSource(i).column();
            m_dataModel->removeColumn(c);
            if(c < m_columnDefinitions.size()) m_columnDefinitions.removeAt(c);
        }
    } else {
        QList<int> cs;
        for(auto i : s) cs << m_proxyModel->mapToSource(i).column();
        std::sort(cs.begin(), cs.end(), std::greater<int>());
        auto l = std::unique(cs.begin(), cs.end());
        cs.erase(l, cs.end());
        for(int c : cs){
            m_dataModel->removeColumn(c);
            if(c < m_columnDefinitions.size()) m_columnDefinitions.removeAt(c);
        }
    }
}

// 数据分列操作
void DataSingleSheet::onSplitColumn() {
    QModelIndex idx = ui->dataTableView->currentIndex();
    if (!idx.isValid()) return;
    int col = m_proxyModel->mapToSource(idx).column();

    InternalSplitDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    QString separator = dlg.getSeparator();
    if (separator.isEmpty()) return;

    int rows = m_dataModel->rowCount();
    // 在当前列后插入新列存放结果
    m_dataModel->insertColumn(col + 1);

    ColumnDefinition def; def.name = "拆分数据";
    if (col + 1 < m_columnDefinitions.size()) m_columnDefinitions.insert(col + 1, def);
    else m_columnDefinitions.append(def);
    m_dataModel->setHeaderData(col + 1, Qt::Horizontal, "拆分数据");

    for (int i = 0; i < rows; ++i) {
        QStandardItem* item = m_dataModel->item(i, col);
        if (!item) continue;
        QString text = item->text();
        int sepIdx = text.indexOf(separator);
        if (sepIdx != -1) {
            // 原列保留前半部分
            item->setText(text.left(sepIdx).trimmed());
            // 新列存放后半部分
            m_dataModel->setItem(i, col + 1, new QStandardItem(text.mid(sepIdx + separator.length()).trimmed()));
        } else {
            m_dataModel->setItem(i, col + 1, new QStandardItem(""));
        }
    }
}

// ============================================================================
// [修改] 弹出窗口槽函数（强制应用统一样式）
// ============================================================================

// 定义列属性弹窗
void DataSingleSheet::onDefineColumns() {
    QStringList h;
    for(int i=0; i<m_dataModel->columnCount(); ++i)
        h << m_dataModel->headerData(i, Qt::Horizontal).toString();

    DataColumnDialog d(h, m_columnDefinitions, this);
    applySheetDialogStyle(&d); // 应用样式

    if(d.exec() == QDialog::Accepted){
        m_columnDefinitions = d.getColumnDefinitions();
        // 更新表头显示
        for(int i=0; i<m_columnDefinitions.size(); ++i)
            if(i < m_dataModel->columnCount())
                m_dataModel->setHeaderData(i, Qt::Horizontal, m_columnDefinitions[i].name);
        emit dataChanged();
    }
}

// 时间列转换弹窗
void DataSingleSheet::onTimeConvert() {
    DataCalculate calc;
    QStringList h;
    for(int i=0; i<m_dataModel->columnCount(); ++i)
        h << m_dataModel->headerData(i, Qt::Horizontal).toString();

    TimeConversionDialog d(h, this);
    applySheetDialogStyle(&d); // 应用样式

    if(d.exec() == QDialog::Accepted){
        auto cfg = d.getConversionConfig();
        auto res = calc.convertTimeColumn(m_dataModel, m_columnDefinitions, cfg);
        if(res.success) showStyledMessage(this, QMessageBox::Information, "成功", "时间列转换完成");
        else showStyledMessage(this, QMessageBox::Warning, "失败", res.errorMessage);
        emit dataChanged();
    }
}

// 压降计算（直接执行，无弹窗，但结果弹窗需应用样式）
void DataSingleSheet::onPressureDropCalc() {
    DataCalculate calc;
    auto res = calc.calculatePressureDrop(m_dataModel, m_columnDefinitions);
    if(res.success) showStyledMessage(this, QMessageBox::Information, "成功", "压降计算完成");
    else showStyledMessage(this, QMessageBox::Warning, "失败", res.errorMessage);
    emit dataChanged();
}

// 井底流压计算弹窗
void DataSingleSheet::onCalcPwf() {
    DataCalculate calc;
    QStringList h;
    for(int i=0; i<m_dataModel->columnCount(); ++i)
        h << m_dataModel->headerData(i, Qt::Horizontal).toString();

    PwfCalculationDialog d(h, this);
    applySheetDialogStyle(&d); // 应用样式

    if(d.exec() == QDialog::Accepted){
        auto cfg = d.getConfig();
        auto res = calc.calculateBottomHolePressure(m_dataModel, m_columnDefinitions, cfg);
        if(res.success) showStyledMessage(this, QMessageBox::Information, "成功", "井底流压计算完成");
        else showStyledMessage(this, QMessageBox::Warning, "失败", res.errorMessage);
        emit dataChanged();
    }
}

// 错误高亮检查
void DataSingleSheet::onHighlightErrors() {
    // 清除原有背景色
    for(int r=0; r<m_dataModel->rowCount(); ++r)
        for(int c=0; c<m_dataModel->columnCount(); ++c)
            if(auto it = m_dataModel->item(r,c)) it->setBackground(Qt::NoBrush);

    // 查找压力列
    int pIdx = -1;
    for(int i=0; i<m_columnDefinitions.size(); ++i)
        if(m_columnDefinitions[i].type == WellTestColumnType::Pressure) pIdx = i;

    int err = 0;
    if(pIdx != -1) {
        for(int r=0; r<m_dataModel->rowCount(); ++r) {
            auto item = m_dataModel->item(r, pIdx);
            // 简单的逻辑检查：压力不能为负
            if(item && item->text().toDouble() < 0) {
                item->setBackground(QColor(255, 200, 200));
                err++;
            }
        }
    }
    showStyledMessage(this, QMessageBox::Information, "检查完成", QString("发现 %1 个错误。").arg(err));
}

void DataSingleSheet::onModelDataChanged() { emit dataChanged(); }

// 序列化保存数据到 JSON 对象
QJsonObject DataSingleSheet::saveToJson() const {
    QJsonObject sheetObj;
    sheetObj["filePath"] = m_filePath;

    QJsonArray headers;
    for(int i=0; i<m_dataModel->columnCount(); ++i)
        headers.append(m_dataModel->headerData(i, Qt::Horizontal).toString());
    sheetObj["headers"] = headers;

    sheetObj["data"] = serializeRows();
    return sheetObj;
}

// 从 JSON 对象加载数据
void DataSingleSheet::loadFromJson(const QJsonObject& jsonSheet) {
    m_dataModel->clear();
    m_columnDefinitions.clear();
    m_filePath = jsonSheet["filePath"].toString();

    QJsonArray headers = jsonSheet["headers"].toArray();
    QStringList sl;
    for(auto v: headers) sl << v.toString();
    m_dataModel->setHorizontalHeaderLabels(sl);

    for(auto s : sl) {
        ColumnDefinition d;
        d.name = s;
        m_columnDefinitions.append(d);
    }

    QJsonArray rows = jsonSheet["data"].toArray();
    deserializeRows(rows);
}

// 辅助：序列化所有行数据
QJsonArray DataSingleSheet::serializeRows() const {
    QJsonArray a;
    for(int i=0; i<m_dataModel->rowCount(); ++i) {
        QJsonArray r;
        for(int j=0; j<m_dataModel->columnCount(); ++j) {
            QStandardItem* item = m_dataModel->item(i, j);
            if (item) {
                r.append(item->text());
            } else {
                r.append(""); // 单元格为空时，存入空字符串
            }
        }
        a.append(r);
    }
    return a;
}

// 辅助：反序列化数据到行
void DataSingleSheet::deserializeRows(const QJsonArray& array) {
    for(auto val : array) {
        QJsonArray r = val.toArray();
        QList<QStandardItem*> l;
        for(auto v : r) l.append(new QStandardItem(v.toString()));
        m_dataModel->appendRow(l);
    }
}
