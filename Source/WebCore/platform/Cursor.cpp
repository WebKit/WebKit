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
#include "NotImplemented.h"
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
    case Type::Pointer:
        return pointerCursor();
    case Type::Cross:
        return crossCursor();
    case Type::Hand:
        return handCursor();
    case Type::IBeam:
        return iBeamCursor();
    case Type::Wait:
        return waitCursor();
    case Type::Help:
        return helpCursor();
    case Type::EastResize:
        return eastResizeCursor();
    case Type::NorthResize:
        return northResizeCursor();
    case Type::NorthEastResize:
        return northEastResizeCursor();
    case Type::NorthWestResize:
        return northWestResizeCursor();
    case Type::SouthResize:
        return southResizeCursor();
    case Type::SouthEastResize:
        return southEastResizeCursor();
    case Type::SouthWestResize:
        return southWestResizeCursor();
    case Type::WestResize:
        return westResizeCursor();
    case Type::NorthSouthResize:
        return northSouthResizeCursor();
    case Type::EastWestResize:
        return eastWestResizeCursor();
    case Type::NorthEastSouthWestResize:
        return northEastSouthWestResizeCursor();
    case Type::NorthWestSouthEastResize:
        return northWestSouthEastResizeCursor();
    case Type::ColumnResize:
        return columnResizeCursor();
    case Type::RowResize:
        return rowResizeCursor();
    case Type::MiddlePanning:
        return middlePanningCursor();
    case Type::EastPanning:
        return eastPanningCursor();
    case Type::NorthPanning:
        return northPanningCursor();
    case Type::NorthEastPanning:
        return northEastPanningCursor();
    case Type::NorthWestPanning:
        return northWestPanningCursor();
    case Type::SouthPanning:
        return southPanningCursor();
    case Type::SouthEastPanning:
        return southEastPanningCursor();
    case Type::SouthWestPanning:
        return southWestPanningCursor();
    case Type::WestPanning:
        return westPanningCursor();
    case Type::Move:
        return moveCursor();
    case Type::VerticalText:
        return verticalTextCursor();
    case Type::Cell:
        return cellCursor();
    case Type::ContextMenu:
        return contextMenuCursor();
    case Type::Alias:
        return aliasCursor();
    case Type::Progress:
        return progressCursor();
    case Type::NoDrop:
        return noDropCursor();
    case Type::Copy:
        return copyCursor();
    case Type::None:
        return noneCursor();
    case Type::NotAllowed:
        return notAllowedCursor();
    case Type::ZoomIn:
        return zoomInCursor();
    case Type::ZoomOut:
        return zoomOutCursor();
    case Type::Grab:
        return grabCursor();
    case Type::Grabbing:
        return grabbingCursor();
    case Type::Custom:
    case Type::Invalid:
        ASSERT_NOT_REACHED();
    }
    return pointerCursor();
}

Cursor::Cursor(Image* image, const IntPoint& hotSpot)
    : m_type(Type::Custom)
    , m_image(image)
    , m_hotSpot(determineHotSpot(image, hotSpot))
{
}

#if ENABLE(MOUSE_CURSOR_SCALE)
Cursor::Cursor(Image* image, const IntPoint& hotSpot, float scale)
    : m_type(Type::Custom)
    , m_image(image)
    , m_hotSpot(determineHotSpot(image, hotSpot))
    , m_imageScaleFactor(scale)
{
}
#endif

Cursor::Cursor(Type type)
    : m_type(type)
{
}

#if !HAVE(NSCURSOR)

PlatformCursor Cursor::platformCursor() const
{
    ensurePlatformCursor();
    return m_platformCursor;
}

#endif

const Cursor& pointerCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Pointer);
    return c;
}

const Cursor& crossCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Cross);
    return c;
}

const Cursor& handCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Hand);
    return c;
}

const Cursor& moveCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Move);
    return c;
}

const Cursor& verticalTextCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::VerticalText);
    return c;
}

const Cursor& cellCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Cell);
    return c;
}

const Cursor& contextMenuCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::ContextMenu);
    return c;
}

const Cursor& aliasCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Alias);
    return c;
}

const Cursor& zoomInCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::ZoomIn);
    return c;
}

const Cursor& zoomOutCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::ZoomOut);
    return c;
}

const Cursor& copyCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Copy);
    return c;
}

const Cursor& noneCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::None);
    return c;
}

const Cursor& progressCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Progress);
    return c;
}

const Cursor& noDropCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NoDrop);
    return c;
}

const Cursor& notAllowedCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NotAllowed);
    return c;
}

const Cursor& iBeamCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::IBeam);
    return c;
}

const Cursor& waitCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Wait);
    return c;
}

const Cursor& helpCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Help);
    return c;
}

const Cursor& eastResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::EastResize);
    return c;
}

const Cursor& northResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NorthResize);
    return c;
}

const Cursor& northEastResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NorthEastResize);
    return c;
}

const Cursor& northWestResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NorthWestResize);
    return c;
}

const Cursor& southResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::SouthResize);
    return c;
}

const Cursor& southEastResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::SouthEastResize);
    return c;
}

const Cursor& southWestResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::SouthWestResize);
    return c;
}

const Cursor& westResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::WestResize);
    return c;
}

const Cursor& northSouthResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NorthSouthResize);
    return c;
}

const Cursor& eastWestResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::EastWestResize);
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NorthEastSouthWestResize);
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NorthWestSouthEastResize);
    return c;
}

const Cursor& columnResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::ColumnResize);
    return c;
}

const Cursor& rowResizeCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::RowResize);
    return c;
}

const Cursor& middlePanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::MiddlePanning);
    return c;
}

const Cursor& eastPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::EastPanning);
    return c;
}

const Cursor& northPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NorthPanning);
    return c;
}

const Cursor& northEastPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NorthEastPanning);
    return c;
}

const Cursor& northWestPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::NorthWestPanning);
    return c;
}

const Cursor& southPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::SouthPanning);
    return c;
}

const Cursor& southEastPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::SouthEastPanning);
    return c;
}

const Cursor& southWestPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::SouthWestPanning);
    return c;
}

const Cursor& westPanningCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::WestPanning);
    return c;
}

const Cursor& grabCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Grab);
    return c;
}

const Cursor& grabbingCursor()
{
    static NeverDestroyed<Cursor> c(Cursor::Type::Grabbing);
    return c;
}

#if !HAVE(NSCURSOR) && !PLATFORM(GTK) && !PLATFORM(WIN)
void Cursor::ensurePlatformCursor() const
{
    notImplemented();
}
#endif

#if !HAVE(NSCURSOR)
void Cursor::setAsPlatformCursor() const
{
    notImplemented();
}
#endif

} // namespace WebCore
