/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "Cursor.h"

namespace WebCore {

Cursor::Cursor(Image* /*image*/)
    : m_impl(0)
{
    // FIXME: Implement custom cursors.
}

Cursor::Cursor(const Cursor& other)
    : m_impl(other.m_impl)
{
}

Cursor::~Cursor()
{
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_impl = other.m_impl;
    return *this;
}

Cursor::Cursor(HCURSOR c)
    : m_impl(c)
{
}

const Cursor& crossCursor()
{
    static Cursor c = LoadCursor(0, IDC_CROSS);
    return c;
}

const Cursor& handCursor()
{
    static Cursor c = LoadCursor(0, IDC_HAND);
    return c;
}

const Cursor& moveCursor()
{
    static Cursor c; // FIXME
    return c;
}

const Cursor& iBeamCursor()
{
    static Cursor c = LoadCursor(0, IDC_IBEAM);
    return c;
}

const Cursor& waitCursor()
{
    static Cursor c = LoadCursor(0, IDC_WAIT);
    return c;
}

const Cursor& helpCursor()
{
    static Cursor c = LoadCursor(0, IDC_HELP);
    return c;
}

const Cursor& eastResizeCursor()
{
    static Cursor c; // FIXME
    return c;
}

const Cursor& northResizeCursor()
{
    static Cursor c; // FIXME
    return c;
}

const Cursor& northEastResizeCursor()
{
    static Cursor c; // FIXME
    return c;
}

const Cursor& northWestResizeCursor()
{
    static Cursor c; // FIXME
    return c;
}

const Cursor& southResizeCursor()
{
    static Cursor c; // FIXME
    return c;
}

const Cursor& southEastResizeCursor()
{
    static Cursor c; // FIXME
    return c;
}

const Cursor& southWestResizeCursor()
{
    static Cursor c; // FIXME
    return c;
}

const Cursor& westResizeCursor()
{
    static Cursor c; // FIXME
    return c;
}

}
