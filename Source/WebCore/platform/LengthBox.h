/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006, 2008, 2015 Apple Inc. All rights reserved.
    Copyright (c) 2012, Google Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef LengthBox_h
#define LengthBox_h

#include "Length.h"
#include "WritingMode.h"
#include <array>

namespace WebCore {

template<typename T> class BoxExtent {
public:
    BoxExtent()
        : m_sides({{ T(0), T(0), T(0), T(0) }})
    {
    }

    BoxExtent(const T& top, const T& right, const T& bottom, const T& left)
        : m_sides({{ top, right, bottom, left }})
    {
    }

    T& at(PhysicalBoxSide side) { return m_sides[side]; }
    T& top() { return at(TopSide); }
    T& right() { return at(RightSide); }
    T& bottom() { return at(BottomSide); }
    T& left() { return at(LeftSide); }

    const T& at(PhysicalBoxSide side) const { return m_sides[side]; }
    const T& top() const { return at(TopSide); }
    const T& right() const { return at(RightSide); }
    const T& bottom() const { return at(BottomSide); }
    const T& left() const { return at(LeftSide); }

    void setAt(PhysicalBoxSide side, const T& v) { at(side) = v; }
    void setTop(const T& top) { setAt(TopSide, top); }
    void setRight(const T& right) { setAt(RightSide, right); }
    void setBottom(const T& bottom) { setAt(BottomSide, bottom); }
    void setLeft(const T& left) { setAt(LeftSide, left); }

    T& before(WritingMode writingMode)
    {
        return at(mapLogicalSideToPhysicalSide(writingMode, BeforeSide));
    }

    T& after(WritingMode writingMode)
    {
        return at(mapLogicalSideToPhysicalSide(writingMode, AfterSide));
    }

    T& start(WritingMode writingMode, TextDirection direction = LTR)
    {
        return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), StartSide));
    }

    T& end(WritingMode writingMode, TextDirection direction = LTR)
    {
        return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), EndSide));
    }

    const T& before(WritingMode writingMode) const
    {
        return at(mapLogicalSideToPhysicalSide(writingMode, BeforeSide));
    }

    const T& after(WritingMode writingMode) const
    {
        return at(mapLogicalSideToPhysicalSide(writingMode, AfterSide));
    }

    const T& start(WritingMode writingMode, TextDirection direction = LTR) const
    {
        return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), StartSide));
    }

    const T& end(WritingMode writingMode, TextDirection direction = LTR) const
    {
        return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), EndSide));
    }

    void setBefore(const T& before, WritingMode writingMode) { this->before(writingMode) = before; }
    void setAfter(const T& after, WritingMode writingMode) { this->after(writingMode) = after; }
    void setStart(const T& start, WritingMode writingMode, TextDirection direction = LTR) { this->start(writingMode, direction) = start; }
    void setEnd(const T& end, WritingMode writingMode, TextDirection direction = LTR) { this->end(writingMode, direction) = end; }

    bool operator==(const BoxExtent& other) const
    {
        return m_sides == other.m_sides;
    }

    bool operator!=(const BoxExtent& other) const
    {
        return m_sides != other.m_sides;
    }

protected:
    std::array<T, 4> m_sides;
};

class LengthBox : public BoxExtent<Length> {
public:
    LengthBox()
        : LengthBox(Auto)
    {
    }

    explicit LengthBox(LengthType type)
        : BoxExtent(Length(type), Length(type), Length(type), Length(type))
    {
    }

    explicit LengthBox(int v)
        : BoxExtent(Length(v, Fixed), Length(v, Fixed), Length(v, Fixed), Length(v, Fixed))
    {
    }

    LengthBox(int top, int right, int bottom, int left)
        : BoxExtent(Length(top, Fixed), Length(right, Fixed), Length(bottom, Fixed), Length(left, Fixed))
    {
    }

    LengthBox(const Length& top, const Length& right, const Length& bottom, const Length& left)
        : BoxExtent(top, right, bottom, left)
    {
    }

    bool isZero() const
    {
        return top().isZero() && right().isZero() && bottom().isZero() && left().isZero();
    }
};

typedef BoxExtent<LayoutUnit> LayoutBoxExtent;
typedef BoxExtent<float> FloatBoxExtent;

TextStream& operator<<(TextStream&, const LengthBox&);

} // namespace WebCore

#endif // LengthBox_h
