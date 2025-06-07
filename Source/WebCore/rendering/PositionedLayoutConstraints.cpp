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

static bool shouldFlipStaticPositionInParent(const RenderBox& outOfFlowBox, const RenderBoxModelObject& containerBlock)
{
    ASSERT(outOfFlowBox.isOutOfFlowPositioned());

    auto* parent = outOfFlowBox.parent();
    if (!parent || parent == &containerBlock || !is<RenderBlock>(*parent))
        return false;
    if (is<RenderGrid>(parent)) {
        // FIXME: Out-of-flow grid item's static position computation is non-existent and enabling proper flipping
        // without implementing the logic in grid layout makes us fail a couple of WPT tests -we pass them now accidentally.
        return false;
    }
    // FIXME: While this ensures flipping when parent is a writing root, computeBlockStaticDistance still does not
    // properly flip when the parent itself is not a writing root but an ancestor between this parent and out-of-flow's containing block.
    return parent->writingMode().isBlockFlipped() && parent->isWritingModeRoot();
}

PositionedLayoutConstraints::PositionedLayoutConstraints(const RenderBox& renderer, const RenderStyle& style, LogicalBoxAxis selfAxis)
    : m_container(downcast<RenderBoxModelObject>(*renderer.container())) // Using containingBlock() would be wrong for relpositioned inlines.
    , m_containingWritingMode(m_container->writingMode())
    , m_writingMode(style.writingMode())
    , m_physicalAxis(selfAxis == LogicalBoxAxis::Inline ? m_writingMode.inlineAxis() : m_writingMode.blockAxis())
    , m_containingAxis(!isOrthogonal() ? selfAxis : oppositeAxis(selfAxis))
    , m_style(style)
    , m_alignment(m_containingAxis == LogicalBoxAxis::Inline ? style.justifySelf() : style.alignSelf())
    , m_defaultAnchorBox(needsAnchor() ? Style::AnchorPositionEvaluator::defaultAnchorForBox(renderer) : nullptr)
    , m_marginBefore { 0_css_px }
    , m_marginAfter { 0_css_px }
    , m_insetBefore { 0_css_px }
    , m_insetAfter { 0_css_px }
{

    // Compute basic containing block info.
    auto containingWidth = renderer.containingBlockLogicalWidthForPositioned(*m_container, false);
    if (LogicalBoxAxis::Inline == m_containingAxis)
        m_containingRange.set(m_container->borderLogicalLeft(), containingWidth);
    else
        m_containingRange.set(m_container->borderBefore(), renderer.containingBlockLogicalHeightForPositioned(*m_container, false));
    m_marginPercentageBasis = containingWidth;
    m_originalContainingRange = m_containingRange;

    // Adjust for grid-area.
    captureGridArea(renderer);

    // Capture the anchor geometry and adjust for position-area.
    captureAnchorGeometry(renderer);

    // Cache insets and margins, etc.
    captureInsets(renderer, selfAxis);

    if (m_useStaticPosition)
        computeStaticPosition(renderer, selfAxis);

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

PositionedLayoutConstraints::PositionedLayoutConstraints(const RenderBox& renderer, LogicalBoxAxis selfAxis)
    : PositionedLayoutConstraints(renderer, renderer.style(), selfAxis)
{
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

void PositionedLayoutConstraints::captureInsets(const RenderBox& renderer, const LogicalBoxAxis selfAxis)
{
    bool isHorizontal = BoxAxis::Horizontal == m_physicalAxis;

    if (isHorizontal) {
        m_bordersPlusPadding = renderer.borderLeft() + renderer.paddingLeft() + renderer.paddingRight() + renderer.borderRight();
        m_useStaticPosition = m_style.left().isAuto() && m_style.right().isAuto() && !m_defaultAnchorBox;
    } else {
        m_bordersPlusPadding = renderer.borderTop() + renderer.paddingTop() + renderer.paddingBottom() + renderer.borderBottom();
        m_useStaticPosition = m_style.top().isAuto() && m_style.bottom().isAuto() && !m_defaultAnchorBox;
    }

    if (LogicalBoxAxis::Inline == selfAxis) {
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

void PositionedLayoutConstraints::captureGridArea(const RenderBox& renderer)
{
    const CheckedPtr gridContainer = dynamicDowncast<RenderGrid>(m_container.get());
    if (!gridContainer)
        return;

    if (LogicalBoxAxis::Inline == m_containingAxis) {
        auto range = gridContainer->gridAreaColumnRangeForOutOfFlow(renderer);
        if (!range)
            return;
        m_containingRange = *range;
        m_marginPercentageBasis = range->size();
    } else {
        auto range = gridContainer->gridAreaRowRangeForOutOfFlow(renderer);
        if (range)
            m_containingRange = *range;
        auto columnRange = gridContainer->gridAreaColumnRangeForOutOfFlow(renderer);
        if (columnRange)
            m_marginPercentageBasis = columnRange->size();
    }

    if (!startIsBefore()) {
        auto containerSize = BoxAxis::Horizontal == m_physicalAxis
            ? gridContainer->width() : gridContainer->height();
        m_containingRange.moveTo(containerSize - m_containingRange.max());
    }
}

void PositionedLayoutConstraints::captureAnchorGeometry(const RenderBox& renderer)
{
    if (!m_defaultAnchorBox)
        return;

    // Store the anchor geometry.
    LayoutRect anchorRect = Style::AnchorPositionEvaluator::computeAnchorRectRelativeToContainingBlock(*m_defaultAnchorBox, *renderer.containingBlock());
    if (BoxAxis::Horizontal == m_physicalAxis)
        m_anchorArea.set(anchorRect.x(), anchorRect.width());
    else
        m_anchorArea.set(anchorRect.y(), anchorRect.height());
    if (m_containingWritingMode.isBlockFlipped() && LogicalBoxAxis::Block == m_containingAxis) {
        // Coordinate fixup for flipped blocks.
        m_anchorArea.moveTo(m_containingRange.max() - m_anchorArea.max() + m_container->borderAfter());
    }

    // Adjust containing block for position-area.
    if (!m_style.positionArea())
        return;
    m_containingRange = adjustForPositionArea(m_containingRange, m_anchorArea, m_physicalAxis);

    // Margin basis is always against the inline axis.
    if (LogicalBoxAxis::Inline == m_containingAxis) {
        m_marginPercentageBasis = m_containingRange.size();
        return;
    }
    // Else we're representing the block axis, but need the inline dimensions.
    auto inlineAxis = oppositeAxis(m_physicalAxis);
    LayoutRange inlineContainingBlock(m_container->borderLogicalLeft(), m_marginPercentageBasis);
    auto inlineAnchorArea = BoxAxis::Horizontal == inlineAxis
        ? LayoutRange { anchorRect.x(), anchorRect.width() }
        : LayoutRange { anchorRect.y(), anchorRect.height() };
    m_marginPercentageBasis = adjustForPositionArea(inlineContainingBlock, inlineAnchorArea, inlineAxis).size();
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

void PositionedLayoutConstraints::resolvePosition(RenderBox::LogicalExtentComputedValues& computedValues) const
{
    // Static position should have resolved one of our insets by now.
    ASSERT(!(m_insetBefore.isAuto() && m_insetAfter.isAuto()));

    auto position = m_insetModifiedContainingRange.min();
    auto usedMarginBefore = marginBeforeValue();
    auto usedMarginAfter = marginAfterValue();

    auto remainingSpace = insetModifiedContainingSize()
        - usedMarginBefore
        - computedValues.m_extent
        - usedMarginAfter;

    // See CSS2 § 10.3.7-8 and 10.6.4-5.
    if (!m_insetBefore.isAuto() && !m_insetAfter.isAuto()) {
        // Calculate auto margins.
        if (m_marginBefore.isAuto() && m_marginAfter.isAuto()) {
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
        } else if (m_marginBefore.isAuto())
            usedMarginBefore = remainingSpace;
        else if (m_marginAfter.isAuto())
            usedMarginAfter = remainingSpace;
        else if (remainingSpace) {
            // Align into remaining space.
            position += resolveAlignmentShift(remainingSpace,
                computedValues.m_extent + usedMarginBefore + usedMarginAfter);
        }
    } else if (m_insetBefore.isAuto())
        position += remainingSpace;
    position += usedMarginBefore;

    computedValues.m_position = position;
    LogicalBoxAxis selfAxis = isOrthogonal() ? oppositeAxis(m_containingAxis) : m_containingAxis;
    if (LogicalBoxAxis::Inline == selfAxis) {
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
    if (unusedSpace < 0_lu && OverflowAlignment::Safe == m_alignment.overflow())
        return startIsBefore ? 0_lu : unusedSpace;

    ItemPosition resolvedAlignment = resolveAlignmentValue();
    if (ItemPosition::Auto == resolvedAlignment)
        resolvedAlignment = ItemPosition::Normal;

    LayoutUnit shift;
    if (ItemPosition::AnchorCenter == resolvedAlignment) {
        auto anchorCenterPosition = m_anchorArea.min() + (m_anchorArea.size() - itemSize) / 2;
        shift = anchorCenterPosition - m_insetModifiedContainingRange.min();
    } else {
        auto alignmentSpace = StyleSelfAlignmentData::adjustmentFromStartEdge(unusedSpace, resolvedAlignment, m_containingAxis, m_containingWritingMode, m_writingMode);
        shift = startIsBefore ? alignmentSpace : unusedSpace - alignmentSpace;
    }

    if (unusedSpace < 0 && ItemPosition::Normal != resolvedAlignment
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
    auto alignmentPosition = m_alignment.position();
    if (ItemPosition::Auto == alignmentPosition)
        alignmentPosition = ItemPosition::Normal;

    if (m_style.positionArea() && ItemPosition::Normal == alignmentPosition)
        return m_style.positionArea()->defaultAlignmentForAxis(m_physicalAxis, m_containingWritingMode, m_writingMode);
    return alignmentPosition;
}

bool PositionedLayoutConstraints::alignmentAppliesStretch(ItemPosition normalAlignment) const
{
    auto alignmentPosition = m_alignment.position();
    if (!m_style.positionArea() && (ItemPosition::Auto == alignmentPosition || ItemPosition::Normal == alignmentPosition))
        alignmentPosition = normalAlignment;
    return ItemPosition::Stretch == alignmentPosition;
}

// MARK: - Static Position Computation

void PositionedLayoutConstraints::computeStaticPosition(const RenderBox& renderer, LogicalBoxAxis selfAxis)
{
    ASSERT(m_useStaticPosition);

    if (is<RenderGrid>(m_container)) {
        // Grid Containers have special behavior, see https://www.w3.org/TR/css-grid/#abspos
        if (m_container.get() == renderer.parent()) {
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
        m_containingRange.moveTo(m_originalContainingRange.min());
    }

    if (selfAxis == LogicalBoxAxis::Inline)
        computeInlineStaticDistance(renderer);
    else
        computeBlockStaticDistance(renderer);
}

void PositionedLayoutConstraints::computeInlineStaticDistance(const RenderBox& renderer)
{
    auto* parent = renderer.parent();
    auto parentWritingMode = parent->writingMode();

    // For orthogonal flows we don't care whether the parent is LTR or RTL because it does not affect the position in our inline axis.
    bool haveOrthogonalWritingModes = parentWritingMode.isOrthogonal(m_writingMode);
    if (parentWritingMode.isLogicalLeftInlineStart() || haveOrthogonalWritingModes) {
        LayoutUnit staticPosition = haveOrthogonalWritingModes
            ? renderer.layer()->staticBlockPosition() - m_container->borderBefore()
            : renderer.layer()->staticInlinePosition() - m_container->borderLogicalLeft();
        for (auto* current = parent; current && current != m_container.get(); current = current->container()) {
            CheckedPtr renderBox = dynamicDowncast<RenderBox>(*current);
            if (!renderBox)
                continue;
            staticPosition += haveOrthogonalWritingModes ? renderBox->logicalTop() : renderBox->logicalLeft();
            if (renderBox->isInFlowPositioned())
                staticPosition += renderBox->isHorizontalWritingMode() ? renderBox->offsetForInFlowPosition().width() : renderBox->offsetForInFlowPosition().height();
        }
        m_insetBefore = Style::InsetEdge::Fixed { staticPosition };
    } else {
        ASSERT(!haveOrthogonalWritingModes);
        LayoutUnit staticPosition = renderer.layer()->staticInlinePosition() + containingSize() + m_container->borderLogicalLeft();
        auto& enclosingBox = parent->enclosingBox();
        if (&enclosingBox != m_container.get() && m_container->isDescendantOf(&enclosingBox)) {
            m_insetAfter = Style::InsetEdge::Fixed { staticPosition };
            return;
        }
        staticPosition -= enclosingBox.logicalWidth();
        for (const RenderElement* current = &enclosingBox; current; current = current->container()) {
            CheckedPtr renderBox = dynamicDowncast<RenderBox>(*current);
            if (!renderBox)
                continue;

            if (current != m_container.get()) {
                staticPosition -= renderBox->logicalLeft();
                if (renderBox->isInFlowPositioned())
                    staticPosition -= renderBox->isHorizontalWritingMode() ? renderBox->offsetForInFlowPosition().width() : renderBox->offsetForInFlowPosition().height();
            }
            if (current == m_container.get())
                break;
        }
        m_insetAfter = Style::InsetEdge::Fixed { staticPosition };
    }
}

void PositionedLayoutConstraints::computeBlockStaticDistance(const RenderBox& renderer)
{
    auto* parent = renderer.parent();
    bool haveOrthogonalWritingModes = parent->writingMode().isOrthogonal(m_writingMode);
    // The static positions from the child's layer are relative to the container block's coordinate space (which is determined
    // by the writing mode and text direction), meaning that for orthogonal flows the logical top of the child (which depends on
    // the child's writing mode) is retrieved from the static inline position instead of the static block position.
    auto staticLogicalTop = haveOrthogonalWritingModes ? renderer.layer()->staticInlinePosition() : renderer.layer()->staticBlockPosition();
    if (shouldFlipStaticPositionInParent(renderer, *m_container)) {
        // Note that at this point we can't resolve static top position completely in flipped case as at this point the height of the child box has not been computed yet.
        // What we can compute here is essentially the "bottom position".
        staticLogicalTop = downcast<RenderBox>(*parent).flipForWritingMode(staticLogicalTop);
    }
    staticLogicalTop -= haveOrthogonalWritingModes ? m_container->borderLogicalLeft() : m_container->borderBefore();
    for (RenderElement* container = parent; container && container != m_container.get(); container = container->container()) {
        auto* renderBox = dynamicDowncast<RenderBox>(*container);
        if (!renderBox)
            continue;
        if (!is<RenderTableRow>(*renderBox))
            staticLogicalTop += haveOrthogonalWritingModes ? renderBox->logicalLeft() : renderBox->logicalTop();
        if (renderBox->isInFlowPositioned())
            staticLogicalTop += renderBox->isHorizontalWritingMode() ? renderBox->offsetForInFlowPosition().height() : renderBox->offsetForInFlowPosition().width();
    }

    // If the parent is RTL then we need to flip the coordinate by setting the logical bottom instead of the logical top. That only needs
    // to be done in case of orthogonal writing modes, for horizontal ones the text direction of the parent does not affect the block position.
    if (haveOrthogonalWritingModes && parent->writingMode().isInlineFlipped())
        m_insetAfter = Style::InsetEdge::Fixed { staticLogicalTop };
    else
        m_insetBefore = Style::InsetEdge::Fixed { staticLogicalTop };
}

void PositionedLayoutConstraints::fixupLogicalLeftPosition(RenderBox::LogicalExtentComputedValues& computedValues) const
{
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

// The |containerLogicalHeightForPositioned| is already aware of orthogonal flows.
// The logicalTop concept is confusing here. It's the logical top from the child's POV. This means that is the physical
// y if the child is vertical or the physical x if the child is horizontal.
void PositionedLayoutConstraints::fixupLogicalTopPosition(RenderBox::LogicalExtentComputedValues& computedValues, const RenderBox& renderer) const
{
    // Deal with differing writing modes here. Our offset needs to be in the containing block's coordinate space. If the containing block is flipped
    // along this axis, then we need to flip the coordinate. This can only happen if the containing block is both a flipped mode and perpendicular to us.
    if (m_useStaticPosition) {
        if (shouldFlipStaticPositionInParent(renderer, *m_container)) {
            // Let's finish computing static top postion inside parents with flipped writing mode now that we've got final height value.
            // see details in computeBlockStaticDistance.
            computedValues.m_position -= computedValues.m_extent;
        }
        if (isBlockOpposing()) {
            computedValues.m_position = m_containingRange.max() - computedValues.m_extent - computedValues.m_position;
            computedValues.m_position += m_containingRange.min();
        }

    }
}

}
