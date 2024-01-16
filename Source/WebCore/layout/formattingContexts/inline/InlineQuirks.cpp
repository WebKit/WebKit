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

#include "config.h"
#include "InlineQuirks.h"

#include "InlineFormattingContext.h"
#include "InlineLineBox.h"
#include "LayoutBoxGeometry.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

InlineQuirks::InlineQuirks(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
{
}

bool InlineQuirks::trailingNonBreakingSpaceNeedsAdjustment(bool isInIntrinsicWidthMode, bool lineHasOverflow) const
{
    if (isInIntrinsicWidthMode || !lineHasOverflow)
        return false;
    auto& rootStyle = formattingContext().root().style();
    return rootStyle.nbspMode() == NBSPMode::Space && rootStyle.textWrapMode() != TextWrapMode::NoWrap && rootStyle.whiteSpaceCollapse() != WhiteSpaceCollapse::BreakSpaces;
}

InlineLayoutUnit InlineQuirks::initialLineHeight() const
{
    ASSERT(!formattingContext().layoutState().inStandardsMode());
    return 0.f;
}

bool InlineQuirks::lineBreakBoxAffectsParentInlineBox(const LineBox& lineBox)
{
    // In quirks mode linebreak boxes (<br>) stop affecting the line box when (assume <br> is nested e.g. <span style="font-size: 100px"><br></span>)
    // 1. the root inline box has content <div>content<br>/div>
    // 2. there's at least one atomic inline level box on the line e.g <div><img><br></div> or <div><span><img></span><br></div>
    // 3. there's at least one inline box with content e.g. <div><span>content</span><br></div>
    if (lineBox.rootInlineBox().hasContent())
        return false;
    if (lineBox.hasAtomicInlineLevelBox())
        return false;
    // At this point we either have only the <br> on the line or inline boxes with or without content.
    auto& inlineLevelBoxes = lineBox.nonRootInlineLevelBoxes();
    ASSERT(!inlineLevelBoxes.isEmpty());
    if (inlineLevelBoxes.size() == 1)
        return true;
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        // Filter out empty inline boxes e.g. <div><span></span><span></span><br></div>
        if (inlineLevelBox.isInlineBox() && inlineLevelBox.hasContent())
            return false;
    }
    return true;
}

bool InlineQuirks::inlineBoxAffectsLineBox(const InlineLevelBox& inlineLevelBox) const
{
    ASSERT(!formattingContext().layoutState().inStandardsMode());
    ASSERT(inlineLevelBox.isInlineBox());
    // Inline boxes (e.g. root inline box or <span>) affects line boxes either through the strut or actual content.
    if (inlineLevelBox.hasContent())
        return true;
    if (inlineLevelBox.isRootInlineBox()) {
        // This root inline box has no direct text content and we are in non-standards mode.
        // Now according to legacy line layout, we need to apply the following list-item specific quirk:
        // We do not create markers for list items when the list-style-type is none, while other browsers do.
        // The side effect of having no marker is that in quirks mode we have to specifically check for list-item
        // and make sure it is treated as if it had content and stretched the line.
        // see LegacyInlineFlowBox c'tor.
        return inlineLevelBox.layoutBox().style().isOriginalDisplayListItemType();
    }
    // Non-root inline boxes (e.g. <span>).
    auto& boxGeometry = formattingContext().geometryForBox(inlineLevelBox.layoutBox());
    if (boxGeometry.horizontalBorderAndPadding()) {
        // Horizontal border and padding make the inline box stretch the line (e.g. <span style="padding: 10px;"></span>).
        return true;
    }
    return false;
}

std::optional<LayoutUnit> InlineQuirks::initialLetterAlignmentOffset(const Box& floatBox, const RenderStyle& lineBoxStyle) const
{
    ASSERT(floatBox.isFloatingPositioned());
    if (!floatBox.style().lineBoxContain().contains(LineBoxContain::InitialLetter))
        return { };
    auto& primaryFontMetrics = lineBoxStyle.fontCascade().metricsOfPrimaryFont();
    auto lineHeight = [&]() -> InlineLayoutUnit {
        if (lineBoxStyle.lineHeight().isNegative())
            return primaryFontMetrics.ascent() + primaryFontMetrics.descent();
        return lineBoxStyle.computedLineHeight();
    };
    auto& floatBoxGeometry = formattingContext().geometryForBox(floatBox);
    return LayoutUnit { primaryFontMetrics.ascent() + (lineHeight() - primaryFontMetrics.height()) / 2 - primaryFontMetrics.capHeight() - floatBoxGeometry.marginBorderAndPaddingBefore() };
}

std::optional<InlineRect> InlineQuirks::adjustedRectForLineGridLineAlign(const InlineRect& rect) const
{
    auto& rootBoxStyle = formattingContext().root().style();
    auto& parentBlockLayoutState = formattingContext().layoutState().parentBlockLayoutState();

    if (rootBoxStyle.lineAlign() == LineAlign::None)
        return { };
    if (!parentBlockLayoutState.lineGrid())
        return { };

    // This implement the legacy -webkit-line-align property.
    // It snaps line edges to a grid defined by an ancestor box.
    auto& lineGrid = *parentBlockLayoutState.lineGrid();
    auto offset = InlineLayoutUnit { lineGrid.layoutOffset.width() + lineGrid.gridOffset.width() };
    auto columnWidth = lineGrid.columnWidth;
    auto leftShift = fmodf(columnWidth - fmodf(rect.left() + offset, columnWidth), columnWidth);
    auto rightShift = -fmodf(rect.right() + offset, columnWidth);

    auto adjustedRect = rect;
    adjustedRect.shiftLeftBy(leftShift);
    adjustedRect.shiftRightBy(rightShift);

    if (adjustedRect.isEmpty())
        return { };

    return adjustedRect;
}

std::optional<InlineLayoutUnit> InlineQuirks::adjustmentForLineGridLineSnap(const LineBox& lineBox) const
{
    auto& rootBoxStyle = formattingContext().root().style();
    auto& inlineLayoutState = formattingContext().layoutState();

    if (rootBoxStyle.lineSnap() == LineSnap::None)
        return { };
    if (!inlineLayoutState.parentBlockLayoutState().lineGrid())
        return { };

    // This implement the legacy -webkit-line-snap property.
    // It snaps line baselines to a grid defined by an ancestor box.

    auto& lineGrid = *inlineLayoutState.parentBlockLayoutState().lineGrid();

    auto gridLineHeight = lineGrid.rowHeight;
    if (!roundToInt(gridLineHeight))
        return { };

    auto& gridFontMetrics = lineGrid.primaryFont->fontMetrics();
    auto lineGridFontAscent = gridFontMetrics.ascent(lineBox.baselineType());
    auto lineGridFontHeight = gridFontMetrics.height();
    auto lineGridHalfLeading = (gridLineHeight - lineGridFontHeight) / 2;

    auto firstLineTop = lineGrid.topRowOffset + lineGrid.gridOffset.height();

    if (lineGrid.paginationOrigin && lineGrid.pageLogicalTop > firstLineTop)
        firstLineTop = lineGrid.paginationOrigin->height() + lineGrid.pageLogicalTop;

    auto firstTextTop = firstLineTop + lineGridHalfLeading;
    auto firstBaselinePosition = firstTextTop + lineGridFontAscent;

    auto rootInlineBoxTop = lineBox.logicalRect().top() + lineBox.logicalRectForRootInlineBox().top();

    auto ascent = lineBox.rootInlineBox().ascent();
    auto logicalHeight = ascent + lineBox.rootInlineBox().descent();
    auto currentBaselinePosition = rootInlineBoxTop + ascent + lineGrid.layoutOffset.height();

    if (rootBoxStyle.lineSnap() == LineSnap::Contain) {
        if (logicalHeight <= lineGridFontHeight)
            firstTextTop += (lineGridFontHeight - logicalHeight) / 2;
        else {
            LayoutUnit numberOfLinesWithLeading { ceilf(static_cast<float>(logicalHeight - lineGridFontHeight) / gridLineHeight) };
            LayoutUnit totalHeight = lineGridFontHeight + numberOfLinesWithLeading * gridLineHeight;
            firstTextTop += (totalHeight - logicalHeight) / 2;
        }
        firstBaselinePosition = firstTextTop + ascent;
    }

    // If we're above the first line, just push to the first line.
    if (currentBaselinePosition < firstBaselinePosition)
        return firstBaselinePosition - currentBaselinePosition;

    // Otherwise we're in the middle of the grid somewhere. Just push to the next line.
    auto baselineOffset = currentBaselinePosition - firstBaselinePosition;
    auto remainder = roundToInt(baselineOffset) % roundToInt(gridLineHeight);
    if (!remainder)
        return { };

    return gridLineHeight - remainder;
}

}
}

