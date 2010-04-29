/*
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>
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

#include <wtf/Assertions.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

namespace WebCore {

static GdkCursor* customCursorNew(CustomCursorType cursorType)
{
    CustomCursor cursor = CustomCursors[cursorType];
    GdkCursor* c = gdk_cursor_new_from_name(gdk_display_get_default(), cursor.name);
    if (!c) {
        const GdkColor fg = { 0, 0, 0, 0 };
        const GdkColor bg = { 65535, 65535, 65535, 65535 };

        GdkPixmap* source = gdk_bitmap_create_from_data(NULL, cursor.bits, 32, 32);
        GdkPixmap* mask = gdk_bitmap_create_from_data(NULL, cursor.mask_bits, 32, 32);
        c = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, cursor.hot_x, cursor.hot_y);
        g_object_unref(source);
        g_object_unref(mask);
    }
    return c;
}


Cursor::Cursor(const Cursor& other)
    : m_impl(other.m_impl)
{
    if (m_impl)
        gdk_cursor_ref(m_impl);
}

Cursor::Cursor(Image* image, const IntPoint& hotSpot)
{
    GdkPixbuf* pixbuf = image->getGdkPixbuf();
    m_impl = gdk_cursor_new_from_pixbuf(gdk_display_get_default(), pixbuf, hotSpot.x(), hotSpot.y());
    g_object_unref(pixbuf);
}

Cursor::~Cursor()
{
    if (m_impl)
        gdk_cursor_unref(m_impl);
}

Cursor& Cursor::operator=(const Cursor& other)
{
    gdk_cursor_ref(other.m_impl);
    gdk_cursor_unref(m_impl);
    m_impl = other.m_impl;
    return *this;
}

Cursor::Cursor(GdkCursor* c)
    : m_impl(c)
{
    m_impl = c;

    // The GdkCursor may be NULL - the default cursor for the window.
    if (c)
        gdk_cursor_ref(c);
}

const Cursor& pointerCursor()
{
    static Cursor c = 0;
    return c;
}

const Cursor& crossCursor()
{
    static Cursor c = gdk_cursor_new(GDK_CROSS);
    return c;
}

const Cursor& handCursor()
{
    static Cursor c = gdk_cursor_new(GDK_HAND2);
    return c;
}

const Cursor& moveCursor()
{
    static Cursor c = gdk_cursor_new(GDK_FLEUR);
    return c;
}

const Cursor& iBeamCursor()
{
    static Cursor c = gdk_cursor_new(GDK_XTERM);
    return c;
}

const Cursor& waitCursor()
{
    static Cursor c = gdk_cursor_new(GDK_WATCH);
    return c;
}

const Cursor& helpCursor()
{
    static Cursor c = gdk_cursor_new(GDK_QUESTION_ARROW);
    return c;
}

const Cursor& eastResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_RIGHT_SIDE);
    return c;
}

const Cursor& northResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_TOP_SIDE);
    return c;
}

const Cursor& northEastResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_TOP_RIGHT_CORNER);
    return c;
}

const Cursor& northWestResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_TOP_LEFT_CORNER);
    return c;
}

const Cursor& southResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_BOTTOM_SIDE);
    return c;
}

const Cursor& southEastResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER);
    return c;
}

const Cursor& southWestResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER);
    return c;
}

const Cursor& westResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_LEFT_SIDE);
    return c;
}

const Cursor& northSouthResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_TOP_TEE);
    return c;
}

const Cursor& eastWestResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_LEFT_SIDE);
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_SIZING);
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_SIZING);
    return c;
}

const Cursor& columnResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW);
    return c;
}

const Cursor& rowResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_SB_V_DOUBLE_ARROW);
    return c;
}
    
const Cursor& middlePanningCursor()
{
    return moveCursor();
}

const Cursor& eastPanningCursor()
{
    return eastResizeCursor();
}

const Cursor& northPanningCursor()
{
    return northResizeCursor();
}

const Cursor& northEastPanningCursor()
{
    return northEastResizeCursor();
}

const Cursor& northWestPanningCursor()
{
    return northWestResizeCursor();
}

const Cursor& southPanningCursor()
{
    return southResizeCursor();
}

const Cursor& southEastPanningCursor()
{
    return southEastResizeCursor();
}

const Cursor& southWestPanningCursor()
{
    return southWestResizeCursor();
}

const Cursor& westPanningCursor()
{
    return westResizeCursor();
}
    

const Cursor& verticalTextCursor()
{
    static Cursor c = customCursorNew(CustomCursorVerticalText);
    return c;
}

const Cursor& cellCursor()
{
    static Cursor c = gdk_cursor_new(GDK_PLUS);
    return c;
}

const Cursor& contextMenuCursor()
{
    static Cursor c = customCursorNew(CustomCursorContextMenu);
    return c;
}

const Cursor& noDropCursor()
{
    static Cursor c = customCursorNew(CustomCursorNoDrop);
    return c;
}

const Cursor& copyCursor()
{
    static Cursor c = customCursorNew(CustomCursorCopy);
    return c;
}

const Cursor& progressCursor()
{
    static Cursor c = customCursorNew(CustomCursorProgress);
    return c;
}

const Cursor& aliasCursor()
{
    static Cursor c = customCursorNew(CustomCursorAlias);
    return c;
}

const Cursor& noneCursor()
{
    static Cursor c = customCursorNew(CustomCursorNone);
    return c;
}

const Cursor& notAllowedCursor()
{
    return noDropCursor();
}

const Cursor& zoomInCursor()
{
    static Cursor c = customCursorNew(CustomCursorZoomIn);
    return c;
}

const Cursor& zoomOutCursor()
{
    static Cursor c = customCursorNew(CustomCursorZoomOut);
    return c;
}

const Cursor& grabCursor()
{
    static Cursor c = customCursorNew(CustomCursorGrab);
    return c;
}

const Cursor& grabbingCursor()
{
    static Cursor c = customCursorNew(CustomCursorGrabbing);
    return c;
}

}
