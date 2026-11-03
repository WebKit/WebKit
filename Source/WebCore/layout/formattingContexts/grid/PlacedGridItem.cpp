/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "PlacedGridItem.h"

#include "GridAreaLines.h"
#include "StyleAlignSelf.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleJustifySelf.h"
#include "UnplacedGridItem.h"

namespace WebCore {
namespace Layout {

// "In the positioning phase—calculating the positioning offsets, margins, borders, and padding—the dimensions of the box and its containing block are
// mapped to the inline size and block size and calculations are performed according to the writing mode of the containing block of the box establishing
// the orthogonal flow." https://drafts.csswg.org/css-writing-modes-4/#orthogonal-flows
// "A grid item is sized within the containing block defined by its grid area." https://drafts.csswg.org/css-grid-1/#grid-item-sizing
// The grid area's axes are the grid container's, so the item's sizes and margins are mapped through the container's writing mode, not the item's own.
// For an orthogonal item, the inline-axis sizes are therefore its block-size properties.
static ComputedSizes computedSizesInContainerAxis(const Style::ComputedStyle& gridItemStyle, WritingMode gridContainerWritingMode, LogicalBoxAxis axis)
{
    if (axis == LogicalBoxAxis::Inline)
        return { gridItemStyle.logicalWidth(gridContainerWritingMode), gridItemStyle.logicalMinWidth(gridContainerWritingMode), gridItemStyle.logicalMaxWidth(gridContainerWritingMode), gridItemStyle.marginStart(gridContainerWritingMode), gridItemStyle.marginEnd(gridContainerWritingMode) };
    return { gridItemStyle.logicalHeight(gridContainerWritingMode), gridItemStyle.logicalMinHeight(gridContainerWritingMode), gridItemStyle.logicalMaxHeight(gridContainerWritingMode), gridItemStyle.marginBefore(gridContainerWritingMode), gridItemStyle.marginAfter(gridContainerWritingMode) };
}

PlacedGridItem::PlacedGridItem(const ElementBox& gridItem, const GridAreaLines& gridAreaLines, const Style::ComputedStyle& gridContainerStyle)
    : PlacedGridItem(gridItem, gridAreaLines, gridContainerStyle, gridItem.style())
{
}

PlacedGridItem::PlacedGridItem(const ElementBox& gridItem, const GridAreaLines& gridAreaLines, const Style::ComputedStyle& gridContainerStyle, const Style::ComputedStyle& gridItemStyle)
    : m_layoutBox(gridItem)
    , m_inlineAxisSizes(computedSizesInContainerAxis(gridItemStyle, gridContainerStyle.writingMode(), LogicalBoxAxis::Inline))
    , m_blockAxisSizes(computedSizesInContainerAxis(gridItemStyle, gridContainerStyle.writingMode(), LogicalBoxAxis::Block))
    , m_inlineAxisAlignment(gridItemStyle.justifySelf().resolve(&gridContainerStyle))
    , m_blockAxisAlignment(gridItemStyle.alignSelf().resolve(&gridContainerStyle))
    , m_writingMode(gridItemStyle.writingMode())
    , m_usedZoom(gridItemStyle.usedZoomForLength())
    , m_gridAreaLines(gridAreaLines)
{
}

} // namespace Layout
} // namespace WebCore
