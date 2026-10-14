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
#include "OutOfFlowGeometry.h"

#include "RenderBox.h"
#include "RenderObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+EvaluationMinimum.h"

namespace WebCore {
namespace Layout {

OutOfFlowGeometry::OutOfFlowGeometry(const RenderBox& box, const ContainingBlock& containingBlock, const StaticPosition& staticPosition, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding)
    : m_box(box)
    , m_containingBlock(containingBlock)
    , m_staticPosition(staticPosition)
    , m_horizontalBorderAndPadding(horizontalBorderAndPadding)
    , m_verticalBorderAndPadding(verticalBorderAndPadding)
{
    ASSERT(containingBlock.writingMode.isHorizontal());
}

const Style::ComputedStyle& OutOfFlowGeometry::style() const
{
    return m_box->style();
}

// The inset-modified containing block (§4.1): with both insets auto, it starts or ends at the static position.
std::pair<LayoutUnit, LayoutUnit> OutOfFlowGeometry::insetModifiedContainingBlock(const AxisValues& values)
{
    auto usesStaticPosition = !values.insetStart && !values.insetEnd;
    auto rangeStart = values.insetStart.value_or(usesStaticPosition && !values.staticPositionIsMarginBoxEnd ? values.staticPosition : 0_lu);
    auto rangeEnd = values.containingBlockSize - values.insetEnd.value_or(usesStaticPosition && values.staticPositionIsMarginBoxEnd ? values.containingBlockSize - values.staticPosition : 0_lu);
    return { rangeStart, rangeEnd };
}

// The geometry of one axis: auto margins (§4.4) and the alignment of the margin box (§4.5) in the inset-modified containing block.
auto OutOfFlowGeometry::computeAxisGeometry(Axis axis, const AxisValues& values, LayoutUnit borderBoxSize) -> AxisGeometry
{
    auto [insetStart, insetEnd, marginStart, marginEnd, containingBlockSize, borderAndPadding, staticPosition, staticPositionIsMarginBoxEnd] = values;
    auto [rangeStart, rangeEnd] = insetModifiedContainingBlock(values);
    borderBoxSize = std::max(borderAndPadding, borderBoxSize);

    auto usedMarginStart = marginStart.value_or(0_lu);
    auto usedMarginEnd = marginEnd.value_or(0_lu);
    if (insetStart && insetEnd && (!marginStart || !marginEnd)) {
        auto remainingSpace = rangeEnd - rangeStart - borderBoxSize - usedMarginStart - usedMarginEnd;
        if (!marginStart && !marginEnd) {
            // A negative remaining space goes to the end margin in the inline axis.
            if (axis == Axis::Inline && remainingSpace < 0)
                usedMarginEnd = remainingSpace;
            else {
                usedMarginStart = remainingSpace / 2;
                usedMarginEnd = remainingSpace - usedMarginStart;
            }
        } else if (!marginStart)
            usedMarginStart = remainingSpace;
        else
            usedMarginEnd = remainingSpace;
    }
    // The margin box aligns with the edge of the stronger inset, the start one when neither is auto.
    auto marginBoxSize = usedMarginStart + borderBoxSize + usedMarginEnd;
    auto alignsWithEnd = !insetStart && (insetEnd || staticPositionIsMarginBoxEnd);
    auto marginBoxStart = alignsWithEnd ? rangeEnd - marginBoxSize : rangeStart;
    return { borderBoxSize, usedMarginStart, usedMarginEnd, marginBoxStart + usedMarginStart };
}

// Percentages of the margins refer to the inline size of the containing block, the insets' to its size in their axis.
auto OutOfFlowGeometry::inlineAxisValues() const -> AxisValues
{
    CheckedRef style = this->style();
    auto zoom = style->usedZoomForLength();
    auto isLeftToRight = m_containingBlock.writingMode.isBidiLTR();
    auto inset = [&](auto& inset) -> std::optional<LayoutUnit> {
        return inset.isAuto() ? std::nullopt : std::optional { Style::evaluateMinimum<LayoutUnit>(inset, m_containingBlock.inlineSize, zoom) };
    };
    auto margin = [&](auto& margin) -> std::optional<LayoutUnit> {
        return margin.isAuto() ? std::nullopt : std::optional { Style::evaluateMinimum<LayoutUnit>(margin, m_containingBlock.inlineSize, zoom) };
    };
    return {
        inset(isLeftToRight ? style->left() : style->right()), inset(isLeftToRight ? style->right() : style->left()),
        margin(style->marginStart(m_containingBlock.writingMode)), margin(style->marginEnd(m_containingBlock.writingMode)),
        m_containingBlock.inlineSize, m_horizontalBorderAndPadding, m_staticPosition.inlinePosition, m_staticPosition.inlinePositionIsMarginBoxEnd
    };
}

auto OutOfFlowGeometry::blockAxisValues() const -> AxisValues
{
    CheckedRef style = this->style();
    auto zoom = style->usedZoomForLength();
    auto inset = [&](auto& inset) -> std::optional<LayoutUnit> {
        return inset.isAuto() ? std::nullopt : std::optional { Style::evaluateMinimum<LayoutUnit>(inset, m_containingBlock.blockSize, zoom) };
    };
    auto margin = [&](auto& margin) -> std::optional<LayoutUnit> {
        return margin.isAuto() ? std::nullopt : std::optional { Style::evaluateMinimum<LayoutUnit>(margin, m_containingBlock.inlineSize, zoom) };
    };
    return {
        inset(style->top()), inset(style->bottom()),
        margin(style->marginBefore(m_containingBlock.writingMode)), margin(style->marginAfter(m_containingBlock.writingMode)),
        m_containingBlock.blockSize, m_verticalBorderAndPadding, m_staticPosition.blockPosition, false
    };
}

template<typename SizeType>
LayoutUnit OutOfFlowGeometry::borderBoxSizeFor(const SizeType& size, LayoutUnit containingBlockSize, LayoutUnit borderAndPadding) const
{
    auto value = Style::evaluate<LayoutUnit>(size, containingBlockSize, style().usedZoomForLength());
    return style().boxSizing() == BoxSizing::BorderBox ? std::max(value, borderAndPadding) : value + borderAndPadding;
}

// The preferred size capped by the maximum size and floored by the minimum size (§4.2). The automatic minimum size is zero (§4.3).
template<typename MinimumSizeType, typename MaximumSizeType>
LayoutUnit OutOfFlowGeometry::clamp(LayoutUnit borderBoxSize, const MinimumSizeType& minimumSize, const MaximumSizeType& maximumSize, LayoutUnit containingBlockSize, LayoutUnit borderAndPadding) const
{
    if (maximumSize.isSpecified())
        borderBoxSize = std::min(borderBoxSize, borderBoxSizeFor(maximumSize, containingBlockSize, borderAndPadding));
    if (minimumSize.isSpecified())
        borderBoxSize = std::max(borderBoxSize, borderBoxSizeFor(minimumSize, containingBlockSize, borderAndPadding));
    return std::max(borderAndPadding, borderBoxSize);
}

bool OutOfFlowGeometry::hasFitContentInlineSize() const
{
    CheckedRef style = this->style();
    return style->width().isAuto() && (style->left().isAuto() || style->right().isAuto());
}

auto OutOfFlowGeometry::computeInlineAxisGeometry(std::optional<IntrinsicWidthConstraints> contentInlineSizes) const -> AxisGeometry
{
    ASSERT(contentInlineSizes || !hasFitContentInlineSize());
    CheckedRef style = this->style();
    auto values = inlineAxisValues();
    auto borderBoxSize = [&] {
        if (style->width().isSpecified())
            return borderBoxSizeFor(style->width(), values.containingBlockSize, values.borderAndPadding);
        // The automatic size is resolved against the inset-modified containing block minus the margins, auto ones as zero.
        auto [rangeStart, rangeEnd] = insetModifiedContainingBlock(values);
        auto stretchFitSize = rangeEnd - rangeStart - values.marginStart.value_or(0_lu) - values.marginEnd.value_or(0_lu);
        if (!hasFitContentInlineSize())
            return stretchFitSize;
        // The fit-content size: the stretch-fit size, floored by the min-content size and capped by the max-content size (css-sizing-3 §5.1).
        return std::min(std::max(contentInlineSizes->minimum + values.borderAndPadding, stretchFitSize), contentInlineSizes->maximum + values.borderAndPadding);
    }();
    return computeAxisGeometry(Axis::Inline, values, clamp(borderBoxSize, style->minWidth(), style->maxWidth(), values.containingBlockSize, values.borderAndPadding));
}

std::optional<LayoutUnit> OutOfFlowGeometry::definiteBorderBoxBlockSize(const AxisValues& values) const
{
    CheckedRef style = this->style();
    if (style->height().isSpecified())
        return clamp(borderBoxSizeFor(style->height(), values.containingBlockSize, values.borderAndPadding), style->minHeight(), style->maxHeight(), values.containingBlockSize, values.borderAndPadding);
    // With neither inset auto, the automatic size is the stretch-fit size (§4.3).
    if (values.insetStart && values.insetEnd) {
        auto stretchFitSize = values.containingBlockSize - *values.insetStart - *values.insetEnd - values.marginStart.value_or(0_lu) - values.marginEnd.value_or(0_lu);
        return clamp(stretchFitSize, style->minHeight(), style->maxHeight(), values.containingBlockSize, values.borderAndPadding);
    }
    return { };
}

std::optional<LayoutUnit> OutOfFlowGeometry::definiteContentBoxBlockSize() const
{
    return definiteBorderBoxBlockSize(blockAxisValues()).transform([&](auto borderBoxSize) {
        return borderBoxSize - m_verticalBorderAndPadding;
    });
}

auto OutOfFlowGeometry::computeBlockAxisGeometry(LayoutUnit intrinsicContentBlockSize) const -> AxisGeometry
{
    CheckedRef style = this->style();
    auto values = blockAxisValues();
    // Otherwise the automatic size is the fit-content size, which is the content's block size in this axis.
    auto borderBoxSize = definiteBorderBoxBlockSize(values).value_or(clamp(intrinsicContentBlockSize + values.borderAndPadding, style->minHeight(), style->maxHeight(), values.containingBlockSize, values.borderAndPadding));
    return computeAxisGeometry(Axis::Block, values, borderBoxSize);
}

}
}
