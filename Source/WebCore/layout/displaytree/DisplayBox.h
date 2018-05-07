/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "LayoutPoint.h"
#include "LayoutRect.h"
#include "LayoutUnit.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

namespace Layout {
class LayoutContext;
class BlockFormattingContext;
}

namespace Display {

class Box {
    WTF_MAKE_ISO_ALLOCATED(Box);
public:
    friend class Layout::LayoutContext;
    friend class Layout::BlockFormattingContext;

    ~Box();

    LayoutRect rect() const;

    LayoutUnit top() const;
    LayoutUnit left() const;
    LayoutUnit bottom() const;
    LayoutUnit right() const;

    LayoutPoint topLeft() const;
    LayoutPoint bottomRight() const;

    LayoutSize size() const;
    LayoutUnit width() const;
    LayoutUnit height() const;

    LayoutUnit marginTop() const;
    LayoutUnit marginLeft() const;
    LayoutUnit marginBottom() const;
    LayoutUnit marginRight() const;

    LayoutRect marginBox() const;
    LayoutRect borderBox() const;
    LayoutRect paddingBox() const;
    LayoutRect contentBox() const;

private:
    Box();

    void setRect(const LayoutRect&);
    void setTopLeft(const LayoutPoint&);
    void setTop(LayoutUnit);
    void setLeft(LayoutUnit);
    void setSize(const LayoutSize&);
    void setWidth(LayoutUnit);
    void setHeight(LayoutUnit);

    void setMargin(LayoutUnit marginTop, LayoutUnit marginLeft, LayoutUnit marginRight, LayoutUnit marginBottom);
    void setBorder(LayoutUnit borderTop, LayoutUnit borderLeft, LayoutUnit borderRight, LayoutUnit borderBottom);
    void setPadding(LayoutUnit paddingTop, LayoutUnit paddingLeft, LayoutUnit paddingRight, LayoutUnit paddingBottom);

#if !ASSERT_DISABLED
    void invalidateTop() { m_hasValidTop = false; }
    void invalidateLeft() { m_hasValidLeft = false; }
    void invalidateWidth() { m_hasValidWidth = false; }
    void invalidateHeight() { m_hasValidHeight = false; }
    void invalidatePosition();
    void invalidateSize();
    void invalidateMargin() { m_hasValidMargin = false; }
    void invalidateBorder() { m_hasValidBorder = false; }
    void invalidatePadding() { m_hasValidPadding = false; }

    bool hasValidPosition() const { return m_hasValidTop && m_hasValidLeft; }
    bool hasValidSize() const { return m_hasValidWidth && m_hasValidHeight; }
    bool hasValidGeometry() const { return hasValidPosition() && hasValidSize(); }
    
    void setHasValidPosition();
    void setHasValidSize();
    void setHasValidGeometry();
    
    void setHasValidMargin();
    void setHasValidBorder();
    void setHasValidPadding();
#endif

    LayoutRect m_rect;

    LayoutUnit m_marginTop;
    LayoutUnit m_marginLeft;
    LayoutUnit m_marginBottom;
    LayoutUnit m_marginRight;

    LayoutUnit m_borderTop;
    LayoutUnit m_borderLeft;
    LayoutUnit m_borderBottom;
    LayoutUnit m_borderRight;

    LayoutUnit m_paddingTop;
    LayoutUnit m_paddingLeft;
    LayoutUnit m_paddingBottom;
    LayoutUnit m_paddingRight;

#if !ASSERT_DISABLED
    bool m_hasValidTop { false };
    bool m_hasValidLeft { false };
    bool m_hasValidWidth { false };
    bool m_hasValidHeight { false };
    bool m_hasValidMargin { false };
    bool m_hasValidBorder { false };
    bool m_hasValidPadding { false };
#endif
};

#if !ASSERT_DISABLED
inline void Box::invalidatePosition()
{
    invalidateTop();
    invalidateLeft();
}

inline void Box::invalidateSize()
{
    invalidateWidth();
    invalidateHeight();
}

inline void Box::setHasValidPosition()
{
    m_hasValidTop = true;
    m_hasValidLeft = true;
}

inline void Box::setHasValidSize()
{
    m_hasValidWidth = true;
    m_hasValidHeight = true;
}

inline void Box::setHasValidGeometry()
{
    setHasValidPosition();
    setHasValidSize();
}
#endif

inline LayoutRect Box::rect() const
{
    ASSERT(hasValidGeometry());
    return m_rect;
}

inline LayoutUnit Box::top() const
{
    ASSERT(m_hasValidTop);
    return m_rect.y();
}

inline LayoutUnit Box::left() const
{
    ASSERT(m_hasValidLeft);
    return m_rect.x();
}

inline LayoutUnit Box::bottom() const
{
    ASSERT(m_hasValidTop && m_hasValidHeight);
    return m_rect.maxY();
}

inline LayoutUnit Box::right() const
{
    ASSERT(m_hasValidLeft && m_hasValidWidth);
    return m_rect.maxX();
}

inline LayoutPoint Box::topLeft() const
{
    ASSERT(hasValidPosition());
    return m_rect.location();
}

inline LayoutPoint Box::bottomRight() const
{
    ASSERT(hasValidGeometry());
    return m_rect.maxXMaxYCorner();
}

inline LayoutSize Box::size() const
{
    ASSERT(hasValidSize());
    return m_rect.size();
}

inline LayoutUnit Box::width() const
{
    ASSERT(m_hasValidWidth);
    return m_rect.width();
}

inline LayoutUnit Box::height() const
{
    ASSERT(m_hasValidHeight);
    return m_rect.height();
}

inline void Box::setRect(const LayoutRect& rect)
{
#if !ASSERT_DISABLED
    setHasValidGeometry();
#endif
    m_rect = rect;
}

inline void Box::setTopLeft(const LayoutPoint& topLeft)
{
#if !ASSERT_DISABLED
    setHasValidPosition();
#endif
    m_rect.setLocation(topLeft);
}

inline void Box::setTop(LayoutUnit top)
{
#if !ASSERT_DISABLED
    m_hasValidTop = true;
#endif
    m_rect.setY(top);
}

inline void Box::setLeft(LayoutUnit left)
{
#if !ASSERT_DISABLED
    m_hasValidLeft = true;
#endif
    m_rect.setX(left);
}

inline void Box::setSize(const LayoutSize& size)
{
#if !ASSERT_DISABLED
    setHasValidSize();
#endif
    m_rect.setSize(size);
}

inline void Box::setWidth(LayoutUnit width)
{
#if !ASSERT_DISABLED
    m_hasValidWidth = true;
#endif
    m_rect.setWidth(width);
}

inline void Box::setHeight(LayoutUnit height)
{
#if !ASSERT_DISABLED
    m_hasValidHeight = true;
#endif
    m_rect.setHeight(height);
}

inline void Box::setMargin(LayoutUnit marginTop, LayoutUnit marginLeft, LayoutUnit marginRight, LayoutUnit marginBottom)
{
#if !ASSERT_DISABLED
    void setHasValidMargin();
#endif
    m_marginTop = marginTop;
    m_marginLeft = marginLeft;
    m_marginBottom = marginBottom;
    m_marginRight = marginRight;
}

inline void Box::setBorder(LayoutUnit borderTop, LayoutUnit borderLeft, LayoutUnit borderRight, LayoutUnit borderBottom)
{
#if !ASSERT_DISABLED
    void setHasValidBorder();
#endif
    m_borderTop = borderTop;
    m_borderLeft = borderLeft;
    m_borderBottom = borderBottom;
    m_borderRight = borderRight;
}

inline void Box::setPadding(LayoutUnit paddingTop, LayoutUnit paddingLeft, LayoutUnit paddingRight, LayoutUnit paddingBottom)
{
#if !ASSERT_DISABLED
    void setHasValidPadding();
#endif
    m_paddingTop = paddingTop;
    m_paddingLeft = paddingLeft;
    m_paddingBottom = paddingBottom;
    m_paddingRight = paddingRight;
}

inline LayoutUnit Box::marginTop() const
{
    ASSERT(m_hasValidMargin);
    return m_marginTop;
}

inline LayoutUnit Box::marginLeft() const
{
    ASSERT(m_hasValidMargin);
    return m_marginLeft;
}

inline LayoutUnit Box::marginBottom() const
{
    ASSERT(m_hasValidMargin);
    return m_marginBottom;
}

inline LayoutUnit Box::marginRight() const
{
    ASSERT(m_hasValidMargin);
    return m_marginRight;
}

}
}
#endif
