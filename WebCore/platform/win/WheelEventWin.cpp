/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#include "PlatformWheelEvent.h"

#include <windows.h>
#include <windowsx.h>

namespace WebCore {

#define HIGH_BIT_MASK_SHORT 0x8000
#define SPI_GETWHEELSCROLLCHARS 0x006C

static IntPoint positionForEvent(HWND hWnd, LPARAM lParam)
{
    POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(hWnd, &point);
    return point;
}

static IntPoint globalPositionForEvent(HWND hWnd, LPARAM lParam)
{
    POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    return point;
}

int PlatformWheelEvent::horizontalLineMultiplier() const
{
    static ULONG scrollChars;
    if (!scrollChars && !SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scrollChars, 0))
        scrollChars = cLineMultiplier;
    return scrollChars;
}

int PlatformWheelEvent::verticalLineMultiplier() const
{
    static ULONG scrollLines;
    if (!scrollLines && !SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0))
        scrollLines = cLineMultiplier;
    return scrollLines;
}
        
PlatformWheelEvent::PlatformWheelEvent(HWND hWnd, WPARAM wParam, LPARAM lParam, bool isHorizontal)
    : m_position(positionForEvent(hWnd, lParam))
    , m_globalPosition(globalPositionForEvent(hWnd, lParam))
    , m_isAccepted(false)
    , m_shiftKey(wParam & MK_SHIFT)
    , m_ctrlKey(wParam & MK_CONTROL)
    , m_altKey(GetKeyState(VK_MENU) & HIGH_BIT_MASK_SHORT)
    , m_metaKey(m_altKey) // FIXME: We'll have to test other browsers
{
    static ULONG scrollLines, scrollChars;
    float delta = GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
    if (isHorizontal) {
        // Windows sends a positive delta for scrolling right, while AppKit
        // sends a negative delta. EventHandler expects the AppKit values,
        // so we have to negate our horizontal delta to match.
        m_deltaX = -delta * horizontalLineMultiplier();
        m_deltaY = 0;
        m_granularity = ScrollByLineWheelEvent;
    } else {
        m_deltaX = 0;
        m_deltaY = delta;
        int verticalMultiplier = verticalLineMultiplier();
        // A multiplier of -1 is used to mean that vertical wheel scrolling should be done by page.
        m_granularity = (verticalMultiplier == -1) ? ScrollByPageWheelEvent : ScrollByLineWheelEvent;
        if (m_granularity == ScrollByLineWheelEvent)
            m_deltaY *= verticalMultiplier;
    }
}

}
