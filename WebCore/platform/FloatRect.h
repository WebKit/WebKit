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

#ifndef FLOATRECTF_H_
#define FLOATRECTF_H_

#include <math.h>
#include "FloatSize.h"
#include "FloatPoint.h"
#include "IntRect.h"

#if __APPLE__
#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGRect NSRect;
#else
typedef struct _NSRect NSRect;
#endif
typedef struct CGRect CGRect;
#endif

namespace WebCore {

class IntRect;

class FloatRect {
public:
    FloatRect();
    FloatRect(FloatPoint p, FloatSize s);
    FloatRect(float, float, float, float);
    FloatRect(const FloatPoint&, const FloatPoint&);
    FloatRect(const IntRect&);
    
#if __APPLE__
#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
    explicit FloatRect(const NSRect&);
#endif
    explicit FloatRect(const CGRect&);
#endif

    bool isNull() const;
    bool isValid() const;
    bool isEmpty() const;

    float x() const { return xp; }
    float y() const { return yp; }
    float left() const { return xp; }
    float top() const { return yp; }
    float right() const;
    float bottom() const;
    float width() const { return w; }
    float height() const { return h; }

    FloatPoint topLeft() const;
    FloatPoint topRight() const;
    FloatPoint bottomRight() const;
    FloatPoint bottomLeft() const;

    FloatSize size() const;
    void setX(float x) { xp = x; }
    void setY(float y) { yp = y; }
    void setWidth(float width) { w = width; }
    void setHeight(float height) { h = height; }
    void setRect(float x, float y, float width, float height) { xp = x; yp = y; w = width; h = height; }
    FloatRect intersect(const FloatRect&) const;
    bool intersects(const FloatRect&) const;
    FloatRect unite(const FloatRect&) const;
    FloatRect normalize() const;

    bool contains(const FloatPoint& point) const { return contains(point.x(), point.y()); }

    bool contains(float x, float y, bool proper = false) const
    {
        if (proper)
            return x > xp && (x < (xp + w - 1)) && y > yp && y < (yp + h - 1);
        return x >= xp && x < (xp + w) && y >= yp && y < (yp + h);
    }

    bool contains(const FloatRect& rect) const { return intersect(rect) == rect; }

    void inflate(float s);

    inline FloatRect operator&(const FloatRect& r) const { return intersect(r); }

#if __APPLE__
#ifndef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
    operator NSRect() const;
#endif
    operator CGRect() const;
#endif

private:
    float xp;
    float yp;
    float w;
    float h;

    friend bool operator==(const FloatRect&, const FloatRect&);
    friend bool operator!=(const FloatRect&, const FloatRect&);
};

inline IntRect enclosingIntRect(const FloatRect& fr)
{
    int x = int(floor(fr.x()));
    int y = int(floor(fr.y()));
    return IntRect(x, y, int(ceil(fr.x() + fr.width())) - x, int(ceil(fr.y() + fr.height())) - y);
}

}

// FIXME: Remove when everything is in the WebCore namespace.
using WebCore::FloatRect;

#endif
