/*
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>
 * Copyright (C) 2010-2012 Igalia S.L.
 * All rights reserved.
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
#include "IntPoint.h"
#include <gdk/gdk.h>

namespace WebCore {

static GRefPtr<GdkCursor> createNamedCursor(const char* name)
{
    return adoptGRef(gdk_cursor_new_from_name(gdk_display_get_default(), name));
}

static GRefPtr<GdkCursor> createCustomCursor(Image* image, const IntPoint& hotSpot)
{
    RefPtr<cairo_surface_t> surface = image->nativeImageForCurrentFrame();
    if (!surface)
        return nullptr;

    IntPoint effectiveHotSpot = determineHotSpot(image, hotSpot);
    return adoptGRef(gdk_cursor_new_from_surface(gdk_display_get_default(), surface.get(), effectiveHotSpot.x(), effectiveHotSpot.y()));
}

void Cursor::ensurePlatformCursor() const
{
    if (m_platformCursor || m_type == Cursor::Pointer)
        return;

    switch (m_type) {
    case Cursor::Pointer:
        // A null GdkCursor is the default cursor for the window.
        m_platformCursor = 0;
        break;
    case Cursor::Cross:
        m_platformCursor = createNamedCursor("crosshair");
        break;
    case Cursor::Hand:
        m_platformCursor = createNamedCursor("pointer");
        break;
    case Cursor::IBeam:
        m_platformCursor = createNamedCursor("text");
        break;
    case Cursor::Wait:
        m_platformCursor = createNamedCursor("wait");
        break;
    case Cursor::Help:
        m_platformCursor = createNamedCursor("help");
        break;
    case Cursor::Move:
    case Cursor::MiddlePanning:
        m_platformCursor = createNamedCursor("move");
        break;
    case Cursor::EastResize:
    case Cursor::EastPanning:
        m_platformCursor = createNamedCursor("e-resize");
        break;
    case Cursor::NorthResize:
    case Cursor::NorthPanning:
        m_platformCursor = createNamedCursor("n-resize");
        break;
    case Cursor::NorthEastResize:
    case Cursor::NorthEastPanning:
        m_platformCursor = createNamedCursor("ne-resize");
        break;
    case Cursor::NorthWestResize:
    case Cursor::NorthWestPanning:
        m_platformCursor = createNamedCursor("nw-resize");
        break;
    case Cursor::SouthResize:
    case Cursor::SouthPanning:
        m_platformCursor = createNamedCursor("s-resize");
        break;
    case Cursor::SouthEastResize:
    case Cursor::SouthEastPanning:
        m_platformCursor = createNamedCursor("se-resize");
        break;
    case Cursor::SouthWestResize:
    case Cursor::SouthWestPanning:
        m_platformCursor = createNamedCursor("sw-resize");
        break;
    case Cursor::WestResize:
    case Cursor::WestPanning:
        m_platformCursor = createNamedCursor("w-resize");
        break;
    case Cursor::NorthSouthResize:
        m_platformCursor = createNamedCursor("ns-resize");
        break;
    case Cursor::EastWestResize:
        m_platformCursor = createNamedCursor("ew-resize");
        break;
    case Cursor::NorthEastSouthWestResize:
        m_platformCursor = createNamedCursor("nesw-resize");
        break;
    case Cursor::NorthWestSouthEastResize:
        m_platformCursor = createNamedCursor("nwse-resize");
        break;
    case Cursor::ColumnResize:
        m_platformCursor = createNamedCursor("col-resize");
        break;
    case Cursor::RowResize:
        m_platformCursor = createNamedCursor("row-resize");
        break;
    case Cursor::VerticalText:
        m_platformCursor = createNamedCursor("vertical-text");
        break;
    case Cursor::Cell:
        m_platformCursor = createNamedCursor("cell");
        break;
    case Cursor::ContextMenu:
        m_platformCursor = createNamedCursor("context-menu");
        break;
    case Cursor::Alias:
        m_platformCursor = createNamedCursor("alias");
        break;
    case Cursor::Progress:
        m_platformCursor = createNamedCursor("progress");
        break;
    case Cursor::NoDrop:
        m_platformCursor = createNamedCursor("no-drop");
        break;
    case Cursor::NotAllowed:
        m_platformCursor = createNamedCursor("not-allowed");
        break;
    case Cursor::Copy:
        m_platformCursor = createNamedCursor("copy");
        break;
    case Cursor::None:
        m_platformCursor = createNamedCursor("none");
        break;
    case Cursor::ZoomIn:
        m_platformCursor = createNamedCursor("zoom-in");
        break;
    case Cursor::ZoomOut:
        m_platformCursor = createNamedCursor("zoom-out");
        break;
    case Cursor::Grab:
        m_platformCursor = createNamedCursor("grab");
        break;
    case Cursor::Grabbing:
        m_platformCursor = createNamedCursor("grabbing");
        break;
    case Cursor::Custom:
        m_platformCursor = createCustomCursor(m_image.get(), m_hotSpot);
        break;
    }
}

}
