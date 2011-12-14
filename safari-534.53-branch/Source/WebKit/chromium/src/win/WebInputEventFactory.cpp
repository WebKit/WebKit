/*
 * Copyright (C) 2006-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebInputEventFactory.h"

#include "WebInputEvent.h"

#include <wtf/Assertions.h>

namespace WebKit {

static const unsigned long defaultScrollLinesPerWheelDelta = 3;
static const unsigned long defaultScrollCharsPerWheelDelta = 1;

// WebKeyboardEvent -----------------------------------------------------------

static bool isKeyPad(WPARAM wparam, LPARAM lparam)
{
    bool keypad = false;
    switch (wparam) {
    case VK_RETURN:
        keypad = (lparam >> 16) & KF_EXTENDED;
        break;
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
        keypad = !((lparam >> 16) & KF_EXTENDED);
        break;
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
    case VK_DIVIDE:
    case VK_MULTIPLY:
    case VK_SUBTRACT:
    case VK_ADD:
    case VK_DECIMAL:
    case VK_CLEAR:
        keypad = true;
        break;
    default:
        keypad = false;
    }
    return keypad;
}

// Loads the state for toggle keys into the event.
static void SetToggleKeyState(WebInputEvent* event)
{
    // Low bit set from GetKeyState indicates "toggled".
    if (::GetKeyState(VK_NUMLOCK) & 1)
        event->modifiers |= WebInputEvent::NumLockOn;
    if (::GetKeyState(VK_CAPITAL) & 1)
        event->modifiers |= WebInputEvent::CapsLockOn;
}

WebKeyboardEvent WebInputEventFactory::keyboardEvent(HWND hwnd, UINT message,
                                                     WPARAM wparam, LPARAM lparam)
{
    WebKeyboardEvent result;

    // TODO(pkasting): http://b/1117926 Are we guaranteed that the message that
    // GetMessageTime() refers to is the same one that we're passed in? Perhaps
    // one of the construction parameters should be the time passed by the
    // caller, who would know for sure.
    result.timeStampSeconds = GetMessageTime() / 1000.0;

    result.windowsKeyCode = result.nativeKeyCode = static_cast<int>(wparam);

    switch (message) {
    case WM_SYSKEYDOWN:
        result.isSystemKey = true;
    case WM_KEYDOWN:
        result.type = WebInputEvent::RawKeyDown;
        break;
    case WM_SYSKEYUP:
        result.isSystemKey = true;
    case WM_KEYUP:
        result.type = WebInputEvent::KeyUp;
        break;
    case WM_IME_CHAR:
        result.type = WebInputEvent::Char;
        break;
    case WM_SYSCHAR:
        result.isSystemKey = true;
        result.type = WebInputEvent::Char;
    case WM_CHAR:
        result.type = WebInputEvent::Char;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (result.type == WebInputEvent::Char || result.type == WebInputEvent::RawKeyDown) {
        result.text[0] = result.windowsKeyCode;
        result.unmodifiedText[0] = result.windowsKeyCode;
    }
    if (result.type != WebInputEvent::Char)
        result.setKeyIdentifierFromWindowsKeyCode();

    if (GetKeyState(VK_SHIFT) & 0x8000)
        result.modifiers |= WebInputEvent::ShiftKey;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        result.modifiers |= WebInputEvent::ControlKey;
    if (GetKeyState(VK_MENU) & 0x8000)
        result.modifiers |= WebInputEvent::AltKey;
    // NOTE: There doesn't seem to be a way to query the mouse button state in
    // this case.

    if (LOWORD(lparam) > 1)
        result.modifiers |= WebInputEvent::IsAutoRepeat;
    if (isKeyPad(wparam, lparam))
        result.modifiers |= WebInputEvent::IsKeyPad;

    SetToggleKeyState(&result);
    return result;
}

// WebMouseEvent --------------------------------------------------------------

static int gLastClickCount;
static double gLastClickTime;

static LPARAM GetRelativeCursorPos(HWND hwnd)
{
    POINT pos = {-1, -1};
    GetCursorPos(&pos);
    ScreenToClient(hwnd, &pos);
    return MAKELPARAM(pos.x, pos.y);
}

void WebInputEventFactory::resetLastClickState()
{
    gLastClickTime = gLastClickCount = 0;
}

WebMouseEvent WebInputEventFactory::mouseEvent(HWND hwnd, UINT message,
                                               WPARAM wparam, LPARAM lparam)
{
    WebMouseEvent result; //(WebInputEvent::Uninitialized());

    switch (message) {
    case WM_MOUSEMOVE:
        result.type = WebInputEvent::MouseMove;
        if (wparam & MK_LBUTTON)
            result.button = WebMouseEvent::ButtonLeft;
        else if (wparam & MK_MBUTTON)
            result.button = WebMouseEvent::ButtonMiddle;
        else if (wparam & MK_RBUTTON)
            result.button = WebMouseEvent::ButtonRight;
        else
            result.button = WebMouseEvent::ButtonNone;
        break;
    case WM_MOUSELEAVE:
        result.type = WebInputEvent::MouseLeave;
        result.button = WebMouseEvent::ButtonNone;
        // set the current mouse position (relative to the client area of the
        // current window) since none is specified for this event
        lparam = GetRelativeCursorPos(hwnd);
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        result.type = WebInputEvent::MouseDown;
        result.button = WebMouseEvent::ButtonLeft;
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        result.type = WebInputEvent::MouseDown;
        result.button = WebMouseEvent::ButtonMiddle;
        break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        result.type = WebInputEvent::MouseDown;
        result.button = WebMouseEvent::ButtonRight;
        break;
    case WM_LBUTTONUP:
        result.type = WebInputEvent::MouseUp;
        result.button = WebMouseEvent::ButtonLeft;
        break;
    case WM_MBUTTONUP:
        result.type = WebInputEvent::MouseUp;
        result.button = WebMouseEvent::ButtonMiddle;
        break;
    case WM_RBUTTONUP:
        result.type = WebInputEvent::MouseUp;
        result.button = WebMouseEvent::ButtonRight;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    // TODO(pkasting): http://b/1117926 Are we guaranteed that the message that
    // GetMessageTime() refers to is the same one that we're passed in? Perhaps
    // one of the construction parameters should be the time passed by the
    // caller, who would know for sure.
    result.timeStampSeconds = GetMessageTime() / 1000.0;

    // set position fields:

    result.x = static_cast<short>(LOWORD(lparam));
    result.y = static_cast<short>(HIWORD(lparam));
    result.windowX = result.x;
    result.windowY = result.y;

    POINT globalPoint = { result.x, result.y };
    ClientToScreen(hwnd, &globalPoint);

    result.globalX = globalPoint.x;
    result.globalY = globalPoint.y;

    // calculate number of clicks:

    // This differs slightly from the WebKit code in WebKit/win/WebView.cpp
    // where their original code looks buggy.
    static int lastClickPositionX;
    static int lastClickPositionY;
    static WebMouseEvent::Button lastClickButton = WebMouseEvent::ButtonLeft;

    double currentTime = result.timeStampSeconds;
    bool cancelPreviousClick =
        (abs(lastClickPositionX - result.x) > (GetSystemMetrics(SM_CXDOUBLECLK) / 2))
        || (abs(lastClickPositionY - result.y) > (GetSystemMetrics(SM_CYDOUBLECLK) / 2))
        || ((currentTime - gLastClickTime) * 1000.0 > GetDoubleClickTime());

    if (result.type == WebInputEvent::MouseDown) {
        if (!cancelPreviousClick && (result.button == lastClickButton))
            ++gLastClickCount;
        else {
            gLastClickCount = 1;
            lastClickPositionX = result.x;
            lastClickPositionY = result.y;
        }
        gLastClickTime = currentTime;
        lastClickButton = result.button;
    } else if (result.type == WebInputEvent::MouseMove
               || result.type == WebInputEvent::MouseLeave) {
        if (cancelPreviousClick) {
            gLastClickCount = 0;
            lastClickPositionX = 0;
            lastClickPositionY = 0;
            gLastClickTime = 0;
        }
    }
    result.clickCount = gLastClickCount;

    // set modifiers:

    if (wparam & MK_CONTROL)
        result.modifiers |= WebInputEvent::ControlKey;
    if (wparam & MK_SHIFT)
        result.modifiers |= WebInputEvent::ShiftKey;
    if (GetKeyState(VK_MENU) & 0x8000)
        result.modifiers |= WebInputEvent::AltKey;
    if (wparam & MK_LBUTTON)
        result.modifiers |= WebInputEvent::LeftButtonDown;
    if (wparam & MK_MBUTTON)
        result.modifiers |= WebInputEvent::MiddleButtonDown;
    if (wparam & MK_RBUTTON)
        result.modifiers |= WebInputEvent::RightButtonDown;

    SetToggleKeyState(&result);
    return result;
}

// WebMouseWheelEvent ---------------------------------------------------------

WebMouseWheelEvent WebInputEventFactory::mouseWheelEvent(HWND hwnd, UINT message,
                                                         WPARAM wparam, LPARAM lparam)
{
    WebMouseWheelEvent result; //(WebInputEvent::Uninitialized());

    result.type = WebInputEvent::MouseWheel;

    // TODO(pkasting): http://b/1117926 Are we guaranteed that the message that
    // GetMessageTime() refers to is the same one that we're passed in? Perhaps
    // one of the construction parameters should be the time passed by the
    // caller, who would know for sure.
    result.timeStampSeconds = GetMessageTime() / 1000.0;

    result.button = WebMouseEvent::ButtonNone;

    // Get key state, coordinates, and wheel delta from event.
    typedef SHORT (WINAPI *GetKeyStateFunction)(int key);
    GetKeyStateFunction getKeyState;
    UINT keyState;
    float wheelDelta;
    bool horizontalScroll = false;
    if ((message == WM_VSCROLL) || (message == WM_HSCROLL)) {
        // Synthesize mousewheel event from a scroll event.  This is needed to
        // simulate middle mouse scrolling in some laptops.  Use GetAsyncKeyState
        // for key state since we are synthesizing the input event.
        getKeyState = GetAsyncKeyState;
        keyState = 0;
        if (getKeyState(VK_SHIFT))
            keyState |= MK_SHIFT;
        if (getKeyState(VK_CONTROL))
            keyState |= MK_CONTROL;
        // NOTE: There doesn't seem to be a way to query the mouse button state
        // in this case.

        POINT cursorPosition = {0};
        GetCursorPos(&cursorPosition);
        result.globalX = cursorPosition.x;
        result.globalY = cursorPosition.y;

        switch (LOWORD(wparam)) {
        case SB_LINEUP:    // == SB_LINELEFT
            wheelDelta = WHEEL_DELTA;
            break;
        case SB_LINEDOWN:  // == SB_LINERIGHT
            wheelDelta = -WHEEL_DELTA;
            break;
        case SB_PAGEUP:
            wheelDelta = 1;
            result.scrollByPage = true;
            break;
        case SB_PAGEDOWN:
            wheelDelta = -1;
            result.scrollByPage = true;
            break;
        default:  // We don't supoprt SB_THUMBPOSITION or SB_THUMBTRACK here.
            wheelDelta = 0;
            break;
        }

        if (message == WM_HSCROLL)
            horizontalScroll = true;
    } else {
        // Non-synthesized event; we can just read data off the event.
        getKeyState = GetKeyState;
        keyState = GET_KEYSTATE_WPARAM(wparam);

        result.globalX = static_cast<short>(LOWORD(lparam));
        result.globalY = static_cast<short>(HIWORD(lparam));

        wheelDelta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam));
        if (message == WM_MOUSEHWHEEL) {
            horizontalScroll = true;
            wheelDelta = -wheelDelta;  // Windows is <- -/+ ->, WebKit <- +/- ->.
        }
    }
    if (keyState & MK_SHIFT)
        horizontalScroll = true;

    // Set modifiers based on key state.
    if (keyState & MK_SHIFT)
        result.modifiers |= WebInputEvent::ShiftKey;
    if (keyState & MK_CONTROL)
        result.modifiers |= WebInputEvent::ControlKey;
    if (getKeyState(VK_MENU) & 0x8000)
        result.modifiers |= WebInputEvent::AltKey;
    if (keyState & MK_LBUTTON)
        result.modifiers |= WebInputEvent::LeftButtonDown;
    if (keyState & MK_MBUTTON)
        result.modifiers |= WebInputEvent::MiddleButtonDown;
    if (keyState & MK_RBUTTON)
        result.modifiers |= WebInputEvent::RightButtonDown;

    SetToggleKeyState(&result);

    // Set coordinates by translating event coordinates from screen to client.
    POINT clientPoint = { result.globalX, result.globalY };
    MapWindowPoints(0, hwnd, &clientPoint, 1);
    result.x = clientPoint.x;
    result.y = clientPoint.y;
    result.windowX = result.x;
    result.windowY = result.y;

    // Convert wheel delta amount to a number of pixels to scroll.
    //
    // How many pixels should we scroll per line?  Gecko uses the height of the
    // current line, which means scroll distance changes as you go through the
    // page or go to different pages.  IE 8 is ~60 px/line, although the value
    // seems to vary slightly by page and zoom level.  Also, IE defaults to
    // smooth scrolling while Firefox doesn't, so it can get away with somewhat
    // larger scroll values without feeling as jerky.  Here we use 100 px per
    // three lines (the default scroll amount is three lines per wheel tick).
    // Even though we have smooth scrolling, we don't make this as large as IE
    // because subjectively IE feels like it scrolls farther than you want while
    // reading articles.
    static const float scrollbarPixelsPerLine = 100.0f / 3.0f;
    wheelDelta /= WHEEL_DELTA;
    float scrollDelta = wheelDelta;
    if (horizontalScroll) {
        unsigned long scrollChars = defaultScrollCharsPerWheelDelta;
        SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scrollChars, 0);
        // TODO(pkasting): Should probably have a different multiplier
        // scrollbarPixelsPerChar here.
        scrollDelta *= static_cast<float>(scrollChars) * scrollbarPixelsPerLine;
    } else {
        unsigned long scrollLines = defaultScrollLinesPerWheelDelta;
        SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
        if (scrollLines == WHEEL_PAGESCROLL)
            result.scrollByPage = true;
        if (!result.scrollByPage)
            scrollDelta *= static_cast<float>(scrollLines) * scrollbarPixelsPerLine;
    }

    // Set scroll amount based on above calculations.  WebKit expects positive
    // deltaY to mean "scroll up" and positive deltaX to mean "scroll left".
    if (horizontalScroll) {
        result.deltaX = scrollDelta;
        result.wheelTicksX = wheelDelta;
    } else {
        result.deltaY = scrollDelta;
        result.wheelTicksY = wheelDelta;
    }

    return result;
}

} // namespace WebKit
