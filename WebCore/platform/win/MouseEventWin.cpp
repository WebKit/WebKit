/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "PlatformMouseEvent.h"

#include <windows.h>

namespace WebCore {

const PlatformMouseEvent::CurrentEventTag PlatformMouseEvent::currentEvent = {};

#define HIGH_BIT_MASK_SHORT 0x8000

static int globalClickCount = 0;
static enum MouseButton globalPrevClickButton = LeftButton;
static DWORD globalPrevClickTime = 0;
static IntPoint globalPrevClickPosition = IntPoint();

static IntPoint positionForEvent(HWND hWnd, LPARAM lParam)
{
    POINT point = {LOWORD(lParam), HIWORD(lParam)};
    return point;
}

static IntPoint globalPositionForEvent(HWND hWnd, LPARAM lParam)
{
    POINT point = {LOWORD(lParam), HIWORD(lParam)};
    ClientToScreen(hWnd, &point);
    return point;
}

PlatformMouseEvent::PlatformMouseEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool activatedWebView)
    : m_position(positionForEvent(hWnd, lParam))
    , m_globalPosition(globalPositionForEvent(hWnd, lParam))
    , m_shiftKey(wParam & MK_SHIFT)
    , m_ctrlKey(wParam & MK_CONTROL)
    , m_altKey(GetKeyState(VK_MENU) & HIGH_BIT_MASK_SHORT)
    , m_metaKey(m_altKey) // FIXME: We'll have to test other browsers
    , m_activatedWebView(activatedWebView)
{
    switch (message) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            m_button = LeftButton;
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
            m_button = RightButton;
            break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
            m_button = MiddleButton;
            break;
        default:
            if (wParam & MK_LBUTTON)
                m_button = LeftButton;
            else if (wParam & MK_RBUTTON)
                m_button = RightButton;
            else if (wParam & MK_MBUTTON)
                m_button = MiddleButton;
            else
                m_button = LeftButton;
            break;
    }

    switch (message) {
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            if (globalPrevClickButton != m_button)
                globalClickCount = 0;
            // FALL THROUGH
        case WM_LBUTTONDBLCLK:  // For these messages, the OS ensures that the
        case WM_MBUTTONDBLCLK:  // previous BUTTONDOWN was for the same button.
        case WM_RBUTTONDBLCLK:
            {
                DWORD curTime = GetTickCount();
                if (curTime - globalPrevClickTime > GetDoubleClickTime() ||
                    m_position != globalPrevClickPosition)
                    globalClickCount = 0;
                globalPrevClickTime = curTime;
            }
            globalPrevClickButton = m_button;
            globalPrevClickPosition = m_position;
            m_clickCount = ++globalClickCount;
            return;
    }

    m_clickCount = (m_button == globalPrevClickButton) ? globalClickCount :
        ((message == WM_LBUTTONUP || message == WM_MBUTTONUP ||
          message == WM_RBUTTONUP) ? 1 : 0);
    return;
}

} // namespace WebCore
