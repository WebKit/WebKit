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

#include "config.h"
#include "LayoutBoxGeometry.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(BoxGeometry);

BoxGeometry::BoxGeometry(const BoxGeometry& other)
    : m_topLeft(other.m_topLeft)
    , m_contentBoxWidth(other.m_contentBoxWidth)
    , m_contentBoxHeight(other.m_contentBoxHeight)
    , m_horizontalMargin(other.m_horizontalMargin)
    , m_verticalMargin(other.m_verticalMargin)
    , m_border(other.m_border)
    , m_padding(other.m_padding)
    , m_verticalSpaceForScrollbar(other.m_verticalSpaceForScrollbar)
    , m_horizontalSpaceForScrollbar(other.m_horizontalSpaceForScrollbar)
#if ASSERT_ENABLED
    , m_hasValidTop(other.m_hasValidTop)
    , m_hasValidLeft(other.m_hasValidLeft)
    , m_hasValidHorizontalMargin(other.m_hasValidHorizontalMargin)
    , m_hasValidVerticalMargin(other.m_hasValidVerticalMargin)
    , m_hasValidBorder(other.m_hasValidBorder)
    , m_hasValidPadding(other.m_hasValidPadding)
    , m_hasValidContentBoxHeight(other.m_hasValidContentBoxHeight)
    , m_hasValidContentBoxWidth(other.m_hasValidContentBoxWidth)
    , m_hasPrecomputedMarginBefore(other.m_hasPrecomputedMarginBefore)
#endif
{
}

BoxGeometry::~BoxGeometry()
{
}

Rect BoxGeometry::marginBox() const
{
    auto borderBox = this->borderBox();

    Rect marginBox;
    marginBox.setTop(borderBox.top() - marginBefore());
    marginBox.setLeft(borderBox.left() - marginStart());
    marginBox.setHeight(borderBox.height() + marginBefore() + marginAfter());
    marginBox.setWidth(borderBox.width() + marginStart() + marginEnd());
    return marginBox;
}

Rect BoxGeometry::borderBox() const
{
    Rect borderBox;
    borderBox.setTopLeft({ });
    borderBox.setSize({ borderBoxWidth(), borderBoxHeight() });
    return borderBox;
}

Rect BoxGeometry::paddingBox() const
{
    auto borderBox = this->borderBox();

    Rect paddingBox;
    paddingBox.setTop(borderBox.top() + borderBefore());
    paddingBox.setLeft(borderBox.left() + borderStart());
    paddingBox.setHeight(borderBox.bottom() - verticalSpaceForScrollbar() - borderAfter() - borderBefore());
    paddingBox.setWidth(borderBox.width() - borderEnd() - horizontalSpaceForScrollbar() - borderStart());
    return paddingBox;
}

Rect BoxGeometry::contentBox() const
{
    Rect contentBox;
    contentBox.setTop(contentBoxTop());
    contentBox.setLeft(contentBoxLeft());
    contentBox.setWidth(contentBoxWidth());
    contentBox.setHeight(contentBoxHeight());
    return contentBox;
}

BoxGeometry BoxGeometry::toVisualGeometry(WritingMode writingMode, bool isLeftToRightDirection, LayoutUnit containerLogicalWidth) const
{
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);
    auto isFlippedBlocksWritingMode = WebCore::isFlippedWritingMode(writingMode);

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
    visualGeometry.m_contentBoxWidth = m_contentBoxHeight;
    visualGeometry.m_contentBoxHeight = m_contentBoxWidth;

    visualGeometry.m_horizontalMargin = !isFlippedBlocksWritingMode ? HorizontalMargin { m_verticalMargin.after, m_verticalMargin.before } : HorizontalMargin { m_verticalMargin.before, m_verticalMargin.after };
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

BoxGeometry BoxGeometry::toLogicalGeometry(WritingMode writingMode, bool isLeftToRightInlineDirection, LayoutUnit containerLogicalWidth) const
{
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);
    auto isFlippedBlocksWritingMode = WebCore::isFlippedWritingMode(writingMode);

    if (isHorizontalWritingMode && !isFlippedBlocksWritingMode && isLeftToRightInlineDirection)
        return *this;

    auto logicalGeometry = *this;
    if (isHorizontalWritingMode) {

        if (!isLeftToRightInlineDirection) {
            logicalGeometry.m_horizontalMargin = { m_horizontalMargin.end, m_horizontalMargin.start };
            logicalGeometry.m_border.horizontal = { m_border.horizontal.right, m_border.horizontal.left };
            if (m_padding)
                logicalGeometry.m_padding->horizontal = { m_padding->horizontal.right, m_padding->horizontal.left };
            logicalGeometry.m_topLeft.setX(containerLogicalWidth - BoxGeometry::borderBoxRect(*this).right());
        }

        // This is regular horizontal-tb with rtl content.
        if (!isFlippedBlocksWritingMode)
            return logicalGeometry;

        // horizontal-bt with ltr/rtl content.
        logicalGeometry.m_verticalMargin = { m_verticalMargin.after, m_verticalMargin.before };
        logicalGeometry.m_border.vertical = { m_border.vertical.bottom, m_border.vertical.top };
        if (m_padding)
            logicalGeometry.m_padding->vertical = { m_padding->vertical.bottom, m_padding->vertical.top };
        return logicalGeometry;
    }

    // vertical-lr/rl with inline direction of ltr/rtl.
    logicalGeometry.m_contentBoxWidth = m_contentBoxHeight;
    logicalGeometry.m_contentBoxHeight = m_contentBoxWidth;
    logicalGeometry.m_horizontalSpaceForScrollbar = m_verticalSpaceForScrollbar;
    logicalGeometry.m_verticalSpaceForScrollbar = m_horizontalSpaceForScrollbar;

    logicalGeometry.m_horizontalMargin = { m_verticalMargin.before, m_verticalMargin.after };
    logicalGeometry.m_border.horizontal = { m_border.vertical.top, m_border.vertical.bottom };
    if (m_padding)
        logicalGeometry.m_padding->horizontal = { m_padding->vertical.top, m_padding->vertical.bottom };

    // Flip inline direction now that we have the logical horizontal decoration.
    if (!isLeftToRightInlineDirection) {
        logicalGeometry.m_horizontalMargin = { logicalGeometry.m_horizontalMargin.end, logicalGeometry.m_horizontalMargin.start };
        logicalGeometry.m_border.horizontal = { logicalGeometry.m_border.horizontal.right, logicalGeometry.m_border.horizontal.left };
        if (logicalGeometry.m_padding)
            logicalGeometry.m_padding->horizontal = { logicalGeometry.m_padding->horizontal.right, logicalGeometry.m_padding->horizontal.left };
    }

    // Flip vertical decoration depending on whether the block direction is left to right or right to left.
    logicalGeometry.m_verticalMargin = !isFlippedBlocksWritingMode ? VerticalMargin { m_horizontalMargin.start, m_horizontalMargin.end } : VerticalMargin { m_horizontalMargin.end, m_horizontalMargin.start };
    logicalGeometry.m_border.vertical = !isFlippedBlocksWritingMode ? VerticalEdges { m_border.horizontal.left, m_border.horizontal.right } : VerticalEdges { m_border.horizontal.right, m_border.horizontal.left };
    if (m_padding)
        logicalGeometry.m_padding->vertical = !isFlippedBlocksWritingMode ? VerticalEdges { m_padding->horizontal.left, m_padding->horizontal.right } : VerticalEdges { m_padding->horizontal.right, m_padding->horizontal.left };

    logicalGeometry.m_topLeft = m_topLeft.transposedPoint();
    if (!isLeftToRightInlineDirection)
        logicalGeometry.m_topLeft.setX(containerLogicalWidth - BoxGeometry::borderBoxRect(*this).bottom());
    return logicalGeometry;
}

}
}

