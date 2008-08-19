/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef IntRect_h
#define IntRect_h

#include "IntPoint.h"
#include <wtf/Platform.h>

#if PLATFORM(CG)
typedef struct CGRect CGRect;
#endif

#if PLATFORM(MAC)
#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGRect NSRect;
#else
typedef struct _NSRect NSRect;
#endif
#endif

#if PLATFORM(WIN)
typedef struct tagRECT RECT;
#elif PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QRect;
QT_END_NAMESPACE
#elif PLATFORM(GTK)
typedef struct _GdkRectangle GdkRectangle;
#endif
#if PLATFORM(SYMBIAN)
class TRect;
#endif

#if PLATFORM(WX)
class wxRect;
#endif

#if PLATFORM(SKIA)
struct SkRect;
struct SkIRect;
#endif

namespace WebCore {

class FloatRect;

class IntRect {
public:
    IntRect() { }
    IntRect(const IntPoint& location, const IntSize& size)
        : m_location(location), m_size(size) { }
    IntRect(int x, int y, int width, int height)
        : m_location(IntPoint(x, y)), m_size(IntSize(width, height)) { }

    explicit IntRect(const FloatRect& rect); // don't do this implicitly since it's lossy
        
    IntPoint location() const { return m_location; }
    IntSize size() const { return m_size; }

    void setLocation(const IntPoint& location) { m_location = location; }
    void setSize(const IntSize& size) { m_size = size; }

    int x() const { return m_location.x(); }
    int y() const { return m_location.y(); }
    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }

    void setX(int x) { m_location.setX(x); }
    void setY(int y) { m_location.setY(y); }
    void setWidth(int width) { m_size.setWidth(width); }
    void setHeight(int height) { m_size.setHeight(height); }

    // Be careful with these functions.  The point is considered to be to the right and below.  These are not
    // substitutes for right() and bottom().
    IntPoint topLeft() const { return m_location; }
    IntPoint topRight() const { return IntPoint(right() - 1, y()); }
    IntPoint bottomLeft() const { return IntPoint(x(), bottom() - 1); }
    IntPoint bottomRight() const { return IntPoint(right() - 1, bottom() - 1); }

    bool isEmpty() const { return m_size.isEmpty(); }

    int right() const { return x() + width(); }
    int bottom() const { return y() + height(); }

    void move(const IntSize& s) { m_location += s; } 
    void move(int dx, int dy) { m_location.move(dx, dy); } 

    bool intersects(const IntRect&) const;
    bool contains(const IntRect&) const;

    // This checks to see if the rect contains x,y in the traditional sense.
    // Equivalent to checking if the rect contains a 1x1 rect below and to the right of (px,py).
    bool contains(int px, int py) const
        { return px >= x() && px < right() && py >= y() && py < bottom(); }
    bool contains(const IntPoint& point) const { return contains(point.x(), point.y()); }

    void intersect(const IntRect&);
    void unite(const IntRect&);

    void inflateX(int dx)
    {
        m_location.setX(m_location.x() - dx);
        m_size.setWidth(m_size.width() + dx + dx);
    }
    void inflateY(int dy)
    {
        m_location.setY(m_location.y() - dy);
        m_size.setHeight(m_size.height() + dy + dy);
    }
    void inflate(int d) { inflateX(d); inflateY(d); }
    void scale(float s);

#if PLATFORM(WX)
    IntRect(const wxRect&);
    operator wxRect() const;
#endif

#if PLATFORM(WIN)
    IntRect(const RECT&);
    operator RECT() const;
#elif PLATFORM(QT)
    IntRect(const QRect&);
    operator QRect() const;
#elif PLATFORM(GTK)
    IntRect(const GdkRectangle&);
    operator GdkRectangle() const;
#endif
#if PLATFORM(SYMBIAN)
    IntRect(const TRect&);
    operator TRect() const;
    TRect Rect() const;
#endif

#if PLATFORM(CG)
    operator CGRect() const;
#endif

#if PLATFORM(SKIA)
    IntRect(const SkIRect&);
    operator SkRect() const;
    operator SkIRect() const;
#endif

#if PLATFORM(MAC) && !defined(NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES)
    operator NSRect() const;
#endif

private:
    IntPoint m_location;
    IntSize m_size;
};

inline IntRect intersection(const IntRect& a, const IntRect& b)
{
    IntRect c = a;
    c.intersect(b);
    return c;
}

inline IntRect unionRect(const IntRect& a, const IntRect& b)
{
    IntRect c = a;
    c.unite(b);
    return c;
}

inline bool operator==(const IntRect& a, const IntRect& b)
{
    return a.location() == b.location() && a.size() == b.size();
}

inline bool operator!=(const IntRect& a, const IntRect& b)
{
    return a.location() != b.location() || a.size() != b.size();
}

#if PLATFORM(CG)
IntRect enclosingIntRect(const CGRect&);
#endif

#if PLATFORM(MAC) && !defined(NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES)
IntRect enclosingIntRect(const NSRect&);
#endif

} // namespace WebCore

#endif // IntRect_h
