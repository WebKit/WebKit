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

    static LayoutUnit borderBoxTop(const BoxGeometry& box) { return box.top(); }
    static LayoutUnit borderBoxLeft(const BoxGeometry& box) { return box.left(); }
    static LayoutPoint borderBoxTopLeft(const BoxGeometry& box) { return box.topLeft(); }
    static Rect borderBoxRect(const BoxGeometry& box) { return { box.top(), box.left(), box.borderBoxWidth(), box.borderBoxHeight() }; }
    static Rect marginBoxRect(const BoxGeometry& box) { return { box.top() - box.marginBefore(), box.left() - box.marginStart(), box.marginBoxWidth(), box.marginBoxHeight() }; }

    struct VerticalEdges {
        LayoutUnit before;
        LayoutUnit after;
    };

    struct HorizontalEdges {
        LayoutUnit start;
        LayoutUnit end;
    };

    struct Edges {
        HorizontalEdges horizontal;
        VerticalEdges vertical;

        LayoutUnit width() const { return horizontal.start + horizontal.end; }
        LayoutUnit height() const { return vertical.before + vertical.after; }
    };

    HorizontalEdges horizontalMargin() const;
    VerticalEdges verticalMargin() const;
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

    LayoutUnit paddingBefore() const;
    LayoutUnit paddingAfter() const;
    LayoutUnit paddingStart() const;
    LayoutUnit paddingEnd() const;
    LayoutUnit verticalPadding() const { return paddingBefore() + paddingAfter(); }
    LayoutUnit horizontalPadding() const { return paddingStart() + paddingEnd(); }

    LayoutUnit borderAndPaddingStart() const { return borderStart() + paddingStart(); }
    LayoutUnit borderAndPaddingEnd() const { return borderEnd() + paddingEnd(); }
    LayoutUnit borderAndPaddingBefore() const { return borderBefore() + paddingBefore(); }
    LayoutUnit borderAndPaddingAfter() const { return borderAfter() + paddingAfter(); }
    LayoutUnit horizontalBorderAndPadding() const { return horizontalBorder() + horizontalPadding(); }
    LayoutUnit verticalBorderAndPadding() const { return verticalBorder() + verticalPadding(); }

    LayoutUnit contentBoxTop() const { return paddingBoxTop() + paddingBefore(); }
    LayoutUnit contentBoxLeft() const { return paddingBoxLeft() + paddingStart(); }
    LayoutUnit contentBoxBottom() const { return contentBoxTop() + contentBoxHeight(); }
    LayoutUnit contentBoxRight() const { return contentBoxLeft() + contentBoxWidth(); }
    LayoutUnit contentBoxHeight() const;
    LayoutUnit contentBoxWidth() const;
    LayoutSize contentBoxSize() const { return { contentBoxWidth(), contentBoxHeight() }; }

    LayoutUnit paddingBoxTop() const { return borderBefore(); }
    LayoutUnit paddingBoxLeft() const { return borderStart(); }
    LayoutUnit paddingBoxBottom() const { return paddingBoxTop() + paddingBoxHeight(); }
    LayoutUnit paddingBoxRight() const { return paddingBoxLeft() + paddingBoxWidth(); }
    LayoutUnit paddingBoxHeight() const { return paddingBefore() + contentBoxHeight() + paddingAfter(); }
    LayoutUnit paddingBoxWidth() const { return paddingStart() + contentBoxWidth() + paddingEnd(); }

    LayoutUnit borderBoxHeight() const { return borderBefore() + paddingBoxHeight() + verticalSpaceForScrollbar() + borderAfter(); }
    LayoutUnit borderBoxWidth() const { return borderStart() + paddingBoxWidth() + horizontalSpaceForScrollbar() + borderEnd(); }
    LayoutUnit marginBoxHeight() const { return marginBefore() + borderBoxHeight() + marginAfter(); }
    LayoutUnit marginBoxWidth() const { return marginStart() + borderBoxWidth() + marginEnd(); }

    LayoutUnit marginBorderAndPaddingBefore() const { return marginBefore() + borderAndPaddingBefore(); }
    LayoutUnit marginBorderAndPaddingAfter() const { return marginAfter() + borderAndPaddingAfter(); }
    LayoutUnit verticalMarginBorderAndPadding() const { return marginBorderAndPaddingBefore() + marginBorderAndPaddingAfter(); }

    LayoutUnit marginBorderAndPaddingStart() const { return marginStart() + borderAndPaddingStart(); }
    LayoutUnit marginBorderAndPaddingEnd() const { return marginEnd() + borderAndPaddingEnd(); }
    LayoutUnit horizontalMarginBorderAndPadding() const { return marginBorderAndPaddingStart() + marginBorderAndPaddingEnd(); }

    LayoutUnit verticalSpaceForScrollbar() const { return m_verticalSpaceForScrollbar; }
    LayoutUnit horizontalSpaceForScrollbar() const { return m_horizontalSpaceForScrollbar; }

    bool hasMarginBorderOrPadding() const { return horizontalMarginBorderAndPadding() || verticalMarginBorderAndPadding(); }

    Rect marginBox() const;
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
    void setContentBoxSize(const LayoutSize&);

    void setHorizontalMargin(HorizontalEdges);
    void setMarginStart(LayoutUnit);
    void setMarginEnd(LayoutUnit);

    void setVerticalMargin(VerticalEdges);

    void setBorder(Edges);
    void setHorizontalBorder(HorizontalEdges);
    void setVerticalBorder(VerticalEdges);

    void setHorizontalPadding(HorizontalEdges);
    void setVerticalPadding(VerticalEdges);
    void setPadding(Edges);

    void setVerticalSpaceForScrollbar(LayoutUnit scrollbarHeight) { m_verticalSpaceForScrollbar = scrollbarHeight; }
    void setHorizontalSpaceForScrollbar(LayoutUnit scrollbarWidth) { m_horizontalSpaceForScrollbar = scrollbarWidth; }

private:
    LayoutUnit top() const;
    LayoutUnit left() const;
    LayoutPoint topLeft() const;

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

    void setHasValidContentBoxHeight() { m_hasValidContentBoxHeight = true; }
    void setHasValidContentBoxWidth() { m_hasValidContentBoxWidth = true; }
#endif // ASSERT_ENABLED

    LayoutPoint m_topLeft;
    LayoutUnit m_contentBoxWidth;
    LayoutUnit m_contentBoxHeight;

    Edges m_margin;
    Edges m_border;
    Edges m_padding;

    LayoutUnit m_verticalSpaceForScrollbar;
    LayoutUnit m_horizontalSpaceForScrollbar;

#if ASSERT_ENABLED
    bool m_hasValidTop { false };
    bool m_hasValidLeft { false };
    bool m_hasValidHorizontalMargin { false };
    bool m_hasValidVerticalMargin { false };
    bool m_hasValidBorder { false };
    bool m_hasValidPadding { false };
    bool m_hasValidContentBoxHeight { false };
    bool m_hasValidContentBoxWidth { false };
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

inline LayoutUnit BoxGeometry::top() const
{
    ASSERT(m_hasValidTop && (m_hasPrecomputedMarginBefore || m_hasValidVerticalMargin));
    return m_topLeft.y();
}

inline LayoutUnit BoxGeometry::left() const
{
    ASSERT(m_hasValidLeft && m_hasValidHorizontalMargin);
    return m_topLeft.x();
}

inline LayoutPoint BoxGeometry::topLeft() const
{
    ASSERT(m_hasValidTop && (m_hasPrecomputedMarginBefore || m_hasValidVerticalMargin));
    ASSERT(m_hasValidLeft && m_hasValidHorizontalMargin);
    return m_topLeft;
}

inline void BoxGeometry::setTopLeft(const LayoutPoint& topLeft)
{
#if ASSERT_ENABLED
    setHasValidTop();
    setHasValidLeft();
#endif
    m_topLeft = topLeft;
}

inline void BoxGeometry::setTop(LayoutUnit top)
{
#if ASSERT_ENABLED
    setHasValidTop();
#endif
    m_topLeft.setY(top);
}

inline void BoxGeometry::setLeft(LayoutUnit left)
{
#if ASSERT_ENABLED
    setHasValidLeft();
#endif
    m_topLeft.setX(left);
}

inline void BoxGeometry::setContentBoxHeight(LayoutUnit height)
{ 
#if ASSERT_ENABLED
    setHasValidContentBoxHeight();
#endif
    m_contentBoxHeight = height;
}

inline void BoxGeometry::setContentBoxWidth(LayoutUnit width)
{ 
#if ASSERT_ENABLED
    setHasValidContentBoxWidth();
#endif
    m_contentBoxWidth = width;
}

inline void BoxGeometry::setContentBoxSize(const LayoutSize& size)
{
    setContentBoxWidth(size.width());
    setContentBoxHeight(size.height());
}

inline LayoutUnit BoxGeometry::contentBoxHeight() const
{
    ASSERT(m_hasValidContentBoxHeight);
    return m_contentBoxHeight;
}

inline LayoutUnit BoxGeometry::contentBoxWidth() const
{
    ASSERT(m_hasValidContentBoxWidth);
    return m_contentBoxWidth;
}

inline void BoxGeometry::setHorizontalMargin(HorizontalEdges margin)
{
#if ASSERT_ENABLED
    setHasValidHorizontalMargin();
#endif
    m_margin.horizontal = margin;
}

inline void BoxGeometry::setMarginStart(LayoutUnit marginStart)
{
#if ASSERT_ENABLED
    setHasValidHorizontalMargin();
#endif
    m_margin.horizontal = { marginStart, m_margin.horizontal.end };
}

inline void BoxGeometry::setMarginEnd(LayoutUnit marginEnd)
{
#if ASSERT_ENABLED
    setHasValidHorizontalMargin();
#endif
    m_margin.horizontal = { m_margin.horizontal.start, marginEnd };
}

inline void BoxGeometry::setVerticalMargin(VerticalEdges margin)
{
#if ASSERT_ENABLED
    setHasValidVerticalMargin();
    invalidatePrecomputedMarginBefore();
#endif
    m_margin.vertical = margin;
}

inline void BoxGeometry::setBorder(Edges border)
{
#if ASSERT_ENABLED
    setHasValidBorder();
#endif
    m_border = border;
}

inline void BoxGeometry::setHorizontalBorder(HorizontalEdges horizontalBorder)
{
#if ASSERT_ENABLED
    setHasValidBorder();
#endif
    m_border.horizontal = horizontalBorder;
}

inline void BoxGeometry::setVerticalBorder(VerticalEdges verticalBorder)
{
#if ASSERT_ENABLED
    setHasValidBorder();
#endif
    m_border.vertical = verticalBorder;
}

inline void BoxGeometry::setPadding(Edges padding)
{
#if ASSERT_ENABLED
    setHasValidPadding();
#endif
    m_padding = padding;
}

inline void BoxGeometry::setHorizontalPadding(HorizontalEdges horizontalPadding)
{
#if ASSERT_ENABLED
    setHasValidPadding();
#endif
    m_padding.horizontal = horizontalPadding;
}

inline void BoxGeometry::setVerticalPadding(VerticalEdges verticalPadding)
{
#if ASSERT_ENABLED
    setHasValidPadding();
#endif
    m_padding.vertical = verticalPadding;
}

inline BoxGeometry::VerticalEdges BoxGeometry::verticalMargin() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_margin.vertical;
}

inline BoxGeometry::HorizontalEdges BoxGeometry::horizontalMargin() const
{
    ASSERT(m_hasValidHorizontalMargin);
    return m_margin.horizontal;
}

inline LayoutUnit BoxGeometry::marginBefore() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_margin.vertical.before;
}

inline LayoutUnit BoxGeometry::marginStart() const
{
    ASSERT(m_hasValidHorizontalMargin);
    return m_margin.horizontal.start;
}

inline LayoutUnit BoxGeometry::marginAfter() const
{
    ASSERT(m_hasValidVerticalMargin);
    return m_margin.vertical.after;
}

inline LayoutUnit BoxGeometry::marginEnd() const
{
    ASSERT(m_hasValidHorizontalMargin);
    return m_margin.horizontal.end;
}

inline LayoutUnit BoxGeometry::paddingBefore() const
{
    ASSERT(m_hasValidPadding);
    return m_padding.vertical.before;
}

inline LayoutUnit BoxGeometry::paddingStart() const
{
    ASSERT(m_hasValidPadding);
    return m_padding.horizontal.start;
}

inline LayoutUnit BoxGeometry::paddingAfter() const
{
    ASSERT(m_hasValidPadding);
    return m_padding.vertical.after;
}

inline LayoutUnit BoxGeometry::paddingEnd() const
{
    ASSERT(m_hasValidPadding);
    return m_padding.horizontal.end;
}

inline LayoutUnit BoxGeometry::borderBefore() const
{
    ASSERT(m_hasValidBorder);
    return m_border.vertical.before;
}

inline LayoutUnit BoxGeometry::borderStart() const
{
    ASSERT(m_hasValidBorder);
    return m_border.horizontal.start;
}

inline LayoutUnit BoxGeometry::borderAfter() const
{
    ASSERT(m_hasValidBorder);
    return m_border.vertical.after;
}

inline LayoutUnit BoxGeometry::borderEnd() const
{
    ASSERT(m_hasValidBorder);
    return m_border.horizontal.end;
}

}
}
