/*
 * Copyright 2009, The Android Open Source Project
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

static Cursor c;
const Cursor& pointerCursor()
{
    notImplemented();
    return c;
}

const Cursor& crossCursor()
{
    notImplemented();
    return c;
}

const Cursor& handCursor()
{
    notImplemented();
    return c;
}

const Cursor& moveCursor()
{
    notImplemented();
    return c;
}

const Cursor& iBeamCursor()
{
    notImplemented();
    return c;
}

const Cursor& waitCursor()
{
    notImplemented();
    return c;
}

const Cursor& helpCursor()
{
    notImplemented();
    return c;
}

const Cursor& eastResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& northResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& northEastResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& northWestResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& southResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& southEastResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& southWestResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& westResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& northSouthResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& eastWestResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& columnResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& rowResizeCursor()
{
    notImplemented();
    return c;
}

const Cursor& verticalTextCursor()
{
    notImplemented();
    return c;
}

const Cursor& cellCursor()
{
    notImplemented();
    return c;
}

const Cursor& contextMenuCursor()
{
    notImplemented();
    return c;
}

const Cursor& noDropCursor()
{
    notImplemented();
    return c;
}

const Cursor& copyCursor()
{
    notImplemented();
    return c;
}

const Cursor& progressCursor()
{
    notImplemented();
    return c;
}

const Cursor& aliasCursor()
{
    notImplemented();
    return c;
}

const Cursor& noneCursor()
{
    notImplemented();
    return c;
}

const Cursor& middlePanningCursor()
{
    notImplemented();
    return c;
}

const Cursor& eastPanningCursor()
{
    notImplemented();
    return c;
}

const Cursor& northPanningCursor()
{
    notImplemented();
    return c;
}

const Cursor& northEastPanningCursor()
{
    notImplemented();
    return c;
}

const Cursor& northWestPanningCursor()
{
    notImplemented();
    return c;
}

const Cursor& southPanningCursor()
{
    notImplemented();
    return c;
}

const Cursor& southEastPanningCursor()
{
    notImplemented();
    return c;
}

const Cursor& southWestPanningCursor()
{
    notImplemented();
    return c;
}

const Cursor& westPanningCursor()
{
    notImplemented();
    return c;
}

const Cursor& grabCursor() {
    notImplemented();
    return c;
}

const Cursor& grabbingCursor() {
    notImplemented();
    return c;
}

} // namespace WebCore
