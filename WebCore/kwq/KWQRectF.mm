/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
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
#import "KWQRectF.h"

#import <algorithm>

using std::max;
using std::min;

QRectF::QRectF() : xp(0.0f), yp(0.0f), w(0.0f), h(0.0f)
{
}

QRectF::QRectF(float x, float y, float width, float height) : xp(x), yp(y), w(width), h(height)
{
}

QRectF::QRectF(QPointF p, FloatSize s) : xp(p.x()), yp(p.y()), w(s.width()), h(s.height())
{
}

QRectF::QRectF(const QPointF &topLeft, const QPointF &bottomRight)
  : xp(topLeft.x()), yp(topLeft.y()),
  w(bottomRight.x() - topLeft.x() + 1.0f), h(bottomRight.y() - topLeft.y() + 1.0f)
{
}

QRectF::QRectF(const QRect& r) : xp(r.x()), yp(r.y()), w(r.width()), h(r.height())
{
}

#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
QRectF::QRectF(const NSRect &r) : xp(r.origin.x), yp(r.origin.y), w(r.size.width), h(r.size.height)
{
}
#endif

QRectF::QRectF(const CGRect &r) : xp(r.origin.x), yp(r.origin.y), w(r.size.width), h(r.size.height)
{
}

bool QRectF::isNull() const
{
    return w == 0.0f && h == 0.0f;
}

bool QRectF::isValid() const
{
    return w > 0.0f && h > 0.0f;
}

bool QRectF::isEmpty() const
{
    return w <= 0.0f || h <= 0.0f;
}

float QRectF::right() const
{
    return xp + w - 1.0f;
}

float QRectF::bottom() const
{
    return yp + h - 1.0f;
}

QPointF QRectF::topLeft() const
{
    return QPointF(xp,yp);
}

QPointF QRectF::topRight() const
{
    return QPointF(right(),top());
}

QPointF QRectF::bottomRight() const
{
    return QPointF(right(),bottom());
}

QPointF QRectF::bottomLeft() const
{
    return QPointF(left(),bottom());
}

FloatSize QRectF::size() const
{
    return FloatSize(w,h);
}

QRectF QRectF::unite(const QRectF &r) const
{
    if (r.isEmpty())
        return *this;

    if (isEmpty())
        return r;

    float nx, ny, nw, nh;

    nx = min(xp, r.xp);
    ny = min(yp, r.yp);

    if (xp + w >= r.xp + r.w)
        nw = xp + w - nx;
    else
        nw = r.xp + r.w - nx;

    if (yp + h >= r.yp + r.h)
        nh = yp + h - ny;
    else
        nh = r.yp + r.h - ny;

    return QRectF(nx, ny, nw, nh);
}

QRectF QRectF::normalize() const
{
    QRectF newRect;

    newRect.xp  = (w < 0.0f) ? (xp - w) : xp;
    newRect.w   = (w < 0.0f) ? -w : w;

    newRect.yp  = (h < 0.0f) ? (yp - h) : yp;
    newRect.h   = (h < 0.0f) ? -h : h;

    return newRect;
}

bool QRectF::intersects(const QRectF &r) const
{
    return intersect(r).isValid();
}

QRectF QRectF::intersect(const QRectF &r) const
{
    float nx, ny, nw, nh;

    nx = max(xp, r.xp);
    ny = max(yp, r.yp);

    if (xp + w <= r.xp + r.w)
        nw = xp + w - nx;
    else
        nw = r.xp + r.w - nx;

    if (yp + h <= r.yp + r.h)
        nh = yp + h - ny;
    else
        nh = r.yp + r.h - ny;

    return QRectF(nx, ny, nw, nh);
}

void QRectF::inflate(float s)
{
    xp -= s;
    yp -= s;
    w += 2.0f * s;
    h += 2.0f * s;
}

#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
QRectF::operator NSRect() const
{
    return NSMakeRect(xp, yp, w, h);
}
#endif

QRectF::operator CGRect() const
{
    return CGRectMake(xp, yp, w, h);
}

bool operator==(const QRectF& a, const QRectF &b)
{
    return a.xp == b.xp && a.yp == b.yp && a.w == b.w && a.h == b.h;
}

bool operator!=(const QRectF &a, const QRectF &b)
{
    return !(a == b);
}
