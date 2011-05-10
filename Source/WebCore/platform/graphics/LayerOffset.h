/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef LayerOffset_h
#define LayerOffset_h

#include "IntPoint.h"
#include "IntSize.h"

namespace WebCore {

// LayerOffset represents an offset, in Layer coordinates, from that Layer's origin.

class LayerOffset {
public:
    LayerOffset() : m_x(0), m_y(0) { }
    LayerOffset(int x, int y) : m_x(x), m_y(y) { }
    LayerOffset(const LayerOffset& other) : m_x(other.m_x), m_y(other.m_y) { }
    LayerOffset(const IntSize& size) : m_x(size.width()), m_y(size.height()) { }
    LayerOffset(const IntPoint& point) : m_x(point.x()), m_y(point.y()) { }

    int x() const { return m_x; }
    int y() const { return m_y; }

    void setX(int x) { m_x = x; }
    void setY(int y) { m_y = y; }

    IntSize toSize() const { return IntSize(m_x, m_y); }
    IntPoint toPoint() const { return IntPoint(m_x, m_y); }

private:
    int m_x;
    int m_y;
};

inline LayerOffset& operator+=(LayerOffset& a, const LayerOffset& b)
{
    a.setX(a.x() + b.x());
    a.setY(a.y() + b.y());
    return a;
}

inline LayerOffset& operator+=(LayerOffset& a, const IntSize& b)
{
    a.setX(a.x() + b.width());
    a.setY(a.y() + b.height());
    return a;
}

inline LayerOffset& operator+=(LayerOffset& a, const IntPoint& b)
{
    a.setX(a.x() + b.x());
    a.setY(a.y() + b.y());
    return a;
}
    
inline LayerOffset& operator-=(LayerOffset& a, const LayerOffset& b)
{
    a.setX(a.x() - b.x());
    a.setY(a.y() - b.y());
    return a;
}

inline LayerOffset& operator-=(LayerOffset& a, const IntSize& b)
{
    a.setX(a.x() - b.width());
    a.setY(a.y() - b.height());
    return a;
}

inline LayerOffset& operator-=(LayerOffset& a, const IntPoint& b)
{
    a.setX(a.x() - b.x());
    a.setY(a.y() - b.y());
    return a;
}
    
inline LayerOffset operator+(const LayerOffset& a, const LayerOffset& b)
{
    return LayerOffset(a.x() + b.x(), a.y() + b.y());
}

inline IntSize operator+(const IntSize& a, const LayerOffset& b)
{
    return IntSize(a.width() + b.x(), a.height() + b.y());
}

inline IntPoint operator+(const IntPoint& a, const LayerOffset& b)
{
    return IntPoint(a.x() + b.x(), a.y() + b.y());
}

inline LayerOffset operator-(const LayerOffset& a, const LayerOffset& b)
{
    return LayerOffset(a.x() - b.x(), a.y() - b.y());
}

inline IntSize operator-(const IntSize& a, const LayerOffset& b)
{
    return IntSize(a.width() - b.x(), a.height() - b.y());
}

inline IntPoint operator-(const IntPoint& a, const LayerOffset& b)
{
    return IntPoint(a.x() - b.x(), a.y() - b.y());
}
    
inline LayerOffset operator-(const LayerOffset& layerOffset)
{
    return LayerOffset(-layerOffset.x(), -layerOffset.y());
}
    
inline bool operator==(const LayerOffset& a, const LayerOffset& b)
{
    return a.x() == b.x() && a.y() == b.y();
}
    
inline bool operator!=(const LayerOffset& a, const LayerOffset& b)
{
    return a.x() != b.x() || a.y() != b.y();
}

} // namespace WebCore

#endif // LayerOffset_h
