/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FractionalLayoutPoint_h
#define FractionalLayoutPoint_h

#include "FloatPoint.h"
#include "FractionalLayoutSize.h"
#include <wtf/MathExtras.h>

#if PLATFORM(QT)
#include <qglobal.h>
QT_BEGIN_NAMESPACE
class QPoint;
class QPointF;
QT_END_NAMESPACE
#endif

namespace WebCore {

class FractionalLayoutPoint {
public:
    FractionalLayoutPoint() : m_x(0), m_y(0) { }
    FractionalLayoutPoint(FractionalLayoutUnit x, FractionalLayoutUnit y) : m_x(x), m_y(y) { }
    FractionalLayoutPoint(const IntPoint& point) : m_x(point.x()), m_y(point.y()) { }
    explicit FractionalLayoutPoint(const FloatPoint& size) : m_x(size.x()), m_y(size.y()) { }
    explicit FractionalLayoutPoint(const FractionalLayoutSize& size) : m_x(size.width()), m_y(size.height()) { }

    static FractionalLayoutPoint zero() { return FractionalLayoutPoint(); }

    FractionalLayoutUnit x() const { return m_x; }
    FractionalLayoutUnit y() const { return m_y; }

    void setX(FractionalLayoutUnit x) { m_x = x; }
    void setY(FractionalLayoutUnit y) { m_y = y; }

    void move(const FractionalLayoutSize& s) { move(s.width(), s.height()); } 
    void moveBy(const FractionalLayoutPoint& offset) { move(offset.x(), offset.y()); }
    void move(FractionalLayoutUnit dx, FractionalLayoutUnit dy) { m_x += dx; m_y += dy; }
    void scale(float sx, float sy)
    {
        m_x *= sx;
        m_y *= sy;
    }
    
    FractionalLayoutPoint expandedTo(const FractionalLayoutPoint& other) const
    {
        return FractionalLayoutPoint(std::max(m_x, other.m_x), std::max(m_y, other.m_y));
    }

    FractionalLayoutPoint shrunkTo(const FractionalLayoutPoint& other) const
    {
        return FractionalLayoutPoint(std::min(m_x, other.m_x), std::min(m_y, other.m_y));
    }

    void clampNegativeToZero()
    {
        *this = expandedTo(zero());
    }

    FractionalLayoutPoint transposedPoint() const
    {
        return FractionalLayoutPoint(m_y, m_x);
    }

#if PLATFORM(QT)
    explicit FractionalLayoutPoint(const QPoint&);
    explicit FractionalLayoutPoint(const QPointF&);
    operator QPointF() const;
#endif

private:
    FractionalLayoutUnit m_x, m_y;
};

inline FractionalLayoutPoint& operator+=(FractionalLayoutPoint& a, const FractionalLayoutSize& b)
{
    a.move(b.width(), b.height());
    return a;
}

inline FractionalLayoutPoint& operator-=(FractionalLayoutPoint& a, const FractionalLayoutSize& b)
{
    a.move(-b.width(), -b.height());
    return a;
}

inline FractionalLayoutPoint operator+(const FractionalLayoutPoint& a, const FractionalLayoutSize& b)
{
    return FractionalLayoutPoint(a.x() + b.width(), a.y() + b.height());
}

inline FractionalLayoutPoint operator+(const FractionalLayoutPoint& a, const FractionalLayoutPoint& b)
{
    return FractionalLayoutPoint(a.x() + b.x(), a.y() + b.y());
}

inline FractionalLayoutSize operator-(const FractionalLayoutPoint& a, const FractionalLayoutPoint& b)
{
    return FractionalLayoutSize(a.x() - b.x(), a.y() - b.y());
}

inline FractionalLayoutPoint operator-(const FractionalLayoutPoint& a, const FractionalLayoutSize& b)
{
    return FractionalLayoutPoint(a.x() - b.width(), a.y() - b.height());
}

inline FractionalLayoutPoint operator-(const FractionalLayoutPoint& point)
{
    return FractionalLayoutPoint(-point.x(), -point.y());
}

inline bool operator==(const FractionalLayoutPoint& a, const FractionalLayoutPoint& b)
{
    return a.x() == b.x() && a.y() == b.y();
}

inline bool operator!=(const FractionalLayoutPoint& a, const FractionalLayoutPoint& b)
{
    return a.x() != b.x() || a.y() != b.y();
}

inline FractionalLayoutPoint toPoint(const FractionalLayoutSize& size)
{
    return FractionalLayoutPoint(size.width(), size.height());
}

inline FractionalLayoutSize toSize(const FractionalLayoutPoint& a)
{
    return FractionalLayoutSize(a.x(), a.y());
}

inline IntPoint flooredIntPoint(const FractionalLayoutPoint& point)
{
    return IntPoint(point.x().toInt(), point.y().toInt());
}

inline IntPoint roundedIntPoint(const FractionalLayoutPoint& point)
{
    return IntPoint(point.x().round(), point.y().round());
}

inline IntPoint roundedIntPoint(const FractionalLayoutSize& size)
{
    return IntPoint(size.width().round(), size.height().round());
}

inline IntPoint ceiledIntPoint(const FractionalLayoutPoint& point)
{
    return IntPoint(point.x().ceil(), point.y().ceil());
}

} // namespace WebCore

#endif // FractionalLayoutPoint_h
