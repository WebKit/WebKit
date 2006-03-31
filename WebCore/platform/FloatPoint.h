/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef FLOATPOINT_H_
#define FLOATPOINT_H_

#include "FloatSize.h"

#if __APPLE__

typedef struct CGPoint CGPoint;

// workaround for <rdar://problem/4294625>
#if !__LP64__ && !NS_BUILD_32_LIKE_64
#undef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
#endif

#if NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
#else
typedef struct _NSPoint NSPoint;
#endif

#endif

namespace WebCore {

class IntPoint;

class FloatPoint {
public:
    FloatPoint() : m_x(0), m_y(0) { }
    FloatPoint(float x, float y) : m_x(x), m_y(y) { }
    FloatPoint(const IntPoint&);

    float x() const { return m_x; }
    float y() const { return m_y; }

    void setX(float x) { m_x = x; }
    void setY(float y) { m_y = y; }
    void move(float dx, float dy) { m_x += dx; m_y += dy; }

#if __APPLE__

    FloatPoint(const CGPoint&);
    operator CGPoint() const;

#if !NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
    FloatPoint(const NSPoint&);
    operator NSPoint() const;
#endif

#endif

private:
    float m_x, m_y;
};

inline FloatPoint& operator+=(FloatPoint& a, const FloatSize& b)
{
    a.move(b.width(), b.height());
    return a;
}

inline FloatPoint& operator-=(FloatPoint& a, const FloatSize& b)
{
    a.move(-b.width(), -b.height());
    return a;
}

inline FloatPoint operator+(const FloatPoint& a, const FloatSize& b)
{
    return FloatPoint(a.x() + b.width(), a.y() + b.height());
}

inline FloatSize operator-(const FloatPoint& a, const FloatPoint& b)
{
    return FloatSize(a.x() - b.x(), a.y() - b.y());
}

inline FloatPoint operator-(const FloatPoint& a, const FloatSize& b)
{
    return FloatPoint(a.x() - b.width(), a.y() - b.height());
}

inline bool operator==(const FloatPoint& a, const FloatPoint& b)
{
    return a.x() == b.x() && a.y() == b.y();
}

inline bool operator!=(const FloatPoint& a, const FloatPoint& b)
{
    return a.x() != b.x() || a.y() != b.y();
}

}

// FIXME: Remove when everything is in the WebCore namespace.
using WebCore::FloatPoint;

#endif
