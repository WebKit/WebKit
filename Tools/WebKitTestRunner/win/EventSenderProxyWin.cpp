/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include <WebCore/NotImplemented.h>

namespace WTR {

LRESULT EventSenderProxy::dispatchMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    MSG msg { };
    msg.hwnd = WKViewGetWindow(m_testController->mainWebView()->platformView());
    msg.message = message;
    msg.wParam = wParam;
    msg.lParam = lParam;
    msg.time = GetTickCount() + static_cast<DWORD>(m_time);
    msg.pt = positionInPoint();

    TranslateMessage(&msg);
    return DispatchMessage(&msg);
}

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
    , m_time(0)
    , m_leftMouseButtonDown(false)
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickButton(kWKEventMouseButtonNoButton)
{
}

EventSenderProxy::~EventSenderProxy()
{
}

void EventSenderProxy::mouseDown(unsigned button, WKEventModifiers wkModifiers)
{
    int messageType;
    switch (button) {
    case 0:
        messageType = WM_LBUTTONDOWN;
        break;
    case 1:
        messageType = WM_MBUTTONDOWN;
        break;
    case 2:
        messageType = WM_RBUTTONDOWN;
        break;
    case 3:
        // fast/events/mouse-click-events expects the 4th button has event.button = 1, so send an WM_MBUTTONDOWN
        messageType = WM_MBUTTONDOWN;
        break;
    default:
        messageType = WM_LBUTTONDOWN;
        break;
    }
    WPARAM wparam = 0;
    dispatchMessage(messageType, wparam, MAKELPARAM(positionInPoint().x, positionInPoint().y));
}

void EventSenderProxy::mouseUp(unsigned button, WKEventModifiers wkModifiers)
{
    int messageType;
    switch (button) {
    case 0:
        messageType = WM_LBUTTONUP;
        break;
    case 1:
        messageType = WM_MBUTTONUP;
        break;
    case 2:
        messageType = WM_RBUTTONUP;
        break;
    case 3:
        // fast/events/mouse-click-events expects the 4th button has event.button = 1, so send an WM_MBUTTONUP
        messageType = WM_MBUTTONUP;
        break;
    default:
        messageType = WM_LBUTTONUP;
        break;
    }
    WPARAM wparam = 0;
    dispatchMessage(messageType, wparam, MAKELPARAM(positionInPoint().x, positionInPoint().y));
}

void EventSenderProxy::mouseMoveTo(double x, double y)
{
    m_position.x = x;
    m_position.y = y;
    dispatchMessage(WM_MOUSEMOVE, 0, MAKELPARAM(positionInPoint().x, positionInPoint().y));
}

void EventSenderProxy::mouseScrollBy(int x, int y)
{
    RECT rect;
    GetWindowRect(WKViewGetWindow(m_testController->mainWebView()->platformView()), &rect);

    if (x) {
        UINT scrollChars = 1;
        SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scrollChars, 0);
        x *= WHEEL_DELTA / scrollChars;
        dispatchMessage(WM_MOUSEHWHEEL, MAKEWPARAM(0, x), MAKELPARAM(rect.left + positionInPoint().x, rect.top + positionInPoint().y));
    }

    if (y) {
        UINT scrollLines = 3;
        SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
        y *= WHEEL_DELTA / scrollLines;
        dispatchMessage(WM_MOUSEWHEEL, MAKEWPARAM(0, y), MAKELPARAM(rect.left + positionInPoint().x, rect.top + positionInPoint().y));
    }
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int, int, int, int)
{
    notImplemented();
}

void EventSenderProxy::continuousMouseScrollBy(int, int, bool)
{
}

void EventSenderProxy::leapForward(int milliseconds)
{
    m_time += milliseconds / 1000.0;
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    notImplemented();
}

} // namespace WTR
