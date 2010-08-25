/*
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>
 * Copyright (C) 2010 Igalia S.L.
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

#include "config.h"
#include "CursorGtk.h"

#include "Image.h"
#include "IntPoint.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <wtf/Assertions.h>

namespace WebCore {

static GdkPixmap* createPixmapFromBits(const unsigned char* bits, const IntSize& size)
{
    cairo_surface_t* dataSurface = cairo_image_surface_create_for_data(const_cast<unsigned char*>(bits), CAIRO_FORMAT_A1, size.width(), size.height(), size.width() / 8);
    GdkPixmap* pixmap = gdk_pixmap_new(0, size.width(), size.height(), 1);
    cairo_t* cr = gdk_cairo_create(pixmap);
    cairo_set_source_surface(cr, dataSurface, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(dataSurface);
    return pixmap;
}

static PlatformRefPtr<GdkCursor> createNamedCursor(CustomCursorType cursorType)
{
    CustomCursor cursor = CustomCursors[cursorType];
    PlatformRefPtr<GdkCursor> c = adoptPlatformRef(gdk_cursor_new_from_name(gdk_display_get_default(), cursor.name));
    if (c)
        return c;

    const GdkColor fg = { 0, 0, 0, 0 };
    const GdkColor bg = { 65535, 65535, 65535, 65535 };
    IntSize cursorSize = IntSize(32, 32);
    PlatformRefPtr<GdkPixmap> source = adoptPlatformRef(createPixmapFromBits(cursor.bits, cursorSize));
    PlatformRefPtr<GdkPixmap> mask = adoptPlatformRef(createPixmapFromBits(cursor.mask_bits, cursorSize));
    return adoptPlatformRef(gdk_cursor_new_from_pixmap(source.get(), mask.get(), &fg, &bg, cursor.hot_x, cursor.hot_y));
}

static PlatformRefPtr<GdkCursor> createCustomCursor(Image* image, const IntPoint& hotSpot)
{
    IntPoint effectiveHotSpot = determineHotSpot(image, hotSpot);
    PlatformRefPtr<GdkPixbuf> pixbuf = adoptPlatformRef(image->getGdkPixbuf());
    return adoptPlatformRef(gdk_cursor_new_from_pixbuf(gdk_display_get_default(), pixbuf.get(), effectiveHotSpot.x(), effectiveHotSpot.y()));
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
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_CROSS));
        break;
    case Cursor::Hand:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_HAND2));
        break;
    case Cursor::IBeam:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_XTERM));
        break;
    case Cursor::Wait:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_WATCH));
        break;
    case Cursor::Help:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_QUESTION_ARROW));
        break;
    case Cursor::Move:
    case Cursor::MiddlePanning:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_FLEUR));
        break;
    case Cursor::EastResize:
    case Cursor::EastPanning:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_RIGHT_SIDE));
        break;
    case Cursor::NorthResize:
    case Cursor::NorthPanning:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_TOP_SIDE));
        break;
    case Cursor::NorthEastResize:
    case Cursor::NorthEastPanning:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_LEFT_SIDE));
        break;
    case Cursor::NorthWestResize:
    case Cursor::NorthWestPanning:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_TOP_LEFT_CORNER));
        break;
    case Cursor::SouthResize:
    case Cursor::SouthPanning:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_BOTTOM_SIDE));
        break;
    case Cursor::SouthEastResize:
    case Cursor::SouthEastPanning:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER));
        break;
    case Cursor::SouthWestResize:
    case Cursor::SouthWestPanning:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER));
        break;
    case Cursor::WestResize:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_LEFT_SIDE));
        break;
    case Cursor::NorthSouthResize:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_TOP_TEE));
        break;
    case Cursor::EastWestResize:
    case Cursor::WestPanning:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_LEFT_SIDE));
        break;
    case Cursor::NorthEastSouthWestResize:
    case Cursor::NorthWestSouthEastResize:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_SIZING));
        break;
    case Cursor::ColumnResize:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW));
        break;
    case Cursor::RowResize:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_SB_V_DOUBLE_ARROW));
        break;
    case Cursor::VerticalText:
        m_platformCursor = createNamedCursor(CustomCursorVerticalText);
        break;
    case Cursor::Cell:
        m_platformCursor = adoptPlatformRef(gdk_cursor_new(GDK_PLUS));
        break;
    case Cursor::ContextMenu:
        m_platformCursor = createNamedCursor(CustomCursorContextMenu);
        break;
    case Cursor::Alias:
        m_platformCursor = createNamedCursor(CustomCursorAlias);
        break;
    case Cursor::Progress:
        m_platformCursor = createNamedCursor(CustomCursorProgress);
        break;
    case Cursor::NoDrop:
    case Cursor::NotAllowed:
        m_platformCursor = createNamedCursor(CustomCursorNoDrop);
        break;
    case Cursor::Copy:
        m_platformCursor = createNamedCursor(CustomCursorCopy);
        break;
    case Cursor::None:
        m_platformCursor = createNamedCursor(CustomCursorNone);
        break;
    case Cursor::ZoomIn:
        m_platformCursor = createNamedCursor(CustomCursorZoomIn);
        break;
    case Cursor::ZoomOut:
        m_platformCursor = createNamedCursor(CustomCursorZoomOut);
        break;
    case Cursor::Grab:
        m_platformCursor = createNamedCursor(CustomCursorGrab);
        break;
    case Cursor::Grabbing:
        m_platformCursor = createNamedCursor(CustomCursorGrabbing);
        break;
    case Cursor::Custom:
        m_platformCursor = createCustomCursor(m_image.get(), m_hotSpot);
        break;
    }
}

Cursor::Cursor(const Cursor& other)
    : m_type(other.m_type)
    , m_image(other.m_image)
    , m_hotSpot(other.m_hotSpot)
    , m_platformCursor(other.m_platformCursor)
{
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_type = other.m_type;
    m_image = other.m_image;
    m_hotSpot = other.m_hotSpot;
    m_platformCursor = other.m_platformCursor;
    return *this;
}

Cursor::~Cursor()
{
}

}
