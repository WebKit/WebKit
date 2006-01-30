/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#import "KWQKCursor.h"

#import "KWQExceptions.h"

// Simple NSDictionary and NSCursor calls shouldn't need protection,
// but creating a cursor with a bad image might throw.

@interface KWQKCursorBundleDummy : NSObject { }
@end
@implementation KWQKCursorBundleDummy
@end

@interface NSCursor (WebCoreCursorAdditions)
+ (NSCursor *)_WebCore_cursorWithName:(NSString *)name hotSpot:(NSPoint)hotSpot;
@end

@implementation NSCursor (WebCoreCursorAdditions)

+ (NSCursor *)_WebCore_cursorWithName:(NSString *)name hotSpot:(NSPoint)hotSpot
{
    static NSMutableDictionary *nameToCursor = nil;
    if (!nameToCursor) {
        nameToCursor = [[NSMutableDictionary alloc] init];
    }
    
    NSCursor *cursor;
    KWQ_BLOCK_EXCEPTIONS;
    cursor = [nameToCursor objectForKey:name];
    if (!cursor) { 
        NSImage *cursorImage = [[NSImage alloc] initWithContentsOfFile:
            [[NSBundle bundleForClass:[KWQKCursorBundleDummy class]]
            pathForResource:name ofType:@"tiff"]];
        if (cursorImage) {
            cursor = [[NSCursor alloc] initWithImage:cursorImage hotSpot:hotSpot];
            [cursorImage release];
            [nameToCursor setObject:cursor forKey:name];
            [cursor release];
        }

    }
    return cursor;
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return nil;
}

@end

QCursor KCursor::arrowCursor() { return QCursor(); }
QCursor KCursor::crossCursor() { return QCursor::makeWithNSCursor([NSCursor crosshairCursor]); }
QCursor KCursor::handCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"linkCursor" hotSpot:NSMakePoint(6.0, 1.0)]); }
QCursor KCursor::sizeAllCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"moveCursor" hotSpot:NSMakePoint(7.0, 7.0)]); }
QCursor KCursor::sizeHorCursor() { return QCursor(); }
QCursor KCursor::sizeVerCursor() { return QCursor(); }
QCursor KCursor::sizeBDiagCursor() { return QCursor(); }
QCursor KCursor::sizeFDiagCursor() { return QCursor(); }
QCursor KCursor::ibeamCursor() { return QCursor::makeWithNSCursor([NSCursor IBeamCursor]); }
QCursor KCursor::waitCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"waitCursor" hotSpot:NSMakePoint(7.0, 7.0)]); }
QCursor KCursor::whatsThisCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"helpCursor" hotSpot:NSMakePoint(8.0, 8.0)]); }

QCursor KCursor::eastResizeCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"eastResizeCursor" hotSpot:NSMakePoint(14.0, 7.0)]); }
QCursor KCursor::northResizeCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"northResizeCursor" hotSpot:NSMakePoint(7.0, 1.0)]); }
QCursor KCursor::northEastResizeCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"northEastResizeCursor" hotSpot:NSMakePoint(14.0, 1.0)]); }
QCursor KCursor::northWestResizeCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"northWestResizeCursor" hotSpot:NSMakePoint(0.0, 0.0)]); }
QCursor KCursor::southResizeCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"southResizeCursor" hotSpot:NSMakePoint(7.0, 14.0)]); }
QCursor KCursor::southEastResizeCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"southEastResizeCursor" hotSpot:NSMakePoint(14.0, 14.0)]); }
QCursor KCursor::southWestResizeCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"southWestResizeCursor" hotSpot:NSMakePoint(1.0, 14.0)]); }
QCursor KCursor::westResizeCursor() { return QCursor::makeWithNSCursor([NSCursor _WebCore_cursorWithName:@"westResizeCursor" hotSpot:NSMakePoint(1.0, 7.0)]); }
