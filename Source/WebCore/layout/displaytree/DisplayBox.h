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
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Display {

class Box {
    WTF_MAKE_ISO_ALLOCATED(Box);
public:
    Box(const Box&);
    Box() = default;
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
    bool isEmpty() const { return size().isEmpty(); }
    Rect rect() const { return { top(), left(), width(), height() }; }
    Rect rectWithMargin() const;

    struct VerticalMargin {
        LayoutUnit before() const { return m_collapsedValues.before.valueOr(m_nonCollapsedValues.before); }
        LayoutUnit after() const { return m_collapsedValues.after.valueOr(m_nonCollapsedValues.after); }
        bool isCollapsedThrough() const { return m_collapsedValues.isCollapsedThrough; }

        struct NonCollapsedValues {
            NonCollapsedValues(Layout::UsedVerticalMargin::NonCollapsedValues);
            NonCollapsedValues() = default;

            LayoutUnit before;
            LayoutUnit after;
        };
        NonCollapsedValues nonCollapsedValues() const { return m_nonCollapsedValues; }

        struct CollapsedValues {
            CollapsedValues(Layout::UsedVerticalMargin::CollapsedValues);
            CollapsedValues() = default;

            Optional<LayoutUnit> before;
            Optional<LayoutUnit> after;
            bool isCollapsedThrough { false };
        };
        CollapsedValues collapsedValues() const { return m_collapsedValues; }
        bool hasCollapsedValues() const { return m_collapsedValues.before || m_collapsedValues.after; }
        void setCollapsedValues(CollapsedValues collapsedValues) { m_collapsedValues = collapsedValues; }

        VerticalMargin(NonCollapsedValues, CollapsedValues);
        VerticalMargin(Layout::UsedVerticalMargin);
        VerticalMargin(Layout::UsedVerticalMargin::NonCollapsedValues);
        VerticalMargin() = default;

    private:
        NonCollapsedValues m_nonCollapsedValues;
        CollapsedValues m_collapsedValues;
    };

    struct HorizontalMargin {
        HorizontalMargin() = default;
        HorizontalMargin(Layout::UsedHorizontalMargin);
        HorizontalMargin(LayoutUnit, LayoutUnit);

        LayoutUnit start;
        LayoutUnit end;
    };
    VerticalMargin verticalMargin() const;
    HorizontalMargin horizontalMargin() const;
    LayoutUnit marginBefore() const;
    LayoutUnit marginStart() const;
    LayoutUnit marginAfter() const;
    LayoutUnit marginEnd() const;
    bool hasCollapsedThroughMargin() const { return m_verticalMargin.isCollapsedThrough(); }
    bool hasClearance() const { return m_hasClearance; }

    LayoutUnit nonCollapsedMarginBefore() const;
    LayoutUnit nonCollapsedMarginAfter() const;

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

    LayoutUnit verticalMarginBorderAndPadding() const { return marginBefore() + verticalBorder() + verticalPadding().valueOr(0) + marginAfter(); }
    LayoutUnit horizontalMarginBorderAndPadding() const { return marginStart() + horizontalBorder() + horizontalPadding().valueOr(0) + marginEnd(); }

    Rect marginBox() const;
    Rect nonCollapsedMarginBox() const;

    Rect borderBox() const;
    Rect paddingBox() const;
    Rect contentBox() const;

#if ASSERT_ENABLED
    void setHasPrecomputedMarginBefore() { m_hasPrecomputedMarginBefore = true; }
#endif

    void setTopLeft(const LayoutPoint&);
    void setTop(LayoutUnit);
    void setLeft(LayoutUnit);
    void moveHorizontally(LayoutUnit offset) { m_topLeft.move(offset, 0_lu); }
    void moveVertically(LayoutUnit offset) { m_topLeft.move(0_lu, offset); }
    void move(const LayoutSize& size) { m_topLeft.move(size); }
    void moveBy(LayoutPoint offset) { m_topLeft.moveBy(offset); }

    void setContentBoxHeight(LayoutUnit);
    void setContentBoxWidth(LayoutUnit);

    void setHorizontalMargin(HorizontalMargin);
    void setVerticalMargin(VerticalMargin);
    void setHasClearance() { m_hasClearance = true; }

    void setBorder(Layout::Edges);

    void setVerticalPadding(Layout::VerticalEdges);
    void setPadding(Optional<Layout::Edges>);

private:
#if ASSERT_ENABLED
    void invalidateMargin();
    void invalidateBorder() { m_hasValidBorder = false; }
    void invalidatePadding() { m_hasValidPadding = false; }
    void invalidatePrecomputedMarginBefore() { m_hasPrecomputedMarginBefore = false; }

    void setHasValidTop() { m_hasValidTop = true; }
    void setHasValidLeft() { m_hasValidLeft = true; }
    void setHasValidVerticalMargin() { m_hasValidVerticalMargin = true; }
    void setHasValidVerticalNonCollapsedMargin() { m_hasValidVerticalNonCollapsedMargin = true; }
    void setHasValidHorizontalMargin() { m_hasValidHorizontalMargin = true; }

    void setHasValidBorder() { m_hasValidBorder = true; }
    void setHasValidPadding() { m_hasValidPadding = true; }

    void setHasValidContentHeight() { m_hasValidContentHeight = true; }
    void setHasValidContentWidth() { m_hasValidContentWidth = true; }
#endif // ASSERT_ENABLED

    LayoutPoint m_topLeft;
    LayoutUnit m_contentWidth;
    LayoutUnit m_contentHeight;

    HorizontalMargin m_horizontalMargin;
    VerticalMargin m_verticalMargin;
    bool m_hasClearance { false };

    Layout::Edges m_border;
    Optional<Layout::Edges> m_padding;

#if ASSERT_ENABLED
    bool m_hasValidTop { false };
    bool m_hasValidLeft { false };
    bool m_hasValidHorizontalMargin { false };
    bool m_hasValidVerticalMargin { false };
    bool m_hasValidVerticalNonCollapsedMargin { false };
    bool m_hasValidBorder { false };
    bool m_hasValidPadding { false };
    bool m_hasValidContentHeight { false };
    bool m_hasValidContentWidth { false };
    bool m_hasPrecomputedMarginBefore { false };
#endif // ASSERT_ENABLED
};

inline Box::VerticalMargin::VerticalMargin(Layout::UsedVerticalMargin usedVerticalMargin)
    : m_nonCollapsedValues(usedVerticalMargin.nonCollapsedValues())
    , m_collapsedValues(usedVerticalMargin.collapsedValues())
{
}

inline Box::VerticalMargin::VerticalMargin(VerticalMargin::NonCollapsedValues nonCollapsedValues, VerticalMargin::CollapsedValues collapsedValues)
    : m_nonCollapsedValues(nonCollapsedValues)
    , m_collapsedValues(collapsedValues)
{
}

inline Box::VerticalMargin::VerticalMargin(Layout::UsedVerticalMargin::NonCollapsedValues nonCollapsedValues)
    : m_nonCollapsedValues(nonCollapsedValues)
{
}

inline Box::VerticalMargin::NonCollapsedValues::NonCollapsedValues(Layout::UsedVerticalMargin::NonCollapsedValues nonCollapsedValues)
    : before(nonCollapsedValues.before)
    , after(nonCollapsedValues.after)
{
}

inline Box::VerticalMargin::CollapsedValues::CollapsedValues(Layout::UsedVerticalMargin::CollapsedValues collapsedValues)
    : before(collapsedValues.before)
    , after(collapsedValues.after)
    , isCollapsedThrough(collapsedValues.isCollapsedThrough)
{
}

inline Box::HorizontalMargin::HorizontalMargin(Layout::UsedHorizontalMargin horizontalMargin)
    : start(horizontalMargin.start)
    , end(horizontalMargin.end)
{
}

inline Box::HorizontalMargin::HorizontalMargin(LayoutUnit start, LayoutUnit end)
    : start(start)
    , end(end)
{
}

#if ASSERT_ENABLED
inline void Box::invalidateMargin()
{
    m_hasValidHorizontalMargin = false;
    m_hasValidVerticalMargin = false;
}
#endif

inline LayoutUnit Box::top() const
{
    ASSERT(m_hasValidTop && (m_hasPrecomputedMarginBefore || m_hasValidVerticalMargin));
    return m_topLeft.y();
}

inline LayoutUnit Box::left() const
{
    ASSERT(m_hasValidLeft && m_hasValidHorizontalMargin);
    return m_topLeft.x();
}

inline LayoutPoint Box::topLeft() const
{
    ASSERT(m_hasValidTop && (m_hasPrecomputedMarginBefore || m_hasValidVerticalMargin));
    ASSERT(m_hasValidLeft && m_hasValidHorizontalMargin);
    return m_topLeft;
}

inline void Box::setTopLeft(const LayoutPoint& topLeft)
{
#if ASSERT_ENABLED
    setHasValidTop();
    setHasValidLeft();
#endif
    m_topLeft = topLeft;
}

inline void Box::setTop(LayoutUnit top)
{
#if ASSERT_ENABLED
    setHasValidTop();
#endif
    m_topLeft.setY(top);
}

inline void Box::setLeft(LayoutUnit left)
{
#if ASSERT_ENABLED
    setHasValidLeft();
#endif
    m_topLeft.setX(left);
}

inline void Box::setContentBoxHeight(LayoutUnit height)
{ 
#if ASSERT_ENABLED
    setHasValidContentHeight();
#endif
    m_contentHeight = height;
}

inline void Box::setContentBoxWidth(LayoutUnit width)
{ 
#if ASSERT_ENABLED
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

inline void Box::setHorizontalMargin(HorizontalMargin margin)
{
#if ASSERT_ENABLED
    setHasValidHorizontalMargin();
#endif
    m_horizontalMargin = margin;
}

inline void Box::setVerticalMargin(VerticalMargin margin)
{
#if ASSERT_ENABLED
    setHasValidVerticalMargin();
    setHasValidVerticalNonCollapsedMargin();
    invalidatePrecomputedMarginBefore();
#endif
    m_verticalMargin = margin;
}

inline void Box::setBorder(Layout::Edges border)
{
#if ASSERT_ENABLED
    setHasValidBorder();
#endif
    m_border = border;
}

inline void Box::setPadding(Optional<Layout::Edges> padding)
{
#if ASSERT_ENABLED
    setHasValidPadding();
#endif
    m_padding = padding;
}

inline void Box::setVerticalPadding(Layout::VerticalEdges verticalPadding)
{
#if ASSERT_ENABLED
    setHasValidPadding();
#endif
    m_padding = Layout::Edges { m_padding ? m_padding->horizontal : Layout::HorizontalEdges(), verticalPadding };
}

inline Rect Box::rectWithMargin() const
{
    auto marginAfter = this->marginAfter();
    if (m_verticalMargin.collapsedValues().isCollapsedThrough)
        marginAfter = 0;
    return { top() - marginBefore(), left() - marginStart(), marginStart() + width() + marginEnd(), marginBefore() + height() + marginAfter };
}

inline Box::VerticalMargin Box::verticalMargin() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_verticalMargin;
}

inline Box::HorizontalMargin Box::horizontalMargin() const
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
