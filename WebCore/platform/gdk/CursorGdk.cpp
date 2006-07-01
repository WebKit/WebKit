/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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
#include "Cursor.h"

#include "DeprecatedString.h"
#include <gdk/gdk.h>
#include <wtf/Assertions.h>

namespace WebCore {

Cursor::Cursor(const Cursor& other)
    : m_impl(other.m_impl)
{
    if (m_impl)
        gdk_cursor_ref(m_impl);
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
    ASSERT(c);
    gdk_cursor_ref(c);
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
    static Cursor c = gdk_cursor_new(GDK_DOUBLE_ARROW);
    return c;
}

const Cursor& rowResizeCursor()
{
    static Cursor c = gdk_cursor_new(GDK_DOUBLE_ARROW);
    return c;
}

}
