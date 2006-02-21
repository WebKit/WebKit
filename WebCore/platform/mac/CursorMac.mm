/*
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

#import "config.h"
#import "Cursor.h"

#import "KWQExceptions.h"
#import "KWQFoundationExtras.h"
#import "Image.h"

@interface WebCoreCursorBundle : NSObject { }
@end

@implementation WebCoreCursorBundle
@end

namespace WebCore {

// Simple NSCursor calls shouldn't need protection,
// but creating a cursor with a bad image might throw.

static NSCursor* createCustomCursor(Image* image)
{
    // FIXME: The cursor won't animate.  Not sure if that's a big deal.
    NSImage* img = image->getNSImage();
    if (!img)
        return 0;
    KWQ_BLOCK_EXCEPTIONS;
    return [[NSCursor alloc] initWithImage:img hotSpot:NSZeroPoint];
    KWQ_UNBLOCK_EXCEPTIONS;
    return 0;
}

// Leak these cursors intentionally, that way we won't waste time trying to clean them
// up at process exit time.
static NSCursor* leakNamedCursor(const char* name, int x, int y)
{
    KWQ_BLOCK_EXCEPTIONS;
    NSString* resourceName = [[NSString alloc] initWithUTF8String:name];
    NSImage* cursorImage = [[NSImage alloc] initWithContentsOfFile:
        [[NSBundle bundleForClass:[WebCoreCursorBundle class]]
        pathForResource:resourceName ofType:@"tiff"]];
    [resourceName release];
    NSCursor* cursor = 0;
    if (cursorImage) {
        cursor = [[NSCursor alloc] initWithImage:cursorImage hotSpot:NSMakePoint(x, y)];
        [cursorImage release];
    }
    return cursor;
    KWQ_UNBLOCK_EXCEPTIONS;
    return 0;
}

Cursor::Cursor(Image* image)
    : m_impl(KWQRetainNSRelease(createCustomCursor(image)))
{
}

Cursor::Cursor(const Cursor& other)
    : m_impl(KWQRetain(other.m_impl))
{
}

Cursor::~Cursor()
{
    KWQRelease(m_impl);
}

Cursor& Cursor::operator=(const Cursor& other)
{
    KWQRetain(other.m_impl);
    KWQRelease(m_impl);
    m_impl = other.m_impl;
    return *this;
}

Cursor::Cursor(NSCursor* c)
    : m_impl(KWQRetain(c))
{
}

const Cursor& crossCursor()
{
    static Cursor c = [NSCursor crosshairCursor];
    return c;
}

const Cursor& handCursor()
{
    // FIXME: Use [NSCursor pointingHandCursor]?
    static Cursor c = leakNamedCursor("linkCursor", 6, 1);
    return c;
}

const Cursor& moveCursor()
{
    static Cursor c = leakNamedCursor("moveCursor", 7, 7);
    return c;
}

const Cursor& iBeamCursor()
{
    static Cursor c = [NSCursor IBeamCursor];
    return c;
}

const Cursor& waitCursor()
{
    static Cursor c = leakNamedCursor("waitCursor", 7, 7);
    return c;
}

const Cursor& helpCursor()
{
    static Cursor c = leakNamedCursor("helpCursor", 8, 8);
    return c;
}

const Cursor& eastResizeCursor()
{
    static Cursor c = leakNamedCursor("eastResizeCursor", 14, 7);
    return c;
}

const Cursor& northResizeCursor()
{
    static Cursor c = leakNamedCursor("northResizeCursor", 7, 1);
    return c;
}

const Cursor& northEastResizeCursor()
{
    static Cursor c = leakNamedCursor("northEastResizeCursor", 14, 1);
    return c;
}

const Cursor& northWestResizeCursor()
{
    static Cursor c = leakNamedCursor("northWestResizeCursor", 0, 0);
    return c;
}

const Cursor& southResizeCursor()
{
    static Cursor c = leakNamedCursor("southResizeCursor", 7, 14);
    return c;
}

const Cursor& southEastResizeCursor()
{
    static Cursor c = leakNamedCursor("southEastResizeCursor", 14, 14);
    return c;
}

const Cursor& southWestResizeCursor()
{
    static Cursor c = leakNamedCursor("southWestResizeCursor", 1, 14);
    return c;
}

const Cursor& westResizeCursor()
{
    static Cursor c = leakNamedCursor("westResizeCursor", 1, 7);
    return c;
}

}
