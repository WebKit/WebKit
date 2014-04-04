/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShapeInterval_h
#define ShapeInterval_h

#include <wtf/Vector.h>

namespace WebCore {

template <typename T>
class ShapeInterval {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShapeInterval(T x1 = 0, T x2 = 0)
        : m_x1(x1)
        , m_x2(x2)
    {
        ASSERT(x2 >= x1);
    }

    T x1() const { return m_x1; }
    T x2() const { return m_x2; }
    T width() const { return m_x2 - m_x1; }
    bool isEmpty() const { return !m_x1 && !m_x2; }

    void setX1(T x1)
    {
        ASSERT(m_x2 >= x1);
        m_x1 = x1;
    }

    void setX2(T x2)
    {
        ASSERT(x2 >= m_x1);
        m_x2 = x2;
    }

    void set(T x1, T x2)
    {
        ASSERT(x2 >= x1);
        m_x1 = x1;
        m_x2 = x2;
    }

    bool overlaps(const ShapeInterval<T>& interval) const
    {
        return x2() >= interval.x1() && x1() <= interval.x2();
    }

    bool contains(const ShapeInterval<T>& interval) const
    {
        return x1() <= interval.x1() && x2() >= interval.x2();
    }

    void unite(const ShapeInterval<T>& interval)
    {
        if (interval.isEmpty())
            return;
        if (isEmpty())
            set(interval.x1(), interval.x2());
        else
            set(std::min<T>(x1(), interval.x1()), std::max<T>(x2(), interval.x2()));
    }

    bool operator==(const ShapeInterval<T>& other) const { return x1() == other.x1() && x2() == other.x2(); }
    bool operator!=(const ShapeInterval<T>& other) const { return !operator==(other); }

private:
    T m_x1;
    T m_x2;
};

typedef ShapeInterval<int> IntShapeInterval;
typedef ShapeInterval<float> FloatShapeInterval;

typedef Vector<IntShapeInterval> IntShapeIntervals;
typedef Vector<FloatShapeInterval> FloatShapeIntervals;

} // namespace WebCore

#endif // ShapeInterval_h
