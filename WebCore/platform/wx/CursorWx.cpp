/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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

#include <wx/defs.h>
#include <wx/cursor.h>

namespace WebCore {

Cursor::Cursor(const Cursor& other)
    : m_impl(other.m_impl)
{
}

Cursor::Cursor(Image*, const IntPoint&) { }

Cursor::~Cursor()
{
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_impl = other.m_impl;
    return *this;
}

Cursor::Cursor(wxCursor* c)
    : m_impl(c)
{
}

const Cursor& pointerCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_ARROW);
    return c;
}

const Cursor& crossCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_CROSS);
    return c;
}

const Cursor& handCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_HAND);
    return c;
}

const Cursor& iBeamCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_IBEAM);
    return c;
}

const Cursor& waitCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_WAIT);
    return c;
}

const Cursor& helpCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_QUESTION_ARROW);
    return c;
}

const Cursor& eastResizeCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_SIZEWE);
    return c;
}

const Cursor& northResizeCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_SIZENS);
    return c;
}

const Cursor& northEastResizeCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_SIZENESW);
    return c;
}

const Cursor& northWestResizeCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_SIZENWSE);
    return c;
}

const Cursor& southResizeCursor()
{
    static Cursor c = northResizeCursor();
    return c;
}

const Cursor& southEastResizeCursor()
{
    static Cursor c = northWestResizeCursor();
    return c;
}

const Cursor& southWestResizeCursor()
{
    static Cursor c = northEastResizeCursor();
    return c;
}

const Cursor& westResizeCursor()
{
    static Cursor c = eastResizeCursor();
    return c;
}

const Cursor& northSouthResizeCursor()
{
    static Cursor c = northResizeCursor();
    return c;
}

const Cursor& eastWestResizeCursor()
{
    static Cursor c = eastResizeCursor();
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    static Cursor c = northEastResizeCursor();
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    static Cursor c = northWestResizeCursor();
    return c;
}

const Cursor& columnResizeCursor()
{
    // FIXME: Windows does not have a standard column resize cursor
    static Cursor c = new wxCursor(wxCURSOR_SIZING);
    return c;
}

const Cursor& rowResizeCursor()
{
    // FIXME: Windows does not have a standard row resize cursor
    static Cursor c = new wxCursor(wxCURSOR_SIZING);
    return c;
}

const Cursor& verticalTextCursor()
{
    return iBeamCursor();
}

const Cursor& cellCursor()
{
    return pointerCursor();
}

const Cursor& contextMenuCursor()
{
    return handCursor();
}

const Cursor& noDropCursor()
{
    return pointerCursor();
}

const Cursor& progressCursor()
{
    return waitCursor();
}

const Cursor& aliasCursor()
{
    return pointerCursor();
}

const Cursor& copyCursor()
{
    return pointerCursor();
}

const Cursor& noneCursor()
{
    // fix me
    return NULL;
}

const Cursor& notAllowedCursor()
{
    static Cursor c = new wxCursor(wxCURSOR_NO_ENTRY);
    return c;
}
}
