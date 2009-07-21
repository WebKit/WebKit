/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 *
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

#include "NotImplemented.h"

#include <app/AppDefs.h>


namespace WebCore {

Cursor::Cursor(PlatformCursor cursor)
    : m_impl(cursor)
{
}

Cursor::Cursor(const Cursor& other)
    : m_impl(other.m_impl)
{
}

Cursor::~Cursor()
{
}

Cursor::Cursor(Image*, const IntPoint&)
{
    notImplemented();
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_impl = other.m_impl;
    return *this;
}

static Cursor globalCursor = const_cast<BCursor*>(B_CURSOR_SYSTEM_DEFAULT);
static Cursor ibeamCursor = const_cast<BCursor*>(B_CURSOR_I_BEAM);

const Cursor& pointerCursor()
{
    return globalCursor;
}

const Cursor& moveCursor()
{
    return globalCursor;
}

const Cursor& crossCursor()
{
    return globalCursor;
}

const Cursor& handCursor()
{
    return globalCursor;
}

const Cursor& iBeamCursor()
{
    return ibeamCursor;
}

const Cursor& waitCursor()
{
    return globalCursor;
}

const Cursor& helpCursor()
{
    return globalCursor;
}

const Cursor& eastResizeCursor()
{
    return globalCursor;
}

const Cursor& northResizeCursor()
{
    return globalCursor;
}

const Cursor& northEastResizeCursor()
{
    return globalCursor;
}

const Cursor& northWestResizeCursor()
{
    return globalCursor;
}

const Cursor& southResizeCursor()
{
    return globalCursor;
}

const Cursor& southEastResizeCursor()
{
    return globalCursor;
}

const Cursor& southWestResizeCursor()
{
    return globalCursor;
}

const Cursor& westResizeCursor()
{
    return globalCursor;
}

const Cursor& northSouthResizeCursor()
{
    return globalCursor;
}

const Cursor& eastWestResizeCursor()
{
    return globalCursor;
}

const Cursor& northEastSouthWestResizeCursor()
{
    return globalCursor;
}

const Cursor& northWestSouthEastResizeCursor()
{
    return globalCursor;
}

const Cursor& columnResizeCursor()
{
    return globalCursor;
}

const Cursor& rowResizeCursor()
{
    return globalCursor;
}

const Cursor& verticalTextCursor()
{
    return globalCursor;
}

const Cursor& cellCursor()
{
    return globalCursor;
}

const Cursor& contextMenuCursor()
{
    return globalCursor;
}

const Cursor& noDropCursor()
{
    return globalCursor;
}

const Cursor& copyCursor()
{
    return globalCursor;
}

const Cursor& progressCursor()
{
    return globalCursor;
}

const Cursor& aliasCursor()
{
    return globalCursor;
}

const Cursor& noneCursor()
{
    return globalCursor;
}

const Cursor& notAllowedCursor()
{
    return globalCursor;
}

const Cursor& zoomInCursor()
{
    return globalCursor;
}

const Cursor& zoomOutCursor()
{
    return globalCursor;
}

const Cursor& grabCursor()
{
    return globalCursor;
}

const Cursor& grabbingCursor()
{
    return globalCursor;
}

} // namespace WebCore

