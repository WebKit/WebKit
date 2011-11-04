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

#include <wtf/Assertions.h>

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
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypePointer));
    return c;
}

const Cursor& crossCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeCross));
    return c;
}

const Cursor& handCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeHand));
    return c;
}

const Cursor& iBeamCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeIBeam));
    return c;
}

const Cursor& waitCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeWait));
    return c;
}

const Cursor& helpCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeHelp));
    return c;
}

const Cursor& eastResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeEastResize));
    return c;
}

const Cursor& northResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNorthResize));
    return c;
}

const Cursor& northEastResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNorthEastResize));
    return c;
}

const Cursor& northWestResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNorthWestResize));
    return c;
}

const Cursor& southResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeSouthResize));
    return c;
}

const Cursor& southEastResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeSouthEastResize));
    return c;
}

const Cursor& southWestResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeSouthWestResize));
    return c;
}

const Cursor& westResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeWestResize));
    return c;
}

const Cursor& northSouthResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNorthSouthResize));
    return c;
}

const Cursor& eastWestResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeEastWestResize));
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNorthEastSouthWestResize));
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNorthWestSouthEastResize));
    return c;
}

const Cursor& columnResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeColumnResize));
    return c;
}

const Cursor& rowResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeRowResize));
    return c;
}

const Cursor& middlePanningCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeMiddlePanning));
    return c;
}

const Cursor& eastPanningCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeEastPanning));
    return c;
}

const Cursor& northPanningCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNorthPanning));
    return c;
}

const Cursor& northEastPanningCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNorthEastPanning));
    return c;
}

const Cursor& northWestPanningCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNorthWestPanning));
    return c;
}

const Cursor& southPanningCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeSouthPanning));
    return c;
}

const Cursor& southEastPanningCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeSouthEastPanning));
    return c;
}

const Cursor& southWestPanningCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeSouthWestPanning));
    return c;
}

const Cursor& westPanningCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeWestPanning));
    return c;
}

const Cursor& moveCursor() 
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeMove));
    return c;
}

const Cursor& verticalTextCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeVerticalText));
    return c;
}

const Cursor& cellCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeCell));
    return c;
}

const Cursor& contextMenuCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeContextMenu));
    return c;
}

const Cursor& aliasCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeAlias));
    return c;
}

const Cursor& progressCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeProgress));
    return c;
}

const Cursor& noDropCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNoDrop));
    return c;
}

const Cursor& copyCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeCopy));
    return c;
}

const Cursor& noneCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNone));
    return c;
}

const Cursor& notAllowedCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeNotAllowed));
    return c;
}

const Cursor& zoomInCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeZoomIn));
    return c;
}

const Cursor& zoomOutCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeZoomOut));
    return c;
}

const Cursor& grabCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeGrab));
    return c;
}

const Cursor& grabbingCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (PlatformCursor::TypeGrabbing));
    return c;
}

} // namespace WebCore

#define COMPILE_ASSERT_MATCHING_ENUM(cursor_name, platform_cursor_name) \
    COMPILE_ASSERT(int(WebCore::Cursor::cursor_name) == int(WebCore::PlatformCursor::platform_cursor_name), mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(Pointer, TypePointer);
COMPILE_ASSERT_MATCHING_ENUM(Cross, TypeCross);
COMPILE_ASSERT_MATCHING_ENUM(Hand, TypeHand);
COMPILE_ASSERT_MATCHING_ENUM(IBeam, TypeIBeam);
COMPILE_ASSERT_MATCHING_ENUM(Wait, TypeWait);
COMPILE_ASSERT_MATCHING_ENUM(Help, TypeHelp);
COMPILE_ASSERT_MATCHING_ENUM(EastResize, TypeEastResize);
COMPILE_ASSERT_MATCHING_ENUM(NorthResize, TypeNorthResize);
COMPILE_ASSERT_MATCHING_ENUM(NorthEastResize, TypeNorthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(NorthWestResize, TypeNorthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(SouthResize, TypeSouthResize);
COMPILE_ASSERT_MATCHING_ENUM(SouthEastResize, TypeSouthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(SouthWestResize, TypeSouthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(WestResize, TypeWestResize);
COMPILE_ASSERT_MATCHING_ENUM(NorthSouthResize, TypeNorthSouthResize);
COMPILE_ASSERT_MATCHING_ENUM(EastWestResize, TypeEastWestResize);
COMPILE_ASSERT_MATCHING_ENUM(NorthEastSouthWestResize, TypeNorthEastSouthWestResize);
COMPILE_ASSERT_MATCHING_ENUM(NorthWestSouthEastResize, TypeNorthWestSouthEastResize);
COMPILE_ASSERT_MATCHING_ENUM(ColumnResize, TypeColumnResize);
COMPILE_ASSERT_MATCHING_ENUM(RowResize, TypeRowResize);
COMPILE_ASSERT_MATCHING_ENUM(MiddlePanning, TypeMiddlePanning);
COMPILE_ASSERT_MATCHING_ENUM(EastPanning, TypeEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(NorthPanning, TypeNorthPanning);
COMPILE_ASSERT_MATCHING_ENUM(NorthEastPanning, TypeNorthEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(NorthWestPanning, TypeNorthWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(SouthPanning, TypeSouthPanning);
COMPILE_ASSERT_MATCHING_ENUM(SouthEastPanning, TypeSouthEastPanning);
COMPILE_ASSERT_MATCHING_ENUM(SouthWestPanning, TypeSouthWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(WestPanning, TypeWestPanning);
COMPILE_ASSERT_MATCHING_ENUM(Move, TypeMove);
COMPILE_ASSERT_MATCHING_ENUM(VerticalText, TypeVerticalText);
COMPILE_ASSERT_MATCHING_ENUM(Cell, TypeCell);
COMPILE_ASSERT_MATCHING_ENUM(ContextMenu, TypeContextMenu);
COMPILE_ASSERT_MATCHING_ENUM(Alias, TypeAlias);
COMPILE_ASSERT_MATCHING_ENUM(Progress, TypeProgress);
COMPILE_ASSERT_MATCHING_ENUM(NoDrop, TypeNoDrop);
COMPILE_ASSERT_MATCHING_ENUM(Copy, TypeCopy);
COMPILE_ASSERT_MATCHING_ENUM(None, TypeNone);
COMPILE_ASSERT_MATCHING_ENUM(NotAllowed, TypeNotAllowed);
COMPILE_ASSERT_MATCHING_ENUM(ZoomIn, TypeZoomIn);
COMPILE_ASSERT_MATCHING_ENUM(ZoomOut, TypeZoomOut);
COMPILE_ASSERT_MATCHING_ENUM(Grab, TypeGrab);
COMPILE_ASSERT_MATCHING_ENUM(Grabbing, TypeGrabbing);
COMPILE_ASSERT_MATCHING_ENUM(Custom, TypeCustom);
