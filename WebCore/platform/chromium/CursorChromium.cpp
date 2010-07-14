/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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
#include "Cursor.h"

namespace WebCore {

Cursor::Cursor(const Cursor& other)
    : m_platformCursor(other.m_platformCursor)
{
}

Cursor::Cursor(Image* image, const IntPoint& hotSpot)
    : m_platformCursor(image, hotSpot)
{
}

Cursor::~Cursor()
{
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_platformCursor = other.m_platformCursor;
    return *this;
}

Cursor::Cursor(PlatformCursor c)
    : m_platformCursor(c)
{
}

const Cursor& pointerCursor()
{
    static const Cursor c(PlatformCursor::TypePointer);
    return c;
}

const Cursor& crossCursor()
{
    static const Cursor c(PlatformCursor::TypeCross);
    return c;
}

const Cursor& handCursor()
{
    static const Cursor c(PlatformCursor::TypeHand);
    return c;
}

const Cursor& iBeamCursor()
{
    static const Cursor c(PlatformCursor::TypeIBeam);
    return c;
}

const Cursor& waitCursor()
{
    static const Cursor c(PlatformCursor::TypeWait);
    return c;
}

const Cursor& helpCursor()
{
    static const Cursor c(PlatformCursor::TypeHelp);
    return c;
}

const Cursor& eastResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeEastResize);
    return c;
}

const Cursor& northResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeNorthResize);
    return c;
}

const Cursor& northEastResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeNorthEastResize);
    return c;
}

const Cursor& northWestResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeNorthWestResize);
    return c;
}

const Cursor& southResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeSouthResize);
    return c;
}

const Cursor& southEastResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeSouthEastResize);
    return c;
}

const Cursor& southWestResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeSouthWestResize);
    return c;
}

const Cursor& westResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeWestResize);
    return c;
}

const Cursor& northSouthResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeNorthSouthResize);
    return c;
}

const Cursor& eastWestResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeEastWestResize);
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeNorthEastSouthWestResize);
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeNorthWestSouthEastResize);
    return c;
}

const Cursor& columnResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeColumnResize);
    return c;
}

const Cursor& rowResizeCursor()
{
    static const Cursor c(PlatformCursor::TypeRowResize);
    return c;
}

const Cursor& middlePanningCursor()
{
    static const Cursor c(PlatformCursor::TypeMiddlePanning);
    return c;
}

const Cursor& eastPanningCursor()
{
    static const Cursor c(PlatformCursor::TypeEastPanning);
    return c;
}

const Cursor& northPanningCursor()
{
    static const Cursor c(PlatformCursor::TypeNorthPanning);
    return c;
}

const Cursor& northEastPanningCursor()
{
    static const Cursor c(PlatformCursor::TypeNorthEastPanning);
    return c;
}

const Cursor& northWestPanningCursor()
{
    static const Cursor c(PlatformCursor::TypeNorthWestPanning);
    return c;
}

const Cursor& southPanningCursor()
{
    static const Cursor c(PlatformCursor::TypeSouthPanning);
    return c;
}

const Cursor& southEastPanningCursor()
{
    static const Cursor c(PlatformCursor::TypeSouthEastPanning);
    return c;
}

const Cursor& southWestPanningCursor()
{
    static const Cursor c(PlatformCursor::TypeSouthWestPanning);
    return c;
}

const Cursor& westPanningCursor()
{
    static const Cursor c(PlatformCursor::TypeWestPanning);
    return c;
}

const Cursor& moveCursor() 
{
    static const Cursor c(PlatformCursor::TypeMove);
    return c;
}

const Cursor& verticalTextCursor()
{
    static const Cursor c(PlatformCursor::TypeVerticalText);
    return c;
}

const Cursor& cellCursor()
{
    static const Cursor c(PlatformCursor::TypeCell);
    return c;
}

const Cursor& contextMenuCursor()
{
    static const Cursor c(PlatformCursor::TypeContextMenu);
    return c;
}

const Cursor& aliasCursor()
{
    static const Cursor c(PlatformCursor::TypeAlias);
    return c;
}

const Cursor& progressCursor()
{
    static const Cursor c(PlatformCursor::TypeProgress);
    return c;
}

const Cursor& noDropCursor()
{
    static const Cursor c(PlatformCursor::TypeNoDrop);
    return c;
}

const Cursor& copyCursor()
{
    static const Cursor c(PlatformCursor::TypeCopy);
    return c;
}

const Cursor& noneCursor()
{
    static const Cursor c(PlatformCursor::TypeNone);
    return c;
}

const Cursor& notAllowedCursor()
{
    static const Cursor c(PlatformCursor::TypeNotAllowed);
    return c;
}

const Cursor& zoomInCursor()
{
    static const Cursor c(PlatformCursor::TypeZoomIn);
    return c;
}

const Cursor& zoomOutCursor()
{
    static const Cursor c(PlatformCursor::TypeZoomOut);
    return c;
}

const Cursor& grabCursor()
{
    return pointerCursor();
}

const Cursor& grabbingCursor()
{
    return pointerCursor();
}

} // namespace WebCore
