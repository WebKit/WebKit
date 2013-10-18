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
    bool isEmpty() const { return m_x1 == m_x2; }

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

    ShapeInterval<T> intersect(const ShapeInterval<T>& interval) const
    {
        ASSERT(overlaps(interval));
        return ShapeInterval<T>(std::max<T>(x1(), interval.x1()), std::min<T>(x2(), interval.x2()));
    }

    typedef Vector<ShapeInterval<T>> ShapeIntervals;
    typedef typename ShapeIntervals::const_iterator const_iterator;
    typedef typename ShapeIntervals::iterator iterator;

    static void uniteShapeIntervals(const ShapeIntervals& a, const ShapeIntervals& b, ShapeIntervals& result)
    {
        ASSERT(shapeIntervalsAreSortedAndDisjoint(a) && shapeIntervalsAreSortedAndDisjoint(b));

        if (a.isEmpty() || a == b) {
            result.appendRange(b.begin(), b.end());
            return;
        }

        if (b.isEmpty()) {
            result.appendRange(a.begin(), a.end());
            return;
        }

        const_iterator aNext = a.begin();
        const_iterator bNext = b.begin();

        while (aNext != a.end() || bNext != b.end()) {
            const_iterator next = (bNext == b.end() || (aNext != a.end() && aNext->x1() < bNext->x1())) ? aNext++ : bNext++;
            if (result.isEmpty() || !result.last().overlaps(*next))
                result.append(*next);
            else
                result.last().setX2(std::max<T>(result.last().x2(), next->x2()));
        }
    }

    static void intersectShapeIntervals(const ShapeIntervals& a, const ShapeIntervals& b, ShapeIntervals& result)
    {
        ASSERT(shapeIntervalsAreSortedAndDisjoint(a) && shapeIntervalsAreSortedAndDisjoint(b));

        if (a.isEmpty() || b.isEmpty())
            return;

        if (a == b) {
            result.appendRange(a.begin(), a.end());
            return;
        }

        const_iterator aNext = a.begin();
        const_iterator bNext = b.begin();
        const_iterator working = aNext->x1() < bNext->x1() ? aNext++ : bNext++;

        while (aNext != a.end() || bNext != b.end()) {
            const_iterator next = (bNext == b.end() || (aNext != a.end() && aNext->x1() < bNext->x1())) ? aNext++ : bNext++;
            if (working->overlaps(*next)) {
                result.append(working->intersect(*next));
                if (next->x2() > working->x2())
                    working = next;
            } else
                working = next;
        }
    }

    static void subtractShapeIntervals(const ShapeIntervals& a, const ShapeIntervals& b, ShapeIntervals& result)
    {
        ASSERT(shapeIntervalsAreSortedAndDisjoint(a) && shapeIntervalsAreSortedAndDisjoint(b));

        if (a.isEmpty() || a == b)
            return;

        if (b.isEmpty()) {
            result.appendRange(a.begin(), a.end());
            return;
        }

        const_iterator aNext = a.begin();
        const_iterator bNext = b.begin();
        ShapeInterval<T> aValue = *aNext;
        ShapeInterval<T> bValue = *bNext;

        do {
            bool aIncrement = false;
            bool bIncrement = false;

            if (bValue.contains(aValue)) {
                aIncrement = true;
            } else if (aValue.contains(bValue)) {
                if (bValue.x1() > aValue.x1())
                    result.append(ShapeInterval<T>(aValue.x1(), bValue.x1()));
                if (aValue.x2() > bValue.x2())
                    aValue.setX1(bValue.x2());
                else
                    aIncrement = true;
                bIncrement = true;
            } else if (aValue.overlaps(bValue)) {
                if (aValue.x1() < bValue.x1()) {
                    result.append(ShapeInterval<T>(aValue.x1(), bValue.x1()));
                    aIncrement = true;
                } else {
                    aValue.setX1(bValue.x2());
                    bIncrement = true;
                }
            } else {
                if (aValue.x1() < bValue.x1()) {
                    result.append(aValue);
                    aIncrement = true;
                } else
                    bIncrement = true;
            }

            if (aIncrement && ++aNext != a.end())
                aValue = *aNext;
            if (bIncrement && ++bNext != b.end())
                bValue = *bNext;

        } while (aNext != a.end() && bNext != b.end());

        if (aNext != a.end()) {
            result.append(aValue);
            result.appendRange(++aNext, a.end());
        }
    }

    bool operator==(const ShapeInterval<T>& other) const { return x1() == other.x1() && x2() == other.x2(); }
    bool operator!=(const ShapeInterval<T>& other) const { return !operator==(other); }

private:
    T m_x1;
    T m_x2;

    static bool shapeIntervalsAreSortedAndDisjoint(const ShapeIntervals& intervals)
    {
        for (unsigned i = 1; i < intervals.size(); i++)
            if (intervals[i - 1].x2() > intervals[i].x1())
                return false;

        return true;
    }
};

typedef ShapeInterval<int> IntShapeInterval;
typedef ShapeInterval<float> FloatShapeInterval;

typedef Vector<IntShapeInterval> IntShapeIntervals;
typedef Vector<FloatShapeInterval> FloatShapeIntervals;

} // namespace WebCore

#endif // ShapeInterval_h
