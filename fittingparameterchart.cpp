/*
 * 文件名: fittingparameterchart.cpp
 * 文件作用: 拟合参数图表管理类实现文件
 * 功能描述:
 * 1. 初始化参数表格样式。
 * 2. 实现 resetParams 逻辑：根据模型类型自动勾选默认拟合参数（isFit=true）。
 * 3. 实现 eventFilter 逻辑：支持鼠标滚轮调节参数，增加了数值上下限检查 (min/max)。
 * 4. 引入 QTimer 实现滚轮事件的防抖动处理，避免快速滚动导致软件闪退。
 * 5. 保持 LfD (无因次缝长) 的自动计算与只读逻辑，LfD 默认显示但不拟合。
 */

#include "fittingparameterchart.h"
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QDebug>
#include <QBrush>
#include <QColor>
#include <QRegularExpression>
#include <QWheelEvent>
#include <algorithm> // for std::clamp

FittingParameterChart::FittingParameterChart(QTableWidget *parentTable, QObject *parent)
    : QObject(parent), m_table(parentTable), m_modelManager(nullptr)
{
    // 初始化滚轮防抖定时器
    m_wheelTimer = new QTimer(this);
    m_wheelTimer->setSingleShot(true);
    m_wheelTimer->setInterval(200); // 200毫秒延迟触发
    connect(m_wheelTimer, &QTimer::timeout, this, &FittingParameterChart::onWheelDebounceTimeout);

    if(m_table) {
        QStringList headers;
        headers << "序号" << "参数名称" << "数值" << "单位";
        m_table->setColumnCount(headers.size());
        m_table->setHorizontalHeaderLabels(headers);

        m_table->horizontalHeader()->setStyleSheet(
            "QHeaderView::section { background-color: #E0E0E0; color: black; font-weight: bold; border: 1px solid #A0A0A0; }"
            );

        m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_table->horizontalHeader()->setStretchLastSection(true);

        m_table->setColumnWidth(0, 40);
        m_table->setColumnWidth(1, 160);
        m_table->setColumnWidth(2, 80);

        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setAlternatingRowColors(false);
        m_table->verticalHeader()->setVisible(false);

        // 安装事件过滤器
        m_table->viewport()->installEventFilter(this);

        // 连接内容变化信号 (用于参数联动)
        connect(m_table, &QTableWidget::itemChanged, this, &FittingParameterChart::onTableItemChanged);
    }
}

// 事件过滤器：处理鼠标滚轮
bool FittingParameterChart::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_table->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);

        QPoint pos = wheelEvent->position().toPoint();
        QTableWidgetItem *item = m_table->itemAt(pos);

        // 仅在数值列（第2列，索引2）且已有对应参数时生效
        if (item && item->column() == 2) {
            int row = item->row();
            QTableWidgetItem *keyItem = m_table->item(row, 1);
            if (!keyItem) return false;
            QString paramName = keyItem->data(Qt::UserRole).toString();

            // 禁止滚动修改 LfD
            if (paramName == "LfD") return true;

            FitParameter *targetParam = nullptr;
            for (auto &p : m_params) {
                if (p.name == paramName) {
                    targetParam = &p;
                    break;
                }
            }

            if (targetParam) {
                QString currentText = item->text();
                // 多值输入不处理
                if (currentText.contains(',') || currentText.contains(QChar(0xFF0C))) {
                    return false;
                }

                bool ok;
                double currentVal = currentText.toDouble(&ok);
                if (ok) {
                    int steps = wheelEvent->angleDelta().y() / 120;
                    double newVal = currentVal + steps * targetParam->step;

                    // [优化] 增加上下限范围检查，防止越界
                    if (targetParam->max > targetParam->min) {
                        if (newVal < targetParam->min) newVal = targetParam->min;
                        if (newVal > targetParam->max) newVal = targetParam->max;
                    }

                    // 立即更新表格显示，保证视觉流畅性
                    item->setText(QString::number(newVal, 'g', 6));
                    targetParam->value = newVal;

                    // [优化] 启动/重置防抖定时器，避免频繁触发重绘
                    m_wheelTimer->start();

                    return true; // 事件已处理
                }
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

void FittingParameterChart::onWheelDebounceTimeout()
{
    // 定时器结束，发出信号通知界面重绘曲线
    emit parameterChangedByWheel();
}

void FittingParameterChart::onTableItemChanged(QTableWidgetItem *item)
{
    if (!item || item->column() != 2) return;

    int row = item->row();
    QTableWidgetItem *keyItem = m_table->item(row, 1);
    if (!keyItem) return;

    QString changedKey = keyItem->data(Qt::UserRole).toString();

    // 联动逻辑：L 或 Lf 变化更新 LfD
    if (changedKey == "L" || changedKey == "Lf") {
        double valL = 0.0;
        double valLf = 0.0;
        QTableWidgetItem* itemLfD = nullptr;

        for(int i = 0; i < m_table->rowCount(); ++i) {
            QTableWidgetItem* k = m_table->item(i, 1);
            QTableWidgetItem* v = m_table->item(i, 2);
            if(k && v) {
                QString key = k->data(Qt::UserRole).toString();
                if (key == "L") valL = v->text().toDouble();
                else if (key == "Lf") valLf = v->text().toDouble();
                else if (key == "LfD") itemLfD = v;
            }
        }

        if (itemLfD && valL > 1e-9) {
            double newLfD = valLf / valL;
            m_table->blockSignals(true);
            itemLfD->setText(QString::number(newLfD, 'g', 6));

            for(auto& p : m_params) {
                if(p.name == "LfD") { p.value = newLfD; break; }
            }
            m_table->blockSignals(false);

            // 联动引起的改变也通过防抖机制触发，防止冲突
            if(!m_wheelTimer->isActive()) m_wheelTimer->start();
        }
    }
}

void FittingParameterChart::setModelManager(ModelManager *m)
{
    m_modelManager = m;
}

// [核心修改] 根据模型类型获取默认需要拟合的参数列表
// 增加了 reD 的显示逻辑，防止边界模型下该参数不可见
QStringList FittingParameterChart::getDefaultFitKeys(ModelManager::ModelType type)
{
    QStringList keys;
    // 公共拟合参数：
    // kf(内区渗透率), km(外区渗透率), L(水平井长), Lf(裂缝半长), nf(裂缝条数)
    // rmD(复合半径), omega1(内区储容比), omega2(外区储容比), lambda1(窜流系数)
    keys << "kf" << "km" << "L" << "Lf" << "nf" << "rmD" << "omega1" << "omega2" << "lambda1";

    // 根据模型类型区分
    // 模型 1, 3, 5: 增加 井筒存储系数(C/cD) 和 表皮系数(S)
    if (type == ModelManager::Model_1 || type == ModelManager::Model_3 || type == ModelManager::Model_5) {
        keys << "cD" << "C" << "S";
    }

    // [优化] 模型 3, 4, 5, 6: 增加 边界半径 reD (有边界模型)
    if (type == ModelManager::Model_3 || type == ModelManager::Model_4 ||
        type == ModelManager::Model_5 || type == ModelManager::Model_6) {
        keys << "reD";
    }

    return keys;
}

void FittingParameterChart::resetParams(ModelManager::ModelType type)
{
    if(!m_modelManager) return;
    m_params.clear();

    QMap<QString, double> defaultMap = m_modelManager->getDefaultParameters(type);

    // 确保默认值中 LfD 计算正确
    double defL = defaultMap.value("L", 1000.0);
    double defLf = defaultMap.value("Lf", 100.0);
    if(defaultMap.contains("LfD") && defL > 1e-9) {
        defaultMap["LfD"] = defLf / defL;
    }

    // 获取当前模型应当默认拟合的参数列表
    QStringList fitKeys = getDefaultFitKeys(type);

    QMapIterator<QString, double> it(defaultMap);
    while(it.hasNext()) {
        it.next();
        FitParameter p;
        p.name = it.key();
        p.value = it.value();

        // [逻辑修改] 如果在默认拟合列表中，则设置 isFit=true，且 isVisible=true
        if (fitKeys.contains(p.name)) {
            p.isFit = true;
            p.isVisible = true;
        } else {
            p.isFit = false;
            p.isVisible = false; // 默认不显示非拟合参数
        }

        // [特殊处理] LfD 始终显示（只读），但绝不拟合
        if (p.name == "LfD") {
            p.isFit = false;
            p.isVisible = true;
        }

        // 设置范围
        if (p.value > 0) {
            p.min = p.value * 0.01; p.max = p.value * 100.0;
        } else {
            p.min = 0.0; p.max = 100.0;
        }

        // 设置步长
        if (p.name == "k" || p.name == "kf" || p.name == "km") p.step = 1.0;
        else if (p.name == "S") p.step = 0.1;
        else if (p.name == "C" || p.name == "cD") p.step = 0.001;
        else if (p.name == "phi") p.step = 0.01;
        else p.step = p.value != 0 ? std::abs(p.value * 0.1) : 0.1;

        // 获取显示信息
        QString symbol, uniSym, unit;
        getParamDisplayInfo(p.name, p.displayName, symbol, uniSym, unit);

        m_params.append(p);
    }
    refreshParamTable();
}

QList<FitParameter> FittingParameterChart::getParameters() const { return m_params; }
void FittingParameterChart::setParameters(const QList<FitParameter> &params) { m_params = params; refreshParamTable(); }

void FittingParameterChart::switchModel(ModelManager::ModelType newType)
{
    QMap<QString, double> oldValues;
    for(const auto& p : m_params) oldValues.insert(p.name, p.value);

    // 重置参数（此处会更新 isFit 和 isVisible 状态）
    resetParams(newType);

    // 恢复旧值
    for(auto& p : m_params) {
        if(oldValues.contains(p.name)) p.value = oldValues[p.name];
    }

    // 再次强制更新 LfD
    for(auto& p : m_params) {
        if(p.name == "LfD") {
            double L = oldValues.value("L", 1000.0);
            double Lf = oldValues.value("Lf", 100.0);
            if(L > 1e-9) p.value = Lf / L;
        }
    }
    refreshParamTable();
}

void FittingParameterChart::updateParamsFromTable()
{
    if(!m_table) return;
    for(int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* itemKey = m_table->item(i, 1);
        if(!itemKey) continue;
        QString key = itemKey->data(Qt::UserRole).toString();
        QTableWidgetItem* itemVal = m_table->item(i, 2);

        QString text = itemVal->text();
        double val = 0.0;
        if (text.contains(',') || text.contains(QChar(0xFF0C))) {
            QString firstPart = text.split(QRegularExpression("[,，]"), Qt::SkipEmptyParts).first();
            val = firstPart.toDouble();
        } else {
            val = text.toDouble();
        }

        for(auto& p : m_params) {
            if(p.name == key) { p.value = val; break; }
        }
    }
}

QMap<QString, QString> FittingParameterChart::getRawParamTexts() const
{
    QMap<QString, QString> rawTexts;
    if(!m_table) return rawTexts;
    for(int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* itemKey = m_table->item(i, 1);
        QTableWidgetItem* itemVal = m_table->item(i, 2);
        if (itemKey && itemVal) {
            QString key = itemKey->data(Qt::UserRole).toString();
            rawTexts.insert(key, itemVal->text());
        }
    }
    return rawTexts;
}

void FittingParameterChart::refreshParamTable()
{
    if(!m_table) return;
    m_table->blockSignals(true);
    m_table->setRowCount(0);
    int serialNo = 1;

    // 先显示勾选拟合的参数 (isVisible 为 true 且 isFit 为 true)
    for(const auto& p : m_params) {
        if(p.isVisible && p.isFit) addRowToTable(p, serialNo, true);
    }
    // 再显示未勾选拟合但需要显示的参数 (如 LfD)
    for(const auto& p : m_params) {
        if(p.isVisible && !p.isFit) addRowToTable(p, serialNo, false);
    }

    m_table->blockSignals(false);
}

void FittingParameterChart::addRowToTable(const FitParameter& p, int& serialNo, bool highlight)
{
    int row = m_table->rowCount();
    m_table->insertRow(row);
    QColor bgColor = highlight ? QColor(255, 255, 224) : Qt::white;

    if (p.name == "LfD") {
        bgColor = QColor(240, 240, 240);
    }

    QTableWidgetItem* numItem = new QTableWidgetItem(QString::number(serialNo++));
    numItem->setFlags(numItem->flags() & ~Qt::ItemIsEditable);
    numItem->setTextAlignment(Qt::AlignCenter);
    numItem->setBackground(bgColor);
    m_table->setItem(row, 0, numItem);

    QString displayNameFull = QString("%1 (%2)").arg(p.displayName).arg(p.name);
    QTableWidgetItem* nameItem = new QTableWidgetItem(displayNameFull);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
    nameItem->setData(Qt::UserRole, p.name);
    nameItem->setBackground(bgColor);
    if(highlight) { QFont f = nameItem->font(); f.setBold(true); nameItem->setFont(f); }
    m_table->setItem(row, 1, nameItem);

    QTableWidgetItem* valItem = new QTableWidgetItem(QString::number(p.value, 'g', 6));
    valItem->setBackground(bgColor);
    if(highlight) { QFont f = valItem->font(); f.setBold(true); valItem->setFont(f); }

    if (p.name == "LfD") {
        valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
        valItem->setForeground(QBrush(Qt::darkGray));
    }

    m_table->setItem(row, 2, valItem);

    QString dummy, symbol, uniSym, unit;
    getParamDisplayInfo(p.name, dummy, symbol, uniSym, unit);
    if(unit == "无因次" || unit == "小数") unit = "-";
    QTableWidgetItem* unitItem = new QTableWidgetItem(unit);
    unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
    unitItem->setBackground(bgColor);
    m_table->setItem(row, 3, unitItem);
}

void FittingParameterChart::getParamDisplayInfo(const QString &name, QString &chName, QString &symbol, QString &uniSym, QString &unit)
{
    if(name == "k")           { chName = "渗透率";         unit = "mD"; }
    else if(name == "h")      { chName = "有效厚度";       unit = "m"; }
    else if(name == "phi")    { chName = "孔隙度";         unit = "小数"; }
    else if(name == "mu")     { chName = "流体粘度";       unit = "mPa·s"; }
    else if(name == "B")      { chName = "体积系数";       unit = "无因次"; }
    else if(name == "Ct")     { chName = "综合压缩系数";   unit = "MPa⁻¹"; }
    else if(name == "rw")     { chName = "井筒半径";       unit = "m"; }
    else if(name == "q")      { chName = "测试产量";       unit = "m³/d"; }
    else if(name == "C")      { chName = "井筒储存系数";   unit = "m³/MPa"; }
    else if(name == "cD")     { chName = "无因次井储";     unit = "无因次"; }
    else if(name == "S")      { chName = "表皮系数";       unit = "无因次"; }
    else if(name == "L")      { chName = "水平井长";       unit = "m"; }
    else if(name == "Lf")     { chName = "裂缝半长";       unit = "m"; }
    else if(name == "nf")     { chName = "裂缝条数";       unit = "条"; }
    else if(name == "kf")     { chName = "内区渗透率";     unit = "D"; }
    else if(name == "km")     { chName = "外区渗透率";     unit = "D"; }
    else if(name == "reD")    { chName = "无因次泄油半径"; unit = "无因次"; }
    else if(name == "lambda1"){ chName = "窜流系数";       unit = "无因次"; }
    else if(name == "omega1") { chName = "内区储容比";     unit = "无因次"; }
    else if(name == "omega2") { chName = "外区储容比";     unit = "无因次"; }
    else if(name == "gamaD")  { chName = "压敏系数";       unit = "无因次"; }
    else if(name == "rmD")    { chName = "复合半径";       unit = "无因次"; }
    else if(name == "LfD")    { chName = "无因次缝长";     unit = "无因次"; }
    else { chName = name; unit = ""; }
    symbol = name; uniSym = name;
}

