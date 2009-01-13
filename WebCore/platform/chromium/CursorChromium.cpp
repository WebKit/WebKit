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
    : m_impl(other.m_impl)
{
}

Cursor::Cursor(Image* image, const IntPoint& hotSpot)
    : m_impl(image, hotSpot)
{
}

Cursor::~Cursor()
{
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_impl = other.m_impl;
    return *this;
}

Cursor::Cursor(PlatformCursor c)
    : m_impl(c)
{
}

const Cursor& pointerCursor()
{
    static const Cursor c(PlatformCursor::typePointer);
    return c;
}

const Cursor& crossCursor()
{
    static const Cursor c(PlatformCursor::typeCross);
    return c;
}

const Cursor& handCursor()
{
    static const Cursor c(PlatformCursor::typeHand);
    return c;
}

const Cursor& iBeamCursor()
{
    static const Cursor c(PlatformCursor::typeIBeam);
    return c;
}

const Cursor& waitCursor()
{
    static const Cursor c(PlatformCursor::typeWait);
    return c;
}

const Cursor& helpCursor()
{
    static const Cursor c(PlatformCursor::typeHelp);
    return c;
}

const Cursor& eastResizeCursor()
{
    static const Cursor c(PlatformCursor::typeEastResize);
    return c;
}

const Cursor& northResizeCursor()
{
    static const Cursor c(PlatformCursor::typeNorthResize);
    return c;
}

const Cursor& northEastResizeCursor()
{
    static const Cursor c(PlatformCursor::typeNorthEastResize);
    return c;
}

const Cursor& northWestResizeCursor()
{
    static const Cursor c(PlatformCursor::typeNorthWestResize);
    return c;
}

const Cursor& southResizeCursor()
{
    static const Cursor c(PlatformCursor::typeSouthResize);
    return c;
}

const Cursor& southEastResizeCursor()
{
    static const Cursor c(PlatformCursor::typeSouthEastResize);
    return c;
}

const Cursor& southWestResizeCursor()
{
    static const Cursor c(PlatformCursor::typeSouthWestResize);
    return c;
}

const Cursor& westResizeCursor()
{
    static const Cursor c(PlatformCursor::typeWestResize);
    return c;
}

const Cursor& northSouthResizeCursor()
{
    static const Cursor c(PlatformCursor::typeNorthSouthResize);
    return c;
}

const Cursor& eastWestResizeCursor()
{
    static const Cursor c(PlatformCursor::typeEastWestResize);
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    static const Cursor c(PlatformCursor::typeNorthEastSouthWestResize);
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    static const Cursor c(PlatformCursor::typeNorthWestSouthEastResize);
    return c;
}

const Cursor& columnResizeCursor()
{
    static const Cursor c(PlatformCursor::typeColumnResize);
    return c;
}

const Cursor& rowResizeCursor()
{
    static const Cursor c(PlatformCursor::typeRowResize);
    return c;
}

const Cursor& middlePanningCursor()
{
    static const Cursor c(PlatformCursor::typeMiddlePanning);
    return c;
}

const Cursor& eastPanningCursor()
{
    static const Cursor c(PlatformCursor::typeEastPanning);
    return c;
}

const Cursor& northPanningCursor()
{
    static const Cursor c(PlatformCursor::typeNorthPanning);
    return c;
}

const Cursor& northEastPanningCursor()
{
    static const Cursor c(PlatformCursor::typeNorthEastPanning);
    return c;
}

const Cursor& northWestPanningCursor()
{
    static const Cursor c(PlatformCursor::typeNorthWestPanning);
    return c;
}

const Cursor& southPanningCursor()
{
    static const Cursor c(PlatformCursor::typeSouthPanning);
    return c;
}

const Cursor& southEastPanningCursor()
{
    static const Cursor c(PlatformCursor::typeSouthEastPanning);
    return c;
}

const Cursor& southWestPanningCursor()
{
    static const Cursor c(PlatformCursor::typeSouthWestPanning);
    return c;
}

const Cursor& westPanningCursor()
{
    static const Cursor c(PlatformCursor::typeWestPanning);
    return c;
}

const Cursor& moveCursor() 
{
    static const Cursor c(PlatformCursor::typeMove);
    return c;
}

const Cursor& verticalTextCursor()
{
    static const Cursor c(PlatformCursor::typeVerticalText);
    return c;
}

const Cursor& cellCursor()
{
    static const Cursor c(PlatformCursor::typeCell);
    return c;
}

const Cursor& contextMenuCursor()
{
    static const Cursor c(PlatformCursor::typeContextMenu);
    return c;
}

const Cursor& aliasCursor()
{
    static const Cursor c(PlatformCursor::typeAlias);
    return c;
}

const Cursor& progressCursor()
{
    static const Cursor c(PlatformCursor::typeProgress);
    return c;
}

const Cursor& noDropCursor()
{
    static const Cursor c(PlatformCursor::typeNoDrop);
    return c;
}

const Cursor& copyCursor()
{
    static const Cursor c(PlatformCursor::typeCopy);
    return c;
}

const Cursor& noneCursor()
{
    static const Cursor c(PlatformCursor::typeNone);
    return c;
}

const Cursor& notAllowedCursor()
{
    static const Cursor c(PlatformCursor::typeNotAllowed);
    return c;
}

const Cursor& zoomInCursor()
{
    static const Cursor c(PlatformCursor::typeZoomIn);
    return c;
}

const Cursor& zoomOutCursor()
{
    static const Cursor c(PlatformCursor::typeZoomOut);
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
