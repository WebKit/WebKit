/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WritingMode.h"
#include <array>
#include <wtf/OptionSet.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

enum class BoxSideFlag : uint8_t {
    Top     = 1 << static_cast<unsigned>(BoxSide::Top),
    Right   = 1 << static_cast<unsigned>(BoxSide::Right),
    Bottom  = 1 << static_cast<unsigned>(BoxSide::Bottom),
    Left    = 1 << static_cast<unsigned>(BoxSide::Left)
};

using BoxSideSet = OptionSet<BoxSideFlag>;

template<typename T> class RectEdges {
public:
    RectEdges() = default;

    RectEdges(const RectEdges&) = default;
    RectEdges& operator=(const RectEdges&) = default;

    template<typename U>
    RectEdges(U&& top, U&& right, U&& bottom, U&& left)
        : m_sides({ { std::forward<T>(top), std::forward<T>(right), std::forward<T>(bottom), std::forward<T>(left) } })
    { }

    T& at(BoxSide side) { return m_sides[static_cast<size_t>(side)]; }
    T& operator[](BoxSide side) { return m_sides[static_cast<size_t>(side)]; }
    T& top() { return at(BoxSide::Top); }
    T& right() { return at(BoxSide::Right); }
    T& bottom() { return at(BoxSide::Bottom); }
    T& left() { return at(BoxSide::Left); }

    const T& at(BoxSide side) const { return m_sides[static_cast<size_t>(side)]; }
    const T& operator[](BoxSide side) const { return m_sides[static_cast<size_t>(side)]; }
    const T& top() const { return at(BoxSide::Top); }
    const T& right() const { return at(BoxSide::Right); }
    const T& bottom() const { return at(BoxSide::Bottom); }
    const T& left() const { return at(BoxSide::Left); }

    void setAt(BoxSide side, const T& v) { at(side) = v; }
    void setTop(const T& top) { setAt(BoxSide::Top, top); }
    void setRight(const T& right) { setAt(BoxSide::Right, right); }
    void setBottom(const T& bottom) { setAt(BoxSide::Bottom, bottom); }
    void setLeft(const T& left) { setAt(BoxSide::Left, left); }

    RectEdges<T> xFlippedCopy() const
    {
        RectEdges<T> copy { *this };
        copy.left() = right();
        copy.right() = left();
        return copy;
    }
    RectEdges<T> yFlippedCopy() const
    {
        RectEdges<T> copy { *this };
        copy.top() = bottom();
        copy.bottom() = top();
        return copy;
    }

    T& before(WritingMode writingMode) { return at(mapLogicalSideToPhysicalSide(writingMode, LogicalBoxSide::BlockStart)); }
    T& after(WritingMode writingMode) { return at(mapLogicalSideToPhysicalSide(writingMode, LogicalBoxSide::BlockEnd)); }
    T& start(WritingMode writingMode, TextDirection direction = TextDirection::LTR) { return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), LogicalBoxSide::InlineStart)); }
    T& end(WritingMode writingMode, TextDirection direction = TextDirection::LTR) { return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), LogicalBoxSide::InlineEnd)); }

    const T& before(WritingMode writingMode) const { return at(mapLogicalSideToPhysicalSide(writingMode, LogicalBoxSide::BlockStart)); }
    const T& after(WritingMode writingMode) const { return at(mapLogicalSideToPhysicalSide(writingMode, LogicalBoxSide::BlockEnd)); }
    const T& start(WritingMode writingMode, TextDirection direction = TextDirection::LTR) const { return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), LogicalBoxSide::InlineStart)); }
    const T& end(WritingMode writingMode, TextDirection direction = TextDirection::LTR) const { return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), LogicalBoxSide::InlineEnd)); }

    void setBefore(const T& before, WritingMode writingMode) { this->before(writingMode) = before; }
    void setAfter(const T& after, WritingMode writingMode) { this->after(writingMode) = after; }
    void setStart(const T& start, WritingMode writingMode, TextDirection direction = TextDirection::LTR) { this->start(writingMode, direction) = start; }
    void setEnd(const T& end, WritingMode writingMode, TextDirection direction = TextDirection::LTR) { this->end(writingMode, direction) = end; }

    RectEdges<T> blockFlippedCopy(WritingMode writingMode) const
    {
        if (isHorizontalWritingMode(writingMode))
            return yFlippedCopy();
        return xFlippedCopy();
    }
    RectEdges<T> inlineFlippedCopy(WritingMode writingMode) const
    {
        if (isHorizontalWritingMode(writingMode))
            return xFlippedCopy();
        return yFlippedCopy();
    }

    friend bool operator==(const RectEdges&, const RectEdges&) = default;

    bool isZero() const
    {
        return !top() && !right() && !bottom() && !left();
    }

private:
    std::array<T, 4> m_sides { };
};

template<typename T>
constexpr RectEdges<T> operator+(const RectEdges<T>& a, const RectEdges<T>& b)
{
    return {
        a.top() + b.top(),
        a.right() + b.right(),
        a.bottom() + b.bottom(),
        a.left() + b.left()
    };
}

template<typename T>
inline RectEdges<T>& operator+=(RectEdges<T>& a, const RectEdges<T>& b)
{
    a = a + b;
    return a;
}

template<typename T>
TextStream& operator<<(TextStream& ts, const RectEdges<T>& edges)
{
    ts << "[top " << edges.top() << " right " << edges.right() << " bottom " << edges.bottom() << " left " << edges.left() << "]";
    return ts;
}

}
