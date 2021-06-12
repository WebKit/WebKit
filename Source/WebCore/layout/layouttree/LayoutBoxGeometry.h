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

    LayoutUnit borderTop() const;
    LayoutUnit borderLeft() const;
    LayoutUnit borderBottom() const;
    LayoutUnit borderRight() const;
    LayoutUnit verticalBorder() const { return borderTop() + borderBottom(); }
    LayoutUnit horizontalBorder() const { return borderLeft() + borderRight(); }

    std::optional<LayoutUnit> paddingTop() const;
    std::optional<LayoutUnit> paddingLeft() const;
    std::optional<LayoutUnit> paddingBottom() const;
    std::optional<LayoutUnit> paddingRight() const;
    std::optional<LayoutUnit> verticalPadding() const;
    std::optional<LayoutUnit> horizontalPadding() const;

    LayoutUnit contentBoxTop() const { return paddingBoxTop() + paddingTop().value_or(0); }
    LayoutUnit contentBoxLeft() const { return paddingBoxLeft() + paddingLeft().value_or(0); }
    LayoutUnit contentBoxBottom() const { return contentBoxTop() + contentBoxHeight(); }
    LayoutUnit contentBoxRight() const { return contentBoxLeft() + contentBoxWidth(); }
    LayoutUnit contentBoxHeight() const;
    LayoutUnit contentBoxWidth() const;

    LayoutUnit paddingBoxTop() const { return borderTop(); }
    LayoutUnit paddingBoxLeft() const { return borderLeft(); }
    LayoutUnit paddingBoxBottom() const { return paddingBoxTop() + paddingBoxHeight(); }
    LayoutUnit paddingBoxRight() const { return paddingBoxLeft() + paddingBoxWidth(); }
    LayoutUnit paddingBoxHeight() const { return paddingTop().value_or(0) + contentBoxHeight() + paddingBottom().value_or(0); }
    LayoutUnit paddingBoxWidth() const { return paddingLeft().value_or(0) + contentBoxWidth() + paddingRight().value_or(0); }

    LayoutUnit borderBoxHeight() const { return borderTop() + paddingBoxHeight() + verticalSpaceForScrollbar() + borderBottom(); }
    LayoutUnit borderBoxWidth() const { return borderLeft() + paddingBoxWidth() + horizontalSpaceForScrollbar() + borderRight(); }
    LayoutUnit marginBoxHeight() const { return marginBefore() + borderBoxHeight() + marginAfter(); }
    LayoutUnit marginBoxWidth() const { return marginStart() + borderBoxWidth() + marginEnd(); }

    LayoutUnit verticalMarginBorderAndPadding() const { return marginBefore() + verticalBorder() + verticalPadding().value_or(0) + marginAfter(); }
    LayoutUnit horizontalMarginBorderAndPadding() const { return marginStart() + horizontalBorder() + horizontalPadding().value_or(0) + marginEnd(); }

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

inline std::optional<LayoutUnit> BoxGeometry::paddingTop() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->vertical.top;
}

inline std::optional<LayoutUnit> BoxGeometry::paddingLeft() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->horizontal.left;
}

inline std::optional<LayoutUnit> BoxGeometry::paddingBottom() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->vertical.bottom;
}

inline std::optional<LayoutUnit> BoxGeometry::paddingRight() const
{
    ASSERT(m_hasValidPadding);
    if (!m_padding)
        return { };
    return m_padding->horizontal.right;
}

inline std::optional<LayoutUnit> BoxGeometry::verticalPadding() const
{
    auto paddingTop = this->paddingTop();
    auto paddingBottom = this->paddingBottom();
    if (!paddingTop && !paddingBottom)
        return { };
    return paddingTop.value_or(0) + paddingBottom.value_or(0);
}

inline std::optional<LayoutUnit> BoxGeometry::horizontalPadding() const
{
    auto paddingLeft = this->paddingLeft();
    auto paddingRight = this->paddingRight();
    if (!paddingLeft && !paddingRight)
        return { };
    return paddingLeft.value_or(0) + paddingRight.value_or(0);
}

inline LayoutUnit BoxGeometry::borderTop() const
{
    ASSERT(m_hasValidBorder);
    return m_border.vertical.top;
}

inline LayoutUnit BoxGeometry::borderLeft() const
{
    ASSERT(m_hasValidBorder);
    return m_border.horizontal.left;
}

inline LayoutUnit BoxGeometry::borderBottom() const
{
    ASSERT(m_hasValidBorder);
    return m_border.vertical.bottom;
}

inline LayoutUnit BoxGeometry::borderRight() const
{
    ASSERT(m_hasValidBorder);
    return m_border.horizontal.right;
}

}
}
#endif
