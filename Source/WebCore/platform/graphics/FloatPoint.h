/*
 * Copyright (C) 2004-2016 Apple Inc.  All rights reserved.
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

#include "FloatSize.h"
#include "IntPoint.h"
#include <wtf/MathExtras.h>

#if PLATFORM(MAC) && defined __OBJC__
#import <Foundation/NSGeometry.h>
#endif

#if USE(CG)
typedef struct CGPoint CGPoint;
#endif

#if PLATFORM(MAC)
#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
#else
typedef struct _NSPoint NSPoint;
#endif
#endif // PLATFORM(MAC)

#if PLATFORM(WIN)
struct D2D_POINT_2F;
typedef D2D_POINT_2F D2D1_POINT_2F;
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class AffineTransform;
class TransformationMatrix;
class IntPoint;
class IntSize;

class FloatPoint {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FloatPoint() { }
    FloatPoint(float x, float y) : m_x(x), m_y(y) { }
    WEBCORE_EXPORT FloatPoint(const IntPoint&);
    explicit FloatPoint(const FloatSize& size) : m_x(size.width()), m_y(size.height()) { }

    static FloatPoint zero() { return FloatPoint(); }
    bool isZero() const { return !m_x && !m_y; }

    WEBCORE_EXPORT static FloatPoint narrowPrecision(double x, double y);

    float x() const { return m_x; }
    float y() const { return m_y; }

    void setX(float x) { m_x = x; }
    void setY(float y) { m_y = y; }

    void set(float x, float y)
    {
        m_x = x;
        m_y = y;
    }

    void move(float dx, float dy)
    {
        m_x += dx;
        m_y += dy;
    }

    void move(const IntSize& a)
    {
        m_x += a.width();
        m_y += a.height();
    }

    void move(const FloatSize& a)
    {
        m_x += a.width();
        m_y += a.height();
    }

    void moveBy(const IntPoint& a)
    {
        m_x += a.x();
        m_y += a.y();
    }

    void moveBy(const FloatPoint& a)
    {
        m_x += a.x();
        m_y += a.y();
    }

    void scale(float scale)
    {
        m_x *= scale;
        m_y *= scale;
    }

    void scale(float scaleX, float scaleY)
    {
        m_x *= scaleX;
        m_y *= scaleY;
    }

    FloatPoint scaled(float scale) const
    {
        return { m_x * scale, m_y * scale };
    }

    FloatPoint scaled(float scaleX, float scaleY) const
    {
        return { m_x * scaleX, m_y * scaleY };
    }

    WEBCORE_EXPORT void normalize();

    float dot(const FloatPoint& a) const
    {
        return m_x * a.x() + m_y * a.y();
    }

    float slopeAngleRadians() const;

    float length() const
    {
        return std::hypot(m_x, m_y);
    }

    float lengthSquared() const
    {
        return m_x * m_x + m_y * m_y;
    }

    WEBCORE_EXPORT FloatPoint constrainedBetween(const FloatPoint& min, const FloatPoint& max) const;

    FloatPoint shrunkTo(const FloatPoint& other) const
    {
        return { std::min(m_x, other.m_x), std::min(m_y, other.m_y) };
    }

    FloatPoint expandedTo(const FloatPoint& other) const
    {
        return { std::max(m_x, other.m_x), std::max(m_y, other.m_y) };
    }

    FloatPoint transposedPoint() const
    {
        return { m_y, m_x };
    }

#if USE(CG)
    WEBCORE_EXPORT FloatPoint(const CGPoint&);
    WEBCORE_EXPORT operator CGPoint() const;
#endif

#if PLATFORM(MAC) && !defined(NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES)
    WEBCORE_EXPORT FloatPoint(const NSPoint&);
    WEBCORE_EXPORT operator NSPoint() const;
#endif

#if PLATFORM(WIN)
    WEBCORE_EXPORT FloatPoint(const D2D1_POINT_2F&);
    WEBCORE_EXPORT operator D2D1_POINT_2F() const;
#endif

    WEBCORE_EXPORT FloatPoint matrixTransform(const TransformationMatrix&) const;
    WEBCORE_EXPORT FloatPoint matrixTransform(const AffineTransform&) const;

private:
    float m_x { 0 };
    float m_y { 0 };
};


inline FloatPoint& operator+=(FloatPoint& a, const FloatSize& b)
{
    a.move(b.width(), b.height());
    return a;
}

inline FloatPoint& operator+=(FloatPoint& a, const FloatPoint& b)
{
    a.move(b.x(), b.y());
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

inline FloatPoint operator+(const FloatPoint& a, const FloatPoint& b)
{
    return FloatPoint(a.x() + b.x(), a.y() + b.y());
}

inline FloatSize operator-(const FloatPoint& a, const FloatPoint& b)
{
    return FloatSize(a.x() - b.x(), a.y() - b.y());
}

inline FloatPoint operator-(const FloatPoint& a, const FloatSize& b)
{
    return FloatPoint(a.x() - b.width(), a.y() - b.height());
}

inline FloatPoint operator-(const FloatPoint& a)
{
    return FloatPoint(-a.x(), -a.y());
}

inline bool operator==(const FloatPoint& a, const FloatPoint& b)
{
    return a.x() == b.x() && a.y() == b.y();
}

inline bool operator!=(const FloatPoint& a, const FloatPoint& b)
{
    return a.x() != b.x() || a.y() != b.y();
}

inline float operator*(const FloatPoint& a, const FloatPoint& b)
{
    // dot product
    return a.dot(b);
}

inline IntSize flooredIntSize(const FloatPoint& p)
{
    return IntSize(clampToInteger(floorf(p.x())), clampToInteger(floorf(p.y())));
}

inline IntPoint roundedIntPoint(const FloatPoint& p)
{
    return IntPoint(clampToInteger(roundf(p.x())), clampToInteger(roundf(p.y())));
}

inline IntPoint flooredIntPoint(const FloatPoint& p)
{
    return IntPoint(clampToInteger(floorf(p.x())), clampToInteger(floorf(p.y())));
}

inline IntPoint ceiledIntPoint(const FloatPoint& p)
{
    return IntPoint(clampToInteger(ceilf(p.x())), clampToInteger(ceilf(p.y())));
}

inline FloatPoint floorPointToDevicePixels(const FloatPoint& p, float deviceScaleFactor)
{
    return FloatPoint(floorf(p.x() * deviceScaleFactor)  / deviceScaleFactor, floorf(p.y() * deviceScaleFactor)  / deviceScaleFactor);
}

inline FloatPoint ceilPointToDevicePixels(const FloatPoint& p, float deviceScaleFactor)
{
    return FloatPoint(ceilf(p.x() * deviceScaleFactor)  / deviceScaleFactor, ceilf(p.y() * deviceScaleFactor)  / deviceScaleFactor);
}

inline FloatSize toFloatSize(const FloatPoint& a)
{
    return FloatSize(a.x(), a.y());
}

inline FloatPoint toFloatPoint(const FloatSize& a)
{
    return FloatPoint(a.width(), a.height());
}

inline bool areEssentiallyEqual(const FloatPoint& a, const FloatPoint& b)
{
    return WTF::areEssentiallyEqual(a.x(), b.x()) && WTF::areEssentiallyEqual(a.y(), b.y());
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FloatPoint&);

}

