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

#include "LayoutGeometryRect.h"
#include "LayoutUnits.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class BoxGeometry {
    WTF_MAKE_ISO_ALLOCATED(BoxGeometry);
public:
    BoxGeometry(const BoxGeometry&);
    BoxGeometry() = default;
    ~BoxGeometry();

    static LayoutUnit borderBoxTop(const BoxGeometry& box) { return box.logicalTop(); }
    static LayoutUnit borderBoxLeft(const BoxGeometry& box) { return box.logicalLeft(); }
    static LayoutPoint borderBoxTopLeft(const BoxGeometry& box) { return box.logicalTopLeft(); }
    static Rect borderBoxRect(const BoxGeometry& box) { return { box.logicalTop(), box.logicalLeft(), box.borderBoxWidth(), box.borderBoxHeight() }; }
    static Rect marginBoxRect(const BoxGeometry& box) { return { box.logicalTop() - box.marginBefore(), box.logicalLeft() - box.marginStart(), box.marginBoxWidth(), box.marginBoxHeight() }; }

    struct VerticalMargin {
        LayoutUnit before;
        LayoutUnit after;
    };
    VerticalMargin verticalMargin() const;

    struct HorizontalMargin {
        LayoutUnit start;
        LayoutUnit end;
    };
    HorizontalMargin horizontalMargin() const;
    LayoutUnit marginBefore() const;
    LayoutUnit marginStart() const;
    LayoutUnit marginAfter() const;
    LayoutUnit marginEnd() const;

    LayoutUnit borderBefore() const;
    LayoutUnit borderAfter() const;
    LayoutUnit borderStart() const;
    LayoutUnit borderEnd() const;
    LayoutUnit verticalBorder() const { return borderBefore() + borderAfter(); }
    LayoutUnit horizontalBorder() const { return borderStart() + borderEnd(); }

    std::optional<LayoutUnit> paddingBefore() const;
    std::optional<LayoutUnit> paddingAfter() const;
    std::optional<LayoutUnit> paddingStart() const;
    std::optional<LayoutUnit> paddingEnd() const;
    std::optional<LayoutUnit> verticalPadding() const;
    std::optional<LayoutUnit> horizontalPadding() const;

    LayoutUnit borderAndPaddingStart() const { return borderStart() + paddingStart().value_or(0_lu); }
    LayoutUnit borderAndPaddingEnd() const { return borderEnd() + paddingEnd().value_or(0_lu); }
    LayoutUnit horizontalBorderAndPadding() const { return horizontalBorder() + horizontalPadding().value_or(0_lu); }
    LayoutUnit verticalBorderAndPadding() const { return verticalBorder() + verticalPadding().value_or(0_lu); }

    LayoutUnit contentBoxTop() const { return paddingBoxTop() + paddingBefore().value_or(0_lu); }
    LayoutUnit contentBoxLeft() const { return paddingBoxLeft() + paddingStart().value_or(0_lu); }
    LayoutUnit contentBoxBottom() const { return contentBoxTop() + contentBoxHeight(); }
    LayoutUnit contentBoxRight() const { return contentBoxLeft() + contentBoxWidth(); }
    LayoutUnit contentBoxHeight() const;
    LayoutUnit contentBoxWidth() const;

    LayoutUnit paddingBoxTop() const { return borderBefore(); }
    LayoutUnit paddingBoxLeft() const { return borderStart(); }
    LayoutUnit paddingBoxBottom() const { return paddingBoxTop() + paddingBoxHeight(); }
    LayoutUnit paddingBoxRight() const { return paddingBoxLeft() + paddingBoxWidth(); }
    LayoutUnit paddingBoxHeight() const { return paddingBefore().value_or(0_lu) + contentBoxHeight() + paddingAfter().value_or(0_lu); }
    LayoutUnit paddingBoxWidth() const { return paddingStart().value_or(0_lu) + contentBoxWidth() + paddingEnd().value_or(0_lu); }

    LayoutUnit borderBoxHeight() const { return borderBefore() + paddingBoxHeight() + verticalSpaceForScrollbar() + borderAfter(); }
    LayoutUnit borderBoxWidth() const { return borderStart() + paddingBoxWidth() + horizontalSpaceForScrollbar() + borderEnd(); }
    LayoutUnit marginBoxHeight() const { return marginBefore() + borderBoxHeight() + marginAfter(); }
    LayoutUnit marginBoxWidth() const { return marginStart() + borderBoxWidth() + marginEnd(); }

    LayoutUnit verticalMarginBorderAndPadding() const { return marginBefore() + verticalBorder() + verticalPadding().value_or(0_lu) + marginAfter(); }
    LayoutUnit marginBorderAndPaddingBefore() const { return marginBefore() + borderBefore() + paddingBefore().value_or(0_lu); }
    LayoutUnit marginBorderAndPaddingAfter() const { return marginAfter() + borderAfter() + paddingAfter().value_or(0_lu); }
    LayoutUnit horizontalMarginBorderAndPadding() const { return marginStart() + horizontalBorder() + horizontalPadding().value_or(0_lu) + marginEnd(); }

    LayoutUnit verticalSpaceForScrollbar() const { return m_verticalSpaceForScrollbar; }
    LayoutUnit horizontalSpaceForScrollbar() const { return m_horizontalSpaceForScrollbar; }

    Rect marginBox() const;
    Rect borderBox() const;
    Rect paddingBox() const;
    Rect contentBox() const;

#if ASSERT_ENABLED
    void setHasPrecomputedMarginBefore() { m_hasPrecomputedMarginBefore = true; }
#endif

    void setLogicalTopLeft(const LayoutPoint&);
    void setLogicalTop(LayoutUnit);
    void setLogicalLeft(LayoutUnit);
    void moveHorizontally(LayoutUnit offset) { m_topLeft.move(offset, 0_lu); }
    void moveVertically(LayoutUnit offset) { m_topLeft.move(0_lu, offset); }
    void move(const LayoutSize& size) { m_topLeft.move(size); }
    void moveBy(LayoutPoint offset) { m_topLeft.moveBy(offset); }

    void setContentBoxHeight(LayoutUnit);
    void setContentBoxWidth(LayoutUnit);

    void setHorizontalMargin(HorizontalMargin);
    void setVerticalMargin(VerticalMargin);

    void setBorder(Layout::Edges);

    void setVerticalPadding(Layout::VerticalEdges);
    void setPadding(std::optional<Layout::Edges>);

    void setVerticalSpaceForScrollbar(LayoutUnit scrollbarHeight) { m_verticalSpaceForScrollbar = scrollbarHeight; }
    void setHorizontalSpaceForScrollbar(LayoutUnit scrollbarWidth) { m_horizontalSpaceForScrollbar = scrollbarWidth; }

    BoxGeometry geometryForWritingModeAndDirection(bool isHorizontalWritingMode, bool isLeftToRightDirection, LayoutUnit containerLogicalWidth) const;

private:
    LayoutUnit logicalTop() const;
    LayoutUnit logicalLeft() const;
    LayoutPoint logicalTopLeft() const;

#if ASSERT_ENABLED
    void invalidateMargin();
    void invalidateBorder() { m_hasValidBorder = false; }
    void invalidatePadding() { m_hasValidPadding = false; }
    void invalidatePrecomputedMarginBefore() { m_hasPrecomputedMarginBefore = false; }

    void setHasValidTop() { m_hasValidTop = true; }
    void setHasValidLeft() { m_hasValidLeft = true; }
    void setHasValidVerticalMargin() { m_hasValidVerticalMargin = true; }
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

    Layout::Edges m_border;
    std::optional<Layout::Edges> m_padding;

    LayoutUnit m_verticalSpaceForScrollbar;
    LayoutUnit m_horizontalSpaceForScrollbar;

#if ASSERT_ENABLED
    bool m_hasValidTop { false };
    bool m_hasValidLeft { false };
    bool m_hasValidHorizontalMargin { false };
    bool m_hasValidVerticalMargin { false };
    bool m_hasValidBorder { false };
    bool m_hasValidPadding { false };
    bool m_hasValidContentHeight { false };
    bool m_hasValidContentWidth { false };
    bool m_hasPrecomputedMarginBefore { false };
#endif // ASSERT_ENABLED
};

#if ASSERT_ENABLED
inline void BoxGeometry::invalidateMargin()
{
    m_hasValidHorizontalMargin = false;
    m_hasValidVerticalMargin = false;
}
#endif

inline LayoutUnit BoxGeometry::logicalTop() const
{
    ASSERT(m_hasValidTop && (m_hasPrecomputedMarginBefore || m_hasValidVerticalMargin));
    return m_topLeft.y();
}

inline LayoutUnit BoxGeometry::logicalLeft() const
{
    ASSERT(m_hasValidLeft && m_hasValidHorizontalMargin);
    return m_topLeft.x();
}

inline LayoutPoint BoxGeometry::logicalTopLeft() const
{
    ASSERT(m_hasValidTop && (m_hasPrecomputedMarginBefore || m_hasValidVerticalMargin));
    ASSERT(m_hasValidLeft && m_hasValidHorizontalMargin);
    return m_topLeft;
}

inline void BoxGeometry::setLogicalTopLeft(const LayoutPoint& topLeft)
{
#if ASSERT_ENABLED
    setHasValidTop();
    setHasValidLeft();
#endif
    m_topLeft = topLeft;
}

inline void BoxGeometry::setLogicalTop(LayoutUnit top)
{
#if ASSERT_ENABLED
    setHasValidTop();
#endif
    m_topLeft.setY(top);
}

inline void BoxGeometry::setLogicalLeft(LayoutUnit left)
{
#if ASSERT_ENABLED
    setHasValidLeft();
#endif
    m_topLeft.setX(left);
}

inline void BoxGeometry::setContentBoxHeight(LayoutUnit height)
{ 
#if ASSERT_ENABLED
    setHasValidContentHeight();
#endif
    m_contentHeight = height;
}

inline void BoxGeometry::setContentBoxWidth(LayoutUnit width)
{ 
#if ASSERT_ENABLED
    setHasValidContentWidth();
#endif
    m_contentWidth = width;
}

inline LayoutUnit BoxGeometry::contentBoxHeight() const
{
    ASSERT(m_hasValidContentHeight);
    return m_contentHeight;
}

inline LayoutUnit BoxGeometry::contentBoxWidth() const
{
    ASSERT(m_hasValidContentWidth);
    return m_contentWidth;
}

inline void BoxGeometry::setHorizontalMargin(HorizontalMargin margin)
{
#if ASSERT_ENABLED
    setHasValidHorizontalMargin();
#endif
    m_horizontalMargin = margin;
}

inline void BoxGeometry::setVerticalMargin(VerticalMargin margin)
{
#if ASSERT_ENABLED
    setHasValidVerticalMargin();
    invalidatePrecomputedMarginBefore();
#endif
    m_verticalMargin = margin;
}

inline void BoxGeometry::setBorder(Layout::Edges border)
{
#if ASSERT_ENABLED
    setHasValidBorder();
#endif
    m_border = border;
}

inline void BoxGeometry::setPadding(std::optional<Layout::Edges> padding)
{
#if ASSERT_ENABLED
    setHasValidPadding();
#endif
    m_padding = padding;
}

inline void BoxGeometry::setVerticalPadding(Layout::VerticalEdges verticalPadding)
{
#if ASSERT_ENABLED
    setHasValidPadding();
#endif
    m_padding = Layout::Edges { m_padding ? m_padding->horizontal : Layout::HorizontalEdges(), verticalPadding };
}

inline BoxGeometry::VerticalMargin BoxGeometry::verticalMargin() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_verticalMargin;
}

inline BoxGeometry::HorizontalMargin BoxGeometry::horizontalMargin() const
{
    ASSERT(m_hasValidHorizontalMargin);
    return m_horizontalMargin;
}

inline LayoutUnit BoxGeometry::marginBefore() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_verticalMargin.before;
}

inline LayoutUnit BoxGeometry::marginStart() const
{
    ASSERT(m_hasValidHorizontalMargin);
    return m_horizontalMargin.start;
}

inline LayoutUnit BoxGeometry::marginAfter() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_verticalMargin.after;
}

inline LayoutUnit BoxGeometry::marginEnd() const
{
    ASSERT(m_hasValidHorizontalMargin);
    return m_horizontalMargin.end;
}

inline std::optional<LayoutUnit> BoxGeometry::paddingBefore() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->vertical.top;
}

inline std::optional<LayoutUnit> BoxGeometry::paddingStart() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->horizontal.left;
}

inline std::optional<LayoutUnit> BoxGeometry::paddingAfter() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->vertical.bottom;
}

inline std::optional<LayoutUnit> BoxGeometry::paddingEnd() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->horizontal.right;
}

inline std::optional<LayoutUnit> BoxGeometry::verticalPadding() const
{
    auto paddingTop = this->paddingBefore();
    auto paddingBottom = this->paddingAfter();
    if (!paddingTop && !paddingBottom)
        return { };
    return paddingTop.value_or(0_lu) + paddingBottom.value_or(0_lu);
}

inline std::optional<LayoutUnit> BoxGeometry::horizontalPadding() const
{
    auto paddingLeft = this->paddingStart();
    auto paddingRight = this->paddingEnd();
    if (!paddingLeft && !paddingRight)
        return { };
    return paddingLeft.value_or(0_lu) + paddingRight.value_or(0_lu);
}

inline LayoutUnit BoxGeometry::borderBefore() const
{
    ASSERT(m_hasValidBorder);
    return m_border.vertical.top;
}

inline LayoutUnit BoxGeometry::borderStart() const
{
    ASSERT(m_hasValidBorder);
    return m_border.horizontal.left;
}

inline LayoutUnit BoxGeometry::borderAfter() const
{
    ASSERT(m_hasValidBorder);
    return m_border.vertical.bottom;
}

inline LayoutUnit BoxGeometry::borderEnd() const
{
    ASSERT(m_hasValidBorder);
    return m_border.horizontal.right;
}

inline BoxGeometry BoxGeometry::geometryForWritingModeAndDirection(bool isHorizontalWritingMode, bool isLeftToRightDirection, LayoutUnit containerLogicalWidth) const
{
    if (isHorizontalWritingMode && isLeftToRightDirection)
        return *this;

    auto visualGeometry = *this;
    if (isHorizontalWritingMode) {
        // Horizontal flip.
        visualGeometry.m_horizontalMargin = { m_horizontalMargin.end, m_horizontalMargin.start };
        visualGeometry.m_border.horizontal = { m_border.horizontal.right, m_border.horizontal.left };
        if (m_padding)
            visualGeometry.m_padding->horizontal = { m_padding->horizontal.right, m_padding->horizontal.left };

        visualGeometry.m_topLeft.setX(containerLogicalWidth - (m_topLeft.x() + borderBoxWidth())); 
        return visualGeometry;
    }

    // Vertical flip.
    visualGeometry.m_contentWidth = m_contentHeight;
    visualGeometry.m_contentHeight = m_contentWidth;

    visualGeometry.m_horizontalMargin = { m_verticalMargin.after, m_verticalMargin.before };
    visualGeometry.m_verticalMargin = { m_horizontalMargin.start, m_horizontalMargin.end };

    auto left = isLeftToRightDirection ? m_topLeft.x() : containerLogicalWidth - (m_topLeft.x() + borderBoxWidth());
    auto marginBoxOffset = LayoutSize { left - m_horizontalMargin.start, m_topLeft.y() - m_verticalMargin.before }.transposedSize();
    visualGeometry.m_topLeft = { visualGeometry.m_horizontalMargin.start, visualGeometry.m_verticalMargin.before };
    visualGeometry.m_topLeft.move(marginBoxOffset);

    visualGeometry.m_border = { { m_border.vertical.bottom, m_border.vertical.top }, { m_border.horizontal.left, m_border.horizontal.right } };

    if (m_padding)
        visualGeometry.m_padding = Layout::Edges { { m_padding->vertical.bottom, m_padding->vertical.top }, { m_padding->horizontal.left, m_padding->horizontal.right } };

    visualGeometry.m_verticalSpaceForScrollbar = m_horizontalSpaceForScrollbar;
    visualGeometry.m_horizontalSpaceForScrollbar = m_verticalSpaceForScrollbar;

    return visualGeometry;
}

}
}
