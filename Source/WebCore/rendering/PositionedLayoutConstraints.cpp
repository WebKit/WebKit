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
#include "PositionedLayoutConstraints.h"

#include "AnchorPositionEvaluator.h"
#include "ContainerNodeInlines.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "PositionArea.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderStyle.h"
#include "RenderTableRow.h"

namespace WebCore {

using namespace CSS::Literals;

PositionedLayoutConstraints::PositionedLayoutConstraints(const RenderBox& renderer, LogicalBoxAxis selfAxis)
    : PositionedLayoutConstraints(renderer, renderer.style(), selfAxis)
{
}

PositionedLayoutConstraints::PositionedLayoutConstraints(const RenderBox& renderer, const RenderStyle& style, LogicalBoxAxis selfAxis)
    : m_renderer(renderer)
    , m_container(downcast<RenderBoxModelObject>(*renderer.container())) // Using containingBlock() would be wrong for relpositioned inlines.
    , m_containingWritingMode(m_container->writingMode())
    , m_writingMode(style.writingMode())
    , m_selfAxis(selfAxis)
    , m_containingAxis(!isOrthogonal() ? selfAxis : oppositeAxis(selfAxis))
    , m_physicalAxis(selfAxis == LogicalBoxAxis::Inline ? m_writingMode.inlineAxis() : m_writingMode.blockAxis())
    , m_style(style)
    , m_alignment(m_containingAxis == LogicalBoxAxis::Inline ? style.justifySelf() : style.alignSelf())
    , m_defaultAnchorBox(needsAnchor() ? Style::AnchorPositionEvaluator::defaultAnchorForBox(renderer) : nullptr)
    , m_marginBefore { 0_css_px }
    , m_marginAfter { 0_css_px }
    , m_insetBefore { 0_css_px }
    , m_insetAfter { 0_css_px }
{
    ASSERT(m_container);

    // Compute basic containing block info.
    auto containingInlineSize = renderer.containingBlockLogicalWidthForPositioned(*m_container, false);
    if (LogicalBoxAxis::Inline == m_containingAxis)
        m_containingRange.set(m_container->borderLogicalLeft(), containingInlineSize);
    else
        m_containingRange.set(m_container->borderBefore(), renderer.containingBlockLogicalHeightForPositioned(*m_container, false));
    m_containingInlineSize = containingInlineSize;
    m_originalContainingRange = m_containingRange;

    // Adjust for scrollable area.
    captureScrollableArea();

    // Adjust for grid-area.
    captureGridArea();

    // Capture the anchor geometry and adjust for position-area.
    captureAnchorGeometry();
}

void PositionedLayoutConstraints::computeInsets()
{
    // Cache insets and margins, etc.
    captureInsets();

    if (m_useStaticPosition)
        computeStaticPosition();

    if (containingCoordsAreFlipped()) {
        // Ideally this check is incorporated into captureInsets() but currently it needs to happen after computeStaticPosition() because containingCoordsAreFlipped() depends on m_useStaticPosition.
        std::swap(m_marginBefore, m_marginAfter);
        std::swap(m_insetBefore, m_insetAfter);
    }

    // Compute the inset-modified containing block.
    m_insetModifiedContainingRange = m_containingRange;
    m_insetModifiedContainingRange.shiftMinEdgeBy(insetBeforeValue());
    m_insetModifiedContainingRange.shiftMaxEdgeBy(-insetAfterValue());
}

bool PositionedLayoutConstraints::needsAnchor() const
{
    return m_style.positionArea() || m_alignment.position() == ItemPosition::AnchorCenter;
}

bool PositionedLayoutConstraints::containingCoordsAreFlipped() const
{
    bool orthogonalOpposing = (m_containingAxis == LogicalBoxAxis::Inline && m_writingMode.isBlockFlipped()) || (m_containingAxis == LogicalBoxAxis::Block && m_containingWritingMode.isBlockFlipped());
    // FIXME: Static position has a confusing implementation. Leaving it alone for now.
    return !m_useStaticPosition && ((isBlockOpposing() && m_containingAxis == LogicalBoxAxis::Block) || (isOrthogonal() && orthogonalOpposing));
}

void PositionedLayoutConstraints::captureInsets()
{
    bool isHorizontal = BoxAxis::Horizontal == m_physicalAxis;

    if (isHorizontal) {
        m_bordersPlusPadding = m_renderer->borderLeft() + m_renderer->paddingLeft() + m_renderer->paddingRight() + m_renderer->borderRight();
        m_useStaticPosition = m_style.left().isAuto() && m_style.right().isAuto() && !m_defaultAnchorBox;
    } else {
        m_bordersPlusPadding = m_renderer->borderTop() + m_renderer->paddingTop() + m_renderer->paddingBottom() + m_renderer->borderBottom();
        m_useStaticPosition = m_style.top().isAuto() && m_style.bottom().isAuto() && !m_defaultAnchorBox;
    }

    if (LogicalBoxAxis::Inline == m_selfAxis) {
        m_marginBefore = isHorizontal ? m_style.marginLeft() : m_style.marginTop();
        m_marginAfter = isHorizontal ? m_style.marginRight() : m_style.marginBottom();
        m_insetBefore = m_style.logicalLeft();
        m_insetAfter = m_style.logicalRight();
    } else {
        m_marginBefore = m_style.marginBefore();
        m_marginAfter = m_style.marginAfter();
        m_insetBefore = m_style.logicalTop();
        m_insetAfter = m_style.logicalBottom();
    }

    if (m_defaultAnchorBox) {
        // If the box uses anchor-center and does have a default anchor box,
        // any auto insets are set to zero.
        if (m_insetBefore.isAuto())
            m_insetBefore = 0_css_px;
        if (m_insetAfter.isAuto())
            m_insetAfter = 0_css_px;
        m_useStaticPosition = false;
    }
}

// MARK: - Adjustments to the containing block.

void PositionedLayoutConstraints::captureScrollableArea()
{
    // FIXME: Extend this logic to other scrollable containing blocks.
    if (!is<RenderView>(m_container) || !m_style.positionArea())
        return;

    auto initialContainingBlock = downcast<RenderBox>(m_container.get());
    for (CheckedPtr child = initialContainingBlock->firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned())
            continue;
        LayoutUnit outerSize = BoxAxis::Vertical == m_physicalAxis
            ? child->height() + std::max(0_lu, child->marginTop() + child->marginBottom())
            : child->width() + std::max(0_lu, child->marginLeft() + child->marginRight());
        if (startIsBefore())
            m_containingRange.floorSizeFromMinEdge(outerSize);
        else
            m_containingRange.floorSizeFromMaxEdge(outerSize);
    }
}

void PositionedLayoutConstraints::captureGridArea()
{
    const CheckedPtr gridContainer = dynamicDowncast<RenderGrid>(m_container.get());
    if (!gridContainer)
        return;

    if (LogicalBoxAxis::Inline == m_containingAxis) {
        m_containingRange = gridContainer->gridAreaRangeForOutOfFlow(m_renderer, Style::GridTrackSizingDirection::Columns);
        m_containingInlineSize = m_containingRange.size();
    } else {
        m_containingRange = gridContainer->gridAreaRangeForOutOfFlow(m_renderer, Style::GridTrackSizingDirection::Rows);
        m_containingInlineSize = gridContainer->gridAreaRangeForOutOfFlow(m_renderer, Style::GridTrackSizingDirection::Columns).size();
    }

    if (!startIsBefore()) {
        auto containerSize = BoxAxis::Horizontal == m_physicalAxis
            ? gridContainer->width() : gridContainer->height();
        m_containingRange.moveTo(containerSize - m_containingRange.max());
    }
}

LayoutRange PositionedLayoutConstraints::extractRange(LayoutRect anchorRect)
{
    LayoutRange anchorRange;
    if (BoxAxis::Horizontal == m_physicalAxis)
        anchorRange.set(anchorRect.x(), anchorRect.width());
    else
        anchorRange.set(anchorRect.y(), anchorRect.height());

    if (m_containingWritingMode.isBlockFlipped() && LogicalBoxAxis::Block == m_containingAxis) {
        // Coordinate fixup for flipped blocks.
        anchorRange.moveTo(m_containingRange.max() - anchorRange.max() + m_container->borderAfter());
    }
    return anchorRange;
}

void PositionedLayoutConstraints::captureAnchorGeometry()
{
    if (!m_defaultAnchorBox)
        return;

    // Store the anchor geometry.
    LayoutRect anchorRect = Style::AnchorPositionEvaluator::computeAnchorRectRelativeToContainingBlock(*m_defaultAnchorBox, *m_container, m_renderer.get());
    m_anchorArea = extractRange(anchorRect);

    // Adjust containing block for position-area.
    if (!m_style.positionArea())
        return;
    m_containingRange = adjustForPositionArea(m_containingRange, m_anchorArea, m_physicalAxis);

    // Margin basis is always against the inline axis.
    if (LogicalBoxAxis::Inline == m_containingAxis) {
        m_containingInlineSize = m_containingRange.size();
        return;
    }
    // Else we're representing the block axis, but need the inline dimensions.
    auto inlineAxis = oppositeAxis(m_physicalAxis);
    LayoutRange inlineContainingBlock(m_container->borderLogicalLeft(), m_containingInlineSize);
    auto inlineAnchorArea = BoxAxis::Horizontal == inlineAxis
        ? LayoutRange { anchorRect.x(), anchorRect.width() }
        : LayoutRange { anchorRect.y(), anchorRect.height() };
    m_containingInlineSize = adjustForPositionArea(inlineContainingBlock, inlineAnchorArea, inlineAxis).size();
}

LayoutRange PositionedLayoutConstraints::adjustForPositionArea(const LayoutRange rangeToAdjust, const LayoutRange anchorArea, const BoxAxis containerAxis)
{
    ASSERT(m_style.positionArea() && m_defaultAnchorBox && needsAnchor());
    ASSERT(anchorArea.size() >= 0);

    auto adjustedRange = rangeToAdjust;
    switch (m_style.positionArea()->coordMatchedTrackForAxis(containerAxis, m_containingWritingMode, m_writingMode)) {
    case PositionAreaTrack::Start:
        adjustedRange.shiftMaxEdgeTo(anchorArea.min());
        adjustedRange.floorSizeFromMaxEdge();
        return adjustedRange;
    case PositionAreaTrack::SpanStart:
        adjustedRange.shiftMaxEdgeTo(anchorArea.max());
        adjustedRange.capMinEdgeTo(anchorArea.min());
        return adjustedRange;
    case PositionAreaTrack::End:
        adjustedRange.shiftMinEdgeTo(anchorArea.max());
        adjustedRange.floorSizeFromMinEdge();
        return adjustedRange;
    case PositionAreaTrack::SpanEnd:
        adjustedRange.shiftMinEdgeTo(anchorArea.min());
        adjustedRange.floorMaxEdgeTo(anchorArea.max());
        return adjustedRange;
    case PositionAreaTrack::Center:
        adjustedRange = anchorArea;
        return adjustedRange;
    case PositionAreaTrack::SpanAll:
        adjustedRange.capMinEdgeTo(anchorArea.min());
        adjustedRange.floorMaxEdgeTo(anchorArea.max());
        return adjustedRange;
    default:
        ASSERT_NOT_REACHED();
        return adjustedRange;
    };
}

// MARK: - Resolving margins and alignment (after sizing).

bool PositionedLayoutConstraints::isEligibleForStaticRangeAlignment(LayoutUnit spaceInStaticRange, LayoutUnit itemSize) const
{

    if (m_containingAxis == LogicalBoxAxis::Inline)
        return false;

    auto* parent = m_renderer->parent();

    if (parent->isRenderBlockFlow())
        return false;

    if (parent->style().isDisplayInlineType())
        return false;

    if (parent->isRenderFlexibleBox())
        return false;

    if (parent->isRenderGrid()) {

        auto& itemStyle = m_renderer->style();
        auto itemResolvedAlignSelf = itemStyle.resolvedAlignSelf(&parent->style(), ItemPosition::Start);
        switch (itemResolvedAlignSelf.position()) {
        case ItemPosition::Center:
        case ItemPosition::FlexEnd:
        case ItemPosition::SelfEnd:
        case ItemPosition::End: {
            if (m_container.get() == parent)
                return false;

            auto& containingBlockStyle = m_container->style();
            if (!containingBlockStyle.writingMode().isHorizontal())
                return false;

            if (!containingBlockStyle.isLeftToRightDirection())
                return false;

            auto& parentStyle = parent->style();
            if (!parentStyle.writingMode().isHorizontal())
                return false;

            if (!parentStyle.isLeftToRightDirection())
                return false;

            if (!itemStyle.writingMode().isHorizontal())
                return false;

            if (!itemStyle.isLeftToRightDirection())
                return false;

            if (itemResolvedAlignSelf.positionType() != ItemPositionType::NonLegacy)
                return false;

            if (itemResolvedAlignSelf.overflow() != OverflowAlignment::Default)
                return false;
            return spaceInStaticRange >= itemSize;
        }
        default:
            return false;
        }
    }

    // We can hit this in certain pieces of content (e.g. see mathml/crashtests/fixed-pos-children.html),
    // but the spec has no definition for a static position rectangle.
    return false;

}

void PositionedLayoutConstraints::resolvePosition(RenderBox::LogicalExtentComputedValues& computedValues) const
{
    // Static position should have resolved one of our insets by now.
    ASSERT(!(m_insetBefore.isAuto() && m_insetAfter.isAuto()));

    auto usedMarginBefore = marginBeforeValue();
    auto usedMarginAfter = marginAfterValue();

    auto remainingSpace = insetModifiedContainingSize()
        - usedMarginBefore
        - computedValues.m_extent
        - usedMarginAfter;

    bool hasAutoBeforeInset = m_insetBefore.isAuto();
    bool hasAutoAfterInset = m_insetAfter.isAuto();
    bool hasAutoBeforeMargin = m_marginBefore.isAuto();
    bool hasAutoAfterMargin = m_marginAfter.isAuto();

    auto distributeSpaceToAutoMargins = [&] {
        ASSERT(!hasAutoBeforeInset && !hasAutoAfterInset && (hasAutoBeforeMargin || hasAutoAfterMargin));

        // Calculate auto margins.
        if (hasAutoBeforeMargin && hasAutoAfterMargin) {
            // Distribute usable space to both margins equally.
            auto usableRemainingSpace = (LogicalBoxAxis::Inline == m_containingAxis)
                ? std::max(0_lu, remainingSpace) : remainingSpace;
            usedMarginBefore = usedMarginAfter = usableRemainingSpace / 2;

            // Distribute unused space to the end side.
            auto unusedSpace = remainingSpace - (usedMarginBefore + usedMarginAfter);
            if (startIsBefore())
                usedMarginAfter += unusedSpace;
            else
                usedMarginBefore += unusedSpace;
        } else if (hasAutoBeforeMargin)
            usedMarginBefore = remainingSpace;
        else if (hasAutoAfterMargin)
            usedMarginAfter = remainingSpace;
    };

    if (!hasAutoBeforeInset && !hasAutoAfterInset && (hasAutoBeforeMargin || hasAutoAfterMargin))
        distributeSpaceToAutoMargins();

    auto alignmentShift = [&] -> LayoutUnit {
        // Align into remaining space.
        auto itemMarginBoxSize = computedValues.m_extent + usedMarginBefore + usedMarginAfter;
        if (!hasAutoBeforeInset && !hasAutoAfterInset && !hasAutoBeforeMargin && !hasAutoAfterMargin && remainingSpace)
            return resolveAlignmentShift(remainingSpace, itemMarginBoxSize);

        if (m_useStaticPosition) {
            auto spaceInStaticRange = [&] -> LayoutUnit {
                if (m_containingAxis == LogicalBoxAxis::Inline)
                    return { };

                auto* parent = m_renderer->parent();
                if (auto* renderGrid = dynamicDowncast<RenderGrid>(parent))
                    return renderGrid->contentBoxLogicalHeight();
                return { };
            }();

            if (isEligibleForStaticRangeAlignment(spaceInStaticRange, itemMarginBoxSize)) {
#if ASSERT_ENABLED
                m_isEligibleForStaticRangeAlignment = true;
#endif
                return resolveAlignmentShift(spaceInStaticRange - itemMarginBoxSize, itemMarginBoxSize);
            }
        }

        if (hasAutoBeforeInset)
            return remainingSpace;

        return { };
    };

    // See CSS2 § 10.3.7-8 and 10.6.4-5.
    auto position = m_insetModifiedContainingRange.min() + usedMarginBefore + alignmentShift();

    computedValues.m_position = position;
    if (LogicalBoxAxis::Inline == m_selfAxis) {
        if (m_writingMode.isLogicalLeftInlineStart() == !containingCoordsAreFlipped()) {
            computedValues.m_margins.m_start = usedMarginBefore;
            computedValues.m_margins.m_end = usedMarginAfter;
        } else {
            computedValues.m_margins.m_start = usedMarginAfter;
            computedValues.m_margins.m_end = usedMarginBefore;
        }
    } else if (containingCoordsAreFlipped()) {
        computedValues.m_margins.m_before = usedMarginAfter;
        computedValues.m_margins.m_after = usedMarginBefore;
    } else {
        computedValues.m_margins.m_before = usedMarginBefore;
        computedValues.m_margins.m_after = usedMarginAfter;
    }
}

LayoutUnit PositionedLayoutConstraints::resolveAlignmentShift(LayoutUnit unusedSpace, LayoutUnit itemSize) const
{
    bool startIsBefore = this->startIsBefore();
    bool isOverflowing = unusedSpace < 0_lu;
    if (isOverflowing && OverflowAlignment::Safe == m_alignment.overflow())
        return startIsBefore ? 0_lu : unusedSpace;

    ItemPosition resolvedAlignment = resolveAlignmentValue();
    ASSERT(ItemPosition::Auto != resolvedAlignment);

    LayoutUnit shift;
    if (ItemPosition::AnchorCenter == resolvedAlignment) {
        auto anchorCenterPosition = m_anchorArea.min() + (m_anchorArea.size() - itemSize) / 2;
        shift = anchorCenterPosition - m_insetModifiedContainingRange.min();
        if (m_alignment.overflow() == OverflowAlignment::Safe) {
            if (startIsBefore) {
                if (shift < 0)
                    shift = 0;
            } else {
                if (shift > unusedSpace)
                    shift = unusedSpace;
            }
        }
        if (!isOverflowing && OverflowAlignment::Default == m_alignment.overflow()) {
            // Avoid introducing overflow of the IMCB.
            if (shift < 0)
                shift = 0;
            else if (shift > unusedSpace)
                shift = unusedSpace;
        }
    } else {
        auto alignmentSpace = StyleSelfAlignmentData::adjustmentFromStartEdge(unusedSpace, resolvedAlignment, m_containingAxis, m_containingWritingMode, m_writingMode);
        shift = startIsBefore ? alignmentSpace : unusedSpace - alignmentSpace;
    }

    if (isOverflowing && ItemPosition::Normal != resolvedAlignment
        && OverflowAlignment::Default == m_alignment.overflow()) {
        // Allow overflow, but try to stay within the containing block.
        // See https://www.w3.org/TR/css-align-3/#auto-safety-position
        auto spaceAfter = std::max(0_lu, m_originalContainingRange.max() - m_insetModifiedContainingRange.max());
        auto spaceBefore = std::max(0_lu, m_insetModifiedContainingRange.min() - m_originalContainingRange.min());

        if (startIsBefore) {
            // Avoid overflow on the end side
            spaceAfter += (unusedSpace - shift);
            if (spaceAfter < 0)
                shift += spaceAfter;
            // Disallow overflow on the start side.
            spaceBefore += shift;
            if (spaceBefore < 0)
                shift -= spaceBefore;
        } else {
            // Avoid overflow on the end side
            spaceBefore += shift;
            if (spaceBefore < 0)
                shift -= spaceBefore;
            // Disallow overflow on the start side.
            spaceAfter += (unusedSpace - shift);
            if (spaceAfter < 0)
                shift += spaceAfter;
        }

    }
    return shift;
}

ItemPosition PositionedLayoutConstraints::resolveAlignmentValue() const
{
    auto alignmentPosition = [&] {
        auto itemPosition = m_alignment.position();
        if (ItemPosition::Auto == itemPosition) {

            if (m_useStaticPosition) {
#if ASSERT_ENABLED
                ASSERT(m_isEligibleForStaticRangeAlignment);
#endif
                auto* parentStyle = m_renderer->parentStyle();
                return m_style.resolvedAlignSelf(parentStyle, ItemPosition::Start).position();
            }
            return ItemPosition::Normal;
        }
        return itemPosition;
    }();

    if (m_style.positionArea() && (ItemPosition::Normal == alignmentPosition || ItemPosition::Dialog == alignmentPosition))
        alignmentPosition = m_style.positionArea()->defaultAlignmentForAxis(m_physicalAxis, m_containingWritingMode, m_writingMode);

    if (!m_defaultAnchorBox && alignmentPosition == ItemPosition::AnchorCenter)
        return ItemPosition::Center;
    return alignmentPosition;
}

bool PositionedLayoutConstraints::alignmentAppliesStretch(ItemPosition normalAlignment) const
{
    auto alignmentPosition = m_alignment.position();
    if (!m_style.positionArea() && (ItemPosition::Auto == alignmentPosition || ItemPosition::Normal == alignmentPosition))
        alignmentPosition = normalAlignment;
    return ItemPosition::Stretch == alignmentPosition;
}

bool PositionedLayoutConstraints::needsGridAreaAdjustmentBeforeStaticPositioning() const
{
    if (m_containingAxis == LogicalBoxAxis::Block)
        return true;

    auto* parent = m_renderer->parent();
    // When the grid container is a parent we do not take the normal static positioning path.
    if (!m_container->isRenderGrid() || parent == m_container)
        return false;

    auto parentWritingMode = parent->writingMode();
    if (parentWritingMode.isLogicalLeftInlineStart() && !parentWritingMode.isOrthogonal(m_writingMode))
        return false;

    return true;
}

// MARK: - Static Position Computation

void PositionedLayoutConstraints::computeStaticPosition()
{
    ASSERT(m_useStaticPosition);

    if (is<RenderGrid>(m_container)) {
        // Grid Containers have special behavior, see https://www.w3.org/TR/css-grid/#abspos
        if (m_container.get() == m_renderer->parent()) {
            // Fake the static layout right here so it integrates with grid-area properly.
            m_useStaticPosition = false; // Avoid the static position code path.
            m_insetBefore = 0_css_px;
            m_insetAfter = 0_css_px;

            if (ItemPosition::Auto == m_alignment.position()) {
                if (LogicalBoxAxis::Inline == m_containingAxis) {
                    auto justifyItems = m_container->style().justifyItems();
                    if (ItemPosition::Legacy != justifyItems.position())
                        m_alignment = justifyItems;
                } else
                    m_alignment = m_container->style().alignItems();
            }
            if (ItemPosition::Auto == m_alignment.position() || ItemPosition::Normal == m_alignment.position())
                m_alignment.setPosition(ItemPosition::Start);
            if (OverflowAlignment::Default == m_alignment.overflow())
                m_alignment.setOverflow(OverflowAlignment::Unsafe);

            // Unclear if this is spec-compliant, but it is the current interop behavior.
            if (m_marginBefore.isAuto())
                m_marginBefore = 0_css_px;
            if (m_marginAfter.isAuto())
                m_marginAfter = 0_css_px;
            return;
        }
        // Rewind grid-area adjustments and fall through to the existing static position code.
        if (needsGridAreaAdjustmentBeforeStaticPositioning())
            m_containingRange.moveTo(m_originalContainingRange.min());
    }

    if (m_selfAxis == LogicalBoxAxis::Inline)
        computeInlineStaticDistance();
    else
        computeBlockStaticDistance();
}

static LayoutPoint positionInContainer(const RenderBox& container, const RenderBox& child, LayoutPoint positionInChild)
{
    auto containerWritingMode = container.writingMode();
    auto childWritingMode = child.writingMode();
    auto childInFlowOffset = child.writingMode().isHorizontal() ? child.offsetForInFlowPosition() : child.offsetForInFlowPosition().transposedSize();
    auto childLogicalLeft = childInFlowOffset.width() + child.logicalLeft();
    auto childLogicalTop = childInFlowOffset.height() + child.logicalTop();

    if (containerWritingMode.isOrthogonal(childWritingMode)) {
        auto topLeft = LayoutPoint { childLogicalLeft + positionInChild.x(), childLogicalTop + positionInChild.y() };
        if (childWritingMode.isBlockFlipped())
            topLeft.setY(childLogicalTop + child.logicalHeight() - positionInChild.y());
        if (containerWritingMode.isBlockFlipped())
            topLeft.setX(childLogicalLeft + child.logicalWidth() - positionInChild.x());
        return topLeft.transposedPoint();
    }

    if (containerWritingMode.isBlockOpposing(childWritingMode))
        return { childLogicalLeft + positionInChild.x(), childLogicalTop + child.logicalHeight() - positionInChild.y() };

    return { childLogicalLeft + positionInChild.x(), childLogicalTop + positionInChild.y() };
}

static LayoutPoint staticDistance(const RenderBoxModelObject& container, const RenderBox& outOfFlowBox)
{
    // Static position is relative to the candidate box's parent (it is computed during normal in-flow layout as if the candidate box was in-flow)
    // 1. traverse the ancestor chain and convert static position relative to each container all the way up to the containing block
    // 2. adjust the final static position with the containing block's border
    // 3. pick x or y depending on what direction we are actually interested in (note that it's always the block directon from the
    //    candidate box's point of view but it could very well be the inline distance from the containing block's point of view.)

    auto initialStaticPosition = [&] {
        // Static position is already in the coordinate system of the container (minus the flip in inline direction).
        auto staticPosition = LayoutPoint { outOfFlowBox.layer()->staticInlinePosition(), outOfFlowBox.layer()->staticBlockPosition() };
        // We are relative to a RenderBox ancestor unless the containing block itself is an inline box.
        auto* staticPositionContainingBlock = outOfFlowBox.parent();
        for (; staticPositionContainingBlock && !is<RenderBox>(staticPositionContainingBlock) && staticPositionContainingBlock != &container; staticPositionContainingBlock = staticPositionContainingBlock->container()) { }
        if (CheckedPtr renderBox = dynamicDowncast<RenderBox>(staticPositionContainingBlock); renderBox && renderBox->writingMode().isInlineFlipped())
            staticPosition.setX(renderBox->logicalWidth() - staticPosition.x());
        return staticPosition;
    };

    auto staticPosition = LayoutPoint { };
    auto* child = &outOfFlowBox;
    auto hasSeenNonInlineBoxContainer = false;
    for (auto* ancestorContainer = child->parent(); ancestorContainer && ancestorContainer != &container; ancestorContainer = ancestorContainer->container()) {
        CheckedPtr containerBox = dynamicDowncast<RenderBox>(*ancestorContainer);
        if (!containerBox || is<RenderTableRow>(*containerBox))
            continue;

        staticPosition = child == &outOfFlowBox ? initialStaticPosition() : positionInContainer(*containerBox, *child, staticPosition);
        child = containerBox.get();
        hasSeenNonInlineBoxContainer = true;
    }

    if (!hasSeenNonInlineBoxContainer && is<RenderInline>(container)) {
        // This is a simple case of when the containing block is formed by a positioned inline box with no block boxes in-between (e.g <span style="position: relative">)
        return initialStaticPosition();
    }

    auto* containingBlock = dynamicDowncast<RenderBox>(container);
    // m_insetBefore is expected to be relative to the padding box (while staticPosition is relative to the border box).
    auto containingBlockBorderSize = LayoutSize { };
    if (containingBlock)
        containingBlockBorderSize = containingBlock->writingMode().isInlineFlipped() ? LayoutSize(containingBlock->borderEnd(), containingBlock->borderBefore()) : LayoutSize(containingBlock->borderStart(), containingBlock->borderBefore());
    else {
        containingBlock = child->containingBlock();
        ASSERT(containingBlock);
        // Note that we don't take the border here as we passed the real containing block.
    }

    staticPosition = child == &outOfFlowBox ? initialStaticPosition() : positionInContainer(*containingBlock, *child, staticPosition);
    staticPosition -= containingBlockBorderSize;
    return staticPosition;
}

void PositionedLayoutConstraints::computeInlineStaticDistance()
{
    // Note that at this point staticPosition is relative to the containing block (x is inline direction, y is block direction)
    // which may not match with the box's slef writing mode.
    auto parentWritingMode = m_renderer->parent()->writingMode();
    auto isParentOrthogonal = parentWritingMode.isOrthogonal(selfWritingMode());
    auto shouldUseInsetAfter = !isParentOrthogonal && parentWritingMode.isInlineFlipped(); // This is what trunk has.
    if (shouldUseInsetAfter && m_containingWritingMode.isOrthogonal(parentWritingMode) && m_containingWritingMode.isBlockFlipped()) {
        // FIXME: Figure out why.
        shouldUseInsetAfter = false;
    }

    auto staticPosition = staticDistance(*m_container, m_renderer.get());
    auto staticDistance = !isOrthogonal() ? staticPosition.x() : staticPosition.y();
    if (CheckedPtr gridContainer = dynamicDowncast<RenderGrid>(m_container.get())) {
        // Special grid handling: move static distance back to relative to border box and adjust with our offset.
        auto containingBlockBorderSize = !isOrthogonal() ? (gridContainer->writingMode().isInlineFlipped() ? gridContainer->borderEnd() : gridContainer->borderStart()) : gridContainer->borderBefore();
        staticDistance += containingBlockBorderSize;
        staticDistance -= m_containingRange.min();
    }

    if (shouldUseInsetAfter) {
        m_insetAfter = Style::InsetEdge::Fixed { containingSize() - staticDistance };
        return;
    }
    m_insetBefore = Style::InsetEdge::Fixed { staticDistance };
}

void PositionedLayoutConstraints::computeBlockStaticDistance()
{
    // Note that at this point staticPosition is relative to the containing block (x is inline direction, y is block direction)
    // which may not match with the box's slef writing mode.
    auto staticPosition = staticDistance(*m_container, m_renderer.get());
    m_insetBefore = Style::InsetEdge::Fixed { !isOrthogonal() ? staticPosition.y() : staticPosition.x() };
}

static bool shouldInlineStaticDistanceAdjustedWithBoxHeight(WritingMode containinigBlockWritingMode, WritingMode parentWritingMode, WritingMode outOfFlowBoxWritingMode)
{
    if (!containinigBlockWritingMode.isOrthogonal(parentWritingMode))
        return false;

    if (parentWritingMode.isOrthogonal(outOfFlowBoxWritingMode))
        return parentWritingMode.isBlockFlipped();

    return containinigBlockWritingMode.isBlockFlipped() && !parentWritingMode.isInlineFlipped();
}

void PositionedLayoutConstraints::fixupLogicalLeftPosition(RenderBox::LogicalExtentComputedValues& computedValues) const
{
    if (m_useStaticPosition) {
        if (m_container.get() != m_renderer->parent() && shouldInlineStaticDistanceAdjustedWithBoxHeight(m_containingWritingMode, m_renderer->parent()->writingMode(), selfWritingMode()))
            computedValues.m_position -= computedValues.m_extent;
        return;
    }

    if (m_writingMode.isHorizontal()) {
        CheckedPtr containingBox = dynamicDowncast<RenderBox>(container());
        if (containingBox && containingBox->shouldPlaceVerticalScrollbarOnLeft())
            computedValues.m_position += containingBox->verticalScrollbarWidth();
    }

    // FIXME: This hack is needed to calculate the logical left position for a 'rtl' relatively
    // positioned, inline because right now, it is using the logical left position
    // of the first line box when really it should use the last line box. When
    // this is fixed elsewhere, this adjustment should be removed.

    CheckedPtr renderInline = dynamicDowncast<RenderInline>(container());
    if (!renderInline || m_containingWritingMode.isLogicalLeftInlineStart())
        return;

    auto firstInlineBox = InlineIterator::lineLeftmostInlineBoxFor(*renderInline);
    if (!firstInlineBox)
        return;

    auto lastInlineBox = [&] {
        auto inlineBox = firstInlineBox;
        for (; inlineBox->nextInlineBoxLineRightward(); inlineBox.traverseInlineBoxLineRightward()) { }
        return inlineBox;
    }();
    if (firstInlineBox == lastInlineBox)
        return;

    auto lastInlineBoxPaddingBoxVisualRight = lastInlineBox->logicalLeftIgnoringInlineDirection() + renderInline->borderLogicalLeft();
    // FIXME: This does not work with decoration break clone.
    auto firstInlineBoxPaddingBoxVisualRight = firstInlineBox->logicalLeftIgnoringInlineDirection();
    auto adjustment = lastInlineBoxPaddingBoxVisualRight - firstInlineBoxPaddingBoxVisualRight;
    computedValues.m_position += adjustment - m_containingRange.min();
}

// FIXME: Let's move this over to RenderBoxModelObject and collapse some of the logic here.
static bool shouldBlockStaticDistanceAdjustedWithBoxHeight(const RenderBoxModelObject& containingBlock, const RenderElement& parent, WritingMode outOfFlowBoxWritingMode)
{
    // This is where we check if the final static position needs to be adjusted with the height of the out-of-flow box.
    // In ::computeBlockStaticDistance we convert the static position relative to the containing block but in some cases
    // this final static position still points to the wrong side of the box (i.e. at computeBlockStaticDistance we don't know yet the height
    // which may contribute to the logical top position. see details below.)

    auto parentWritingMode = parent.writingMode();
    auto containinigBlockWritingMode = containingBlock.writingMode();

    if (containinigBlockWritingMode.blockDirection() == parentWritingMode.blockDirection() && parentWritingMode.blockDirection() == outOfFlowBoxWritingMode.blockDirection())
        return false;

    auto isParentInlineFlipped = parentWritingMode.isInlineFlipped();
    if (containinigBlockWritingMode.isOrthogonal(outOfFlowBoxWritingMode)) {
        if (&containingBlock == &parent) {
            // <div id=containinigBlock class=rtl>
            //   <div id=outOfFlowBox class=ltr>
            return isParentInlineFlipped;
        }

        if (!containinigBlockWritingMode.isOrthogonal(parentWritingMode))
            return isParentInlineFlipped;

        return parentWritingMode.isBlockFlipped();
    }

    ASSERT(containinigBlockWritingMode.blockDirection() == outOfFlowBoxWritingMode.blockDirection() || containinigBlockWritingMode.isBlockOpposing(outOfFlowBoxWritingMode));
    if (!parentWritingMode.isOrthogonal(containinigBlockWritingMode)) {
        // inline direction does not matter as all participants are on the same axis.
        if (containinigBlockWritingMode.blockDirection() == outOfFlowBoxWritingMode.blockDirection()) {
            // <div id=containinigBlock class=vrl>
            //   <div id=parent class=vlr>
            //     <div id=outOfFlowBox class=vrl>
            return true;
        }

        // <div id=containinigBlock class=vlr>
        //   <div id=parent class=vrl>
        //     <div id=outOfFlowBox class=vrl>
        return parentWritingMode.isBlockOpposing(containinigBlockWritingMode);
    }

    // Orhogonal parent.
    // <div id=containinigBlock class=vrl>
    //   <div id=parent class=htb>
    //     <div id=outOfFlowBox class=vlr>
    return containinigBlockWritingMode.isBlockFlipped() != isParentInlineFlipped;
}

void PositionedLayoutConstraints::adjustLogicalTopWithLogicalHeightIfNeeded(RenderBox::LogicalExtentComputedValues& computedValues) const
{
    if (!m_useStaticPosition || m_selfAxis != LogicalBoxAxis::Block)
        return;
    if (shouldBlockStaticDistanceAdjustedWithBoxHeight(*m_container, *m_renderer->parent(), m_writingMode))
        computedValues.m_position -= computedValues.m_extent;
}

}
