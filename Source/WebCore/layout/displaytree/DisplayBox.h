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
#include "RenderStyleConstants.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class RenderStyle;

namespace Layout {
class BlockFormattingContext;
class FormattingContext;
class LayoutContext;
}

namespace Display {

class Box {
    WTF_MAKE_ISO_ALLOCATED(Box);
public:
    friend class Layout::BlockFormattingContext;
    friend class Layout::FormattingContext;
    friend class Layout::LayoutContext;

    class Rect {
    public:
        Rect() = default;
        Rect(const LayoutPoint&, const LayoutSize&);
        
        LayoutUnit top() const;
        LayoutUnit left() const;
        LayoutPoint topLeft() const;

        LayoutUnit bottom() const;
        LayoutUnit right() const;        
        LayoutPoint bottomRight() const;

        LayoutUnit width() const;
        LayoutUnit height() const;
        LayoutSize size() const;

        void setTop(LayoutUnit);
        void setLeft(LayoutUnit);
        void setTopLeft(const LayoutPoint&);
        void setWidth(LayoutUnit);
        void setHeight(LayoutUnit);

        void shiftLeftTo(LayoutUnit);
        void shiftRightTo(LayoutUnit);
        void shiftTopTo(LayoutUnit);
        void shiftBottomTo(LayoutUnit);

        void expand(LayoutUnit, LayoutUnit);

        operator LayoutRect() const { return m_rect; }

    private:
#if !ASSERT_DISABLED
        void invalidateTop() { m_hasValidTop = false; }
        void invalidateLeft() { m_hasValidLeft = false; }
        void invalidateWidth() { m_hasValidWidth = false; }
        void invalidateHeight() { m_hasValidHeight = false; }
        void invalidatePosition();

        bool hasValidPosition() const { return m_hasValidTop && m_hasValidLeft; }
        bool hasValidSize() const { return m_hasValidWidth && m_hasValidHeight; }
        bool hasValidGeometry() const { return hasValidPosition() && hasValidSize(); }
    
        void setHasValidPosition();

        bool m_hasValidTop { false };
        bool m_hasValidLeft { false };
        bool m_hasValidWidth { false };
        bool m_hasValidHeight { false };
#endif
        LayoutRect m_rect;
    };

    ~Box();

    Rect rect() const { return m_rect; }

    LayoutUnit top() const { return m_rect.top(); }
    LayoutUnit left() const { return m_rect.left(); }
    LayoutUnit bottom() const { return m_rect.bottom(); }
    LayoutUnit right() const { return m_rect.right(); }

    LayoutPoint topLeft() const { return m_rect.topLeft(); }
    LayoutPoint bottomRight() const { return m_rect.bottomRight(); }

    LayoutSize size() const { return m_rect.size(); }
    LayoutUnit width() const { return m_rect.width(); }
    LayoutUnit height() const { return m_rect.height(); }

    LayoutUnit marginTop() const;
    LayoutUnit marginLeft() const;
    LayoutUnit marginBottom() const;
    LayoutUnit marginRight() const;

    LayoutUnit borderTop() const;
    LayoutUnit borderLeft() const;
    LayoutUnit borderBottom() const;
    LayoutUnit borderRight() const;

    LayoutUnit paddingTop() const;
    LayoutUnit paddingLeft() const;
    LayoutUnit paddingBottom() const;
    LayoutUnit paddingRight() const;

    Rect marginBox() const;
    Rect borderBox() const;
    Rect paddingBox() const;
    Rect contentBox() const;

private:
    Box(const RenderStyle&);

    struct Style {
        Style(const RenderStyle&);

        BoxSizing boxSizing { BoxSizing::ContentBox };
    };

    void setTopLeft(const LayoutPoint& topLeft) { m_rect.setTopLeft(topLeft); }
    void setTop(LayoutUnit top) { m_rect.setTop(top); }
    void setLeft(LayoutUnit left) { m_rect.setLeft(left); }
    void setWidth(LayoutUnit width) { m_rect.setWidth(width); }
    void setHeight(LayoutUnit height) { m_rect.setHeight(height); }

    struct Edges {
        Edges() = default;
        Edges(LayoutUnit top, LayoutUnit left, LayoutUnit bottom, LayoutUnit right)
            : top(top)
            , left(left)
            , bottom(bottom)
            , right(right)
            { }

        LayoutUnit top;
        LayoutUnit left;
        LayoutUnit bottom;
        LayoutUnit right;
    };
    void setMargin(Edges);
    void setBorder(Edges);
    void setPadding(Edges);

#if !ASSERT_DISABLED
    void invalidateMargin() { m_hasValidMargin = false; }
    void invalidateBorder() { m_hasValidBorder = false; }
    void invalidatePadding() { m_hasValidPadding = false; }

    void setHasValidMargin();
    void setHasValidBorder();
    void setHasValidPadding();
#endif

    const Style m_style;

    Rect m_rect;

    Edges m_margin;
    Edges m_border;
    Edges m_padding;

#if !ASSERT_DISABLED
    bool m_hasValidMargin { false };
    bool m_hasValidBorder { false };
    bool m_hasValidPadding { false };
#endif
};

#if !ASSERT_DISABLED
inline void Box::Rect::invalidatePosition()
{
    invalidateTop();
    invalidateLeft();
}

inline void Box::Rect::setHasValidPosition()
{
    m_hasValidTop = true;
    m_hasValidLeft = true;
}
#endif

inline Box::Rect::Rect(const LayoutPoint& topLeft, const LayoutSize& size)
    : m_rect(topLeft, size)
{

}

inline LayoutUnit Box::Rect::top() const
{
    ASSERT(m_hasValidTop);
    return m_rect.y();
}

inline LayoutUnit Box::Rect::left() const
{
    ASSERT(m_hasValidLeft);
    return m_rect.x();
}

inline LayoutUnit Box::Rect::bottom() const
{
    ASSERT(m_hasValidTop && m_hasValidHeight);
    return m_rect.maxY();
}

inline LayoutUnit Box::Rect::right() const
{
    ASSERT(m_hasValidLeft && m_hasValidWidth);
    return m_rect.maxX();
}

inline LayoutPoint Box::Rect::topLeft() const
{
    ASSERT(hasValidPosition());
    return m_rect.minXMinYCorner();
}

inline LayoutPoint Box::Rect::bottomRight() const
{
    ASSERT(hasValidGeometry());
    return m_rect.maxXMaxYCorner();
}

inline LayoutSize Box::Rect::size() const
{
    ASSERT(hasValidSize());
    return m_rect.size();
}

inline LayoutUnit Box::Rect::width() const
{
    ASSERT(m_hasValidWidth);
    return m_rect.width();
}

inline LayoutUnit Box::Rect::height() const
{
    ASSERT(m_hasValidHeight);
    return m_rect.height();
}

inline void Box::Rect::setTopLeft(const LayoutPoint& topLeft)
{
#if !ASSERT_DISABLED
    setHasValidPosition();
#endif
    m_rect.setLocation(topLeft);
}

inline void Box::Rect::setTop(LayoutUnit top)
{
#if !ASSERT_DISABLED
    m_hasValidTop = true;
#endif
    m_rect.setY(top);
}

inline void Box::Rect::setLeft(LayoutUnit left)
{
#if !ASSERT_DISABLED
    m_hasValidLeft = true;
#endif
    m_rect.setX(left);
}

inline void Box::Rect::setWidth(LayoutUnit width)
{
#if !ASSERT_DISABLED
    m_hasValidWidth = true;
#endif
    ASSERT(m_hasValidLeft);
    m_rect.setWidth(width);
}

inline void Box::Rect::setHeight(LayoutUnit height)
{
#if !ASSERT_DISABLED
    m_hasValidHeight = true;
#endif
    ASSERT(m_hasValidTop);
    m_rect.setHeight(height);
}

inline void Box::Rect::shiftLeftTo(LayoutUnit left)
{
    ASSERT(m_hasValidLeft);
    m_rect.shiftXEdgeTo(left);
}

inline void Box::Rect::shiftRightTo(LayoutUnit right)
{
    ASSERT(m_hasValidLeft && m_hasValidWidth);
    m_rect.shiftMaxXEdgeTo(right);
}

inline void Box::Rect::shiftTopTo(LayoutUnit top)
{
    ASSERT(m_hasValidTop);
    m_rect.shiftYEdgeTo(top);
}

inline void Box::Rect::shiftBottomTo(LayoutUnit bottom)
{
    ASSERT(m_hasValidTop && m_hasValidHeight);
    m_rect.shiftMaxYEdgeTo(bottom);
}

inline void Box::Rect::expand(LayoutUnit width, LayoutUnit height)
{
    ASSERT(hasValidGeometry());
    m_rect.expand(width, height);
}

inline void Box::setMargin(Edges margin)
{
#if !ASSERT_DISABLED
    void setHasValidMargin();
#endif
    m_margin = margin;
}

inline void Box::setBorder(Edges border)
{
#if !ASSERT_DISABLED
    void setHasValidBorder();
#endif
    m_border = border;
}

inline void Box::setPadding(Edges padding)
{
#if !ASSERT_DISABLED
    void setHasValidPadding();
#endif
    m_padding = padding;
}

inline LayoutUnit Box::marginTop() const
{
    ASSERT(m_hasValidMargin);
    return m_margin.top;
}

inline LayoutUnit Box::marginLeft() const
{
    ASSERT(m_hasValidMargin);
    return m_margin.left;
}

inline LayoutUnit Box::marginBottom() const
{
    ASSERT(m_hasValidMargin);
    return m_margin.bottom;
}

inline LayoutUnit Box::marginRight() const
{
    ASSERT(m_hasValidMargin);
    return m_margin.right;
}

inline LayoutUnit Box::paddingTop() const
{
    ASSERT(m_hasValidPadding);
    return m_padding.top;
}

inline LayoutUnit Box::paddingLeft() const
{
    ASSERT(m_hasValidPadding);
    return m_padding.left;
}

inline LayoutUnit Box::paddingBottom() const
{
    ASSERT(m_hasValidPadding);
    return m_padding.bottom;
}

inline LayoutUnit Box::paddingRight() const
{
    ASSERT(m_hasValidPadding);
    return m_padding.right;
}

inline LayoutUnit Box::borderTop() const
{
    ASSERT(m_hasValidBorder);
    return m_border.top;
}

inline LayoutUnit Box::borderLeft() const
{
    ASSERT(m_hasValidBorder);
    return m_border.left;
}

inline LayoutUnit Box::borderBottom() const
{
    ASSERT(m_hasValidBorder);
    return m_border.bottom;
}

inline LayoutUnit Box::borderRight() const
{
    ASSERT(m_hasValidBorder);
    return m_border.right;
}

}
}
#endif
