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
#import "KWQRect.h"

#import <algorithm>

using std::max;
using std::min;

QRect::QRect() : xp(0), yp(0), w(0), h(0)
{
}

QRect::QRect(int x, int y, int width, int height) : xp(x), yp(y), w(width), h(height)
{
}

QRect::QRect(QPoint p, IntSize s) : xp(p.x()), yp(p.y()), w(s.width()), h(s.height())
{
}

QRect::QRect(const QPoint &topLeft, const QPoint &bottomRight)
{
    xp = topLeft.x();
    yp = topLeft.y();
    w = bottomRight.x() - topLeft.x() + 1;
    h = bottomRight.y() - topLeft.y() + 1;
}

#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
QRect::QRect(const NSRect &r) : xp((int)r.origin.x), yp((int)r.origin.y), w((int)r.size.width), h((int)r.size.height)
{
}
#endif

QRect::QRect(const CGRect &r) : xp((int)r.origin.x), yp((int)r.origin.y), w((int)r.size.width), h((int)r.size.height)
{
}

bool QRect::isNull() const
{
    return w == 0 && h == 0;
}

bool QRect::isValid() const
{
    return w > 0 && h > 0;
}

bool QRect::isEmpty() const
{
    return w <= 0 || h <= 0;
}

int QRect::right() const
{
    return xp + w - 1;
}

int QRect::bottom() const
{
    return yp + h - 1;
}

QPoint QRect::topLeft() const
{
    return QPoint(xp,yp);
}

QPoint QRect::topRight() const
{
    return QPoint(right(),top());
}

QPoint QRect::bottomRight() const
{
    return QPoint(right(),bottom());
}

QPoint QRect::bottomLeft() const
{
    return QPoint(left(),bottom());
}

IntSize QRect::size() const
{
    return IntSize(w,h);
}

QRect QRect::unite(const QRect &r) const
{
    if (r.isEmpty())
        return *this;
    
    if (isEmpty())
        return r;

    int nx, ny, nw, nh;

    nx = min(xp, r.xp);
    ny = min(yp, r.yp);

    if (xp + w >= r.xp + r.w) {
        nw = xp + w - nx;
    } else {
        nw = r.xp + r.w - nx; 
    }

    if (yp + h >= r.yp + r.h) {
        nh = yp + h - ny;
    } else {
        nh = r.yp + r.h - ny; 
    }

    return QRect(nx, ny, nw, nh);
}

QRect QRect::normalize() const
{
    QRect newRect;
    
    newRect.xp  = (w < 0) ? (xp - w) : xp;
    newRect.w   = (w < 0) ? -w : w;
    
    newRect.yp  = (h < 0) ? (yp - h) : yp;
    newRect.h   = (h < 0) ? -h : h;
    
    return newRect;
}

bool QRect::intersects(const QRect &r) const
{
    return intersect(r).isValid();
}

QRect QRect::intersect(const QRect &r) const
{
    int nx, ny, nw, nh;

    nx = max(xp, r.xp);
    ny = max(yp, r.yp);

    if (xp + w <= r.xp + r.w) {
        nw = xp + w - nx;
    } else {
        nw = r.xp + r.w - nx; 
    }

    if (yp + h <= r.yp + r.h) {
        nh = yp + h - ny;
    } else {
        nh = r.yp + r.h - ny; 
    }

    return QRect(nx, ny, nw, nh);
}

void QRect::inflate(int s)
{
    xp -= s;
    yp -= s;
    w += 2*s;
    h += 2*s;
}

#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
QRect::operator NSRect() const
{
    return NSMakeRect(xp, yp, w, h);
}
#endif

QRect::operator CGRect() const
{
    return CGRectMake(xp, yp, w, h);
}

bool operator==(const QRect &a, const QRect &b)
{
    return a.xp == b.xp && a.yp == b.yp && a.w == b.w && a.h == b.h;
}

bool operator!=(const QRect &a, const QRect &b)
{
    return !(a == b);
}
