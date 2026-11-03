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
{
    ASSERT(!containingBlock.writingMode.isOrthogonal(box.writingMode()));
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
    if (axis == Axis::Inline) {
        auto usesStaticPosition = !defaultAnchor && style->insetStart(writingMode).isAuto() && style->insetEnd(writingMode).isAuto();
        return {
            inset(style->insetStart(writingMode)), inset(style->insetEnd(writingMode)),
            margin(style->marginStart(writingMode)), margin(style->marginEnd(writingMode)),
            containingBlockStart, containingBlockEnd - containingBlockStart, m_inlineAxisBorderAndPadding,
            m_staticPosition.inlinePosition, m_staticPosition.inlinePositionIsMarginBoxEnd, m_staticPosition.inlineAlignment,
            selfAlignment(Axis::Inline, usesStaticPosition), defaultAnchor
        };
    }
    auto usesStaticPosition = !defaultAnchor && style->insetBefore(writingMode).isAuto() && style->insetAfter(writingMode).isAuto();
    return {
        inset(style->insetBefore(writingMode)), inset(style->insetAfter(writingMode)),
        margin(style->marginBefore(writingMode)), margin(style->marginAfter(writingMode)),
        containingBlockStart, containingBlockEnd - containingBlockStart, m_blockAxisBorderAndPadding,
        m_staticPosition.blockPosition, false, m_staticPosition.blockAlignment,
        selfAlignment(Axis::Block, usesStaticPosition), defaultAnchor
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
    auto writingMode = m_containingBlock.writingMode;
    if (!style->logicalWidth(writingMode).isAuto())
        return false;
    auto values = axisValues(Axis::Inline);
    return ((!values.insetStart || !values.insetEnd) && !values.defaultAnchor) || !alignmentAppliesStretch(Axis::Inline);
}

auto OutOfFlowGeometry::computeInlineAxisGeometry(std::optional<IntrinsicWidthConstraints> contentInlineSizes) const -> AxisGeometry
{
    ASSERT(contentInlineSizes || !hasFitContentInlineSize());
    CheckedRef style = this->style();
    auto writingMode = m_containingBlock.writingMode;
    auto values = axisValues(Axis::Inline);
    auto borderBoxSize = [&] {
        if (auto& inlineSize = style->logicalWidth(writingMode); inlineSize.isSpecified())
            return borderBoxSizeFor(inlineSize, values.containingBlockSize, values.borderAndPadding);
        // The automatic size is resolved against the inset-modified containing block minus the margins, auto ones as zero.
        auto [rangeStart, rangeEnd] = insetModifiedContainingBlock(values);
        auto stretchFitSize = rangeEnd - rangeStart - values.marginStart.value_or(0_lu) - values.marginEnd.value_or(0_lu);
        if (!hasFitContentInlineSize())
            return stretchFitSize;
        // The fit-content size: the stretch-fit size, floored by the min-content size and capped by the max-content size (css-sizing-3 §5.1).
        return std::min(std::max(contentInlineSizes->minimum + values.borderAndPadding, stretchFitSize), contentInlineSizes->maximum + values.borderAndPadding);
    }();
    return computeAxisGeometry(Axis::Inline, values, clamp(borderBoxSize, style->logicalMinWidth(writingMode), style->logicalMaxWidth(writingMode), values.containingBlockSize, values.borderAndPadding));
}

std::optional<LayoutUnit> OutOfFlowGeometry::definiteBorderBoxBlockSize(const AxisValues& values) const
{
    CheckedRef style = this->style();
    auto writingMode = m_containingBlock.writingMode;
    auto& minimumBlockSize = style->logicalMinHeight(writingMode);
    auto& maximumBlockSize = style->logicalMaxHeight(writingMode);
    if (auto& blockSize = style->logicalHeight(writingMode); blockSize.isSpecified())
        return clamp(borderBoxSizeFor(blockSize, values.containingBlockSize, values.borderAndPadding), minimumBlockSize, maximumBlockSize, values.containingBlockSize, values.borderAndPadding);
    // With neither inset auto, the automatic size is the stretch-fit size when the alignment stretches (§4.3).
    if (values.insetStart && values.insetEnd && alignmentAppliesStretch(Axis::Block)) {
        auto stretchFitSize = values.containingBlockSize - *values.insetStart - *values.insetEnd - values.marginStart.value_or(0_lu) - values.marginEnd.value_or(0_lu);
        return clamp(stretchFitSize, minimumBlockSize, maximumBlockSize, values.containingBlockSize, values.borderAndPadding);
    }
    return { };
}

std::optional<LayoutUnit> OutOfFlowGeometry::definiteContentBoxBlockSize() const
{
    return definiteBorderBoxBlockSize(axisValues(Axis::Block)).transform([&](auto borderBoxSize) {
        return borderBoxSize - m_blockAxisBorderAndPadding;
    });
}

auto OutOfFlowGeometry::computeReplacedInlineAxisGeometry(LayoutUnit borderBoxSize) const -> AxisGeometry
{
    return computeAxisGeometry(Axis::Inline, axisValues(Axis::Inline), borderBoxSize);
}

auto OutOfFlowGeometry::computeReplacedBlockAxisGeometry(LayoutUnit borderBoxSize) const -> AxisGeometry
{
    return computeAxisGeometry(Axis::Block, axisValues(Axis::Block), borderBoxSize);
}

auto OutOfFlowGeometry::computeBlockAxisGeometry(LayoutUnit intrinsicContentBlockSize) const -> AxisGeometry
{
    CheckedRef style = this->style();
    auto values = axisValues(Axis::Block);
    // Otherwise the automatic size is the fit-content size, which is the content's block size in this axis.
    auto writingMode = m_containingBlock.writingMode;
    auto borderBoxSize = definiteBorderBoxBlockSize(values).value_or(clamp(intrinsicContentBlockSize + values.borderAndPadding, style->logicalMinHeight(writingMode), style->logicalMaxHeight(writingMode), values.containingBlockSize, values.borderAndPadding));
    return computeAxisGeometry(Axis::Block, values, borderBoxSize);
}

}
}
