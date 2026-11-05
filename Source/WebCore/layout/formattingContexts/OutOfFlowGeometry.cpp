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

#include "BlockFormattingUtils.h"
#include "RenderBox.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePositionArea.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+EvaluationMinimum.h"

namespace WebCore {
namespace Layout {

OutOfFlowGeometry::OutOfFlowGeometry(const RenderBox& box, const ContainingBlock& containingBlock, const StaticPosition& staticPosition, const std::optional<DefaultAnchor>& defaultAnchor, LayoutUnit inlineAxisBorderAndPadding, LayoutUnit blockAxisBorderAndPadding)
    : m_box(box)
    , m_containingBlock(containingBlock)
    , m_staticPosition(staticPosition)
    , m_defaultAnchor(defaultAnchor)
    , m_inlineAxisBorderAndPadding(inlineAxisBorderAndPadding)
    , m_blockAxisBorderAndPadding(blockAxisBorderAndPadding)
    , m_isOrthogonal(containingBlock.writingMode.isOrthogonal(box.writingMode()))
{
}

auto OutOfFlowGeometry::containingBlockAxis(Axis axis) const -> Axis
{
    if (!m_isOrthogonal)
        return axis;
    return axis == Axis::Inline ? Axis::Block : Axis::Inline;
}

bool OutOfFlowGeometry::hasAutoInset(Axis containingBlockAxis) const
{
    CheckedRef style = this->style();
    auto writingMode = m_containingBlock.writingMode;
    if (containingBlockAxis == Axis::Inline)
        return style->insetStart(writingMode).isAuto() || style->insetEnd(writingMode).isAuto();
    return style->insetBefore(writingMode).isAuto() || style->insetAfter(writingMode).isAuto();
}

const Style::ComputedStyle& OutOfFlowGeometry::style() const
{
    return m_box->style();
}

// The inset-modified containing block (§4.1): with both insets auto, it starts or ends at the static position, unless the box is positioned
// against its default anchor.
std::pair<LayoutUnit, LayoutUnit> OutOfFlowGeometry::insetModifiedContainingBlock(const AxisValues& values)
{
    auto usesStaticPosition = !values.insetStart && !values.insetEnd && !values.defaultAnchor;
    auto rangeStart = values.containingBlockStart + values.insetStart.value_or(usesStaticPosition && !values.staticPositionIsMarginBoxEnd ? values.staticPosition : 0_lu);
    auto rangeEnd = values.containingBlockStart + values.containingBlockSize - values.insetEnd.value_or(usesStaticPosition && values.staticPositionIsMarginBoxEnd ? values.containingBlockSize - values.staticPosition : 0_lu);
    return { rangeStart, rangeEnd };
}

static BoxAxis physicalAxis(bool isInlineAxis, WritingMode writingMode)
{
    return isInlineAxis ? writingMode.inlineAxis() : writingMode.blockAxis();
}

// The default anchor in the axis, when position-area, or anchor-center in the axis, positions the box against it (css-anchor-position-1 §3,
// PositionedLayoutConstraints::needsAnchor).
std::optional<std::pair<LayoutUnit, LayoutUnit>> OutOfFlowGeometry::defaultAnchorRange(Axis axis) const
{
    if (!m_defaultAnchor)
        return { };
    CheckedRef style = this->style();
    auto alignment = axis == Axis::Inline ? style->justifySelf().resolve() : style->alignSelf().resolve();
    if (style->positionArea().isNone() && alignment.position() != ItemPosition::AnchorCenter)
        return { };
    if (axis == Axis::Inline)
        return std::pair { m_defaultAnchor->inlineStart, m_defaultAnchor->inlineStart + m_defaultAnchor->inlineSize };
    return std::pair { m_defaultAnchor->blockStart, m_defaultAnchor->blockStart + m_defaultAnchor->blockSize };
}

// The range of the containing block in the axis, which position-area makes the region of the grid that the edges of the default anchor make
// with the containing block's (css-anchor-position-1 §3.1, PositionedLayoutConstraints::adjustForPositionArea).
std::pair<LayoutUnit, LayoutUnit> OutOfFlowGeometry::containingBlockRange(Axis axis) const
{
    auto size = axis == Axis::Inline ? m_containingBlock.inlineSize : m_containingBlock.blockSize;
    auto positionArea = style().positionArea().tryValue();
    if (!positionArea || !m_defaultAnchor)
        return { 0_lu, size };
    auto start = 0_lu;
    auto end = size;
    if (auto& scrollableContainingBlock = m_containingBlock.scrollableContainingBlock) {
        start = axis == Axis::Inline ? scrollableContainingBlock->x() : scrollableContainingBlock->y();
        end = axis == Axis::Inline ? scrollableContainingBlock->maxX() : scrollableContainingBlock->maxY();
    }
    auto [anchorStart, anchorEnd] = *defaultAnchorRange(axis);
    auto writingMode = m_containingBlock.writingMode;
    auto track = positionArea->coordMatchedTrackForAxis(physicalAxis(axis == Axis::Inline, writingMode), writingMode, m_box->writingMode());
    // The track is in the direction of the render tree's coordinates, which is against the inline direction when that is flipped.
    if (axis == Axis::Inline && writingMode.isInlineFlipped()) {
        switch (track) {
        case Style::PositionAreaTrack::Start:
            track = Style::PositionAreaTrack::End;
            break;
        case Style::PositionAreaTrack::SpanStart:
            track = Style::PositionAreaTrack::SpanEnd;
            break;
        case Style::PositionAreaTrack::End:
            track = Style::PositionAreaTrack::Start;
            break;
        case Style::PositionAreaTrack::SpanEnd:
            track = Style::PositionAreaTrack::SpanStart;
            break;
        case Style::PositionAreaTrack::Center:
        case Style::PositionAreaTrack::SpanAll:
            break;
        }
    }
    switch (track) {
    case Style::PositionAreaTrack::Start:
        return { std::min(start, anchorStart), anchorStart };
    case Style::PositionAreaTrack::SpanStart:
        return { std::min(start, anchorStart), anchorEnd };
    case Style::PositionAreaTrack::End:
        return { anchorEnd, std::max(end, anchorEnd) };
    case Style::PositionAreaTrack::SpanEnd:
        return { anchorStart, std::max(end, anchorEnd) };
    case Style::PositionAreaTrack::Center:
        return { anchorStart, anchorEnd };
    case Style::PositionAreaTrack::SpanAll:
        return { std::min(start, anchorStart), std::max(end, anchorEnd) };
    }
    ASSERT_NOT_REACHED();
    return { start, end };
}

// The self-alignment property in the axis, in which auto behaves as normal (css-align-3 §6.1). With both insets auto, it only applies in the
// inline axis of a box whose parent is a block container, where auto takes the parent's justify-items, and aligns the box from its static
// position (css-align-3 §6.1.4, PositionedLayoutConstraints::shouldAlignStaticPositionInInlineAxis).
StyleSelfAlignmentData OutOfFlowGeometry::selfAlignment(Axis axis, bool usesStaticPosition) const
{
    CheckedRef style = this->style();
    if (!usesStaticPosition) {
        auto alignment = axis == Axis::Inline ? style->justifySelf().resolve() : style->alignSelf().resolve();
        // position-area aligns a normal one toward the default anchor (css-anchor-position-1 §3.1.1).
        if (auto positionArea = style->positionArea().tryValue(); positionArea && alignment.isNormal())
            alignment.setPosition(positionArea->defaultAlignmentForAxis(physicalAxis(axis == Axis::Inline, m_containingBlock.writingMode), m_containingBlock.writingMode, m_box->writingMode()));
        return alignment;
    }
    CheckedPtr parent = m_box->parent();
    if (axis == Axis::Block || !parent || !parent->isBlockContainer())
        return { ItemPosition::Normal };
    auto alignment = style->justifySelf().resolve(&parent->style());
    return alignment.isNormalStretchOrLegacy() ? StyleSelfAlignmentData { ItemPosition::Normal } : alignment;
}

// Whether an automatic size is the stretch-fit size, given the insets (§4.3): normal alignment stretches, unless position-area aligns it.
bool OutOfFlowGeometry::alignmentAppliesStretch(Axis axis) const
{
    CheckedRef style = this->style();
    auto alignment = axis == Axis::Inline ? style->justifySelf().resolve() : style->alignSelf().resolve();
    return alignment.isStretch() || (alignment.isNormal() && style->positionArea().isNone());
}

// Where self-alignment puts the margin box in the inset-modified containing block. anchor-center centers it on the default anchor, inside the
// inset-modified containing block unless that overflows. Unless the alignment is unsafe, a margin box that overflows the inset-modified
// containing block stays inside the containing block where it can, at the start side last, except past the end sides of a scroll container
// (css-align-3 §4.4, css-anchor-position-1 §3.3, PositionedLayoutConstraints::resolveAlignmentShift).
LayoutUnit OutOfFlowGeometry::alignmentOffset(Axis axis, const AxisValues& values, LayoutUnit rangeStart, LayoutUnit rangeEnd, LayoutUnit freeSpace) const
{
    auto& alignment = values.selfAlignment;
    auto offset = [&] {
        if (alignment.position() != ItemPosition::AnchorCenter || !values.defaultAnchor)
            return StyleSelfAlignmentData::adjustmentFromStartEdge(freeSpace, alignment.position(), axis == Axis::Inline ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block, m_containingBlock.writingMode, m_box->writingMode());
        auto [anchorStart, anchorEnd] = *values.defaultAnchor;
        auto marginBoxSize = rangeEnd - rangeStart - freeSpace;
        auto offset = anchorStart + (anchorEnd - anchorStart - marginBoxSize) / 2 - rangeStart;
        if (alignment.overflow() == OverflowAlignment::Safe)
            offset = std::max(0_lu, offset);
        if (freeSpace >= 0 && alignment.overflow() != OverflowAlignment::Unsafe)
            offset = std::clamp(offset, 0_lu, freeSpace);
        return offset;
    }();
    if (freeSpace >= 0 || alignment.isNormal() || alignment.overflow() == OverflowAlignment::Unsafe)
        return offset;
    auto containingBlockSize = axis == Axis::Inline ? m_containingBlock.inlineSize : m_containingBlock.blockSize;
    auto spaceAtEnd = std::max(0_lu, containingBlockSize - rangeEnd) + freeSpace - offset;
    if (spaceAtEnd < 0 && !m_containingBlock.isScrollContainer)
        offset += spaceAtEnd;
    auto spaceAtStart = std::max(0_lu, rangeStart) + offset;
    if (spaceAtStart < 0)
        offset -= spaceAtStart;
    return offset;
}

// The geometry of one axis: auto margins (§4.4) and the alignment of the margin box (§4.5) in the inset-modified containing block.
auto OutOfFlowGeometry::computeAxisGeometry(Axis axis, const AxisValues& values, LayoutUnit borderBoxSize) const -> AxisGeometry
{
    auto [insetStart, insetEnd, marginStart, marginEnd, containingBlockStart, containingBlockSize, borderAndPadding, staticPosition, staticPositionIsMarginBoxEnd, staticPositionAlignment, selfAlignment, defaultAnchor] = values;
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
    auto marginBoxSize = usedMarginStart + borderBoxSize + usedMarginEnd;
    auto marginBoxStart = [&] {
        // The self-alignment property aligns it when neither the insets nor the margins are auto, against the default anchor, and from the
        // static position (selfAlignment).
        if ((insetStart && insetEnd && marginStart && marginEnd) || (!insetStart && !insetEnd && (defaultAnchor || !selfAlignment.isNormal())))
            return rangeStart + alignmentOffset(axis, values, rangeStart, rangeEnd, rangeEnd - rangeStart - marginBoxSize);
        // Otherwise the margin box aligns with the edge of the stronger inset, the start one when neither is auto.
        auto alignsWithEnd = !insetStart && (insetEnd || staticPositionIsMarginBoxEnd);
        return alignsWithEnd ? rangeEnd - marginBoxSize : rangeStart;
    }();
    // The size comes from the inset-modified containing block at the start of the static-position rectangle, as on the render tree path,
    // which lays the box out again at the aligned position (FlexLayout::setStaticPositionForPositionedLayout).
    if (!insetStart && !insetEnd && !defaultAnchor && staticPositionAlignment) {
        auto freeSpace = staticPositionAlignment->rectangleSize - marginBoxSize;
        auto offset = [&] {
            if (freeSpace < 0 && staticPositionAlignment->isSafe)
                return 0_lu;
            switch (staticPositionAlignment->position) {
            case StaticPosition::Alignment::Position::Start:
                return 0_lu;
            case StaticPosition::Alignment::Position::Center:
                return freeSpace / 2;
            case StaticPosition::Alignment::Position::End:
                return freeSpace;
            }
            ASSERT_NOT_REACHED();
            return 0_lu;
        }();
        marginBoxStart += staticPositionIsMarginBoxEnd ? -offset : offset;
    }
    return { borderBoxSize, usedMarginStart, usedMarginEnd, marginBoxStart + usedMarginStart };
}

// Percentages of the margins refer to the inline size of the containing block, the insets' to its size in their axis. Against the default anchor,
// auto margins are zero, and so are auto insets unless the alignment is normal (PositionedLayoutConstraints::resolvePosition).
auto OutOfFlowGeometry::axisValues(Axis axis) const -> AxisValues
{
    CheckedRef style = this->style();
    auto zoom = style->usedZoomForLength();
    auto writingMode = m_containingBlock.writingMode;
    auto [containingBlockStart, containingBlockEnd] = containingBlockRange(axis);
    auto [containingBlockInlineStart, containingBlockInlineEnd] = containingBlockRange(Axis::Inline);
    auto defaultAnchor = defaultAnchorRange(axis);
    auto alignment = axis == Axis::Inline ? style->justifySelf().resolve() : style->alignSelf().resolve();
    auto inset = [&](auto& inset) -> std::optional<LayoutUnit> {
        if (inset.isAuto())
            return defaultAnchor && !alignment.isNormal() ? std::optional { 0_lu } : std::nullopt;
        return Style::evaluateMinimum<LayoutUnit>(inset, containingBlockEnd - containingBlockStart, zoom);
    };
    auto margin = [&](auto& margin) -> std::optional<LayoutUnit> {
        if (margin.isAuto())
            return defaultAnchor ? std::optional { 0_lu } : std::nullopt;
        return Style::evaluateMinimum<LayoutUnit>(margin, containingBlockInlineEnd - containingBlockInlineStart, zoom);
    };
    auto borderAndPadding = axis == containingBlockAxis(Axis::Inline) ? m_inlineAxisBorderAndPadding : m_blockAxisBorderAndPadding;
    if (axis == Axis::Inline) {
        auto usesStaticPosition = !defaultAnchor && style->insetStart(writingMode).isAuto() && style->insetEnd(writingMode).isAuto();
        return {
            inset(style->insetStart(writingMode)), inset(style->insetEnd(writingMode)),
            margin(style->marginStart(writingMode)), margin(style->marginEnd(writingMode)),
            containingBlockStart, containingBlockEnd - containingBlockStart, borderAndPadding,
            m_staticPosition.inlinePosition, m_staticPosition.inlinePositionIsMarginBoxEnd, m_staticPosition.inlineAlignment,
            selfAlignment(Axis::Inline, usesStaticPosition), defaultAnchor
        };
    }
    auto usesStaticPosition = !defaultAnchor && style->insetBefore(writingMode).isAuto() && style->insetAfter(writingMode).isAuto();
    return {
        inset(style->insetBefore(writingMode)), inset(style->insetAfter(writingMode)),
        margin(style->marginBefore(writingMode)), margin(style->marginAfter(writingMode)),
        containingBlockStart, containingBlockEnd - containingBlockStart, borderAndPadding,
        m_staticPosition.blockPosition, false, m_staticPosition.blockAlignment,
        selfAlignment(Axis::Block, usesStaticPosition), defaultAnchor
    };
}

auto OutOfFlowGeometry::keywordSizes(const AxisValues& values, std::optional<LayoutUnit> minContent, std::optional<LayoutUnit> maxContent, std::optional<LayoutUnit> fillAvailable) const -> KeywordSizes
{
    // The inset-modified containing block minus the margins, auto ones as zero.
    auto [rangeStart, rangeEnd] = insetModifiedContainingBlock(values);
    auto stretchFit = rangeEnd - rangeStart - values.marginStart.value_or(0_lu) - values.marginEnd.value_or(0_lu);
    return { minContent, maxContent, stretchFit, fillAvailable };
}

// The stretch-fit size, floored by the min-content size and capped by the max-content size (css-sizing-3 §5.1).
std::optional<LayoutUnit> OutOfFlowGeometry::fitContentSize(const KeywordSizes& keywordSizes)
{
    if (!keywordSizes.minContent || !keywordSizes.maxContent)
        return { };
    return std::min(std::max(*keywordSizes.minContent, keywordSizes.stretchFit), *keywordSizes.maxContent);
}

template<typename SizeType>
std::optional<LayoutUnit> OutOfFlowGeometry::borderBoxSizeFor(const SizeType& size, const AxisValues& values, const KeywordSizes& keywordSizes) const
{
    auto borderAndPadding = values.borderAndPadding;
    if (size.isSpecified()) {
        auto value = Style::evaluate<LayoutUnit>(size, values.containingBlockSize, style().usedZoomForLength());
        return style().boxSizing() == BoxSizing::BorderBox ? std::max(value, borderAndPadding) : value + borderAndPadding;
    }
    auto atLeastBorderAndPadding = [&](std::optional<LayoutUnit> borderBoxSize) {
        return borderBoxSize.transform([&](auto size) {
            return std::max(size, borderAndPadding);
        });
    };
    if (size.isMinContent())
        return atLeastBorderAndPadding(keywordSizes.minContent);
    if (size.isMaxContent())
        return atLeastBorderAndPadding(keywordSizes.maxContent);
    if (size.isFitContent())
        return atLeastBorderAndPadding(fitContentSize(keywordSizes));
    if (size.isFillAvailable() && keywordSizes.fillAvailable)
        return atLeastBorderAndPadding(keywordSizes.fillAvailable);
    if (size.isStretch())
        return atLeastBorderAndPadding(keywordSizes.stretchFit);
    return { };
}

// The preferred size capped by the maximum size and floored by the minimum size (§4.2). The automatic minimum size is zero (§4.3).
template<typename MinimumSizeType, typename MaximumSizeType>
LayoutUnit OutOfFlowGeometry::clamp(LayoutUnit borderBoxSize, const MinimumSizeType& minimumSize, const MaximumSizeType& maximumSize, const AxisValues& values, const KeywordSizes& keywordSizes) const
{
    if (auto maximum = borderBoxSizeFor(maximumSize, values, keywordSizes))
        borderBoxSize = std::min(borderBoxSize, *maximum);
    if (auto minimum = borderBoxSizeFor(minimumSize, values, keywordSizes))
        borderBoxSize = std::max(borderBoxSize, *minimum);
    return std::max(values.borderAndPadding, borderBoxSize);
}

// With both sizes automatic, the axis with an auto inset when only one has one, and otherwise the block axis.
auto OutOfFlowGeometry::ratioDependentAxis() const -> std::optional<Axis>
{
    CheckedRef style = this->style();
    if (!style->aspectRatio().hasRatio())
        return { };
    if (auto ratio = style->logicalAspectRatio(); !ratio || std::isnan(ratio))
        return { };
    // The intrinsic sizing keywords in the block axis are the automatic size (css-sizing-3 §3.2).
    auto& blockSize = style->logicalHeight();
    auto inlineSizeIsAutomatic = style->logicalWidth().isAuto();
    auto blockSizeIsAutomatic = blockSize.isAuto() || blockSize.isIntrinsic();
    if (inlineSizeIsAutomatic != blockSizeIsAutomatic)
        return inlineSizeIsAutomatic ? Axis::Inline : Axis::Block;
    if (!inlineSizeIsAutomatic)
        return { };
    return hasAutoInset(containingBlockAxis(Axis::Inline)) && !hasAutoInset(containingBlockAxis(Axis::Block)) ? Axis::Inline : Axis::Block;
}

bool OutOfFlowGeometry::hasFitContentInlineSize() const
{
    if (!style().logicalWidth().isAuto() || ratioDependentAxis() == Axis::Inline)
        return false;
    auto axis = containingBlockAxis(Axis::Inline);
    auto values = axisValues(axis);
    return ((!values.insetStart || !values.insetEnd) && !values.defaultAnchor) || !alignmentAppliesStretch(axis);
}

bool OutOfFlowGeometry::needsContentInlineSizes() const
{
    CheckedRef style = this->style();
    if (style->logicalWidth().isIntrinsic() || style->logicalMinWidth().isIntrinsic() || style->logicalMaxWidth().isIntrinsic())
        return true;
    return hasFitContentInlineSize() || (ratioDependentAxis() == Axis::Inline && style->logicalMinWidth().isAuto() && m_box->firstChild());
}

auto OutOfFlowGeometry::computeInlineAxisGeometry(std::optional<IntrinsicWidthConstraints> contentInlineSizes) const -> AxisGeometry
{
    ASSERT(contentInlineSizes || !needsContentInlineSizes());
    CheckedRef style = this->style();
    auto writingMode = m_box->writingMode();
    auto axis = containingBlockAxis(Axis::Inline);
    auto values = axisValues(axis);
    auto ratioDependentAxis = this->ratioDependentAxis();
    auto contentSize = [&](LayoutUnit IntrinsicWidthConstraints::* size) {
        return contentInlineSizes.transform([&](auto& sizes) {
            return sizes.*size + values.borderAndPadding;
        });
    };
    auto keywordSizes = this->keywordSizes(values, contentSize(&IntrinsicWidthConstraints::minimum), contentSize(&IntrinsicWidthConstraints::maximum), { });
    auto borderBoxSize = [&] {
        if (auto size = borderBoxSizeFor(style->logicalWidth(writingMode), values, keywordSizes))
            return *size;
        // A preferred aspect ratio gives it from the definite block size.
        if (ratioDependentAxis == Axis::Inline) {
            if (auto blockSize = definiteBorderBoxBlockSize(axisValues(containingBlockAxis(Axis::Block))))
                return *BlockFormattingUtils::aspectRatioBorderBoxInlineSize(m_box, *blockSize, values.borderAndPadding, m_blockAxisBorderAndPadding);
        }
        // The automatic size is resolved against the inset-modified containing block minus the margins, auto ones as zero.
        if (!hasFitContentInlineSize())
            return keywordSizes.stretchFit;
        return *fitContentSize(keywordSizes);
    }();
    // The min and max inline sizes, with those the ratio transfers from the min and max block sizes when it gives the block size, and when it
    // gives the inline size an auto min inline size is the min-content size (css-sizing-4 §5.1, RenderBox::computeOutOfFlowPositionedLogicalWidth).
    auto maximumSize = borderBoxSizeFor(style->logicalMaxWidth(writingMode), values, keywordSizes).value_or(LayoutUnit::max());
    auto minimumSize = values.borderAndPadding;
    if (auto minimumInlineSize = borderBoxSizeFor(style->logicalMinWidth(writingMode), values, keywordSizes))
        minimumSize = std::max(minimumSize, *minimumInlineSize);
    else if (ratioDependentAxis == Axis::Inline && contentInlineSizes)
        minimumSize = std::max(minimumSize, contentInlineSizes->minimum + values.borderAndPadding);
    if (ratioDependentAxis == Axis::Block) {
        if (auto limits = BlockFormattingUtils::aspectRatioInlineSizeLimits(m_box, values.borderAndPadding, m_blockAxisBorderAndPadding, axisValues(containingBlockAxis(Axis::Block)).containingBlockSize, { }, writingMode)) {
            maximumSize = std::min(maximumSize, limits->second);
            minimumSize = std::max(minimumSize, limits->first);
        }
    }
    return computeAxisGeometry(axis, values, std::max(minimumSize, std::min(borderBoxSize, maximumSize)));
}

std::optional<LayoutUnit> OutOfFlowGeometry::definiteBorderBoxBlockSize(const AxisValues& values, std::optional<LayoutUnit> borderBoxInlineSize) const
{
    CheckedRef style = this->style();
    auto writingMode = m_box->writingMode();
    auto& minimumBlockSize = style->logicalMinHeight(writingMode);
    auto& maximumBlockSize = style->logicalMaxHeight(writingMode);
    // The content block size is not known yet, which the min-content, max-content and fit-content keywords would take.
    auto keywordSizes = this->keywordSizes(values, { }, { }, fillAvailableBlockSize());
    auto& blockSize = style->logicalHeight(writingMode);
    if (auto size = borderBoxSizeFor(blockSize, values, keywordSizes))
        return clamp(*size, minimumBlockSize, maximumBlockSize, values, keywordSizes);
    // A preferred aspect ratio gives it from the inline size, which makes it definite (css-sizing-4 §5.1).
    if (ratioDependentAxis() == Axis::Block) {
        if (!borderBoxInlineSize)
            return { };
        return BlockFormattingUtils::aspectRatioBorderBoxBlockSize(m_box, *borderBoxInlineSize, m_inlineAxisBorderAndPadding, values.borderAndPadding, values.containingBlockSize, { }, writingMode);
    }
    // With neither inset auto, the automatic size is the stretch-fit size when the alignment stretches (§4.3).
    if (blockSize.isAuto() && values.insetStart && values.insetEnd && alignmentAppliesStretch(containingBlockAxis(Axis::Block)))
        return clamp(keywordSizes.stretchFit, minimumBlockSize, maximumBlockSize, values, keywordSizes);
    return { };
}

std::optional<LayoutUnit> OutOfFlowGeometry::definiteContentBoxBlockSize(std::optional<LayoutUnit> borderBoxInlineSize) const
{
    return definiteBorderBoxBlockSize(axisValues(containingBlockAxis(Axis::Block)), borderBoxInlineSize).transform([&](auto borderBoxSize) {
        return borderBoxSize - m_blockAxisBorderAndPadding;
    });
}

auto OutOfFlowGeometry::computeReplacedInlineAxisGeometry(LayoutUnit borderBoxSize) const -> AxisGeometry
{
    auto axis = containingBlockAxis(Axis::Inline);
    return computeAxisGeometry(axis, axisValues(axis), borderBoxSize);
}

auto OutOfFlowGeometry::computeReplacedBlockAxisGeometry(LayoutUnit borderBoxSize) const -> AxisGeometry
{
    auto axis = containingBlockAxis(Axis::Block);
    return computeAxisGeometry(axis, axisValues(axis), borderBoxSize);
}

auto OutOfFlowGeometry::computeBlockAxisGeometry(LayoutUnit intrinsicContentBlockSize, LayoutUnit borderBoxInlineSize) const -> AxisGeometry
{
    CheckedRef style = this->style();
    auto axis = containingBlockAxis(Axis::Block);
    auto values = axisValues(axis);
    auto writingMode = m_box->writingMode();
    // The size a preferred aspect ratio gives, whose automatic minimum is the content's block size (css-sizing-4 §5.1).
    if (ratioDependentAxis() == Axis::Block) {
        if (auto borderBoxSize = BlockFormattingUtils::aspectRatioBorderBoxBlockSize(m_box, borderBoxInlineSize, m_inlineAxisBorderAndPadding, values.borderAndPadding, values.containingBlockSize, { }, writingMode, intrinsicContentBlockSize))
            return computeAxisGeometry(axis, values, *borderBoxSize);
    }
    // Otherwise the automatic size is the fit-content size, which is the content's block size in this axis, and so are the min-content,
    // max-content and fit-content keywords (css-sizing-3 §3.2).
    auto contentBlockSize = intrinsicContentBlockSize + values.borderAndPadding;
    auto keywordSizes = this->keywordSizes(values, contentBlockSize, contentBlockSize, fillAvailableBlockSize());
    auto borderBoxSize = [&] {
        auto& blockSize = style->logicalHeight(writingMode);
        if (auto size = borderBoxSizeFor(blockSize, values, keywordSizes))
            return *size;
        if (blockSize.isAuto() && values.insetStart && values.insetEnd && alignmentAppliesStretch(axis))
            return keywordSizes.stretchFit;
        return contentBlockSize;
    }();
    return computeAxisGeometry(axis, values, clamp(borderBoxSize, style->logicalMinHeight(writingMode), style->logicalMaxHeight(writingMode), values, keywordSizes));
}

}
}
