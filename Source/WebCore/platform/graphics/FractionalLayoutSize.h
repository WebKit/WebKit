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

#ifndef FractionalLayoutSize_h
#define FractionalLayoutSize_h

#include "FloatSize.h"
#include "FractionalLayoutUnit.h"
#include "IntSize.h"

namespace WebCore {

class FractionalLayoutPoint;

class FractionalLayoutSize {
public:
    FractionalLayoutSize() : m_width(0), m_height(0) { }
    FractionalLayoutSize(const IntSize& size) : m_width(size.width()), m_height(size.height()) { }
    FractionalLayoutSize(FractionalLayoutUnit width, FractionalLayoutUnit height) : m_width(width), m_height(height) { }

    explicit FractionalLayoutSize(const FloatSize& size) : m_width(size.width()), m_height(size.height()) { }
    
    FractionalLayoutUnit width() const { return m_width; }
    FractionalLayoutUnit height() const { return m_height; }

    void setWidth(FractionalLayoutUnit width) { m_width = width; }
    void setHeight(FractionalLayoutUnit height) { m_height = height; }

    bool isEmpty() const { return m_width <= 0 || m_height <= 0; }
    bool isZero() const { return !m_width && !m_height; }

    float aspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    
    void expand(FractionalLayoutUnit width, FractionalLayoutUnit height)
    {
        m_width += width;
        m_height += height;
    }
    
    void scale(float scale)
    {
        m_width = static_cast<int>(static_cast<float>(m_width) * scale);
        m_height = static_cast<int>(static_cast<float>(m_height) * scale);
    }
    
    FractionalLayoutSize expandedTo(const FractionalLayoutSize& other) const
    {
        return FractionalLayoutSize(m_width > other.m_width ? m_width : other.m_width,
            m_height > other.m_height ? m_height : other.m_height);
    }

    FractionalLayoutSize shrunkTo(const FractionalLayoutSize& other) const
    {
        return FractionalLayoutSize(m_width < other.m_width ? m_width : other.m_width,
            m_height < other.m_height ? m_height : other.m_height);
    }

    void clampNegativeToZero()
    {
        *this = expandedTo(FractionalLayoutSize());
    }

    FractionalLayoutSize transposedSize() const
    {
        return FractionalLayoutSize(m_height, m_width);
    }

private:
    FractionalLayoutUnit m_width, m_height;
};

inline FractionalLayoutSize& operator+=(FractionalLayoutSize& a, const FractionalLayoutSize& b)
{
    a.setWidth(a.width() + b.width());
    a.setHeight(a.height() + b.height());
    return a;
}

inline FractionalLayoutSize& operator-=(FractionalLayoutSize& a, const FractionalLayoutSize& b)
{
    a.setWidth(a.width() - b.width());
    a.setHeight(a.height() - b.height());
    return a;
}

inline FractionalLayoutSize operator+(const FractionalLayoutSize& a, const FractionalLayoutSize& b)
{
    return FractionalLayoutSize(a.width() + b.width(), a.height() + b.height());
}

inline FractionalLayoutSize operator-(const FractionalLayoutSize& a, const FractionalLayoutSize& b)
{
    return FractionalLayoutSize(a.width() - b.width(), a.height() - b.height());
}

inline FractionalLayoutSize operator-(const FractionalLayoutSize& size)
{
    return FractionalLayoutSize(-size.width(), -size.height());
}

inline bool operator==(const FractionalLayoutSize& a, const FractionalLayoutSize& b)
{
    return a.width() == b.width() && a.height() == b.height();
}

inline bool operator!=(const FractionalLayoutSize& a, const FractionalLayoutSize& b)
{
    return a.width() != b.width() || a.height() != b.height();
}

inline IntSize flooredIntSize(const FractionalLayoutSize& s)
{
    return IntSize(s.width().toInt(), s.height().toInt());
}

inline IntSize roundedIntSize(const FractionalLayoutSize& s)
{
    return IntSize(s.width().round(), s.height().round());
}

IntSize pixelSnappedIntSize(const FractionalLayoutSize&, const FractionalLayoutPoint&);

} // namespace WebCore

#endif // FractionalLayoutSize_h
