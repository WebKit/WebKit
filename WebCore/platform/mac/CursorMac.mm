/*
 * Copyright (C) 2004, 2006 Apple Inc. All rights reserved.
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

#import "BlockExceptions.h"
#import "FoundationExtras.h"
#import "Image.h"
#import "IntPoint.h"
#import <wtf/StdLibExtras.h>

@interface WebCoreCursorBundle : NSObject { }
@end

@implementation WebCoreCursorBundle
@end

namespace WebCore {

// Simple NSCursor calls shouldn't need protection,
// but creating a cursor with a bad image might throw.

static NSCursor* createCustomCursor(Image* image, const IntPoint& hotspot)
{
    // FIXME: The cursor won't animate.  Not sure if that's a big deal.
    NSImage* img = image->getNSImage();
    if (!img)
        return 0;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[NSCursor alloc] initWithImage:img hotSpot:hotspot];
    END_BLOCK_OBJC_EXCEPTIONS;
    return 0;
}

// Leak these cursors intentionally, that way we won't waste time trying to clean them
// up at process exit time.
static NSCursor* leakNamedCursor(const char* name, int x, int y)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSString* resourceName = [[NSString alloc] initWithUTF8String:name];
    NSImage* cursorImage = [[NSImage alloc] initWithContentsOfFile:
        [[NSBundle bundleForClass:[WebCoreCursorBundle class]]
        pathForResource:resourceName ofType:@"png"]];
    [resourceName release];
    NSCursor* cursor = 0;
    if (cursorImage) {
        NSPoint hotSpotPoint = {x, y}; // workaround for 4213314
        cursor = [[NSCursor alloc] initWithImage:cursorImage hotSpot:hotSpotPoint];
        [cursorImage release];
    }
    return cursor;
    END_BLOCK_OBJC_EXCEPTIONS;
    return nil;
}

Cursor::Cursor(Image* image, const IntPoint& hotspot)
    : m_impl(HardRetainWithNSRelease(createCustomCursor(image, hotspot)))
{
}

Cursor::Cursor(const Cursor& other)
    : m_impl(HardRetain(other.m_impl))
{
}

Cursor::~Cursor()
{
    HardRelease(m_impl);
}

Cursor& Cursor::operator=(const Cursor& other)
{
    HardRetain(other.m_impl);
    HardRelease(m_impl);
    m_impl = other.m_impl;
    return *this;
}

Cursor::Cursor(NSCursor* c)
    : m_impl(HardRetain(c))
{
}

const Cursor& pointerCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, ([NSCursor arrowCursor]));
    return c;
}

const Cursor& crossCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("crossHairCursor", 11, 11)));
    return c;
}

const Cursor& handCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("linkCursor", 6, 1)));
    return c;
}

const Cursor& moveCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("moveCursor", 7, 7)));
    return c;
}

const Cursor& verticalTextCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("verticalTextCursor", 7, 7)));
    return c;
}

const Cursor& cellCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("cellCursor", 7, 7)));
    return c;
}

const Cursor& contextMenuCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("contextMenuCursor", 3, 2)));
    return c;
}

const Cursor& aliasCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("aliasCursor", 11, 3)));
    return c;
}

const Cursor& zoomInCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("zoomInCursor", 7, 7)));
    return c;
}

const Cursor& zoomOutCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("zoomOutCursor", 7, 7)));
    return c;
}

const Cursor& copyCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("copyCursor", 3, 2)));
    return c;
}

const Cursor& noneCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("noneCursor", 7, 7)));
    return c;
}

const Cursor& progressCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("progressCursor", 3, 2)));
    return c;
}

const Cursor& noDropCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("noDropCursor", 3, 1)));
    return c;
}

const Cursor& notAllowedCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("notAllowedCursor", 11, 11)));
    return c;
}

const Cursor& iBeamCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, ([NSCursor IBeamCursor]));
    return c;
}

const Cursor& waitCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("waitCursor", 7, 7)));
    return c;
}

const Cursor& helpCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("helpCursor", 8, 8)));
    return c;
}

const Cursor& eastResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("eastResizeCursor", 14, 7)));
    return c;
}

const Cursor& northResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("northResizeCursor", 7, 1)));
    return c;
}

const Cursor& northEastResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("northEastResizeCursor", 14, 1)));
    return c;
}

const Cursor& northWestResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("northWestResizeCursor", 0, 0)));
    return c;
}

const Cursor& southResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("southResizeCursor", 7, 14)));
    return c;
}

const Cursor& southEastResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("southEastResizeCursor", 14, 14)));
    return c;
}

const Cursor& southWestResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("southWestResizeCursor", 1, 14)));
    return c;
}

const Cursor& westResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("westResizeCursor", 1, 7)));
    return c;
}

const Cursor& northSouthResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("northSouthResizeCursor", 7, 7)));
    return c;
}

const Cursor& eastWestResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("eastWestResizeCursor", 7, 7)));
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("northEastSouthWestResizeCursor", 7, 7)));
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, (leakNamedCursor("northWestSouthEastResizeCursor", 7, 7)));
    return c;
}

const Cursor& columnResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, ([NSCursor resizeLeftRightCursor]));
    return c;
}

const Cursor& rowResizeCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, ([NSCursor resizeUpDownCursor]));
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

const Cursor& grabCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, ([NSCursor openHandCursor]));
    return c;
}

const Cursor& grabbingCursor()
{
    DEFINE_STATIC_LOCAL(Cursor, c, ([NSCursor closedHandCursor]));
    return c;
}

}
