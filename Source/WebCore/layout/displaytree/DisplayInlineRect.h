/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutUnits.h"

namespace WebCore {
namespace Display {

class InlineRect {
public:
    InlineRect() = default;
    InlineRect(InlineLayoutUnit top, InlineLayoutUnit left, InlineLayoutUnit width, InlineLayoutUnit height);
    InlineRect(const InlineLayoutPoint& topLeft, InlineLayoutUnit width, InlineLayoutUnit height);
    InlineRect(const InlineLayoutPoint& topLeft, const InlineLayoutSize&);
    
    InlineLayoutUnit top() const;
    InlineLayoutUnit left() const;
    InlineLayoutPoint topLeft() const;

    InlineLayoutUnit bottom() const;
    InlineLayoutUnit right() const;        

    InlineLayoutUnit width() const;
    InlineLayoutUnit height() const;
    InlineLayoutSize size() const;

    void setTop(InlineLayoutUnit);
    void setBottom(InlineLayoutUnit);
    void setLeft(InlineLayoutUnit);
    void setTopLeft(const InlineLayoutPoint&);
    void setWidth(InlineLayoutUnit);
    void setHeight(InlineLayoutUnit);

    void moveHorizontally(InlineLayoutUnit);
    void moveVertically(InlineLayoutUnit);
    void moveBy(InlineLayoutPoint);

    void expand(Optional<InlineLayoutUnit>, Optional<InlineLayoutUnit>);
    void expandToContain(const InlineRect&);
    void expandHorizontally(InlineLayoutUnit delta) { expand(delta, { }); }
    void expandVertically(InlineLayoutUnit delta) { expand({ }, delta); }
    void expandVerticallyToContain(const InlineRect&);
    void inflate(InlineLayoutUnit);

    operator InlineLayoutRect() const;

private:
#if ASSERT_ENABLED
    void invalidateTop() { m_hasValidTop = false; }
    void invalidateLeft() { m_hasValidLeft = false; }
    void invalidateWidth() { m_hasValidWidth = false; }
    void invalidateHeight() { m_hasValidHeight = false; }
    void invalidatePosition();

    bool hasValidPosition() const { return m_hasValidTop && m_hasValidLeft; }
    bool hasValidSize() const { return m_hasValidWidth && m_hasValidHeight; }
    bool hasValidGeometry() const { return hasValidPosition() && hasValidSize(); }

    void setHasValidPosition();
    void setHasValidSize();

    bool m_hasValidTop { false };
    bool m_hasValidLeft { false };
    bool m_hasValidWidth { false };
    bool m_hasValidHeight { false };
#endif // ASSERT_ENABLED
    InlineLayoutRect m_rect;
};

inline InlineRect::InlineRect(InlineLayoutUnit top, InlineLayoutUnit left, InlineLayoutUnit width, InlineLayoutUnit height)
    : m_rect(left, top, width, height)
{
#if ASSERT_ENABLED
    m_hasValidTop = true;
    m_hasValidLeft = true;
    m_hasValidWidth = true;
    m_hasValidHeight = true;
#endif
}

inline InlineRect::InlineRect(const InlineLayoutPoint& topLeft, InlineLayoutUnit width, InlineLayoutUnit height)
    : InlineRect(topLeft.y(), topLeft.x(), width, height)
{
}

inline InlineRect::InlineRect(const InlineLayoutPoint& topLeft, const InlineLayoutSize& size)
    : InlineRect(topLeft.y(), topLeft.x(), size.width(), size.height())
{
}

#if ASSERT_ENABLED
inline void InlineRect::invalidatePosition()
{
    invalidateTop();
    invalidateLeft();
}

inline void InlineRect::setHasValidPosition()
{
    m_hasValidTop = true;
    m_hasValidLeft = true;
}

inline void InlineRect::setHasValidSize()
{
    m_hasValidWidth = true;
    m_hasValidHeight = true;
}
#endif // ASSERT_ENABLED

inline InlineLayoutUnit InlineRect::top() const
{
    ASSERT(m_hasValidTop);
    return m_rect.y();
}

inline InlineLayoutUnit InlineRect::left() const
{
    ASSERT(m_hasValidLeft);
    return m_rect.x();
}

inline InlineLayoutUnit InlineRect::bottom() const
{
    ASSERT(m_hasValidTop && m_hasValidHeight);
    return m_rect.maxY();
}

inline InlineLayoutUnit InlineRect::right() const
{
    ASSERT(m_hasValidLeft && m_hasValidWidth);
    return m_rect.maxX();
}

inline InlineLayoutPoint InlineRect::topLeft() const
{
    ASSERT(hasValidPosition());
    return m_rect.minXMinYCorner();
}

inline InlineLayoutSize InlineRect::size() const
{
    ASSERT(hasValidSize());
    return m_rect.size();
}

inline InlineLayoutUnit InlineRect::width() const
{
    ASSERT(m_hasValidWidth);
    return m_rect.width();
}

inline InlineLayoutUnit InlineRect::height() const
{
    ASSERT(m_hasValidHeight);
    return m_rect.height();
}

inline void InlineRect::setTopLeft(const InlineLayoutPoint& topLeft)
{
#if ASSERT_ENABLED
    setHasValidPosition();
#endif
    m_rect.setLocation(topLeft);
}

inline void InlineRect::setTop(InlineLayoutUnit top)
{
#if ASSERT_ENABLED
    m_hasValidTop = true;
#endif
    m_rect.setY(top);
}

inline void InlineRect::setBottom(InlineLayoutUnit bottom)
{
#if ASSERT_ENABLED
    m_hasValidTop = true;
    m_hasValidHeight = true;
#endif
    m_rect.shiftMaxYEdgeTo(bottom);
}

inline void InlineRect::setLeft(InlineLayoutUnit left)
{
#if ASSERT_ENABLED
    m_hasValidLeft = true;
#endif
    m_rect.setX(left);
}

inline void InlineRect::setWidth(InlineLayoutUnit width)
{
#if ASSERT_ENABLED
    m_hasValidWidth = true;
#endif
    m_rect.setWidth(width);
}

inline void InlineRect::setHeight(InlineLayoutUnit height)
{
#if ASSERT_ENABLED
    m_hasValidHeight = true;
#endif
    m_rect.setHeight(height);
}

inline void InlineRect::moveHorizontally(InlineLayoutUnit offset)
{
    ASSERT(m_hasValidLeft);
    m_rect.move(InlineLayoutSize { offset, 0 });
}

inline void InlineRect::moveVertically(InlineLayoutUnit offset)
{
    ASSERT(m_hasValidTop);
    m_rect.move(InlineLayoutSize { 0, offset });
}

inline void InlineRect::moveBy(InlineLayoutPoint offset)
{
    ASSERT(m_hasValidTop);
    ASSERT(m_hasValidLeft);
    m_rect.moveBy(offset);
}

inline void InlineRect::expand(Optional<InlineLayoutUnit> width, Optional<InlineLayoutUnit> height)
{
    ASSERT(!width || m_hasValidWidth);
    ASSERT(!height || m_hasValidHeight);
    m_rect.expand(width.valueOr(0), height.valueOr(0));
}

inline void InlineRect::expandToContain(const InlineRect& other)
{
#if ASSERT_ENABLED
    m_hasValidTop = true;
    m_hasValidLeft = true;
    m_hasValidWidth = true;
    m_hasValidHeight = true;
#endif
    m_rect.uniteEvenIfEmpty(other);
}

inline void InlineRect::expandVerticallyToContain(const InlineRect& other)
{
    auto containTop = std::min(top(), other.top());
    auto containBottom = std::max(bottom(), other.bottom());
    setTop(containTop);
    setBottom(containBottom);
}

inline void InlineRect::inflate(InlineLayoutUnit inflate)
{
    ASSERT(hasValidGeometry());
    m_rect.inflate(inflate);
}

inline InlineRect::operator InlineLayoutRect() const
{
    ASSERT(hasValidGeometry()); 
    return m_rect;
}

}
}
#endif
