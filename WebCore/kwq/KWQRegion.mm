/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <qregion.h>

#include <kwqdebug.h>

const QRegion QRegion::null;

void QRegion::_initialize() {
    data = calloc(1, sizeof(struct KWQRegionData));
    data->type = Rectangle;
}

QRegion::QRegion()
{
    _initialize();
    data->path = [[NSBezierPath bezierPath] retain];
}

QRegion::QRegion(const QRect &rect)
{
    NSRect nsrect;

    _initialize();

    nsrect = NSMakeRect(rect.x(), rect.y(), rect.width(), rect.height());

    data->path = [[NSBezierPath bezierPathWithRect:nsrect] retain];
}

QRegion::QRegion(int x, int y, int w, int h, RegionType t=Rectangle)
{
    NSRect rect;
    
    _initialize();
    rect = NSMakeRect(x,y,w,h);

    if (t == Rectangle) {
        data->path = [[NSBezierPath bezierPathWithRect:rect] retain];
        data->type = Rectangle;
    }
    else { // Ellipse
        data->path = [[NSBezierPath bezierPathWithOvalInRect:rect] retain];
        data->type = Ellipse;
    }
}

QRegion::QRegion(const QPointArray &arr)
{
    _initialize();
    _logNotYetImplemented();
}

QRegion::QRegion(const QRegion &other)
{
    _initialize();
    data->path = [[NSBezierPath bezierPath] retain];
    [data->path appendBezierPath:other.data->path];
}

QRegion::~QRegion()
{
    [data->path release];
    free(data);
}

QRegion QRegion::intersect(const QRegion &region) const
{
    _logNotYetImplemented();
    return region;
}

bool QRegion::contains(const QPoint &point) const
{
    NSPoint nspoint;

    nspoint = NSMakePoint(point.x(), point.y());
    
    return [data->path containsPoint:nspoint] ? 1 : 0;
}

bool QRegion::isNull() const
{
    return [data->path elementCount] == 0 ? 1 : 0;
}

QRegion &QRegion::operator=(const QRegion &other)
{
    if (data->path) {
        [data->path release];
    }
    data->path = [[NSBezierPath bezierPath] retain];
    [data->path appendBezierPath:other.data->path];
    return *this;
}
