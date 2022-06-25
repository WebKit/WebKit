/*
 * Copyright (C) 2004-2016 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "IntSize.h"
#include <cmath>

#if USE(CG)
typedef struct CGPoint CGPoint;
#endif

#if !PLATFORM(IOS_FAMILY)
#if OS(DARWIN)
#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
#else
typedef struct _NSPoint NSPoint;
#endif
#endif
#endif // !PLATFORM(IOS_FAMILY)

#if PLATFORM(WIN)
typedef struct tagPOINT POINT;
typedef struct tagPOINTS POINTS;
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class FloatPoint;
class IntRect;

class IntPoint {
public:
    IntPoint() : m_x(0), m_y(0) { }
    IntPoint(int x, int y) : m_x(x), m_y(y) { }
    explicit IntPoint(const IntSize& size) : m_x(size.width()), m_y(size.height()) { }
    WEBCORE_EXPORT explicit IntPoint(const FloatPoint&); // don't do this implicitly since it's lossy

    static IntPoint zero() { return IntPoint(); }
    bool isZero() const { return !m_x && !m_y; }

    int x() const { return m_x; }
    int y() const { return m_y; }

    void setX(int x) { m_x = x; }
    void setY(int y) { m_y = y; }

    void move(const IntSize& s) { move(s.width(), s.height()); } 
    void moveBy(const IntPoint& offset) { move(offset.x(), offset.y()); }
    void move(int dx, int dy) { m_x += dx; m_y += dy; }
    void scale(float sx, float sy)
    {
        m_x = lroundf(static_cast<float>(m_x * sx));
        m_y = lroundf(static_cast<float>(m_y * sy));
    }

    void scale(float scale)
    {
        this->scale(scale, scale);
    }
    
    IntPoint expandedTo(const IntPoint& other) const
    {
        return {
            m_x > other.m_x ? m_x : other.m_x,
            m_y > other.m_y ? m_y : other.m_y
        };
    }

    IntPoint shrunkTo(const IntPoint& other) const
    {
        return {
            m_x < other.m_x ? m_x : other.m_x,
            m_y < other.m_y ? m_y : other.m_y
        };
    }

    WEBCORE_EXPORT IntPoint constrainedBetween(const IntPoint& min, const IntPoint& max) const;
    
    WEBCORE_EXPORT IntPoint constrainedWithin(const IntRect&) const;

    int distanceSquaredToPoint(const IntPoint&) const;

    void clampNegativeToZero()
    {
        *this = expandedTo(zero());
    }

    IntPoint transposedPoint() const
    {
        return IntPoint(m_y, m_x);
    }

#if USE(CG)
    WEBCORE_EXPORT explicit IntPoint(const CGPoint&); // don't do this implicitly since it's lossy
    WEBCORE_EXPORT operator CGPoint() const;
#endif

#if !PLATFORM(IOS_FAMILY)
#if OS(DARWIN) && !defined(NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES)
    WEBCORE_EXPORT explicit IntPoint(const NSPoint&); // don't do this implicitly since it's lossy
    WEBCORE_EXPORT operator NSPoint() const;
#endif
#endif // !PLATFORM(IOS_FAMILY)

#if PLATFORM(WIN)
    IntPoint(const POINT&);
    operator POINT() const;
    IntPoint(const POINTS&);
    operator POINTS() const;
#endif

private:
    int m_x, m_y;
};

inline IntPoint& operator+=(IntPoint& a, const IntSize& b)
{
    a.move(b.width(), b.height());
    return a;
}

inline IntPoint& operator-=(IntPoint& a, const IntSize& b)
{
    a.move(-b.width(), -b.height());
    return a;
}

inline IntPoint operator+(const IntPoint& a, const IntSize& b)
{
    return IntPoint(a.x() + b.width(), a.y() + b.height());
}

inline IntPoint operator+(const IntPoint& a, const IntPoint& b)
{
    return IntPoint(a.x() + b.x(), a.y() + b.y());
}

inline IntSize operator-(const IntPoint& a, const IntPoint& b)
{
    return IntSize(a.x() - b.x(), a.y() - b.y());
}

inline IntPoint operator-(const IntPoint& a, const IntSize& b)
{
    return IntPoint(a.x() - b.width(), a.y() - b.height());
}

inline IntPoint operator-(const IntPoint& point)
{
    return IntPoint(-point.x(), -point.y());
}

inline bool operator==(const IntPoint& a, const IntPoint& b)
{
    return a.x() == b.x() && a.y() == b.y();
}

inline bool operator!=(const IntPoint& a, const IntPoint& b)
{
    return a.x() != b.x() || a.y() != b.y();
}

inline IntSize toIntSize(const IntPoint& a)
{
    return IntSize(a.x(), a.y());
}

inline int IntPoint::distanceSquaredToPoint(const IntPoint& point) const
{
    return ((*this) - point).diagonalLengthSquared();
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const IntPoint&);

} // namespace WebCore

namespace WTF {
template<> struct DefaultHash<WebCore::IntPoint>;
template<> struct HashTraits<WebCore::IntPoint>;
}
