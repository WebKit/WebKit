/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <qregion.h>

#import <kwqdebug.h>

QRegion::QRegion()
    : paths([[NSArray alloc] init])
{
}

QRegion::QRegion(const QRect &rect)
    : paths([[NSArray arrayWithObject:[NSBezierPath bezierPathWithRect:rect]] retain])
{
}

QRegion::QRegion(int x, int y, int w, int h, RegionType t)
{
    if (t == Rectangle) {
        paths = [[NSArray arrayWithObject:[NSBezierPath bezierPathWithRect:NSMakeRect(x, y, w, h)]] retain];
    } else { // Ellipse
        paths = [[NSArray arrayWithObject:[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(x, y, w, h)]] retain];
    }
}

QRegion::QRegion(const QPointArray &arr)
{
    NSBezierPath *path = [[NSBezierPath alloc] init];
    [path moveToPoint:arr[0]];
    for (uint i = 1; i < arr.count(); ++i) {
        [path lineToPoint:arr[i]];
    }
    paths = [[NSArray arrayWithObject:path] retain];
    [path release];
}

QRegion::QRegion(NSArray *array)
    : paths([[NSArray alloc] initWithArray:array copyItems:true])
{
}

QRegion::~QRegion()
{
    [paths release];
}

QRegion::QRegion(const QRegion &other)
    : paths([[NSArray alloc] initWithArray:other.paths copyItems:true])
{
}

QRegion &QRegion::operator=(const QRegion &other)
{
    if (this == &other)
        return *this;
    [paths release];
    paths = [[NSArray alloc] initWithArray:other.paths copyItems:true];
    return *this;
}

QRegion QRegion::intersect(const QRegion &other) const
{
    return [paths arrayByAddingObjectsFromArray:other.paths];
}

bool QRegion::contains(const QPoint &point) const
{
    if ([paths count] == 0) {
        return false;
    }
    NSEnumerator *e = [paths objectEnumerator];
    NSBezierPath *path;
    while ((path = [e nextObject])) {
        if (![path containsPoint:point]) {
            return false;
        }
    }
    return true;
}

bool QRegion::isNull() const
{
    // FIXME: Note that intersection can lead to an empty QRegion that
    // still won't return true for isNull. But this doesn't matter since
    // intersection is hardly used in KHTML, and never with isNull.
    if ([paths count] == 0) {
        return true;
    }
    NSEnumerator *e = [paths objectEnumerator];
    NSBezierPath *path;
    while ((path = [e nextObject])) {
        if ([path isEmpty]) {
            return true;
        }
    }
    return false;
}

void QRegion::translate(int deltaX, int deltaY)
{
    NSAffineTransform *translation = [[NSAffineTransform alloc] init];
    [translation translateXBy:deltaX yBy:deltaY];
    [paths makeObjectsPerformSelector:@selector(transformUsingAffineTransform:) withObject:translation];    
    [translation release];
}

QRect QRegion::boundingRect() const
{
    // Note that this returns the intersection of the bounds of all the paths.
    // That's not quite the same as the bounds of the intersection of all the
    // paths, but that doesn't matter because intersection is hardly used at all.
    
    NSEnumerator *e = [paths objectEnumerator];
    NSBezierPath *path = [e nextObject];
    if (path == nil) {
	return QRect();
    }
    NSRect bounds = [path bounds];
    while ((path = [e nextObject])) {
        bounds = NSIntersectionRect(bounds, [path bounds]);
    }
    return QRect(bounds);
}

void QRegion::setClip() const
{
    NSEnumerator *e = [paths objectEnumerator];
    NSBezierPath *path = [e nextObject];
    if (path == nil) {
        [[NSBezierPath bezierPath] setClip];
        return;
    }
    [path setClip];
    NSRect bounds = [path bounds];
    while ((path = [e nextObject])) {
        [path addClip];
    }
}
