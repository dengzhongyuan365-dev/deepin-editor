// SPDX-FileCopyrightText: 2011-2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "replacebar.h"
#include "../common/utils.h"

#include <QDebug>

// 不同布局模式下界面参数，不完全对应设计图固定值，调整后实际像素值和设计图对应
const int s_RBHeight = 60;
const QMargins s_RBContetsMargins = {16, 0, 10, 0};
const int s_RBCloseBtnSize = 30;
const int s_RBHeight_Compact = 40;
const QMargins s_RBContetsMarginsCompact = {16, 0, 6, 0};
const int s_RBCloseBtnSizeCompact = 26;
const int s_RBCloseIconSizeCompact = 27;

ReplaceBar::ReplaceBar(QWidget *parent)
    :  DFloatingWidget(parent)
{
    qDebug() << "ReplaceBar constructor start";
    // Init.
    hide();
    setFixedHeight(60);
    qDebug() << "ReplaceBar initialized with height 60";
    // Init layout and widgets.
    m_layout = new QHBoxLayout();
    m_layout->setSpacing(10);
    m_layout->setContentsMargins(16, 0, 10, 0);
    m_layout->setAlignment(Qt::AlignVCenter);
    m_replaceLabel = new QLabel(tr("Find"));
    m_replaceLine = new LineBar();
    m_withLabel = new QLabel(tr("Replace With"));
    m_withLine = new LineBar();
    m_replaceButton = new QPushButton(tr("Replace"));
    m_replaceSkipButton = new QPushButton(tr("Skip"));
    m_replaceRestButton = new QPushButton(tr("Replace Rest"));
    m_replaceAllButton = new QPushButton(tr("Replace All"));
    m_closeButton = new DIconButton(DStyle::SP_CloseButton);
    m_closeButton->setFlat(true);
    m_closeButton->setFixedSize(30, 30);
    m_closeButton->setEnabledCircle(true);
    m_closeButton->setIconSize(QSize(30, 30));

    m_layout->addWidget(m_replaceLabel);
    m_layout->addLayout(createVerticalLine(m_replaceLine));
    m_layout->addWidget(m_withLabel);
    m_layout->addLayout(createVerticalLine(m_withLine));
    m_layout->addWidget(m_replaceButton);
    m_layout->addWidget(m_replaceSkipButton);
    m_layout->addWidget(m_replaceRestButton);
    m_layout->addWidget(m_replaceAllButton);
    m_layout->addWidget(m_closeButton);
    this->setLayout(m_layout);

    // Make button don't grab keyboard focus after click it.
    #if 0
    m_replaceButton->setFocusPolicy(Qt::NoFocus);
    m_replaceSkipButton->setFocusPolicy(Qt::NoFocus);
    m_replaceRestButton->setFocusPolicy(Qt::NoFocus);
    m_replaceAllButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    #endif
    connect(m_replaceLine, &LineBar::signal_sentText, this, &ReplaceBar::change, Qt::QueuedConnection);

    connect(this, &ReplaceBar::pressEsc, this, &ReplaceBar::replaceClose, Qt::QueuedConnection);

    //connect(m_replaceLine, &LineBar::pressEnter, this, &ReplaceBar::handleReplaceNext, Qt::QueuedConnection);         //Shielded by Hengbo for new demand.
    connect(m_withLine, &LineBar::returnPressed, this, &ReplaceBar::handleReplaceNext, Qt::QueuedConnection);
    connect(m_replaceLine, &LineBar::pressCtrlEnter, this, &ReplaceBar::handleSkip,Qt::QueuedConnection);
    connect(m_withLine, &LineBar::pressCtrlEnter, this, &ReplaceBar::handleSkip,Qt::QueuedConnection);
    connect(m_replaceLine, &LineBar::pressAltEnter, this, &ReplaceBar::handleReplaceRest, Qt::QueuedConnection);
    connect(m_withLine, &LineBar::pressAltEnter, this, &ReplaceBar::handleReplaceRest, Qt::QueuedConnection);
    connect(m_replaceLine, &LineBar::pressMetaEnter, this, &ReplaceBar::handleReplaceAll, Qt::QueuedConnection);
    connect(m_withLine, &LineBar::pressMetaEnter, this, &ReplaceBar::handleReplaceAll, Qt::QueuedConnection);
    connect(m_replaceLine, &LineBar::returnPressed, this, &ReplaceBar::handleContentChanged, Qt::QueuedConnection);
    connect(m_replaceButton, &QPushButton::clicked, this, &ReplaceBar::handleReplaceNext, Qt::QueuedConnection);
    connect(m_replaceSkipButton, &QPushButton::clicked, this, &ReplaceBar::handleSkip,Qt::QueuedConnection);
    connect(m_replaceRestButton, &QPushButton::clicked, this, &ReplaceBar::handleReplaceRest, Qt::QueuedConnection);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &ReplaceBar::handleReplaceAll, Qt::QueuedConnection);

    connect(m_closeButton, &DIconButton::clicked, this, &ReplaceBar::replaceClose, Qt::QueuedConnection);

#ifdef DTKWIDGET_CLASS_DSizeMode
    updateSizeMode();
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::sizeModeChanged, this, &ReplaceBar::updateSizeMode);
#endif
    qDebug() << "ReplaceBar constructor end";
}

bool ReplaceBar::isFocus()
{
    qDebug() << "isFocus";
    return m_replaceLine->hasFocus();
}

void ReplaceBar::focus()
{
    qDebug() << "focus";
    m_replaceLine->lineEdit()->setFocus();
    qDebug() << "focus success";
}

void ReplaceBar::activeInput(QString text, QString file, int row, int column, int scrollOffset)
{
    qDebug() << "Activating replace bar for file:" << file
                      << "at row:" << row << "column:" << column;
    // Try fill keyword with select text.
    m_withLine->lineEdit()->clear();
    m_replaceLine->lineEdit()->clear();
    m_replaceLine->lineEdit()->insert(text);
    m_replaceLine->lineEdit()->selectAll();
    qDebug() << "Initial search text set, length:" << text.length();

    // Show.
    show();
    qDebug() << "Replace bar shown";

    // Save file info for back to position.
    m_replaceFile = file;
    m_replaceFileRow = row;
    m_replaceFileColumn = column;
    m_replaceFileSrollOffset = scrollOffset;
    qDebug() << "Saved position info - row:" << row
                       << "column:" << column << "scrollOffset:" << scrollOffset;

    // Focus.
    focus();
    qDebug() << "Focus set to replace input";
}

void ReplaceBar::handleSkip()
{
    qDebug() << "handleSkip";
    emit replaceSkip(m_replaceFile, m_replaceLine->lineEdit()->text());
}

void ReplaceBar::replaceClose()
{
    qDebug() << "replaceClose";
    searched = false;
    hide();
    emit sigReplacebarClose();
    qDebug() << "replaceClose end";
}

void ReplaceBar::handleContentChanged()
{
    qDebug() << "handleContentChanged";
    updateSearchKeyword(m_replaceFile, m_replaceLine->lineEdit()->text());
}

void ReplaceBar::handleReplaceNext()
{
    qDebug() << "handleReplaceNext";
    QString searchText = m_replaceLine->lineEdit()->text();
    QString replaceText = m_withLine->lineEdit()->text();
    qDebug() << "Replacing next occurrence, search length:"
                      << searchText.length() << "replace length:" << replaceText.length();
                      
    if (!searched) {
        qDebug() << "First search, removing previous keywords";
        emit removeSearchKeyword();
        emit beforeReplace(searchText);
    }
    replaceNext(m_replaceFile, searchText, replaceText);
    searched = true;
    qDebug() << "Replace next completed";
}

void ReplaceBar::handleReplaceRest()
{
    qDebug() << "handleReplaceRest";
    replaceRest(m_replaceLine->lineEdit()->text(), m_withLine->lineEdit()->text());
}

void ReplaceBar::handleReplaceAll()
{
    qDebug() << "handleReplaceAll";
    replaceAll(m_replaceLine->lineEdit()->text(), m_withLine->lineEdit()->text());
}

void ReplaceBar::hideEvent(QHideEvent *)
{
    qDebug() << "hideEvent";
    searched = false;
    removeSearchKeyword();
}

bool ReplaceBar::focusNextPrevChild(bool)
{
    qDebug() << "focusNextPrevChild";
    // Make keyword jump between two EditLine widgets.
    auto *editWidget = qobject_cast<LineBar *>(focusWidget());
    if (editWidget != nullptr) {
        qDebug() << "editWidget" << editWidget;
        if (editWidget == m_replaceLine) {
            m_withLine->lineEdit()->setFocus();
            qDebug() << "focus withLine";
            return true;
        } else if (editWidget == m_withLine) {
            m_replaceLine->lineEdit()->setFocus();
            qDebug() << "focus replaceLine";
            return true;
        }
    }
    qDebug() << "focusNextPrevChild end";
    return false;
}

void ReplaceBar::keyPressEvent(QKeyEvent *e)
{
    qDebug() << "keyPressEvent";
    const QString &key = Utils::getKeyshortcut(e);
    if (key == "Esc") {
        qDebug() << "Esc";
        QWidget::hide();
        emit sigReplacebarClose();
        qDebug() << "sigReplacebarClose";
    }
    if (m_closeButton->hasFocus() && key == "Tab") {
        qDebug() << "Tab";
        m_replaceLine->lineEdit()->setFocus();
    } else {
        DFloatingWidget::keyPressEvent(e);
    }
    if (key == "Enter") {
        qDebug() << "Enter";
        if (m_replaceAllButton->hasFocus()) {
            qDebug() << "replaceAllButton";
            m_replaceAllButton->click();
        }
        if (m_replaceButton->hasFocus()) {
            qDebug() << "replaceButton";
            m_replaceButton->click();
        }
        if (m_replaceRestButton->hasFocus()) {
            qDebug() << "replaceRestButton";
            m_replaceRestButton->click();
        }
        if (m_replaceSkipButton->hasFocus()) {
            qDebug() << "replaceSkipButton";
            m_replaceSkipButton->click();
        }
    }
    qDebug() << "keyPressEvent end";
}

/**
   @brief 根据界面布局模式 `DGuiApplicationHelper::isCompactMode()` 切换当前界面布局参数。
        需要注意，界面参数同设计图参数并非完全一致，而是按照实际的显示像素值进行比对。
 */
void ReplaceBar::updateSizeMode()
{
    qDebug() << "updateSizeMode";
#ifdef DTKWIDGET_CLASS_DSizeMode
    bool isCompact = DGuiApplicationHelper::isCompactMode();
    qDebug() << "Updating size mode, compact:" << isCompact;
    
    if (isCompact) {
        qDebug() << "isCompact";
        setFixedHeight(s_RBHeight_Compact);
        m_closeButton->setFixedSize(s_RBCloseBtnSizeCompact, s_RBCloseBtnSizeCompact);
        m_closeButton->setIconSize(QSize(s_RBCloseIconSizeCompact, s_RBCloseIconSizeCompact));

        m_layout->setContentsMargins(s_RBContetsMarginsCompact);
        m_layout->invalidate();
    } else {
        qDebug() << "is not compact";
        setFixedHeight(s_RBHeight);
        m_closeButton->setFixedSize(s_RBCloseBtnSize, s_RBCloseBtnSize);
        m_closeButton->setIconSize(QSize(s_RBCloseBtnSize, s_RBCloseBtnSize));

        m_layout->setContentsMargins(s_RBContetsMargins);
        m_layout->invalidate();
    }
#endif
    qDebug() << "updateSizeMode end";
}

/**
   @brief 单独处理输入框 \a bar，由于Qt坐标机制，奇数处理存在累计误差，
    导致显示效果和预期存在差异，添加 spacer 强制输入框居中，组合完成返回布局。
 */
QVBoxLayout *ReplaceBar::createVerticalLine(QWidget *content) const
{
    qDebug() << "createVerticalLine";
    QVBoxLayout *lineBarLayout = new QVBoxLayout();
    lineBarLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
    lineBarLayout->addWidget(content);
    lineBarLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
    qDebug() << "createVerticalLine end";
    return lineBarLayout;
}

void ReplaceBar::setMismatchAlert(bool isAlert)
{
    qDebug() << "setMismatchAlert";
    m_replaceLine->setAlert(isAlert);
}

void ReplaceBar::setsearched(bool _)
{
    qDebug() << "setsearched";
    searched = _;
}

void ReplaceBar::change()
{
    qDebug() << "change";
    searched = false;
}
