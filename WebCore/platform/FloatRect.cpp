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
#include "FloatRect.h"
#include <algorithm>

using std::max;
using std::min;

namespace WebCore {

FloatRect::FloatRect() : xp(0.0f), yp(0.0f), w(0.0f), h(0.0f)
{
}

FloatRect::FloatRect(float x, float y, float width, float height) : xp(x), yp(y), w(width), h(height)
{
}

FloatRect::FloatRect(FloatPoint p, FloatSize s) : xp(p.x()), yp(p.y()), w(s.width()), h(s.height())
{
}

FloatRect::FloatRect(const FloatPoint &topLeft, const FloatPoint &bottomRight)
  : xp(topLeft.x()), yp(topLeft.y()),
  w(bottomRight.x() - topLeft.x() + 1.0f), h(bottomRight.y() - topLeft.y() + 1.0f)
{
}

FloatRect::FloatRect(const IntRect& r) : xp(r.x()), yp(r.y()), w(r.width()), h(r.height())
{
}

bool FloatRect::isNull() const
{
    return w == 0.0f && h == 0.0f;
}

bool FloatRect::isValid() const
{
    return w > 0.0f && h > 0.0f;
}

bool FloatRect::isEmpty() const
{
    return w <= 0.0f || h <= 0.0f;
}

float FloatRect::right() const
{
    return xp + w - 1.0f;
}

float FloatRect::bottom() const
{
    return yp + h - 1.0f;
}

FloatPoint FloatRect::topLeft() const
{
    return FloatPoint(xp,yp);
}

FloatPoint FloatRect::topRight() const
{
    return FloatPoint(right(),top());
}

FloatPoint FloatRect::bottomRight() const
{
    return FloatPoint(right(),bottom());
}

FloatPoint FloatRect::bottomLeft() const
{
    return FloatPoint(left(),bottom());
}

FloatSize FloatRect::size() const
{
    return FloatSize(w,h);
}

FloatRect FloatRect::unite(const FloatRect &r) const
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

    return FloatRect(nx, ny, nw, nh);
}

FloatRect FloatRect::normalize() const
{
    FloatRect newRect;

    newRect.xp  = (w < 0.0f) ? (xp - w) : xp;
    newRect.w   = (w < 0.0f) ? -w : w;

    newRect.yp  = (h < 0.0f) ? (yp - h) : yp;
    newRect.h   = (h < 0.0f) ? -h : h;

    return newRect;
}

bool FloatRect::intersects(const FloatRect &r) const
{
    return intersect(r).isValid();
}

FloatRect FloatRect::intersect(const FloatRect &r) const
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

    return FloatRect(nx, ny, nw, nh);
}

void FloatRect::inflate(float s)
{
    xp -= s;
    yp -= s;
    w += 2.0f * s;
    h += 2.0f * s;
}

bool operator==(const FloatRect& a, const FloatRect &b)
{
    return a.xp == b.xp && a.yp == b.yp && a.w == b.w && a.h == b.h;
}

bool operator!=(const FloatRect &a, const FloatRect &b)
{
    return !(a == b);
}

}
