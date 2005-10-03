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
#import "KWQRegion.h"
#import "KWQFoundationExtras.h"

// None of the NSBezierPath calls here can possibly throw an NSException.
// Some path calls do this when the path is empty, but we always make
// those when the path is guaranteed non-empty.

QRegion::QRegion(const QRect &rect)
    : path(KWQRetain([NSBezierPath bezierPathWithRect:rect]))
{
}

QRegion::QRegion(int x, int y, int w, int h, RegionType t)
{
    if (t == Rectangle) {
        path = KWQRetain([NSBezierPath bezierPathWithRect:NSMakeRect(x, y, w, h)]);
    } else { // Ellipse
        path = KWQRetain([NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x, y, w, h)]);
    }
}

QRegion::QRegion(const QPointArray &arr)
{
    path = KWQRetainNSRelease([[NSBezierPath alloc] init]);
    [path moveToPoint:arr[0]];
    // the moveToPoint: guarantees the path is not empty, which means lineToPoint:
    // can't throw.
    for (uint i = 1; i < arr.count(); ++i) {
        [path lineToPoint:arr[i]];
    }
}

QRegion::~QRegion()
{
    KWQRelease(path);
}

QRegion::QRegion(const QRegion &other)
    : path(KWQRetainNSRelease([other.path copy]))
{
}

QRegion &QRegion::operator=(const QRegion &other)
{
    if (path == other.path) {
        return *this;
    }
    KWQRelease(path);
    path = KWQRetainNSRelease([other.path copy]);
    return *this;
}

bool QRegion::contains(const QPoint &point) const
{
    return [path containsPoint:point];
}

void QRegion::translate(int deltaX, int deltaY)
{
    NSAffineTransform *translation = [[NSAffineTransform alloc] init];
    [translation translateXBy:deltaX yBy:deltaY];
    [path transformUsingAffineTransform:translation];    
    [translation release];
}

QRect QRegion::boundingRect() const
{
    return path ? QRect([path bounds]) : QRect();
}
