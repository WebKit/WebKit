/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EventSenderProxy.h"

#include "PlatformWebView.h"
#include "TestController.h"
#include <QKeyEvent>
#include <WebKit2/WKPagePrivate.h>
#include <WebKit2/WKStringQt.h>

namespace WTR {

#define KEYCODE_DEL         127
#define KEYCODE_BACKSPACE   8
#define KEYCODE_LEFTARROW   0xf702
#define KEYCODE_RIGHTARROW  0xf703
#define KEYCODE_UPARROW     0xf700
#define KEYCODE_DOWNARROW   0xf701

Qt::KeyboardModifiers getModifiers(WKEventModifiers modifiersRef)
{
    Qt::KeyboardModifiers modifiers = 0;

    if (modifiersRef & kWKEventModifiersControlKey)
        modifiers |= Qt::ControlModifier;
    if (modifiersRef & kWKEventModifiersShiftKey)
        modifiers |= Qt::ShiftModifier;
    if (modifiersRef & kWKEventModifiersAltKey)
        modifiers |= Qt::AltModifier;
    if (modifiersRef & kWKEventModifiersMetaKey)
        modifiers |= Qt::MetaModifier;

    return modifiers;
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers modifiersRef, unsigned location, double timestamp)
{
    WKPageSetShouldSendKeyboardEventSynchronously(m_testController->mainWebView()->page(), true);

    const QString key = WKStringCopyQString(keyRef);
    QString keyText = key;

    Qt::KeyboardModifiers modifiers = getModifiers(modifiersRef);

    if (location == 3)
        modifiers |= Qt::KeypadModifier;
    int code = 0;
    if (key.length() == 1) {
        code = key.unicode()->unicode();
        // map special keycodes used by the tests to something that works for Qt/X11
        if (code == '\r') {
            code = Qt::Key_Return;
        } else if (code == '\t') {
            code = Qt::Key_Tab;
            if (modifiers == Qt::ShiftModifier)
                code = Qt::Key_Backtab;
            keyText = QString();
        } else if (code == KEYCODE_DEL || code == KEYCODE_BACKSPACE) {
            code = Qt::Key_Backspace;
            if (modifiers == Qt::AltModifier)
                modifiers = Qt::ControlModifier;
            keyText = QString();
        } else if (code == 'o' && modifiers == Qt::ControlModifier) {
            // Mimic the emacs ctrl-o binding on Mac by inserting a paragraph
            // separator and then putting the cursor back to its original
            // position. Allows us to pass emacs-ctrl-o.html
            keyText = QLatin1String("\n");
            code = '\n';
            modifiers = 0;
            QKeyEvent event(QEvent::KeyPress, code, modifiers, keyText);
            m_testController->mainWebView()->sendEvent(&event);
            QKeyEvent event2(QEvent::KeyRelease, code, modifiers, keyText);
            m_testController->mainWebView()->sendEvent(&event2);
            keyText = QString();
            code = Qt::Key_Left;
        } else if (code == 'y' && modifiers == Qt::ControlModifier) {
            keyText = QLatin1String("c");
            code = 'c';
        } else if (code == 'k' && modifiers == Qt::ControlModifier) {
            keyText = QLatin1String("x");
            code = 'x';
        } else if (code == 'a' && modifiers == Qt::ControlModifier) {
            keyText = QString();
            code = Qt::Key_Home;
            modifiers = 0;
        } else if (code == KEYCODE_LEFTARROW) {
            keyText = QString();
            code = Qt::Key_Left;
            if (modifiers & Qt::MetaModifier) {
                code = Qt::Key_Home;
                modifiers &= ~Qt::MetaModifier;
            }
        } else if (code == KEYCODE_RIGHTARROW) {
            keyText = QString();
            code = Qt::Key_Right;
            if (modifiers & Qt::MetaModifier) {
                code = Qt::Key_End;
                modifiers &= ~Qt::MetaModifier;
            }
        } else if (code == KEYCODE_UPARROW) {
            keyText = QString();
            code = Qt::Key_Up;
            if (modifiers & Qt::MetaModifier) {
                code = Qt::Key_PageUp;
                modifiers &= ~Qt::MetaModifier;
            }
        } else if (code == KEYCODE_DOWNARROW) {
            keyText = QString();
            code = Qt::Key_Down;
            if (modifiers & Qt::MetaModifier) {
                code = Qt::Key_PageDown;
                modifiers &= ~Qt::MetaModifier;
            }
        } else if (code == 'a' && modifiers == Qt::ControlModifier) {
            keyText = QString();
            code = Qt::Key_Home;
            modifiers = 0;
        } else
            code = key.unicode()->toUpper().unicode();
    } else {
        if (key.startsWith(QLatin1Char('F')) && key.count() <= 3) {
            keyText = keyText.mid(1);
            int functionKey = keyText.toInt();
            Q_ASSERT(functionKey >= 1 && functionKey <= 35);
            code = Qt::Key_F1 + (functionKey - 1);
        // map special keycode strings used by the tests to something that works for Qt/X11
        } else if (key == QLatin1String("leftArrow")) {
            keyText = QString();
            code = Qt::Key_Left;
        } else if (key == QLatin1String("rightArrow")) {
            keyText = QString();
            code = Qt::Key_Right;
        } else if (key == QLatin1String("upArrow")) {
            keyText = QString();
            code = Qt::Key_Up;
        } else if (key == QLatin1String("downArrow")) {
            keyText = QString();
            code = Qt::Key_Down;
        } else if (key == QLatin1String("pageUp")) {
            keyText = QString();
            code = Qt::Key_PageUp;
        } else if (key == QLatin1String("pageDown")) {
            keyText = QString();
            code = Qt::Key_PageDown;
        } else if (key == QLatin1String("home")) {
            keyText = QString();
            code = Qt::Key_Home;
        } else if (key == QLatin1String("end")) {
            keyText = QString();
            code = Qt::Key_End;
        } else if (key == QLatin1String("insert")) {
            keyText = QString();
            code = Qt::Key_Insert;
        } else if (key == QLatin1String("delete")) {
            keyText = QString();
            code = Qt::Key_Delete;
        } else if (key == QLatin1String("printScreen")) {
            keyText = QString();
            code = Qt::Key_Print;
        } else if (key == QLatin1String("menu")) {
            keyText = QString();
            code = Qt::Key_Menu;
        }
    }
    QKeyEvent event(QEvent::KeyPress, code, modifiers, keyText);
    m_testController->mainWebView()->sendEvent(&event);
    QKeyEvent event2(QEvent::KeyRelease, code, modifiers, keyText);
    m_testController->mainWebView()->sendEvent(&event2);

    WKPageSetShouldSendKeyboardEventSynchronously(m_testController->mainWebView()->page(), false);
}

} // namespace WTR
