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

namespace WebCore {
    
template<typename T> class RectEdges {
public:
    RectEdges() = default;
    
    template<typename U>
    RectEdges(U&& top, U&& right, U&& bottom, U&& left)
        : m_sides({ { std::forward<T>(top), std::forward<T>(right), std::forward<T>(bottom), std::forward<T>(left) } })
    { }
    
    T& at(PhysicalBoxSide side) { return m_sides[static_cast<size_t>(side)]; }
    T& top() { return at(PhysicalBoxSide::Top); }
    T& right() { return at(PhysicalBoxSide::Right); }
    T& bottom() { return at(PhysicalBoxSide::Bottom); }
    T& left() { return at(PhysicalBoxSide::Left); }
    
    const T& at(PhysicalBoxSide side) const { return m_sides[static_cast<size_t>(side)]; }
    const T& top() const { return at(PhysicalBoxSide::Top); }
    const T& right() const { return at(PhysicalBoxSide::Right); }
    const T& bottom() const { return at(PhysicalBoxSide::Bottom); }
    const T& left() const { return at(PhysicalBoxSide::Left); }
    
    void setAt(PhysicalBoxSide side, const T& v) { at(side) = v; }
    void setTop(const T& top) { setAt(PhysicalBoxSide::Top, top); }
    void setRight(const T& right) { setAt(PhysicalBoxSide::Right, right); }
    void setBottom(const T& bottom) { setAt(PhysicalBoxSide::Bottom, bottom); }
    void setLeft(const T& left) { setAt(PhysicalBoxSide::Left, left); }
    
    T& before(WritingMode writingMode) { return at(mapLogicalSideToPhysicalSide(writingMode, LogicalBoxSide::Before)); }
    T& after(WritingMode writingMode) { return at(mapLogicalSideToPhysicalSide(writingMode, LogicalBoxSide::After)); }
    T& start(WritingMode writingMode, TextDirection direction = TextDirection::LTR) { return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), LogicalBoxSide::Start)); }
    T& end(WritingMode writingMode, TextDirection direction = TextDirection::LTR) { return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), LogicalBoxSide::End)); }
    
    const T& before(WritingMode writingMode) const { return at(mapLogicalSideToPhysicalSide(writingMode, LogicalBoxSide::Before)); }
    const T& after(WritingMode writingMode) const { return at(mapLogicalSideToPhysicalSide(writingMode, LogicalBoxSide::After)); }
    const T& start(WritingMode writingMode, TextDirection direction = TextDirection::LTR) const { return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), LogicalBoxSide::Start)); }
    const T& end(WritingMode writingMode, TextDirection direction = TextDirection::LTR) const { return at(mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), LogicalBoxSide::End)); }
    
    void setBefore(const T& before, WritingMode writingMode) { this->before(writingMode) = before; }
    void setAfter(const T& after, WritingMode writingMode) { this->after(writingMode) = after; }
    void setStart(const T& start, WritingMode writingMode, TextDirection direction = TextDirection::LTR) { this->start(writingMode, direction) = start; }
    void setEnd(const T& end, WritingMode writingMode, TextDirection direction = TextDirection::LTR) { this->end(writingMode, direction) = end; }
    
    bool operator==(const RectEdges& other) const { return m_sides == other.m_sides; }
    bool operator!=(const RectEdges& other) const { return m_sides != other.m_sides; }

private:
    std::array<T, 4> m_sides {{0, 0, 0, 0}};
};

}
