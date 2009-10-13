/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "EventSenderQt.h"

//#include <QtDebug>
EventSender::EventSender(QWebPage* parent)
    : QObject(parent)
{
    m_page = parent;
}

void EventSender::mouseDown(int button)
{
    Qt::MouseButton mouseButton;
    switch (button) {
    case 0:
        mouseButton = Qt::LeftButton;
        break;
    case 1:
        mouseButton = Qt::MidButton;
        break;
    case 2:
        mouseButton = Qt::RightButton;
        break;
    case 3:
        // fast/events/mouse-click-events expects the 4th button to be treated as the middle button
        mouseButton = Qt::MidButton;
        break;
    default:
        mouseButton = Qt::LeftButton;
        break;
    }

//     qDebug() << "EventSender::mouseDown" << frame;
    QMouseEvent event(QEvent::MouseButtonPress, m_mousePos, mouseButton, mouseButton, Qt::NoModifier);
    QApplication::sendEvent(m_page, &event);
}

void EventSender::mouseUp(int button)
{
    Qt::MouseButton mouseButton;
    switch (button) {
    case 0:
        mouseButton = Qt::LeftButton;
        break;
    case 1:
        mouseButton = Qt::MidButton;
        break;
    case 2:
        mouseButton = Qt::RightButton;
        break;
    case 3:
        // fast/events/mouse-click-events expects the 4th button to be treated as the middle button
        mouseButton = Qt::MidButton;
        break;
    default:
        mouseButton = Qt::LeftButton;
        break;
    }

//     qDebug() << "EventSender::mouseUp" << frame;
    QMouseEvent event(QEvent::MouseButtonRelease, m_mousePos, mouseButton, mouseButton, Qt::NoModifier);
    QApplication::sendEvent(m_page, &event);
}

void EventSender::mouseMoveTo(int x, int y)
{
//     qDebug() << "EventSender::mouseMoveTo" << x << y;
    m_mousePos = QPoint(x, y);
    QMouseEvent event(QEvent::MouseMove, m_mousePos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(m_page, &event);
}

void EventSender::leapForward(int ms)
{
    m_timeLeap += ms;
    //qDebug() << "EventSender::leapForward" << ms;
}

void EventSender::keyDown(const QString& string, const QStringList& modifiers)
{
    QString s = string;
    Qt::KeyboardModifiers modifs = 0;
    for (int i = 0; i < modifiers.size(); ++i) {
        const QString& m = modifiers.at(i);
        if (m == "ctrlKey")
            modifs |= Qt::ControlModifier;
        else if (m == "shiftKey")
            modifs |= Qt::ShiftModifier;
        else if (m == "altKey")
            modifs |= Qt::AltModifier;
        else if (m == "metaKey")
            modifs |= Qt::MetaModifier;
    }
    int code = 0;
    if (string.length() == 1) {
        code = string.unicode()->unicode();
        //qDebug() << ">>>>>>>>> keyDown" << code << (char)code;
        // map special keycodes used by the tests to something that works for Qt/X11
        if (code == '\t') {
            code = Qt::Key_Tab;
            if (modifs == Qt::ShiftModifier)
                code = Qt::Key_Backtab;
            s = QString();
        } else if (code == 127) {
            code = Qt::Key_Backspace;
            if (modifs == Qt::AltModifier)
                modifs = Qt::ControlModifier;
            s = QString();
        } else if (code == 'o' && modifs == Qt::ControlModifier) {
            s = QLatin1String("\n");
            code = '\n';
            modifs = 0;
        } else if (code == 'y' && modifs == Qt::ControlModifier) {
            s = QLatin1String("c");
            code = 'c';
        } else if (code == 'k' && modifs == Qt::ControlModifier) {
            s = QLatin1String("x");
            code = 'x';
        } else if (code == 'a' && modifs == Qt::ControlModifier) {
            s = QString();
            code = Qt::Key_Home;
            modifs = 0;
        } else if (code == 0xf702) {
            s = QString();
            code = Qt::Key_Left;
            if (modifs & Qt::MetaModifier) {
                code = Qt::Key_Home;
                modifs &= ~Qt::MetaModifier;
            }
        } else if (code == 0xf703) {
            s = QString();
            code = Qt::Key_Right;
            if (modifs & Qt::MetaModifier) {
                code = Qt::Key_End;
                modifs &= ~Qt::MetaModifier;
            }
        } else if (code == 0xf700) {
            s = QString();
            code = Qt::Key_Up;
            if (modifs & Qt::MetaModifier) {
                code = Qt::Key_PageUp;
                modifs &= ~Qt::MetaModifier;
            }
        } else if (code == 0xf701) {
            s = QString();
            code = Qt::Key_Down;
            if (modifs & Qt::MetaModifier) {
                code = Qt::Key_PageDown;
                modifs &= ~Qt::MetaModifier;
            }
        } else if (code == 'a' && modifs == Qt::ControlModifier) {
            s = QString();
            code = Qt::Key_Home;
            modifs = 0;
        } else
            code = string.unicode()->toUpper().unicode();
    } else {
        //qDebug() << ">>>>>>>>> keyDown" << string;

        if (string.startsWith(QLatin1Char('F')) && string.count() <= 3) {
            s = s.mid(1);
            int functionKey = s.toInt();
            Q_ASSERT(functionKey >= 1 && functionKey <= 35);
            code = Qt::Key_F1 + (functionKey - 1);
        // map special keycode strings used by the tests to something that works for Qt/X11
        } else if (string == QLatin1String("leftArrow")) {
            s = QString();
            code = Qt::Key_Left;
        } else if (string == QLatin1String("rightArrow")) {
            s = QString();
            code = Qt::Key_Right;
        } else if (string == QLatin1String("upArrow")) {
            s = QString();
            code = Qt::Key_Up;
        } else if (string == QLatin1String("downArrow")) {
            s = QString();
            code = Qt::Key_Down;
        } else if (string == QLatin1String("pageUp")) {
            s = QString();
            code = Qt::Key_PageUp;
        } else if (string == QLatin1String("pageDown")) {
            s = QString();
            code = Qt::Key_PageDown;
        } else if (string == QLatin1String("home")) {
            s = QString();
            code = Qt::Key_Home;
        } else if (string == QLatin1String("end")) {
            s = QString();
            code = Qt::Key_End;
        } else if (string == QLatin1String("delete")) {
            s = QString();
            code = Qt::Key_Delete;
        }
    }
    QKeyEvent event(QEvent::KeyPress, code, modifs, s);
    QApplication::sendEvent(m_page, &event);
    QKeyEvent event2(QEvent::KeyRelease, code, modifs, s);
    QApplication::sendEvent(m_page, &event2);
}

void EventSender::contextClick()
{
    QMouseEvent event(QEvent::MouseButtonPress, m_mousePos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QApplication::sendEvent(m_page, &event);
    QMouseEvent event2(QEvent::MouseButtonRelease, m_mousePos, Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QApplication::sendEvent(m_page, &event2);
}

void EventSender::scheduleAsynchronousClick()
{
    QMouseEvent* event = new QMouseEvent(QEvent::MouseButtonPress, m_mousePos, Qt::LeftButton, Qt::RightButton, Qt::NoModifier);
    QApplication::postEvent(m_page, event);
    QMouseEvent* event2 = new QMouseEvent(QEvent::MouseButtonRelease, m_mousePos, Qt::LeftButton, Qt::RightButton, Qt::NoModifier);
    QApplication::postEvent(m_page, event2);
}

QWebFrame* EventSender::frameUnderMouse() const
{
    QWebFrame* frame = m_page->mainFrame();

redo:
    QList<QWebFrame*> children = frame->childFrames();
    for (int i = 0; i < children.size(); ++i) {
        if (children.at(i)->geometry().contains(m_mousePos)) {
            frame = children.at(i);
            goto redo;
        }
    }
    if (frame->geometry().contains(m_mousePos))
        return frame;
    return 0;
}
