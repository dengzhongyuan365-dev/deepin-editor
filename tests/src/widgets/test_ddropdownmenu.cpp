/*
* Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
*
* Author:      Xiao Zhiguo <xiaozhiguo@uniontech.com>
* Maintainer:  Xiao Zhiguo <xiaozhiguo@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "test_ddropdownmenu.h"

#include <QKeyEvent>
#include <QFlags>
#include <QSignalSpy>
#include <QColor>

// 测试函数 DDropdownMenu::setFontEx
TEST_F(test_ddropdownmenu, setFontEx)
{
    do {
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->setFontEx(QFont());
        EXPECT_EQ(dropMenu->m_font, QFont());
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::setCurrentAction
TEST_F(test_ddropdownmenu, setCurrentAction)
{
    do {
        // 测试场景1: 非空指针
        DDropdownMenu *dropMenu = new DDropdownMenu();
        QAction *action = new QAction();
        dropMenu->setCurrentAction(action);
        delete action;
        delete dropMenu;
    } while (false);

    do {
        DDropdownMenu *dropMenu = new DDropdownMenu();
        // 测试场景2: 空指针
        dropMenu->setCurrentAction(nullptr);
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::setCurrentTextOnly
TEST_F(test_ddropdownmenu, setCurrentTextOnly)
{
    do {
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->setCurrentTextOnly("test");
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::setText
TEST_F(test_ddropdownmenu, setText)
{
    do {
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->setText("test");
        EXPECT_EQ(dropMenu->m_text, "test");
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::setTheme
TEST_F(test_ddropdownmenu, setTheme)
{
    do {
        // 场景1: 正确主题
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->setTheme("dark");
        delete dropMenu;
    } while (false);

    do {
        // 场景2: 错误主题
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->setTheme("error");
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::setChildrenFocus
TEST_F(test_ddropdownmenu, setChildrenFocus)
{
    do {
        // 场景1: true
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->setChildrenFocus(true);
        QFlags<Qt::FocusPolicy> flags = QFlags<Qt::FocusPolicy>(dropMenu->m_pToolButton->focusPolicy());
        EXPECT_EQ(flags.testFlag(Qt::StrongFocus), true);
        delete dropMenu;
    } while (false);

    do {
        // 场景2:false
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->setChildrenFocus(false);
        QFlags<Qt::FocusPolicy> flags = QFlags<Qt::FocusPolicy>(dropMenu->m_pToolButton->focusPolicy());
        EXPECT_EQ(flags.testFlag(Qt::NoFocus), true);
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::getButton
TEST_F(test_ddropdownmenu, getButton)
{
    do {
        DDropdownMenu *dropMenu = new DDropdownMenu();
        DToolButton *button = dropMenu->getButton();
        EXPECT_NE(dropMenu->m_pToolButton, nullptr);
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::createEncodeMenu
TEST_F(test_ddropdownmenu, createEncodeMenu)
{
    do {
        // 场景1: sm_groupEncodeVec为空
        DDropdownMenu::sm_groupEncodeVec.clear();
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->createEncodeMenu();
        EXPECT_GT(DDropdownMenu::sm_groupEncodeVec.size(), 0);
        delete dropMenu;
    } while (false);

    do {
        // 场景1: sm_groupEncodeVec不为空
        DDropdownMenu::sm_groupEncodeVec.clear();
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->createEncodeMenu();
        dropMenu->createEncodeMenu();
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::createHighLightMenu
TEST_F(test_ddropdownmenu, createHighLightMenu)
{
    do {
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->createHighLightMenu();
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::createIcon
TEST_F(test_ddropdownmenu, createIcon)
{
    do {
        // 场景1: m_bPressed为true
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->m_bPressed = true;
        dropMenu->createIcon();
        delete dropMenu;
    } while (false);

    do {
        // 场景1: m_bPressed为false
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->m_bPressed = false;
        dropMenu->createIcon();
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::OnFontChangedSlot
TEST_F(test_ddropdownmenu, OnFontChangedSlot)
{
    do {
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->OnFontChangedSlot(QFont());
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::eventFilter
TEST_F(test_ddropdownmenu, eventFilter)
{
//    do {
//        // 场景1: enter键的keypress事件
//        DDropdownMenu *dropMenu = new DDropdownMenu();
//        QSignalBlocker signalBlock(dropMenu);
//        QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
//        bool result = dropMenu->eventFilter(dropMenu->m_pToolButton, event);
//        EXPECT_TRUE(result);
//        delete event;
//        delete dropMenu;
//    } while (false);

//    do {
//        // 场景2: Key_Left键的keypress事件
//        DDropdownMenu *dropMenu = new DDropdownMenu();
//        QSignalBlocker signalBlock(dropMenu);
//        QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
//        bool result = dropMenu->eventFilter(dropMenu->m_pToolButton, event);
//        EXPECT_FALSE(result);
//        delete event;
//        delete dropMenu;
//    } while (false);

//    do {
//        // 场景3: MouseButtonPress事件
//        DDropdownMenu *dropMenu = new DDropdownMenu();
//        QSignalBlocker signalBlock(dropMenu);
//        QSignalSpy spy(dropMenu, &DDropdownMenu::requestContextMenu);
//        QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonPress, QPointF(),
//                                             Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
//        bool result = dropMenu->eventFilter(dropMenu->m_pToolButton, event);
//        EXPECT_FALSE(result);
//        delete event;
//        delete dropMenu;
//    } while (false);

//    do {
//        // 场景4: LeftButton的MouseButtonRelease事件
//        DDropdownMenu *dropMenu = new DDropdownMenu();
//        QSignalBlocker signalBlock(dropMenu);
//        QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonRelease, QPointF(),
//                                             Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
//        bool result = dropMenu->eventFilter(dropMenu->m_pToolButton, event);
//        EXPECT_TRUE(result);
//        delete event;
//        delete dropMenu;
//    } while (false);

//    do {
//        // 场景5: 非LeftButton的MouseButtonRelease事件
//        DDropdownMenu *dropMenu = new DDropdownMenu();
//        QSignalBlocker signalBlock(dropMenu);
//        QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonRelease, QPointF(),
//                                             Qt::RightButton, Qt::RightButton, Qt::NoModifier);
//        bool result = dropMenu->eventFilter(dropMenu->m_pToolButton, event);
//        EXPECT_TRUE(result);
//        delete event;
//        delete dropMenu;
//    } while (false);
}

// 测试函数 DDropdownMenu::setSvgColor
TEST_F(test_ddropdownmenu, setSvgColor)
{
    do {
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->setSvgColor("#FF0000");
        delete dropMenu;
    } while (false);
}

// 测试函数 DDropdownMenu::SetSVGBackColor
TEST_F(test_ddropdownmenu, SetSVGBackColor)
{
    do {
        QByteArray data = "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">"
                          "<text x=\"0\" y=\"15\" fill=\"red\">I love SVG</text>"
                          "</svg>";
        QDomDocument doc;
        doc.setContent(data);
        QDomElement elem = doc.documentElement();
        DDropdownMenu *dropMenu = new DDropdownMenu();
        dropMenu->SetSVGBackColor(elem, "fill", "#FF0000");
        delete dropMenu;
    } while (false);
}
