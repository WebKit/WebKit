/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Cursor.h"

#include "Image.h"
#include "IntRect.h"
#include <wtf/Assertions.h>

namespace WebCore {

IntPoint determineHotSpot(Image* image, const IntPoint& specifiedHotSpot)
{
    if (image->isNull())
        return IntPoint();

    // Hot spot must be inside cursor rectangle.
    IntRect imageRect = IntRect(image->rect());
    if (imageRect.contains(specifiedHotSpot))
        return specifiedHotSpot;

    // If hot spot is not specified externally, it can be extracted from some image formats (e.g. .cur).
    IntPoint intrinsicHotSpot;
    bool imageHasIntrinsicHotSpot = image->getHotSpot(intrinsicHotSpot);
    if (imageHasIntrinsicHotSpot && imageRect.contains(intrinsicHotSpot))
        return intrinsicHotSpot;

    return IntPoint();
}

const Cursor& Cursor::fromType(Cursor::Type type)
{
    switch (type) {
    case Cursor::Pointer:
        return pointerCursor();
    case Cursor::Cross:
        return crossCursor();
    case Cursor::Hand:
        return handCursor();
    case Cursor::IBeam:
        return iBeamCursor();
    case Cursor::Wait:
        return waitCursor();
    case Cursor::Help:
        return helpCursor();
    case Cursor::EastResize:
        return eastResizeCursor();
    case Cursor::NorthResize:
        return northResizeCursor();
    case Cursor::NorthEastResize:
        return northEastResizeCursor();
    case Cursor::NorthWestResize:
        return northWestResizeCursor();
    case Cursor::SouthResize:
        return southResizeCursor();
    case Cursor::SouthEastResize:
        return southEastResizeCursor();
    case Cursor::SouthWestResize:
        return southWestResizeCursor();
    case Cursor::WestResize:
        return westResizeCursor();
    case Cursor::NorthSouthResize:
        return northSouthResizeCursor();
    case Cursor::EastWestResize:
        return eastWestResizeCursor();
    case Cursor::NorthEastSouthWestResize:
        return northEastSouthWestResizeCursor();
    case Cursor::NorthWestSouthEastResize:
        return northWestSouthEastResizeCursor();
    case Cursor::ColumnResize:
        return columnResizeCursor();
    case Cursor::RowResize:
        return rowResizeCursor();
    case Cursor::MiddlePanning:
        return middlePanningCursor();
    case Cursor::EastPanning:
        return eastPanningCursor();
    case Cursor::NorthPanning:
        return northPanningCursor();
    case Cursor::NorthEastPanning:
        return northEastPanningCursor();
    case Cursor::NorthWestPanning:
        return northWestPanningCursor();
    case Cursor::SouthPanning:
        return southPanningCursor();
    case Cursor::SouthEastPanning:
        return southEastPanningCursor();
    case Cursor::SouthWestPanning:
        return southWestPanningCursor();
    case Cursor::WestPanning:
        return westPanningCursor();
    case Cursor::Move:
        return moveCursor();
    case Cursor::VerticalText:
        return verticalTextCursor();
    case Cursor::Cell:
        return cellCursor();
    case Cursor::ContextMenu:
        return contextMenuCursor();
    case Cursor::Alias:
        return aliasCursor();
    case Cursor::Progress:
        return progressCursor();
    case Cursor::NoDrop:
        return noDropCursor();
    case Cursor::Copy:
        return copyCursor();
    case Cursor::None:
        return noneCursor();
    case Cursor::NotAllowed:
        return notAllowedCursor();
    case Cursor::ZoomIn:
        return zoomInCursor();
    case Cursor::ZoomOut:
        return zoomOutCursor();
    case Cursor::Grab:
        return grabCursor();
    case Cursor::Grabbing:
        return grabbingCursor();
    case Cursor::Custom:
        ASSERT_NOT_REACHED();
    }
    return pointerCursor();
}

Cursor::Cursor(Image* image, const IntPoint& hotSpot)
    : m_type(Custom)
    , m_image(image)
    , m_hotSpot(determineHotSpot(image, hotSpot))
#if ENABLE(MOUSE_CURSOR_SCALE)
    , m_imageScaleFactor(1)
#endif
    , m_platformCursor(0)
{
}

#if ENABLE(MOUSE_CURSOR_SCALE)
Cursor::Cursor(Image* image, const IntPoint& hotSpot, float scale)
    : m_type(Custom)
    , m_image(image)
    , m_hotSpot(determineHotSpot(image, hotSpot))
    , m_imageScaleFactor(scale)
    , m_platformCursor(0)
{
}
#endif

Cursor::Cursor(Type type)
    : m_type(type)
#if ENABLE(MOUSE_CURSOR_SCALE)
    , m_imageScaleFactor(1)
#endif
    , m_platformCursor(0)
{
}

#if !PLATFORM(COCOA)

PlatformCursor Cursor::platformCursor() const
{
    ensurePlatformCursor();
    return m_platformCursor;
}

#endif

const Cursor& pointerCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Pointer));
    return c;
}

const Cursor& crossCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Cross));
    return c;
}

const Cursor& handCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Hand));
    return c;
}

const Cursor& moveCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Move));
    return c;
}

const Cursor& verticalTextCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::VerticalText));
    return c;
}

const Cursor& cellCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Cell));
    return c;
}

const Cursor& contextMenuCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::ContextMenu));
    return c;
}

const Cursor& aliasCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Alias));
    return c;
}

const Cursor& zoomInCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::ZoomIn));
    return c;
}

const Cursor& zoomOutCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::ZoomOut));
    return c;
}

const Cursor& copyCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Copy));
    return c;
}

const Cursor& noneCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::None));
    return c;
}

const Cursor& progressCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Progress));
    return c;
}

const Cursor& noDropCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NoDrop));
    return c;
}

const Cursor& notAllowedCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NotAllowed));
    return c;
}

const Cursor& iBeamCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::IBeam));
    return c;
}

const Cursor& waitCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Wait));
    return c;
}

const Cursor& helpCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Help));
    return c;
}

const Cursor& eastResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::EastResize));
    return c;
}

const Cursor& northResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthResize));
    return c;
}

const Cursor& northEastResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthEastResize));
    return c;
}

const Cursor& northWestResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthWestResize));
    return c;
}

const Cursor& southResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthResize));
    return c;
}

const Cursor& southEastResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthEastResize));
    return c;
}

const Cursor& southWestResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthWestResize));
    return c;
}

const Cursor& westResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::WestResize));
    return c;
}

const Cursor& northSouthResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthSouthResize));
    return c;
}

const Cursor& eastWestResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::EastWestResize));
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthEastSouthWestResize));
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthWestSouthEastResize));
    return c;
}

const Cursor& columnResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::ColumnResize));
    return c;
}

const Cursor& rowResizeCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::RowResize));
    return c;
}

const Cursor& middlePanningCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::MiddlePanning));
    return c;
}
    
const Cursor& eastPanningCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::EastPanning));
    return c;
}
    
const Cursor& northPanningCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthPanning));
    return c;
}
    
const Cursor& northEastPanningCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthEastPanning));
    return c;
}
    
const Cursor& northWestPanningCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::NorthWestPanning));
    return c;
}
    
const Cursor& southPanningCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthPanning));
    return c;
}
    
const Cursor& southEastPanningCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthEastPanning));
    return c;
}
    
const Cursor& southWestPanningCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::SouthWestPanning));
    return c;
}
    
const Cursor& westPanningCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::WestPanning));
    return c;
}

const Cursor& grabCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Grab));
    return c;
}

const Cursor& grabbingCursor()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Cursor, c, (Cursor::Grabbing));
    return c;
}

} // namespace WebCore
