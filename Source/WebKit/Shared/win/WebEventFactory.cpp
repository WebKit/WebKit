/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebEventFactory.h"

#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/Scrollbar.h>
#include <WebCore/WindowsKeyboardCodes.h>
#include <windowsx.h>
#include <wtf/ASCIICType.h>

namespace WebKit {

using namespace WebCore;

static inline LPARAM relativeCursorPosition(HWND hwnd)
{
    POINT point = { -1, -1 };
    ::GetCursorPos(&point);
    ::ScreenToClient(hwnd, &point);
    return MAKELPARAM(point.x, point.y);
}

static inline POINT point(LPARAM lParam)
{
    POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    return point;
}

static int horizontalScrollChars()
{
    static ULONG scrollChars;
    if (!scrollChars && !::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scrollChars, 0))
        scrollChars = 1;
    return scrollChars;
}

static int verticalScrollLines()
{
    static ULONG scrollLines;
    if (!scrollLines && !::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0))
        scrollLines = 3;
    return scrollLines;
}

static inline int clickCount(WebEvent::Type type, WebMouseEvent::Button button, const POINT& position, double timeStampSeconds)
{
    static int gLastClickCount;
    static double gLastClickTime;
    static POINT lastClickPosition;
    static WebMouseEvent::Button lastClickButton = WebMouseEvent::LeftButton;

    bool cancelPreviousClick = (std::abs(lastClickPosition.x - position.x) > (::GetSystemMetrics(SM_CXDOUBLECLK) / 2))
        || (std::abs(lastClickPosition.y - position.y) > (::GetSystemMetrics(SM_CYDOUBLECLK) / 2))
        || ((timeStampSeconds - gLastClickTime) * 1000.0 > ::GetDoubleClickTime());

    if (type == WebEvent::MouseDown) {
        if (!cancelPreviousClick && (button == lastClickButton))
            ++gLastClickCount;
        else {
            gLastClickCount = 1;
            lastClickPosition = position;
        }
        gLastClickTime = timeStampSeconds;
        lastClickButton = button;
    } else if (type == WebEvent::MouseMove) {
        if (cancelPreviousClick) {
            gLastClickCount = 0;
            lastClickPosition.x = 0;
            lastClickPosition.y = 0;
            gLastClickTime = 0;
        }
    }

    return gLastClickCount;
}

static inline bool IsKeyInDownState(int vk)
{
    return ::GetKeyState(vk) & 0x8000;
}

static inline OptionSet<WebEvent::Modifier> modifiersForEvent(WPARAM wparam)
{
    OptionSet<WebEvent::Modifier> modifiers;
    if (wparam & MK_CONTROL)
        modifiers.add(WebEvent::Modifier::ControlKey);
    if (wparam & MK_SHIFT)
        modifiers.add(WebEvent::Modifier::ShiftKey);
    if (IsKeyInDownState(VK_MENU))
        modifiers.add(WebEvent::Modifier::AltKey);
    return modifiers;
}

static inline OptionSet<WebEvent::Modifier> modifiersForCurrentKeyState()
{
    OptionSet<WebEvent::Modifier> modifiers;
    if (IsKeyInDownState(VK_CONTROL))
        modifiers.add(WebEvent::Modifier::ControlKey);
    if (IsKeyInDownState(VK_SHIFT))
        modifiers.add(WebEvent::Modifier::ShiftKey);
    if (IsKeyInDownState(VK_MENU))
        modifiers.add(WebEvent::Modifier::AltKey);
    return modifiers;
}

static inline WebEvent::Type keyboardEventTypeForEvent(UINT message)
{
    switch (message) {
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        return WebEvent::RawKeyDown;
        break;
    case WM_SYSKEYUP:
    case WM_KEYUP:
        return WebEvent::KeyUp;
        break;
    case WM_IME_CHAR:
    case WM_SYSCHAR:
    case WM_CHAR:
        return WebEvent::Char;
        break;
    default:
        ASSERT_NOT_REACHED();
        return WebEvent::Char;
    }
}

static inline bool isSystemKeyEvent(UINT message)
{
    switch (message) {
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_SYSCHAR:
        return true;
    default:
        return false;
    }
}

static bool isKeypadEvent(WPARAM wParam, LPARAM lParam, WebEvent::Type type)
{
    if (type != WebEvent::RawKeyDown && type != WebEvent::KeyUp)
        return false;

    switch (wParam) {
    case VK_NUMLOCK:
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
    case VK_MULTIPLY:
    case VK_ADD:
    case VK_SEPARATOR:
    case VK_SUBTRACT:
    case VK_DECIMAL:
    case VK_DIVIDE:
        return true;
    case VK_RETURN:
        return HIWORD(lParam) & KF_EXTENDED;
    case VK_INSERT:
    case VK_DELETE:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_END:
    case VK_HOME:
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
        return !(HIWORD(lParam) & KF_EXTENDED);
    default:
        return false;
    }
}

static String textFromEvent(WPARAM wparam, WebEvent::Type type)
{
    if (type != WebEvent::Char)
        return String();

    UChar c = static_cast<UChar>(wparam);
    return String(&c, 1);
}

static String unmodifiedTextFromEvent(WPARAM wparam, WebEvent::Type type)
{
    if (type != WebEvent::Char)
        return String();

    UChar c = static_cast<UChar>(wparam);
    return String(&c, 1);
}

static String keyIdentifierFromEvent(WPARAM wparam, WebEvent::Type type)
{
    if (type == WebEvent::Char)
        return String();

    unsigned short keyCode = static_cast<unsigned short>(wparam);
    switch (keyCode) {
    case VK_MENU:
        return String("Alt");
    case VK_CONTROL:
        return String("Control");
    case VK_SHIFT:
        return String("Shift");
    case VK_CAPITAL:
        return String("CapsLock");
    case VK_LWIN:
    case VK_RWIN:
        return String("Win");
    case VK_CLEAR:
        return String("Clear");
    case VK_DOWN:
        return String("Down");
    case VK_END:
        return String("End");
    case VK_RETURN:
        return String("Enter");
    case VK_EXECUTE:
        return String("Execute");
    case VK_F1:
        return String("F1");
    case VK_F2:
        return String("F2");
    case VK_F3:
        return String("F3");
    case VK_F4:
        return String("F4");
    case VK_F5:
        return String("F5");
    case VK_F6:
        return String("F6");
    case VK_F7:
        return String("F7");
    case VK_F8:
        return String("F8");
    case VK_F9:
        return String("F9");
    case VK_F10:
        return String("F11");
    case VK_F12:
        return String("F12");
    case VK_F13:
        return String("F13");
    case VK_F14:
        return String("F14");
    case VK_F15:
        return String("F15");
    case VK_F16:
        return String("F16");
    case VK_F17:
        return String("F17");
    case VK_F18:
        return String("F18");
    case VK_F19:
        return String("F19");
    case VK_F20:
        return String("F20");
    case VK_F21:
        return String("F21");
    case VK_F22:
        return String("F22");
    case VK_F23:
        return String("F23");
    case VK_F24:
        return String("F24");
    case VK_HELP:
        return String("Help");
    case VK_HOME:
        return String("Home");
    case VK_INSERT:
        return String("Insert");
    case VK_LEFT:
        return String("Left");
    case VK_NEXT:
        return String("PageDown");
    case VK_PRIOR:
        return String("PageUp");
    case VK_PAUSE:
        return String("Pause");
    case VK_SNAPSHOT:
        return String("PrintScreen");
    case VK_RIGHT:
        return String("Right");
    case VK_SCROLL:
        return String("Scroll");
    case VK_SELECT:
        return String("Select");
    case VK_UP:
        return String("Up");
    case VK_DELETE:
        return String("U+007F"); // Standard says that DEL becomes U+007F.
    default:
        return String::format("U+%04X", toASCIIUpper(keyCode));
    }
}

WebMouseEvent WebEventFactory::createWebMouseEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool didActivateWebView)
{
    WebEvent::Type type;
    WebMouseEvent::Button button = WebMouseEvent::NoButton;
    switch (message) {
    case WM_MOUSEMOVE:
        type = WebEvent::MouseMove;
        if (wParam & MK_LBUTTON)
            button = WebMouseEvent::LeftButton;
        else if (wParam & MK_MBUTTON)
            button = WebMouseEvent::MiddleButton;
        else if (wParam & MK_RBUTTON)
            button = WebMouseEvent::RightButton;
        break;
    case WM_MOUSELEAVE:
        type = WebEvent::MouseMove;
        if (wParam & MK_LBUTTON)
            button = WebMouseEvent::LeftButton;
        else if (wParam & MK_MBUTTON)
            button = WebMouseEvent::MiddleButton;
        else if (wParam & MK_RBUTTON)
            button = WebMouseEvent::RightButton;

        // Set the current mouse position (relative to the client area of the
        // current window) since none is specified for this event.
        lParam = relativeCursorPosition(hWnd);
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        type = WebEvent::MouseDown;
        button = WebMouseEvent::LeftButton;
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        type = WebEvent::MouseDown;
        button = WebMouseEvent::MiddleButton;
        break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        type = WebEvent::MouseDown;
        button = WebMouseEvent::RightButton;
        break;
    case WM_LBUTTONUP:
        type = WebEvent::MouseUp;
        button = WebMouseEvent::LeftButton;
        break;
    case WM_MBUTTONUP:
        type = WebEvent::MouseUp;
        button = WebMouseEvent::MiddleButton;
        break;
    case WM_RBUTTONUP:
        type = WebEvent::MouseUp;
        button = WebMouseEvent::RightButton;
        break;
    default:
        ASSERT_NOT_REACHED();
        type = WebEvent::KeyDown;
    }

    POINT position = point(lParam);
    POINT globalPosition = position;
    ::ClientToScreen(hWnd, &globalPosition);

    double timestamp = ::GetTickCount() * 0.001; // ::GetTickCount returns milliseconds (Chrome uses GetMessageTime() / 1000.0)

    int clickCount = WebKit::clickCount(type, button, position, timestamp);
    auto modifiers = modifiersForEvent(wParam);

    return WebMouseEvent(type, button, 0, position, globalPosition, 0, 0, 0, clickCount, modifiers, WallTime::now(), didActivateWebView);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    POINT globalPosition = point(lParam);
    POINT position = globalPosition;
    ::ScreenToClient(hWnd, &position);

    WebWheelEvent::Granularity granularity = WebWheelEvent::ScrollByPixelWheelEvent;

    auto modifiers = modifiersForEvent(wParam);

    int deltaX = 0;
    int deltaY = 0;
    int wheelTicksX = 0;
    int wheelTicksY = 0;

    float delta = GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);
    bool isMouseHWheel = (message == WM_MOUSEHWHEEL);
    if (isMouseHWheel) {
        wheelTicksX = delta;
        wheelTicksY = 0;
        delta = -delta;
    } else {
        wheelTicksX = 0;
        wheelTicksY = delta;
    }
    if (isMouseHWheel || (modifiers & WebEvent::Modifier::ShiftKey)) {
        deltaX = delta * static_cast<float>(horizontalScrollChars()) * WebCore::cScrollbarPixelsPerLine;
        deltaY = 0;
        granularity = WebWheelEvent::ScrollByPixelWheelEvent;
    } else {
        deltaX = 0;
        deltaY = delta;
        int verticalMultiplier = verticalScrollLines();
        if (verticalMultiplier == WHEEL_PAGESCROLL)
            granularity = WebWheelEvent::ScrollByPageWheelEvent;
        else {
            granularity = WebWheelEvent::ScrollByPixelWheelEvent;
            deltaY *= static_cast<float>(verticalMultiplier) * WebCore::cScrollbarPixelsPerLine;
        }
    }

    return WebWheelEvent(WebEvent::Wheel, position, globalPosition, FloatSize(deltaX, deltaY), FloatSize(wheelTicksX, wheelTicksY), granularity, modifiers, WallTime::now());
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    WebEvent::Type type = keyboardEventTypeForEvent(message);
    String text = textFromEvent(wparam, type);
    String unmodifiedText = unmodifiedTextFromEvent(wparam, type);
    String keyIdentifier = keyIdentifierFromEvent(wparam, type);
    int windowsVirtualKeyCode = static_cast<int>(wparam);
    int nativeVirtualKeyCode = static_cast<int>(wparam);
    int macCharCode = 0;
    bool autoRepeat = HIWORD(lparam) & KF_REPEAT;
    bool isKeypad = isKeypadEvent(wparam, lparam, type);
    bool isSystemKey = isSystemKeyEvent(message);
    auto modifiers = modifiersForCurrentKeyState();

    return WebKeyboardEvent(type, text, unmodifiedText, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, macCharCode, autoRepeat, isKeypad, isSystemKey, modifiers, WallTime::now());
}

#if ENABLE(TOUCH_EVENTS)
WebTouchEvent WebEventFactory::createWebTouchEvent(const GdkEvent* event, Vector<WebPlatformTouchPoint>&& touchPoints)
{
    return WebTouchEvent();
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebKit
