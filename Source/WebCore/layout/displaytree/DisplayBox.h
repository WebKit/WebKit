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

#include "DisplayRect.h"
#include "LayoutUnits.h"
#include "RenderStyleConstants.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class RenderStyle;

namespace Layout {
class BlockFormattingContext;
class FloatAvoider;
class FloatBox;
class FormattingContext;
class FloatingContext;
class InlineFormattingContext;
class LayoutState;
class TableFormattingContext;
}

namespace Display {

class Box {
    WTF_MAKE_ISO_ALLOCATED(Box);
public:
    friend class Layout::BlockFormattingContext;
    friend class Layout::FloatAvoider;
    friend class Layout::FloatBox;
    friend class Layout::FormattingContext;
    friend class Layout::FloatingContext;
    friend class Layout::InlineFormattingContext;
    friend class Layout::LayoutState;
    friend class Layout::TableFormattingContext;

    Box(const RenderStyle&);
    Box(const Box&);
    ~Box();

    LayoutUnit top() const;
    LayoutUnit left() const;
    LayoutUnit bottom() const { return top() + height(); }
    LayoutUnit right() const { return left() + width(); }

    LayoutPoint topLeft() const;
    LayoutPoint bottomRight() const { return { right(), bottom() }; }

    LayoutSize size() const { return { width(), height() }; }
    LayoutUnit width() const { return borderLeft() + paddingBoxWidth() + borderRight(); }
    LayoutUnit height() const { return borderTop() + paddingBoxHeight() + borderBottom(); }
    Rect rect() const { return { top(), left(), width(), height() }; }
    Rect rectWithMargin() const;

    Layout::UsedVerticalMargin verticalMargin() const;
    Layout::UsedHorizontalMargin horizontalMargin() const;
    LayoutUnit marginBefore() const;
    LayoutUnit marginStart() const;
    LayoutUnit marginAfter() const;
    LayoutUnit marginEnd() const;
    bool hasCollapsedThroughMargin() const { return m_verticalMargin.isCollapsedThrough(); }
    bool hasClearance() const { return m_hasClearance; }

    LayoutUnit nonCollapsedMarginBefore() const;
    LayoutUnit nonCollapsedMarginAfter() const;
    Optional<LayoutUnit> computedMarginStart() const;
    Optional<LayoutUnit> computedMarginEnd() const;

    LayoutUnit borderTop() const;
    LayoutUnit borderLeft() const;
    LayoutUnit borderBottom() const;
    LayoutUnit borderRight() const;
    LayoutUnit verticalBorder() const { return borderTop() + borderBottom(); }
    LayoutUnit horizontalBorder() const { return borderLeft() + borderRight(); }

    Optional<LayoutUnit> paddingTop() const;
    Optional<LayoutUnit> paddingLeft() const;
    Optional<LayoutUnit> paddingBottom() const;
    Optional<LayoutUnit> paddingRight() const;
    Optional<LayoutUnit> verticalPadding() const;
    Optional<LayoutUnit> horizontalPadding() const;

    LayoutUnit contentBoxTop() const { return paddingBoxTop() + paddingTop().valueOr(0); }
    LayoutUnit contentBoxLeft() const { return paddingBoxLeft() + paddingLeft().valueOr(0); }
    LayoutUnit contentBoxBottom() const { return contentBoxTop() + contentBoxHeight(); }
    LayoutUnit contentBoxRight() const { return contentBoxLeft() + contentBoxWidth(); }
    LayoutUnit contentBoxHeight() const;
    LayoutUnit contentBoxWidth() const;

    LayoutUnit paddingBoxTop() const { return borderTop(); }
    LayoutUnit paddingBoxLeft() const { return borderLeft(); }
    LayoutUnit paddingBoxBottom() const { return paddingBoxTop() + paddingBoxHeight(); }
    LayoutUnit paddingBoxRight() const { return paddingBoxLeft() + paddingBoxWidth(); }
    LayoutUnit paddingBoxHeight() const { return paddingTop().valueOr(0) + contentBoxHeight() + paddingBottom().valueOr(0); }
    LayoutUnit paddingBoxWidth() const { return paddingLeft().valueOr(0) + contentBoxWidth() + paddingRight().valueOr(0); }

    LayoutUnit borderBoxHeight() const { return borderTop() + paddingBoxHeight() + borderBottom(); }
    LayoutUnit borderBoxWidth() const { return borderLeft() + paddingBoxWidth() + borderRight(); }
    LayoutUnit marginBoxHeight() const { return marginBefore() + borderBoxHeight() + marginAfter(); }
    LayoutUnit marginBoxWidth() const { return marginStart() + borderBoxWidth() + marginEnd(); }

    LayoutUnit horizontalMarginBorderAndPadding() const { return marginStart() + horizontalBorder() + horizontalPadding().valueOr(0) + marginEnd(); }

    Rect marginBox() const;
    Rect nonCollapsedMarginBox() const;

    Rect borderBox() const;
    Rect paddingBox() const;
    Rect contentBox() const;

#if !ASSERT_DISABLED
    void setHasEstimatedMarginBefore() { m_hasEstimatedMarginBefore = true; }
#endif

private:
    struct Style {
        Style(const RenderStyle&);

        BoxSizing boxSizing { BoxSizing::ContentBox };
    };

    void setTopLeft(const LayoutPoint&);
    void setTop(LayoutUnit);
    void setLeft(LayoutUnit);
    void moveHorizontally(LayoutUnit offset) { m_topLeft.move(offset, 0_lu); }
    void moveVertically(LayoutUnit offset) { m_topLeft.move(0_lu, offset); }

    void setContentBoxHeight(LayoutUnit);
    void setContentBoxWidth(LayoutUnit);

    void setHorizontalMargin(Layout::UsedHorizontalMargin);
    void setVerticalMargin(Layout::UsedVerticalMargin);
    void setHorizontalComputedMargin(Layout::ComputedHorizontalMargin);
    void setHasClearance() { m_hasClearance = true; }

    void setBorder(Layout::Edges);
    void setPadding(Optional<Layout::Edges>);

#if !ASSERT_DISABLED
    void invalidateMargin();
    void invalidateBorder() { m_hasValidBorder = false; }
    void invalidatePadding() { m_hasValidPadding = false; }
    void invalidateEstimatedMarginBefore() { m_hasEstimatedMarginBefore = false; }

    void setHasValidTop() { m_hasValidTop = true; }
    void setHasValidLeft() { m_hasValidLeft = true; }
    void setHasValidVerticalMargin() { m_hasValidVerticalMargin = true; }
    void setHasValidVerticalNonCollapsedMargin() { m_hasValidVerticalNonCollapsedMargin = true; }
    void setHasValidHorizontalComputedMargin() { m_hasValidHorizontalComputedMargin = true; }
    void setHasValidHorizontalMargin() { m_hasValidHorizontalMargin = true; }

    void setHasValidBorder() { m_hasValidBorder = true; }
    void setHasValidPadding() { m_hasValidPadding = true; }

    void setHasValidContentHeight() { m_hasValidContentHeight = true; }
    void setHasValidContentWidth() { m_hasValidContentWidth = true; }
#endif

    const Style m_style;

    LayoutPoint m_topLeft;
    LayoutUnit m_contentWidth;
    LayoutUnit m_contentHeight;

    Layout::UsedHorizontalMargin m_horizontalMargin;
    Layout::UsedVerticalMargin m_verticalMargin;
    Layout::ComputedHorizontalMargin m_horizontalComputedMargin;
    bool m_hasClearance { false };

    Layout::Edges m_border;
    Optional<Layout::Edges> m_padding;

#if !ASSERT_DISABLED
    bool m_hasValidTop { false };
    bool m_hasValidLeft { false };
    bool m_hasValidHorizontalMargin { false };
    bool m_hasValidVerticalMargin { false };
    bool m_hasValidVerticalNonCollapsedMargin { false };
    bool m_hasValidHorizontalComputedMargin { false };
    bool m_hasValidBorder { false };
    bool m_hasValidPadding { false };
    bool m_hasValidContentHeight { false };
    bool m_hasValidContentWidth { false };
    bool m_hasEstimatedMarginBefore { false };
#endif
};

#if !ASSERT_DISABLED
inline void Box::invalidateMargin()
{
    m_hasValidHorizontalMargin = false;
    m_hasValidVerticalMargin = false;
}
#endif

inline LayoutUnit Box::top() const
{
    ASSERT(m_hasValidTop && (m_hasEstimatedMarginBefore || m_hasValidVerticalMargin));
    return m_topLeft.y();
}

inline LayoutUnit Box::left() const
{
    ASSERT(m_hasValidLeft && m_hasValidHorizontalMargin);
    return m_topLeft.x();
}

inline LayoutPoint Box::topLeft() const
{
    ASSERT(m_hasValidTop && (m_hasEstimatedMarginBefore || m_hasValidVerticalMargin));
    ASSERT(m_hasValidLeft && m_hasValidHorizontalMargin);
    return m_topLeft;
}

inline void Box::setTopLeft(const LayoutPoint& topLeft)
{
#if !ASSERT_DISABLED
    setHasValidTop();
    setHasValidLeft();
#endif
    m_topLeft = topLeft;
}

inline void Box::setTop(LayoutUnit top)
{
#if !ASSERT_DISABLED
    setHasValidTop();
#endif
    m_topLeft.setY(top);
}

inline void Box::setLeft(LayoutUnit left)
{
#if !ASSERT_DISABLED
    setHasValidLeft();
#endif
    m_topLeft.setX(left);
}

inline void Box::setContentBoxHeight(LayoutUnit height)
{ 
#if !ASSERT_DISABLED
    setHasValidContentHeight();
#endif
    m_contentHeight = height;
}

inline void Box::setContentBoxWidth(LayoutUnit width)
{ 
#if !ASSERT_DISABLED
    setHasValidContentWidth();
#endif
    m_contentWidth = width;
}

inline LayoutUnit Box::contentBoxHeight() const
{
    ASSERT(m_hasValidContentHeight);
    return m_contentHeight;
}

inline LayoutUnit Box::contentBoxWidth() const
{
    ASSERT(m_hasValidContentWidth);
    return m_contentWidth;
}

inline void Box::setHorizontalMargin(Layout::UsedHorizontalMargin margin)
{
#if !ASSERT_DISABLED
    setHasValidHorizontalMargin();
#endif
    m_horizontalMargin = margin;
}

inline void Box::setVerticalMargin(Layout::UsedVerticalMargin margin)
{
#if !ASSERT_DISABLED
    setHasValidVerticalMargin();
    setHasValidVerticalNonCollapsedMargin();
    invalidateEstimatedMarginBefore();
#endif
    m_verticalMargin = margin;
}

inline void Box::setHorizontalComputedMargin(Layout::ComputedHorizontalMargin margin)
{
#if !ASSERT_DISABLED
    setHasValidHorizontalComputedMargin();
#endif
    m_horizontalComputedMargin = margin;
}

inline void Box::setBorder(Layout::Edges border)
{
#if !ASSERT_DISABLED
    setHasValidBorder();
#endif
    m_border = border;
}

inline void Box::setPadding(Optional<Layout::Edges> padding)
{
#if !ASSERT_DISABLED
    setHasValidPadding();
#endif
    m_padding = padding;
}

inline Rect Box::rectWithMargin() const
{
    auto marginAfter = this->marginAfter();
    if (m_verticalMargin.collapsedValues().isCollapsedThrough)
        marginAfter = 0;
    return { top() - marginBefore(), left() - marginStart(), marginStart() + width() + marginEnd(), marginBefore() + height() + marginAfter };
}

inline Layout::UsedVerticalMargin Box::verticalMargin() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_verticalMargin;
}

inline Layout::UsedHorizontalMargin Box::horizontalMargin() const
{
    ASSERT(m_hasValidHorizontalMargin);
    return m_horizontalMargin;
}

inline LayoutUnit Box::marginBefore() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_verticalMargin.before();
}

inline LayoutUnit Box::marginStart() const
{
    ASSERT(m_hasValidHorizontalMargin);
    return m_horizontalMargin.start;
}

inline LayoutUnit Box::marginAfter() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_verticalMargin.after();
}

inline LayoutUnit Box::marginEnd() const
{
    ASSERT(m_hasValidHorizontalMargin);
    return m_horizontalMargin.end;
}

inline LayoutUnit Box::nonCollapsedMarginBefore() const
{
    ASSERT(m_hasValidVerticalNonCollapsedMargin);
    return m_verticalMargin.nonCollapsedValues().before;
}

inline LayoutUnit Box::nonCollapsedMarginAfter() const
{
    ASSERT(m_hasValidVerticalNonCollapsedMargin);
    return m_verticalMargin.nonCollapsedValues().after;
}

inline Optional<LayoutUnit> Box::computedMarginStart() const
{
    ASSERT(m_hasValidHorizontalComputedMargin);
    return m_horizontalComputedMargin.start;
}

inline Optional<LayoutUnit> Box::computedMarginEnd() const
{
    ASSERT(m_hasValidHorizontalComputedMargin);
    return m_horizontalComputedMargin.end;
}

inline Optional<LayoutUnit> Box::paddingTop() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->vertical.top;
}

inline Optional<LayoutUnit> Box::paddingLeft() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->horizontal.left;
}

inline Optional<LayoutUnit> Box::paddingBottom() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->vertical.bottom;
}

inline Optional<LayoutUnit> Box::paddingRight() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->horizontal.right;
}

inline Optional<LayoutUnit> Box::verticalPadding() const
{
    auto paddingTop = this->paddingTop();
    auto paddingBottom = this->paddingBottom();
    if (!paddingTop && !paddingBottom)
        return { };
    return paddingTop.valueOr(0) + paddingBottom.valueOr(0);
}

inline Optional<LayoutUnit> Box::horizontalPadding() const
{
    auto paddingLeft = this->paddingLeft();
    auto paddingRight = this->paddingRight();
    if (!paddingLeft && !paddingRight)
        return { };
    return paddingLeft.valueOr(0) + paddingRight.valueOr(0);
}

inline LayoutUnit Box::borderTop() const
{
    ASSERT(m_hasValidBorder);
    return m_border.vertical.top;
}

inline LayoutUnit Box::borderLeft() const
{
    ASSERT(m_hasValidBorder);
    return m_border.horizontal.left;
}

inline LayoutUnit Box::borderBottom() const
{
    ASSERT(m_hasValidBorder);
    return m_border.vertical.bottom;
}

inline LayoutUnit Box::borderRight() const
{
    ASSERT(m_hasValidBorder);
    return m_border.horizontal.right;
}

}
}
#endif
