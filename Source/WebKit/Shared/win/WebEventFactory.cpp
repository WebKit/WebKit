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

#include <WebCore/GDIUtilities.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/Scrollbar.h>
#include <WebCore/WindowsKeyNames.h>
#include <WebCore/WindowsKeyboardCodes.h>
#include <windowsx.h>
#include <wtf/ASCIICType.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>

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

static unsigned verticalScrollLines()
{
    static ULONG scrollLines;
    if (!scrollLines && !::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0))
        scrollLines = 3;
    return scrollLines;
}

static inline int clickCount(WebEventType type, WebMouseEventButton button, const POINT& position, double timeStampSeconds)
{
    static int gLastClickCount;
    static double gLastClickTime;
    static POINT lastClickPosition;
    static WebMouseEventButton lastClickButton = WebMouseEventButton::Left;

    bool cancelPreviousClick = (std::abs(lastClickPosition.x - position.x) > (::GetSystemMetrics(SM_CXDOUBLECLK) / 2))
        || (std::abs(lastClickPosition.y - position.y) > (::GetSystemMetrics(SM_CYDOUBLECLK) / 2))
        || ((timeStampSeconds - gLastClickTime) * 1000.0 > getDoubleClickTime());

    if (type == WebEventType::MouseDown) {
        if (!cancelPreviousClick && (button == lastClickButton))
            ++gLastClickCount;
        else {
            gLastClickCount = 1;
            lastClickPosition = position;
        }
        gLastClickTime = timeStampSeconds;
        lastClickButton = button;
    } else if (type == WebEventType::MouseMove) {
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

static inline OptionSet<WebEventModifier> modifiersForEvent(WPARAM wparam)
{
    OptionSet<WebEventModifier> modifiers;
    if (wparam & MK_CONTROL)
        modifiers.add(WebEventModifier::ControlKey);
    if (wparam & MK_SHIFT)
        modifiers.add(WebEventModifier::ShiftKey);
    if (IsKeyInDownState(VK_MENU))
        modifiers.add(WebEventModifier::AltKey);
    return modifiers;
}

static inline OptionSet<WebEventModifier> modifiersForCurrentKeyState()
{
    OptionSet<WebEventModifier> modifiers;
    if (IsKeyInDownState(VK_CONTROL))
        modifiers.add(WebEventModifier::ControlKey);
    if (IsKeyInDownState(VK_SHIFT))
        modifiers.add(WebEventModifier::ShiftKey);
    if (IsKeyInDownState(VK_MENU))
        modifiers.add(WebEventModifier::AltKey);
    return modifiers;
}

static inline WebEventType keyboardEventTypeForEvent(UINT message)
{
    switch (message) {
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        return WebEventType::RawKeyDown;
        break;
    case WM_SYSKEYUP:
    case WM_KEYUP:
        return WebEventType::KeyUp;
        break;
    case WM_IME_CHAR:
    case WM_SYSCHAR:
    case WM_CHAR:
        return WebEventType::Char;
        break;
    default:
        ASSERT_NOT_REACHED();
        return WebEventType::Char;
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

static bool isKeypadEvent(WPARAM wParam, LPARAM lParam, WebEventType type)
{
    if (type != WebEventType::RawKeyDown && type != WebEventType::KeyUp)
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

static String textFromEvent(WPARAM wparam, WebEventType type)
{
    if (type != WebEventType::Char)
        return String();

    char16_t c = static_cast<char16_t>(wparam);
    return span(c);
}

static String unmodifiedTextFromEvent(WPARAM wparam, WebEventType type)
{
    if (type != WebEventType::Char)
        return String();

    char16_t c = static_cast<char16_t>(wparam);
    return span(c);
}

static String keyIdentifierFromEvent(WPARAM wparam, WebEventType type)
{
    if (type == WebEventType::Char)
        return String();

    unsigned short keyCode = static_cast<unsigned short>(wparam);
    switch (keyCode) {
    case VK_MENU:
        return "Alt"_s;
    case VK_CONTROL:
        return "Control"_s;
    case VK_SHIFT:
        return "Shift"_s;
    case VK_CAPITAL:
        return "CapsLock"_s;
    case VK_LWIN:
    case VK_RWIN:
        return "Win"_s;
    case VK_CLEAR:
        return "Clear"_s;
    case VK_DOWN:
        return "Down"_s;
    case VK_END:
        return "End"_s;
    case VK_RETURN:
        return "Enter"_s;
    case VK_EXECUTE:
        return "Execute"_s;
    case VK_F1:
        return "F1"_s;
    case VK_F2:
        return "F2"_s;
    case VK_F3:
        return "F3"_s;
    case VK_F4:
        return "F4"_s;
    case VK_F5:
        return "F5"_s;
    case VK_F6:
        return "F6"_s;
    case VK_F7:
        return "F7"_s;
    case VK_F8:
        return "F8"_s;
    case VK_F9:
        return "F9"_s;
    case VK_F10:
        return "F11"_s;
    case VK_F12:
        return "F12"_s;
    case VK_F13:
        return "F13"_s;
    case VK_F14:
        return "F14"_s;
    case VK_F15:
        return "F15"_s;
    case VK_F16:
        return "F16"_s;
    case VK_F17:
        return "F17"_s;
    case VK_F18:
        return "F18"_s;
    case VK_F19:
        return "F19"_s;
    case VK_F20:
        return "F20"_s;
    case VK_F21:
        return "F21"_s;
    case VK_F22:
        return "F22"_s;
    case VK_F23:
        return "F23"_s;
    case VK_F24:
        return "F24"_s;
    case VK_HELP:
        return "Help"_s;
    case VK_HOME:
        return "Home"_s;
    case VK_INSERT:
        return "Insert"_s;
    case VK_LEFT:
        return "Left"_s;
    case VK_NEXT:
        return "PageDown"_s;
    case VK_PRIOR:
        return "PageUp"_s;
    case VK_PAUSE:
        return "Pause"_s;
    case VK_SNAPSHOT:
        return "PrintScreen"_s;
    case VK_RIGHT:
        return "Right"_s;
    case VK_SCROLL:
        return "Scroll"_s;
    case VK_SELECT:
        return "Select"_s;
    case VK_UP:
        return "Up"_s;
    case VK_DELETE:
        return "U+007F"_s; // Standard says that DEL becomes U+007F.
    default:
        return makeString("U+"_s, hex(toASCIIUpper(keyCode), 4));
    }
}

WebMouseEvent WebEventFactory::createWebMouseEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool didActivateWebView, float deviceScaleFactor)
{
    WebEventType type;
    WebMouseEventButton button = WebMouseEventButton::None;
    switch (message) {
    case WM_MOUSEMOVE:
        type = WebEventType::MouseMove;
        if (wParam & MK_LBUTTON)
            button = WebMouseEventButton::Left;
        else if (wParam & MK_MBUTTON)
            button = WebMouseEventButton::Middle;
        else if (wParam & MK_RBUTTON)
            button = WebMouseEventButton::Right;
        break;
    case WM_MOUSELEAVE:
        type = WebEventType::MouseMove;
        if (wParam & MK_LBUTTON)
            button = WebMouseEventButton::Left;
        else if (wParam & MK_MBUTTON)
            button = WebMouseEventButton::Middle;
        else if (wParam & MK_RBUTTON)
            button = WebMouseEventButton::Right;

        // Set the current mouse position (relative to the client area of the
        // current window) since none is specified for this event.
        lParam = relativeCursorPosition(hWnd);
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        type = WebEventType::MouseDown;
        button = WebMouseEventButton::Left;
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        type = WebEventType::MouseDown;
        button = WebMouseEventButton::Middle;
        break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        type = WebEventType::MouseDown;
        button = WebMouseEventButton::Right;
        break;
    case WM_LBUTTONUP:
        type = WebEventType::MouseUp;
        button = WebMouseEventButton::Left;
        break;
    case WM_MBUTTONUP:
        type = WebEventType::MouseUp;
        button = WebMouseEventButton::Middle;
        break;
    case WM_RBUTTONUP:
        type = WebEventType::MouseUp;
        button = WebMouseEventButton::Right;
        break;
    default:
        ASSERT_NOT_REACHED();
        type = WebEventType::KeyDown;
    }

    POINT positionPoint = point(lParam);
    POINT globalPositionPoint = positionPoint;
    ::ClientToScreen(hWnd, &globalPositionPoint);
    FloatPoint globalPosition(globalPositionPoint);
    globalPosition.scale(1 / deviceScaleFactor);
    FloatPoint position = positionPoint;
    position.scale(1 / deviceScaleFactor);

    double timestamp = ::GetTickCount() * 0.001; // ::GetTickCount returns milliseconds (Chrome uses GetMessageTime() / 1000.0)

    int clickCount = WebKit::clickCount(type, button, positionPoint, timestamp);
    auto modifiers = modifiersForEvent(wParam);
    auto buttons = buttonsForEvent(wParam);

    return WebMouseEvent( { type, modifiers, WallTime::now() }, button, buttons, flooredIntPoint(position), flooredIntPoint(globalPosition), 0, 0, 0, clickCount, didActivateWebView);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, float deviceScaleFactor)
{
    POINT positionPoint = point(lParam);
    FloatPoint globalPosition = positionPoint;
    ::ScreenToClient(hWnd, &positionPoint);
    globalPosition.scale(1 / deviceScaleFactor);
    FloatPoint position = positionPoint;
    position.scale(1 / deviceScaleFactor);

    WebWheelEvent::Granularity granularity = WebWheelEvent::ScrollByPixelWheelEvent;

    auto modifiers = modifiersForEvent(wParam);

    float deltaX = 0;
    float deltaY = 0;
    float wheelTicksX = 0;
    float wheelTicksY = 0;

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
    if (isMouseHWheel || (modifiers & WebEventModifier::ShiftKey)) {
        deltaX = delta * static_cast<float>(horizontalScrollChars()) * WebCore::cScrollbarPixelsPerLine;
        deltaY = 0;
        granularity = WebWheelEvent::ScrollByPixelWheelEvent;
    } else {
        deltaX = 0;
        deltaY = delta;
        unsigned verticalMultiplier = verticalScrollLines();
        if (verticalMultiplier == WHEEL_PAGESCROLL)
            granularity = WebWheelEvent::ScrollByPageWheelEvent;
        else {
            granularity = WebWheelEvent::ScrollByPixelWheelEvent;
            deltaY *= static_cast<float>(verticalMultiplier) * WebCore::cScrollbarPixelsPerLine;
        }
    }

    return WebWheelEvent( { WebEventType::Wheel, modifiers, WallTime::now() }, flooredIntPoint(position), flooredIntPoint(globalPosition), FloatSize(deltaX, deltaY), FloatSize(wheelTicksX, wheelTicksY), granularity);
}

static WindowsKeyNames& windowsKeyNames()
{
    static NeverDestroyed<WindowsKeyNames> keyNames;
    return keyNames;
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto type = keyboardEventTypeForEvent(message);
    String text = textFromEvent(wparam, type);
    String unmodifiedText = unmodifiedTextFromEvent(wparam, type);
    String key = message == WM_CHAR ? windowsKeyNames().domKeyFromChar(wparam) : windowsKeyNames().domKeyFromParams(wparam, lparam);
    String code = windowsKeyNames().domCodeFromLParam(lparam);
    String keyIdentifier = keyIdentifierFromEvent(wparam, type);
    int windowsVirtualKeyCode = static_cast<int>(wparam);
    int nativeVirtualKeyCode = static_cast<int>(wparam);
    int macCharCode = 0;
    bool autoRepeat = HIWORD(lparam) & KF_REPEAT;
    bool isKeypad = isKeypadEvent(wparam, lparam, type);
    bool isSystemKey = isSystemKeyEvent(message);
    auto modifiers = modifiersForCurrentKeyState();

    return WebKeyboardEvent( { type, modifiers, WallTime::now() }, text, unmodifiedText, key, code, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, macCharCode, autoRepeat, isKeypad, isSystemKey);
}

#if ENABLE(TOUCH_EVENTS)
WebTouchEvent WebEventFactory::createWebTouchEvent()
{
    return WebTouchEvent({ WebEventType::TouchMove, OptionSet<WebEventModifier> { }, WallTime::now() }, { }, { }, { });
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebKit
