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
#include "KWQRegion.h"

#include <ApplicationServices/ApplicationServices.h>

QRegion::QRegion(const IntRect &rect)
{
    path = CGPathCreateMutable();
    CGPathAddRect(path, 0, rect);
}

QRegion::QRegion(int x, int y, int w, int h, RegionType t)
{
    path = CGPathCreateMutable();
    if (t == Rectangle)
        CGPathAddRect(path, 0, CGRectMake(x, y, w, h));
    else // Ellipse
        CGPathAddEllipseInRect(path, 0, CGRectMake(x, y, w, h));
}

QRegion::QRegion(const IntPointArray &arr)
{
    path = CGPathCreateMutable();
    CGPathMoveToPoint(path, 0, arr[0].x(), arr[0].y());
    for (uint i = 1; i < arr.count(); ++i)
        CGPathAddLineToPoint(path, 0, arr[i].x(), arr[i].y());
    CGPathCloseSubpath(path);
}

QRegion::~QRegion()
{
    CGPathRelease(path);
}

QRegion::QRegion(const QRegion &other)
    : path(CGPathCreateMutableCopy(other.path))
{
}

QRegion &QRegion::operator=(const QRegion &other)
{
    if (path == other.path) {
        return *this;
    }
    CGPathRelease(path);
    path = CGPathCreateMutableCopy(other.path);
    return *this;
}

bool QRegion::contains(const IntPoint &point) const
{
    return CGPathContainsPoint(path, 0, point, false);
}

void QRegion::translate(int deltaX, int deltaY)
{
    CGAffineTransform translation = CGAffineTransformMake(1, 0, 0, 1, deltaX, deltaY);
    CGMutablePathRef newPath = CGPathCreateMutable();
    CGPathAddPath(newPath, &translation, path);
    CGPathRelease(path);
    path = newPath;
}

IntRect QRegion::boundingRect() const
{
    return path ? IntRect(CGPathGetBoundingBox(path)) : IntRect();
}
