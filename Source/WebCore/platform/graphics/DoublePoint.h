/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/DoubleSize.h>
#include <WebCore/IntPoint.h>
#include <wtf/Platform.h>
#include <wtf/TZoneMalloc.h>

#if USE(CG)
typedef struct CGPoint CGPoint;
#endif

#if PLATFORM(MAC)
typedef struct CGPoint NSPoint;
#endif // PLATFORM(MAC)

namespace WTF {
class TextStream;
}

namespace WebCore {

class IntSize;

class DoublePoint {
    WTF_MAKE_TZONE_ALLOCATED(DoublePoint);
public:
    constexpr DoublePoint() = default;
    constexpr DoublePoint(double x, double y) : m_x(x), m_y(y) { }
    WEBCORE_EXPORT DoublePoint(const IntPoint&);
    explicit DoublePoint(const DoubleSize& size)
        : m_x(size.width()), m_y(size.height()) { }

    static constexpr DoublePoint zero() { return DoublePoint(); }
    constexpr bool isZero() const { return !m_x && !m_y; }

    constexpr double x() const { return m_x; }
    constexpr double y() const { return m_y; }

    void move(double dx, double dy)
    {
        m_x += dx;
        m_y += dy;
    }

    void move(const IntSize& a)
    {
        m_x += a.width();
        m_y += a.height();
    }

    void move(const DoubleSize& a)
    {
        m_x += a.width();
        m_y += a.height();
    }

    void moveBy(const DoublePoint& a)
    {
        m_x += a.x();
        m_y += a.y();
    }

    void scale(double scale)
    {
        m_x *= scale;
        m_y *= scale;
    }

    constexpr DoublePoint scaled(double scale) const
    {
        return { m_x * scale, m_y * scale };
    }

#if USE(CG)
    WEBCORE_EXPORT DoublePoint(const CGPoint&);
    WEBCORE_EXPORT operator CGPoint() const;
    CGPoint toCG() const { return static_cast<CGPoint>(*this); }
#endif

#if PLATFORM(WIN)
    WEBCORE_EXPORT DoublePoint(const POINT&);
#endif

    WEBCORE_EXPORT String toJSONString() const;
    WEBCORE_EXPORT Ref<JSON::Object> toJSONObject() const;

    friend bool operator==(const DoublePoint&, const DoublePoint&) = default;

private:
    double m_x { 0 };
    double m_y { 0 };
};

constexpr DoublePoint operator+(const DoublePoint& a, const DoubleSize& b)
{
    return DoublePoint(a.x() + b.width(), a.y() + b.height());
}

constexpr DoublePoint operator+(const DoublePoint& a, const DoublePoint& b)
{
    return DoublePoint(a.x() + b.x(), a.y() + b.y());
}

constexpr DoubleSize operator-(const DoublePoint& a, const DoublePoint& b)
{
    return DoubleSize(a.x() - b.x(), a.y() - b.y());
}

constexpr DoublePoint operator-(const DoublePoint& a, const DoubleSize& b)
{
    return DoublePoint(a.x() - b.width(), a.y() - b.height());
}

inline IntPoint flooredIntPoint(const DoublePoint& p)
{
    return IntPoint(clampToInteger(floor(p.x())), clampToInteger(floor(p.y())));
}

inline IntPoint roundedIntPoint(const DoublePoint& p)
{
    return IntPoint(clampToInteger(round(p.x())), clampToInteger(round(p.y())));
}

inline DoubleSize toDoubleSize(const DoublePoint& a)
{
    return DoubleSize(a.x(), a.y());
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const DoublePoint&);

} // namespace WebCore

namespace WTF {

template<typename Type> struct LogArgument;
template <>
struct LogArgument<WebCore::DoublePoint> {
    static String toString(const WebCore::DoublePoint& point)
    {
        return point.toJSONString();
    }
};

} // namespace WTF

