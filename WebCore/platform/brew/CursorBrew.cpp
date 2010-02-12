/*
 * Copyright (C) 2010 Company 100, Inc.
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#define LOG_TAG "WebCore"

#include "config.h"
#include "Cursor.h"

#include "NotImplemented.h"

#include <wtf/StdLibExtras.h>

namespace WebCore {

Cursor::Cursor(Image*, const IntPoint&)
{
    notImplemented();
}

Cursor::Cursor(const Cursor&)
{
    notImplemented();
}

Cursor::~Cursor()
{
    notImplemented();
}

Cursor& Cursor::operator=(const Cursor&)
{
    notImplemented();
    return *this;
}

static inline Cursor& dummyCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, ());
    return c;
}

const Cursor& pointerCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& crossCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& handCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& moveCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& iBeamCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& waitCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& helpCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& eastResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& northResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& northEastResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& northWestResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& southResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& southEastResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& southWestResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& westResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& northSouthResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& eastWestResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& northEastSouthWestResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& northWestSouthEastResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& columnResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& rowResizeCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& verticalTextCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& cellCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& contextMenuCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& noDropCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& copyCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& progressCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& aliasCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& noneCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& notAllowedCursor()
{
    return dummyCursor();
}

const Cursor& zoomInCursor()
{
    return dummyCursor();
}

const Cursor& zoomOutCursor()
{
    return dummyCursor();
}

const Cursor& middlePanningCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& eastPanningCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& northPanningCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& northEastPanningCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& northWestPanningCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& southPanningCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& southEastPanningCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& southWestPanningCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& westPanningCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& grabCursor()
{
    notImplemented();
    return dummyCursor();
}

const Cursor& grabbingCursor()
{
    notImplemented();
    return dummyCursor();
}

} // namespace WebCore
