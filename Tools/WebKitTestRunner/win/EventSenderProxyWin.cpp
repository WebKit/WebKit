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
    msg.time = GetTickCount();
    msg.pt = positionInPoint();

    TranslateMessage(&msg);
    return DispatchMessage(&msg);
}

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
    , m_leftMouseButtonDown(false)
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickButton(kWKEventMouseButtonNoButton)
{
}

EventSenderProxy::~EventSenderProxy()
{
}

void EventSenderProxy::mouseDown(unsigned button, WKEventModifiers wkModifiers, WKStringRef pointerType)
{
    int messageType;
    switch (button) {
    case 0:
        messageType = WM_LBUTTONDOWN;
        m_leftMouseButtonDown = true;
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

void EventSenderProxy::mouseUp(unsigned button, WKEventModifiers wkModifiers, WKStringRef pointerType)
{
    int messageType;
    switch (button) {
    case 0:
        messageType = WM_LBUTTONUP;
        m_leftMouseButtonDown = false;
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

void EventSenderProxy::mouseMoveTo(double x, double y, WKStringRef pointerType)
{
    m_position.x = x;
    m_position.y = y;
    WPARAM wParam = m_leftMouseButtonDown ? MK_LBUTTON : 0;
    dispatchMessage(WM_MOUSEMOVE, wParam, MAKELPARAM(positionInPoint().x, positionInPoint().y));
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
    Sleep(milliseconds);
}

static unsigned makeKeyDataForScanCode(int virtualKeyCode)
{
    unsigned scancode = MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_VSC);
    int keyData = scancode & 0xFF;

    bool forceExtended = false;
    switch (virtualKeyCode) {
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_END:
    case VK_HOME:
    case VK_INSERT:
    case VK_DELETE:
    case VK_DIVIDE:
    case VK_NUMLOCK:
    case VK_RCONTROL:
    case VK_RMENU:
        // Some keys need to turn on KF_EXTENDED explicitly
        forceExtended = true;
        break;
    default:
        break;
    }

    scancode = scancode >> 8;
    if (scancode == 0xe0 || scancode == 0xe1 || forceExtended)
        keyData += KF_EXTENDED;
    unsigned repeat = 1;
    return keyData << 16 | repeat;
}

static void pumpMessageQueue()
{
    MSG msg;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    int virtualKeyCode = 0;
    int charCode = 0;

    bool needsShiftKeyModifier = false;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
        virtualKeyCode = VK_LEFT;
    else if (WKStringIsEqualToUTF8CString(keyRef, "rightArrow"))
        virtualKeyCode = VK_RIGHT;
    else if (WKStringIsEqualToUTF8CString(keyRef, "upArrow"))
        virtualKeyCode = VK_UP;
    else if (WKStringIsEqualToUTF8CString(keyRef, "downArrow"))
        virtualKeyCode = VK_DOWN;
    else if (WKStringIsEqualToUTF8CString(keyRef, "pageUp"))
        virtualKeyCode = VK_PRIOR;
    else if (WKStringIsEqualToUTF8CString(keyRef, "pageDown"))
        virtualKeyCode = VK_NEXT;
    else if (WKStringIsEqualToUTF8CString(keyRef, "home"))
        virtualKeyCode = VK_HOME;
    else if (WKStringIsEqualToUTF8CString(keyRef, "end"))
        virtualKeyCode = VK_END;
    else if (WKStringIsEqualToUTF8CString(keyRef, "insert"))
        virtualKeyCode = VK_INSERT;
    else if (WKStringIsEqualToUTF8CString(keyRef, "delete"))
        virtualKeyCode = VK_DELETE;
    else if (WKStringIsEqualToUTF8CString(keyRef, "printScreen"))
        virtualKeyCode = VK_SNAPSHOT;
    else if (WKStringIsEqualToUTF8CString(keyRef, "menu"))
        virtualKeyCode = VK_APPS;
    else if (WKStringIsEqualToUTF8CString(keyRef, "leftControl"))
        virtualKeyCode = VK_LCONTROL;
    else if (WKStringIsEqualToUTF8CString(keyRef, "leftShift"))
        virtualKeyCode = VK_LSHIFT;
    else if (WKStringIsEqualToUTF8CString(keyRef, "leftAlt"))
        virtualKeyCode = VK_LMENU;
    else if (WKStringIsEqualToUTF8CString(keyRef, "rightControl"))
        virtualKeyCode = VK_RCONTROL;
    else if (WKStringIsEqualToUTF8CString(keyRef, "rightShift"))
        virtualKeyCode = VK_RSHIFT;
    else if (WKStringIsEqualToUTF8CString(keyRef, "rightAlt"))
        virtualKeyCode = VK_RMENU;
    else {
        size_t keyLength = WKStringGetLength(keyRef);
        static const char shiftedUSCharacters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ~!@#$%^&*()_+{}|:\"<>?";
        wchar_t keyStr[3];
        WKStringGetCharacters(keyRef, keyStr, _countof(keyStr));
        if (keyLength == 1) {
            charCode = keyStr[0];
            virtualKeyCode = LOBYTE(VkKeyScan(charCode));
            if (strchr(shiftedUSCharacters, charCode))
                needsShiftKeyModifier = true;
        } else if (keyStr[0] == 'F') {
            if (keyLength == 2 && isASCIIDigit(keyStr[1]))
                virtualKeyCode = VK_F1 + keyStr[1] - '1';
            else if (keyLength == 3 && keyStr[1] == '1' && isASCIIDigit(keyStr[2]))
                virtualKeyCode = VK_F10 + keyStr[2] - '0';
        }
    }

    unsigned keyData = makeKeyDataForScanCode(virtualKeyCode);

    switch (virtualKeyCode) {
    case VK_LCONTROL:
    case VK_RCONTROL:
        virtualKeyCode = VK_CONTROL;
        break;
    case VK_LSHIFT:
    case VK_RSHIFT:
        virtualKeyCode = VK_SHIFT;
        break;
    case VK_LMENU:
    case VK_RMENU:
        virtualKeyCode = VK_MENU;
        break;
    default:
        break;
    }

    bool isSysKey = false;
    BYTE keyState[256];
    if (wkModifiers || needsShiftKeyModifier) {
        GetKeyboardState(keyState);

        BYTE newKeyState[256];
        memcpy(newKeyState, keyState, sizeof(keyState));

        if (wkModifiers & kWKEventModifiersControlKey)
            newKeyState[VK_CONTROL] = 0x80;
        if (wkModifiers & kWKEventModifiersShiftKey || needsShiftKeyModifier)
            newKeyState[VK_SHIFT] = 0x80;
        if (wkModifiers & kWKEventModifiersAltKey) {
            newKeyState[VK_MENU] = 0x80;
            isSysKey = true;
        }

        SetKeyboardState(newKeyState);
    }

    if (virtualKeyCode != 255)
        dispatchMessage(isSysKey ? WM_SYSKEYDOWN : WM_KEYDOWN, virtualKeyCode, keyData);
    else {
        // For characters that do not exist in the active keyboard layout,
        // ::TranslateMessage will not work, so we post an WM_CHAR event ourselves.
        dispatchMessage(WM_CHAR, charCode, 0);
    }
    pumpMessageQueue();

    keyData |= (KF_UP | KF_REPEAT) << 16;
    dispatchMessage(isSysKey ? WM_SYSKEYUP : WM_KEYUP, virtualKeyCode, keyData);
    pumpMessageQueue();

    if (wkModifiers || needsShiftKeyModifier)
        SetKeyboardState(keyState);
}

void EventSenderProxy::rawKeyDown(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
}

void EventSenderProxy::rawKeyUp(WKStringRef key, WKEventModifiers modifiers, unsigned keyLocation)
{
}

#if ENABLE(TOUCH_EVENTS)
void EventSenderProxy::addTouchPoint(int, int)
{
}

void EventSenderProxy::updateTouchPoint(int, int, int)
{
}

void EventSenderProxy::setTouchModifier(WKEventModifiers, bool)
{
}

void EventSenderProxy::setTouchPointRadius(int, int)
{
}

void EventSenderProxy::touchStart()
{
}

void EventSenderProxy::touchMove()
{
}

void EventSenderProxy::touchEnd()
{
}

void EventSenderProxy::touchCancel()
{
}

void EventSenderProxy::clearTouchPoints()
{
}

void EventSenderProxy::releaseTouchPoint(int)
{
}

void EventSenderProxy::cancelTouchPoint(int)
{
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WTR
