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

#if !PLATFORM(IOS_FAMILY)

#include "Image.h"
#include "IntRect.h"
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>

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
    if (auto intrinsicHotSpot = image->hotSpot()) {
        if (imageRect.contains(intrinsicHotSpot.value()))
            return intrinsicHotSpot.value();
    }

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
    static NeverDestroyed<Cursor> c(Cursor::Pointer);
    return c;
}

const Cursor& crossCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Cross);
    return c;
}

const Cursor& handCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Hand);
    return c;
}

const Cursor& moveCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Move);
    return c;
}

const Cursor& verticalTextCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::VerticalText);
    return c;
}

const Cursor& cellCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Cell);
    return c;
}

const Cursor& contextMenuCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::ContextMenu);
    return c;
}

const Cursor& aliasCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Alias);
    return c;
}

const Cursor& zoomInCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::ZoomIn);
    return c;
}

const Cursor& zoomOutCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::ZoomOut);
    return c;
}

const Cursor& copyCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Copy);
    return c;
}

const Cursor& noneCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::None);
    return c;
}

const Cursor& progressCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Progress);
    return c;
}

const Cursor& noDropCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NoDrop);
    return c;
}

const Cursor& notAllowedCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NotAllowed);
    return c;
}

const Cursor& iBeamCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::IBeam);
    return c;
}

const Cursor& waitCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Wait);
    return c;
}

const Cursor& helpCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Help);
    return c;
}

const Cursor& eastResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::EastResize);
    return c;
}

const Cursor& northResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NorthResize);
    return c;
}

const Cursor& northEastResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NorthEastResize);
    return c;
}

const Cursor& northWestResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NorthWestResize);
    return c;
}

const Cursor& southResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::SouthResize);
    return c;
}

const Cursor& southEastResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::SouthEastResize);
    return c;
}

const Cursor& southWestResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::SouthWestResize);
    return c;
}

const Cursor& westResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::WestResize);
    return c;
}

const Cursor& northSouthResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NorthSouthResize);
    return c;
}

const Cursor& eastWestResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::EastWestResize);
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NorthEastSouthWestResize);
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NorthWestSouthEastResize);
    return c;
}

const Cursor& columnResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::ColumnResize);
    return c;
}

const Cursor& rowResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::RowResize);
    return c;
}

const Cursor& middlePanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::MiddlePanning);
    return c;
}
    
const Cursor& eastPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::EastPanning);
    return c;
}
    
const Cursor& northPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NorthPanning);
    return c;
}
    
const Cursor& northEastPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NorthEastPanning);
    return c;
}
    
const Cursor& northWestPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::NorthWestPanning);
    return c;
}
    
const Cursor& southPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::SouthPanning);
    return c;
}
    
const Cursor& southEastPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::SouthEastPanning);
    return c;
}
    
const Cursor& southWestPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::SouthWestPanning);
    return c;
}
    
const Cursor& westPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::WestPanning);
    return c;
}

const Cursor& grabCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Grab);
    return c;
}

const Cursor& grabbingCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Grabbing);
    return c;
}
    

} // namespace WebCore

#endif // !PLATFORM(IOS_FAMILY)

