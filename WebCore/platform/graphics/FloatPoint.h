/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef FloatPoint_h
#define FloatPoint_h

#include "FloatSize.h"
#include "IntPoint.h"
#include <wtf/MathExtras.h>
#include <wtf/Platform.h>

#if PLATFORM(CG)
typedef struct CGPoint CGPoint;
#endif

#if PLATFORM(MAC) || (PLATFORM(CHROMIUM) && OS(DARWIN))
#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
#else
typedef struct _NSPoint NSPoint;
#endif
#endif

#if PLATFORM(QT)
#include "qglobal.h"
QT_BEGIN_NAMESPACE
class QPointF;
QT_END_NAMESPACE
#endif

#if PLATFORM(HAIKU)
class BPoint;
#endif

#if PLATFORM(SKIA)
struct SkPoint;
#endif

namespace WebCore {

class AffineTransform;
class TransformationMatrix;
class IntPoint;

class FloatPoint {
public:
    FloatPoint() : m_x(0), m_y(0) { }
    FloatPoint(float x, float y) : m_x(x), m_y(y) { }
    FloatPoint(const IntPoint&);

    static FloatPoint narrowPrecision(double x, double y);

    float x() const { return m_x; }
    float y() const { return m_y; }

    void setX(float x) { m_x = x; }
    void setY(float y) { m_y = y; }
    void move(float dx, float dy) { m_x += dx; m_y += dy; }

#if PLATFORM(CG)
    FloatPoint(const CGPoint&);
    operator CGPoint() const;
#endif

#if (PLATFORM(MAC) && !defined(NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES)) \
        || (PLATFORM(CHROMIUM) && OS(DARWIN))
    FloatPoint(const NSPoint&);
    operator NSPoint() const;
#endif

#if PLATFORM(QT)
    FloatPoint(const QPointF&);
    operator QPointF() const;
#endif

#if PLATFORM(HAIKU)
    FloatPoint(const BPoint&);
    operator BPoint() const;
#endif

#if PLATFORM(SKIA)
    operator SkPoint() const;
    FloatPoint(const SkPoint&);
#endif

    FloatPoint matrixTransform(const TransformationMatrix&) const;
    FloatPoint matrixTransform(const AffineTransform&) const;

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

inline IntPoint roundedIntPoint(const FloatPoint& p)
{
    return IntPoint(static_cast<int>(roundf(p.x())), static_cast<int>(roundf(p.y())));
}

}

#endif
