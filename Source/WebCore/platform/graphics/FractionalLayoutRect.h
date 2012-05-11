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

#ifndef FractionalLayoutRect_h
#define FractionalLayoutRect_h

#include "FractionalLayoutPoint.h"
#include "IntRect.h"
#include <wtf/Vector.h>

#if PLATFORM(QT)
#include <qglobal.h>
QT_BEGIN_NAMESPACE
class QRect;
class QRectF;
QT_END_NAMESPACE
#endif

namespace WebCore {

class FloatRect;

class FractionalLayoutRect {
public:
    FractionalLayoutRect() { }
    FractionalLayoutRect(const FractionalLayoutPoint& location, const FractionalLayoutSize& size)
        : m_location(location), m_size(size) { }
    FractionalLayoutRect(FractionalLayoutUnit x, FractionalLayoutUnit y, FractionalLayoutUnit width, FractionalLayoutUnit height)
        : m_location(FractionalLayoutPoint(x, y)), m_size(FractionalLayoutSize(width, height)) { }
    FractionalLayoutRect(const FloatPoint& location, const FloatSize& size)
        : m_location(location), m_size(size) { }
    FractionalLayoutRect(const IntRect& rect) : m_location(rect.location()), m_size(rect.size()) { }

    explicit FractionalLayoutRect(const FloatRect&); // don't do this implicitly since it's lossy
        
    FractionalLayoutPoint location() const { return m_location; }
    FractionalLayoutSize size() const { return m_size; }

    IntPoint pixelSnappedLocation() const { return roundedIntPoint(m_location); }
    IntSize pixelSnappedSize() const { return IntSize(snapSizeToPixel(m_size.width(), m_location.x()), snapSizeToPixel(m_size.height(), m_location.y())); }

    void setLocation(const FractionalLayoutPoint& location) { m_location = location; }
    void setSize(const FractionalLayoutSize& size) { m_size = size; }

    FractionalLayoutUnit x() const { return m_location.x(); }
    FractionalLayoutUnit y() const { return m_location.y(); }
    FractionalLayoutUnit maxX() const { return x() + width(); }
    FractionalLayoutUnit maxY() const { return y() + height(); }
    FractionalLayoutUnit width() const { return m_size.width(); }
    FractionalLayoutUnit height() const { return m_size.height(); }

    int pixelSnappedX() const { return x().round(); }
    int pixelSnappedY() const { return y().round(); }
    int pixelSnappedWidth() const { return snapSizeToPixel(width(), x()); }
    int pixelSnappedHeight() const { return snapSizeToPixel(height(), y()); }
    int pixelSnappedMaxX() const { return (m_location.x() + m_size.width()).round(); }
    int pixelSnappedMaxY() const { return (m_location.y() + m_size.height()).round(); }

    void setX(FractionalLayoutUnit x) { m_location.setX(x); }
    void setY(FractionalLayoutUnit y) { m_location.setY(y); }
    void setWidth(FractionalLayoutUnit width) { m_size.setWidth(width); }
    void setHeight(FractionalLayoutUnit height) { m_size.setHeight(height); }

    bool isEmpty() const { return m_size.isEmpty(); }

    // NOTE: The result is rounded to integer values, and thus may be not the exact
    // center point.
    FractionalLayoutPoint center() const { return FractionalLayoutPoint(x() + width() / 2, y() + height() / 2); }

    void move(const FractionalLayoutSize& size) { m_location += size; } 
    void moveBy(const FractionalLayoutPoint& offset) { m_location.move(offset.x(), offset.y()); }
    void move(FractionalLayoutUnit dx, FractionalLayoutUnit dy) { m_location.move(dx, dy); } 

    void expand(const FractionalLayoutSize& size) { m_size += size; }
    void expand(FractionalLayoutUnit dw, FractionalLayoutUnit dh) { m_size.expand(dw, dh); }
    void contract(const FractionalLayoutSize& size) { m_size -= size; }
    void contract(FractionalLayoutUnit dw, FractionalLayoutUnit dh) { m_size.expand(-dw, -dh); }

    void shiftXEdgeTo(FractionalLayoutUnit edge)
    {
        FractionalLayoutUnit delta = edge - x();
        setX(edge);
        setWidth(std::max<FractionalLayoutUnit>(0, width() - delta));
    }
    void shiftMaxXEdgeTo(FractionalLayoutUnit edge)
    {
        FractionalLayoutUnit delta = edge - maxX();
        setWidth(std::max<FractionalLayoutUnit>(0, width() + delta));
    }
    void shiftYEdgeTo(FractionalLayoutUnit edge)
    {
        FractionalLayoutUnit delta = edge - y();
        setY(edge);
        setHeight(std::max<FractionalLayoutUnit>(0, height() - delta));
    }
    void shiftMaxYEdgeTo(FractionalLayoutUnit edge)
    {
        FractionalLayoutUnit delta = edge - maxY();
        setHeight(std::max<FractionalLayoutUnit>(0, height() + delta));
    }

    FractionalLayoutPoint minXMinYCorner() const { return m_location; } // typically topLeft
    FractionalLayoutPoint maxXMinYCorner() const { return FractionalLayoutPoint(m_location.x() + m_size.width(), m_location.y()); } // typically topRight
    FractionalLayoutPoint minXMaxYCorner() const { return FractionalLayoutPoint(m_location.x(), m_location.y() + m_size.height()); } // typically bottomLeft
    FractionalLayoutPoint maxXMaxYCorner() const { return FractionalLayoutPoint(m_location.x() + m_size.width(), m_location.y() + m_size.height()); } // typically bottomRight
    
    bool intersects(const FractionalLayoutRect&) const;
    bool contains(const FractionalLayoutRect&) const;

    // This checks to see if the rect contains x,y in the traditional sense.
    // Equivalent to checking if the rect contains a 1x1 rect below and to the right of (px,py).
    bool contains(FractionalLayoutUnit px, FractionalLayoutUnit py) const
        { return px >= x() && px < maxX() && py >= y() && py < maxY(); }
    bool contains(const FractionalLayoutPoint& point) const { return contains(point.x(), point.y()); }

    void intersect(const FractionalLayoutRect&);
    void unite(const FractionalLayoutRect&);
    void uniteIfNonZero(const FractionalLayoutRect&);

    void inflateX(FractionalLayoutUnit dx)
    {
        m_location.setX(m_location.x() - dx);
        m_size.setWidth(m_size.width() + dx + dx);
    }
    void inflateY(FractionalLayoutUnit dy)
    {
        m_location.setY(m_location.y() - dy);
        m_size.setHeight(m_size.height() + dy + dy);
    }
    void inflate(FractionalLayoutUnit d) { inflateX(d); inflateY(d); }
    void scale(float s);

    FractionalLayoutRect transposedRect() const { return FractionalLayoutRect(m_location.transposedPoint(), m_size.transposedSize()); }

    static FractionalLayoutRect infiniteRect() {return FractionalLayoutRect(FractionalLayoutUnit::min() / 2, FractionalLayoutUnit::min() / 2, FractionalLayoutUnit::max(), FractionalLayoutUnit::max()); }

#if PLATFORM(QT)
    explicit FractionalLayoutRect(const QRect&);
    explicit FractionalLayoutRect(const QRectF&);
    operator QRectF() const;
#endif

private:
    FractionalLayoutPoint m_location;
    FractionalLayoutSize m_size;
};

inline FractionalLayoutRect intersection(const FractionalLayoutRect& a, const FractionalLayoutRect& b)
{
    FractionalLayoutRect c = a;
    c.intersect(b);
    return c;
}

inline FractionalLayoutRect unionRect(const FractionalLayoutRect& a, const FractionalLayoutRect& b)
{
    FractionalLayoutRect c = a;
    c.unite(b);
    return c;
}

FractionalLayoutRect unionRect(const Vector<FractionalLayoutRect>&);

inline bool operator==(const FractionalLayoutRect& a, const FractionalLayoutRect& b)
{
    return a.location() == b.location() && a.size() == b.size();
}

inline bool operator!=(const FractionalLayoutRect& a, const FractionalLayoutRect& b)
{
    return a.location() != b.location() || a.size() != b.size();
}

inline IntRect pixelSnappedIntRect(const FractionalLayoutRect& rect)
{
#if ENABLE(SUBPIXEL_LAYOUT)
    IntPoint roundedLocation = roundedIntPoint(rect.location());
    return IntRect(roundedLocation, IntSize((rect.x() + rect.width()).round() - roundedLocation.x(),
                                            (rect.y() + rect.height()).round() - roundedLocation.y()));
#else
    return IntRect(rect);
#endif
}

IntRect enclosingIntRect(const FractionalLayoutRect&);
FractionalLayoutRect enclosingFractionalLayoutRect(const FloatRect&);

} // namespace WebCore

#endif // FractionalLayoutRect_h
