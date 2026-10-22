/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "BlockFormattingUtils.h"

#include "LayoutBoxGeometry.h"
#include "RenderBox.h"
#include "RenderObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+EvaluationMinimum.h"

namespace WebCore {
namespace Layout {

template<typename SizeType>
static std::optional<LayoutUnit> borderBoxBlockSizeFor(const SizeType& size, const Style::ComputedStyle& style, LayoutUnit borderAndPaddingBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize)
{
    if (!size.isSpecified())
        return { };
    // A percentage of an indefinite height behaves as auto (CSS 2.2 §10.5).
    if (size.isPercentOrCalculated() && !containingBlockContentBoxBlockSize)
        return { };
    auto value = Style::evaluate<LayoutUnit>(size, containingBlockContentBoxBlockSize.value_or(0_lu), style.usedZoomForLength());
    if (style.boxSizing() == BoxSizing::BorderBox)
        return std::max(value, borderAndPaddingBlockSize);
    return value + borderAndPaddingBlockSize;
}

LayoutUnit BlockFormattingUtils::borderBoxBlockSize(const RenderBox& box, LayoutUnit borderAndPaddingBlockSize, LayoutUnit intrinsicContentBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, WritingMode writingMode)
{
    CheckedRef style = box.style();
    auto blockSize = borderBoxBlockSizeFor(style->logicalHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize).value_or(intrinsicContentBlockSize + borderAndPaddingBlockSize);
    if (auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize))
        blockSize = std::min(blockSize, *maximumBlockSize);
    if (auto minimumBlockSize = borderBoxBlockSizeFor(style->logicalMinHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize))
        blockSize = std::max(blockSize, *minimumBlockSize);
    return blockSize;
}

bool BlockFormattingUtils::blockSizeBehavesAsAuto(const Style::PreferredSize& blockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize)
{
    return blockSize.isAuto() || (blockSize.isPercentOrCalculated() && !containingBlockContentBoxBlockSize);
}

std::optional<LayoutUnit> BlockFormattingUtils::definiteContentBoxBlockSize(const RenderBox& box, const BoxGeometry& geometry, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, WritingMode writingMode)
{
    // Anonymous block boxes are ignored when resolving percentage values that would refer to them (CSS 2.2 §9.2.1.1).
    if (box.isAnonymous())
        return containingBlockContentBoxBlockSize;
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding();
    CheckedRef style = box.style();
    if (!borderBoxBlockSizeFor(style->logicalHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize))
        return { };
    return borderBoxBlockSize(box, borderAndPaddingBlockSize, { }, containingBlockContentBoxBlockSize, writingMode) - borderAndPaddingBlockSize;
}

auto BlockFormattingUtils::inlineSizeAndMargins(const RenderBox& box, LayoutUnit borderAndPaddingInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle) -> InlineSizeAndMargins
{
    auto containingBlockWritingMode = containingBlockStyle.writingMode();
    CheckedRef style = box.style();
    auto zoom = style->usedZoomForLength();
    auto& marginStartLength = style->marginStart(containingBlockWritingMode);
    auto& marginEndLength = style->marginEnd(containingBlockWritingMode);
    auto usedMargin = [&](auto& margin) {
        return margin.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(margin, containingBlockInlineSize, zoom);
    };
    auto borderBoxInlineSizeFor = [&](auto& size) {
        auto value = Style::evaluate<LayoutUnit>(size, containingBlockInlineSize, zoom);
        if (style->boxSizing() == BoxSizing::BorderBox)
            return std::max(value, borderAndPaddingInlineSize);
        return value + borderAndPaddingInlineSize;
    };

    auto marginStart = usedMargin(marginStartLength);
    auto marginEnd = usedMargin(marginEndLength);

    auto borderBoxInlineSize = [&] {
        auto& inlineSize = style->logicalWidth(containingBlockWritingMode);
        if (inlineSize.isSpecified())
            return borderBoxInlineSizeFor(inlineSize);
        // FIXME: The sizing keywords (min-content, max-content, fit-content, stretch) are kept out by coverage.
        return std::max(borderAndPaddingInlineSize, containingBlockInlineSize - marginStart - marginEnd);
    }();
    if (auto& maximumWidth = style->logicalMaxWidth(containingBlockWritingMode); maximumWidth.isSpecified())
        borderBoxInlineSize = std::min(borderBoxInlineSize, borderBoxInlineSizeFor(maximumWidth));
    if (auto& minimumWidth = style->logicalMinWidth(containingBlockWritingMode); minimumWidth.isSpecified())
        borderBoxInlineSize = std::max(borderBoxInlineSize, borderBoxInlineSizeFor(minimumWidth));

    if (borderBoxInlineSize < containingBlockInlineSize) {
        if (marginStartLength.isAuto() && marginEndLength.isAuto()) {
            marginStart = std::max(0_lu, (containingBlockInlineSize - borderBoxInlineSize) / 2);
            marginEnd = containingBlockInlineSize - borderBoxInlineSize - marginStart;
        } else if (marginEndLength.isAuto())
            marginEnd = containingBlockInlineSize - borderBoxInlineSize - marginStart;
        else if (marginStartLength.isAuto())
            marginStart = containingBlockInlineSize - borderBoxInlineSize - marginEnd;
    }
    // An over-constrained box ignores its end margin, which takes what makes the equality true. Like RenderBox::computeLogicalWidth,
    // the end margin keeps its value in a scroll container, where it adds to the scrollable overflow.
    auto isScrollContainer = [&](Overflow overflow) {
        return overflow != Overflow::Visible && overflow != Overflow::Clip;
    };
    if (containingBlockInlineSize && !isScrollContainer(containingBlockStyle.overflowX()) && !isScrollContainer(containingBlockStyle.overflowY()))
        marginEnd = containingBlockInlineSize - borderBoxInlineSize - marginStart;
    return { borderBoxInlineSize, marginStart, marginEnd };
}

}
}
