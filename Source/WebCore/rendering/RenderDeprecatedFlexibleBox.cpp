/*
 * This file is part of the render object implementation for KHTML.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderDeprecatedFlexibleBox.h"

#include "FontCascade.h"
#include "LayoutRepainter.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderDeprecatedFlexibleBox);

class FlexBoxIterator {
public:
    FlexBoxIterator(RenderDeprecatedFlexibleBox* parent)
        : m_box(parent)
        , m_largestOrdinal(1)
    {
        if (m_box->style().boxOrient() == BoxOrient::Horizontal && !m_box->style().isLeftToRightDirection())
            m_forward = m_box->style().boxDirection() != BoxDirection::Normal;
        else
            m_forward = m_box->style().boxDirection() == BoxDirection::Normal;
        if (!m_forward) {
            // No choice, since we're going backwards, we have to find out the highest ordinal up front.
            RenderBox* child = m_box->firstChildBox();
            while (child) {
                if (child->style().boxOrdinalGroup() > m_largestOrdinal)
                    m_largestOrdinal = child->style().boxOrdinalGroup();
                child = child->nextSiblingBox();
            }
        }

        reset();
    }

    void reset()
    {
        m_currentChild = nullptr;
        m_ordinalIteration = std::numeric_limits<unsigned>::max();
    }

    RenderBox* first()
    {
        reset();
        return next();
    }

    RenderBox* next()
    {
        do {
            if (!m_currentChild) {
                ++m_ordinalIteration;

                if (!m_ordinalIteration)
                    m_currentOrdinal = m_forward ? 1 : m_largestOrdinal;
                else {
                    if (m_ordinalIteration > m_ordinalValues.size())
                        return nullptr;

                    // Only copy+sort the values once per layout even if the iterator is reset.
                    if (static_cast<size_t>(m_ordinalValues.size()) != m_sortedOrdinalValues.size()) {
                        m_sortedOrdinalValues = copyToVector(m_ordinalValues);
                        std::sort(m_sortedOrdinalValues.begin(), m_sortedOrdinalValues.end());
                    }
                    m_currentOrdinal = m_forward ? m_sortedOrdinalValues[m_ordinalIteration - 1] : m_sortedOrdinalValues[m_sortedOrdinalValues.size() - m_ordinalIteration];
                }

                m_currentChild = m_forward ? m_box->firstChildBox() : m_box->lastChildBox();
            } else
                m_currentChild = m_forward ? m_currentChild->nextSiblingBox() : m_currentChild->previousSiblingBox();

            if (m_currentChild && notFirstOrdinalValue())
                m_ordinalValues.add(m_currentChild->style().boxOrdinalGroup());
        } while (!m_currentChild || m_currentChild->isExcludedFromNormalLayout() || (!m_currentChild->isAnonymous()
                 && m_currentChild->style().boxOrdinalGroup() != m_currentOrdinal));
        return m_currentChild;
    }

private:
    bool notFirstOrdinalValue()
    {
        unsigned int firstOrdinalValue = m_forward ? 1 : m_largestOrdinal;
        return m_currentOrdinal == firstOrdinalValue && m_currentChild->style().boxOrdinalGroup() != firstOrdinalValue;
    }

    RenderDeprecatedFlexibleBox* m_box;
    RenderBox* m_currentChild;
    bool m_forward;
    unsigned m_currentOrdinal;
    unsigned m_largestOrdinal;
    HashSet<unsigned> m_ordinalValues;
    Vector<unsigned> m_sortedOrdinalValues;
    unsigned m_ordinalIteration;
};

RenderDeprecatedFlexibleBox::RenderDeprecatedFlexibleBox(Element& element, RenderStyle&& style)
    : RenderBlock(element, WTFMove(style), 0)
{
    setChildrenInline(false); // All of our children must be block-level
    m_stretchingChildren = false;
}

RenderDeprecatedFlexibleBox::~RenderDeprecatedFlexibleBox() = default;

static LayoutUnit marginWidthForChild(RenderBox* child)
{
    // A margin basically has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins simply become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length marginLeft = child->style().marginLeft();
    Length marginRight = child->style().marginRight();
    LayoutUnit margin;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

static bool childDoesNotAffectWidthOrFlexing(RenderObject* child)
{
    // Positioned children and collapsed children don't affect the min/max width.
    return child->isOutOfFlowPositioned() || child->style().visibility() == Visibility::Collapse;
}

static LayoutUnit widthForChild(RenderBox* child)
{
    if (child->hasOverridingLogicalWidth())
        return child->overridingLogicalWidth();
    return child->logicalWidth();
}

static LayoutUnit heightForChild(RenderBox* child)
{
    if (child->hasOverridingLogicalHeight())
        return child->overridingLogicalHeight();
    return child->logicalHeight();
}

static LayoutUnit contentWidthForChild(RenderBox* child)
{
    return std::max<LayoutUnit>(0, widthForChild(child) - child->borderAndPaddingLogicalWidth());
}

static LayoutUnit contentHeightForChild(RenderBox* child)
{
    return std::max<LayoutUnit>(0, heightForChild(child) - child->borderAndPaddingLogicalHeight());
}

void RenderDeprecatedFlexibleBox::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    auto* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    if (oldStyle && !oldStyle->lineClamp().isNone() && newStyle.lineClamp().isNone())
        clearLineClamp();

    RenderBlock::styleWillChange(diff, newStyle);
}

void RenderDeprecatedFlexibleBox::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    auto addScrollbarWidth = [&]() {
        LayoutUnit scrollbarWidth = intrinsicScrollbarLogicalWidth();
        maxLogicalWidth += scrollbarWidth;
        minLogicalWidth += scrollbarWidth;
    };

    if (shouldApplySizeContainment(*this)) {
        addScrollbarWidth();
        return;
    }

    if (hasMultipleLines() || isVertical()) {
        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (childDoesNotAffectWidthOrFlexing(child))
                continue;

            LayoutUnit margin = marginWidthForChild(child);
            LayoutUnit width = child->minPreferredLogicalWidth() + margin;
            minLogicalWidth = std::max(width, minLogicalWidth);

            width = child->maxPreferredLogicalWidth() + margin;
            maxLogicalWidth = std::max(width, maxLogicalWidth);
        }
    } else {
        for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
            if (childDoesNotAffectWidthOrFlexing(child))
                continue;

            LayoutUnit margin = marginWidthForChild(child);
            minLogicalWidth += child->minPreferredLogicalWidth() + margin;
            maxLogicalWidth += child->maxPreferredLogicalWidth() + margin;
        }
    }

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);
    addScrollbarWidth();
}

void RenderDeprecatedFlexibleBox::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;
    if (style().width().isFixed() && style().width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(style().width());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().minWidth(), style().maxWidth(), borderAndPaddingLogicalWidth());

    setPreferredLogicalWidthsDirty(false);
}

// Use an inline capacity of 8, since flexbox containers usually have less than 8 children.
typedef Vector<LayoutRect, 8> ChildFrameRects;
typedef Vector<LayoutSize, 8> ChildLayoutDeltas;

static void appendChildFrameRects(RenderDeprecatedFlexibleBox* box, ChildFrameRects& childFrameRects)
{
    FlexBoxIterator iterator(box);
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (!child->isOutOfFlowPositioned())
            childFrameRects.append(child->frameRect());
    }
}

static void appendChildLayoutDeltas(RenderDeprecatedFlexibleBox* box, ChildLayoutDeltas& childLayoutDeltas)
{
    FlexBoxIterator iterator(box);
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (!child->isOutOfFlowPositioned())
            childLayoutDeltas.append(LayoutSize());
    }
}

static void repaintChildrenDuringLayoutIfMoved(RenderDeprecatedFlexibleBox* box, const ChildFrameRects& oldChildRects)
{
    size_t childIndex = 0;
    FlexBoxIterator iterator(box);
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!box->selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldChildRects[childIndex]);
        
        ++childIndex;
    }
    ASSERT(childIndex == oldChildRects.size());
}

void RenderDeprecatedFlexibleBox::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

        resetLogicalHeightBeforeLayoutIfNeeded();
        preparePaginationBeforeBlockLayout(relayoutChildren);

        LayoutSize previousSize = size();

        updateLogicalWidth();
        updateLogicalHeight();

        if (previousSize != size()
            || (parent()->isDeprecatedFlexibleBox() && parent()->style().boxOrient() == BoxOrient::Horizontal
                && parent()->style().boxAlign() == BoxAlignment::Stretch))
            relayoutChildren = true;

        setHeight(0);

        m_stretchingChildren = false;

#if ASSERT_ENABLED
        LayoutSize oldLayoutDelta = view().frameView().layoutContext().layoutDelta();
#endif

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        layoutExcludedChildren(relayoutChildren);

        ChildFrameRects oldChildRects;
        appendChildFrameRects(this, oldChildRects);

        if (isHorizontal())
            layoutHorizontalBox(relayoutChildren);
        else
            layoutVerticalBox(relayoutChildren);

        repaintChildrenDuringLayoutIfMoved(this, oldChildRects);
        ASSERT(view().frameView().layoutContext().layoutDeltaMatches(oldLayoutDelta));

        LayoutUnit oldClientAfterEdge = clientLogicalBottom();
        updateLogicalHeight();

        if (previousSize.height() != height())
            relayoutChildren = true;

        layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());

        computeOverflow(oldClientAfterEdge);
    }

    updateLayerTransform();

    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (layoutState && layoutState->pageLogicalHeight())
        setPageLogicalOffset(layoutState->pageLogicalOffset(this, logicalTop()));

    // Update our scrollbars if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    // Repaint with our new bounds if they are different from our old bounds.
    repainter.repaintAfterLayout();

    clearNeedsLayout();
}

// The first walk over our kids is to find out if we have any flexible children.
static void gatherFlexChildrenInfo(FlexBoxIterator& iterator, bool relayoutChildren, unsigned int& highestFlexGroup, unsigned int& lowestFlexGroup, bool& haveFlex)
{
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        // Check to see if this child flexes.
        if (!childDoesNotAffectWidthOrFlexing(child) && child->style().boxFlex() > 0.0f) {
            // We always have to lay out flexible objects again, since the flex distribution
            // may have changed, and we need to reallocate space.
            child->clearOverridingContentSize();
            if (!relayoutChildren)
                child->setChildNeedsLayout(MarkOnlyThis);
            haveFlex = true;
            unsigned flexGroup = child->style().boxFlexGroup();
            if (lowestFlexGroup == 0)
                lowestFlexGroup = flexGroup;
            if (flexGroup < lowestFlexGroup)
                lowestFlexGroup = flexGroup;
            if (flexGroup > highestFlexGroup)
                highestFlexGroup = flexGroup;
        }
    }
}

static void layoutChildIfNeededApplyingDelta(RenderBox* child, const LayoutSize& layoutDelta)
{
    if (!child->needsLayout())
        return;
    
    child->view().frameView().layoutContext().addLayoutDelta(layoutDelta);
    child->layoutIfNeeded();
    child->view().frameView().layoutContext().addLayoutDelta(-layoutDelta);
}

void RenderDeprecatedFlexibleBox::layoutHorizontalBox(bool relayoutChildren)
{
    LayoutUnit toAdd = borderBottom() + paddingBottom() + horizontalScrollbarHeight();
    LayoutUnit yPos = borderTop() + paddingTop();
    LayoutUnit xPos = borderLeft() + paddingLeft();
    bool heightSpecified = false;
    LayoutUnit oldHeight;

    LayoutUnit remainingSpace;

    FlexBoxIterator iterator(this);
    unsigned int highestFlexGroup = 0;
    unsigned int lowestFlexGroup = 0;
    bool haveFlex = false, flexingChildren = false; 
    gatherFlexChildrenInfo(iterator, relayoutChildren, highestFlexGroup, lowestFlexGroup, haveFlex);

    beginUpdateScrollInfoAfterLayoutTransaction();

    ChildLayoutDeltas childLayoutDeltas;
    appendChildLayoutDeltas(this, childLayoutDeltas);

    // We do 2 passes.  The first pass is simply to lay everyone out at
    // their preferred widths. The subsequent passes handle flexing the children.
    // The first pass skips flexible objects completely.
    do {
        // Reset our height.
        setHeight(yPos);

        xPos = borderLeft() + paddingLeft();

        size_t childIndex = 0;

        // Our first pass is done without flexing.  We simply lay the children
        // out within the box.  We have to do a layout first in order to determine
        // our box's intrinsic height.
        LayoutUnit maxAscent, maxDescent;
        for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
            if (relayoutChildren)
                child->setChildNeedsLayout(MarkOnlyThis);

            if (child->isOutOfFlowPositioned())
                continue;
            
            LayoutSize& childLayoutDelta = childLayoutDeltas[childIndex++];
            
            // Compute the child's vertical margins.
            child->computeAndSetBlockDirectionMargins(*this);

            child->markForPaginationRelayoutIfNeeded();
            
            // Apply the child's current layout delta.
            layoutChildIfNeededApplyingDelta(child, childLayoutDelta);

            // Update our height and overflow height.
            if (style().boxAlign() == BoxAlignment::Baseline) {
                LayoutUnit ascent = child->firstLineBaseline().value_or(child->height() + child->marginBottom());
                ascent += child->marginTop();
                LayoutUnit descent = (child->height() + child->verticalMarginExtent()) - ascent;

                // Update our maximum ascent.
                maxAscent = std::max(maxAscent, ascent);

                // Update our maximum descent.
                maxDescent = std::max(maxDescent, descent);

                // Now update our height.
                setHeight(std::max(yPos + maxAscent + maxDescent, height()));
            }
            else
                setHeight(std::max(height(), yPos + child->height() + child->verticalMarginExtent()));
        }
        ASSERT(childIndex == childLayoutDeltas.size());

        if (!iterator.first() && hasLineIfEmpty())
            setHeight(height() + lineHeight(true, style().isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));

        setHeight(height() + toAdd);

        oldHeight = height();
        updateLogicalHeight();

        relayoutChildren = false;
        if (oldHeight != height())
            heightSpecified = true;

        // Now that our height is actually known, we can place our boxes.
        childIndex = 0;
        m_stretchingChildren = (style().boxAlign() == BoxAlignment::Stretch);
        for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
            if (child->isOutOfFlowPositioned()) {
                child->containingBlock()->insertPositionedObject(*child);
                RenderLayer* childLayer = child->layer();
                childLayer->setStaticInlinePosition(xPos); // FIXME: Not right for regions.
                if (childLayer->staticBlockPosition() != yPos) {
                    childLayer->setStaticBlockPosition(yPos);
                    if (child->style().hasStaticBlockPosition(style().isHorizontalWritingMode()))
                        child->setChildNeedsLayout(MarkOnlyThis);
                }
                continue;
            }
            
            LayoutSize& childLayoutDelta = childLayoutDeltas[childIndex++];
            
            if (child->style().visibility() == Visibility::Collapse) {
                // visibility: collapsed children do not participate in our positioning.
                // But we need to lay them out.
                layoutChildIfNeededApplyingDelta(child, childLayoutDelta);
                continue;
            }

            // We need to see if this child's height has changed, since we make block elements
            // fill the height of a containing box by default.
            // Now do a layout.
            LayoutUnit oldChildHeight = child->height();
            child->updateLogicalHeight();
            if (oldChildHeight != child->height())
                child->setChildNeedsLayout(MarkOnlyThis);

            child->markForPaginationRelayoutIfNeeded();

            layoutChildIfNeededApplyingDelta(child, childLayoutDelta);

            // We can place the child now, using our value of box-align.
            xPos += child->marginLeft();
            LayoutUnit childY = yPos;
            switch (style().boxAlign()) {
            case BoxAlignment::Center:
                childY += child->marginTop() + std::max<LayoutUnit>(0, (contentHeight() - (child->height() + child->verticalMarginExtent())) / 2);
                break;
            case BoxAlignment::Baseline: {
                LayoutUnit ascent = child->firstLineBaseline().value_or(child->height() + child->marginBottom());
                ascent += child->marginTop();
                childY += child->marginTop() + (maxAscent - ascent);
                break;
            }
            case BoxAlignment::End:
                childY += contentHeight() - child->marginBottom() - child->height();
                break;
            default: // BoxAlignment::Start
                childY += child->marginTop();
                break;
            }

            placeChild(child, LayoutPoint(xPos, childY), &childLayoutDelta);

            xPos += child->width() + child->marginRight();
        }
        ASSERT(childIndex == childLayoutDeltas.size());

        remainingSpace = borderLeft() + paddingLeft() + contentWidth() - xPos;

        m_stretchingChildren = false;
        if (flexingChildren)
            haveFlex = false; // We're done.
        else if (haveFlex) {
            // We have some flexible objects.  See if we need to grow/shrink them at all.
            if (!remainingSpace)
                break;

            // Allocate the remaining space among the flexible objects.  If we are trying to
            // grow, then we go from the lowest flex group to the highest flex group.  For shrinking,
            // we go from the highest flex group to the lowest group.
            bool expanding = remainingSpace > 0;
            unsigned int start = expanding ? lowestFlexGroup : highestFlexGroup;
            unsigned int end = expanding? highestFlexGroup : lowestFlexGroup;
            for (unsigned int i = start; i <= end && remainingSpace; i++) {
                // Always start off by assuming the group can get all the remaining space.
                LayoutUnit groupRemainingSpace = remainingSpace;
                do {
                    // Flexing consists of multiple passes, since we have to change ratios every time an object hits its max/min-width
                    // For a given pass, we always start off by computing the totalFlex of all objects that can grow/shrink at all, and
                    // computing the allowed growth before an object hits its min/max width (and thus
                    // forces a totalFlex recomputation).
                    LayoutUnit groupRemainingSpaceAtBeginning = groupRemainingSpace;
                    float totalFlex = 0.0f;
                    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                        if (allowedChildFlex(child, expanding, i))
                            totalFlex += child->style().boxFlex();
                    }
                    LayoutUnit spaceAvailableThisPass = groupRemainingSpace;
                    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                        LayoutUnit allowedFlex = allowedChildFlex(child, expanding, i);
                        if (allowedFlex) {
                            LayoutUnit projectedFlex = (allowedFlex == LayoutUnit::max()) ? allowedFlex : LayoutUnit(allowedFlex * (totalFlex / child->style().boxFlex()));
                            spaceAvailableThisPass = expanding ? std::min(spaceAvailableThisPass, projectedFlex) : std::max(spaceAvailableThisPass, projectedFlex);
                        }
                    }

                    // The flex groups may not have any flexible objects this time around.
                    if (!spaceAvailableThisPass || totalFlex == 0.0f) {
                        // If we just couldn't grow/shrink any more, then it's time to transition to the next flex group.
                        groupRemainingSpace = 0;
                        continue;
                    }

                    // Now distribute the space to objects.
                    for (RenderBox* child = iterator.first(); child && spaceAvailableThisPass && totalFlex; child = iterator.next()) {
                        if (child->style().visibility() == Visibility::Collapse)
                            continue;

                        if (allowedChildFlex(child, expanding, i)) {
                            LayoutUnit spaceAdd = LayoutUnit(spaceAvailableThisPass * (child->style().boxFlex() / totalFlex));
                            if (spaceAdd) {
                                child->setOverridingLogicalWidth(widthForChild(child) + spaceAdd);
                                flexingChildren = true;
                                relayoutChildren = true;
                            }

                            spaceAvailableThisPass -= spaceAdd;
                            remainingSpace -= spaceAdd;
                            groupRemainingSpace -= spaceAdd;

                            totalFlex -= child->style().boxFlex();
                        }
                    }
                    if (groupRemainingSpace == groupRemainingSpaceAtBeginning) {
                        // This is not advancing, avoid getting stuck by distributing the remaining pixels.
                        LayoutUnit spaceAdd = groupRemainingSpace > 0 ? 1 : -1;
                        for (RenderBox* child = iterator.first(); child && groupRemainingSpace; child = iterator.next()) {
                            if (allowedChildFlex(child, expanding, i)) {
                                child->setOverridingLogicalWidth(widthForChild(child) + spaceAdd);
                                flexingChildren = true;
                                relayoutChildren = true;
                                remainingSpace -= spaceAdd;
                                groupRemainingSpace -= spaceAdd;
                            }
                        }
                    }
                } while (absoluteValue(groupRemainingSpace) >= 1);
            }

            // We didn't find any children that could grow.
            if (haveFlex && !flexingChildren)
                haveFlex = false;
        }
    } while (haveFlex);

    endAndCommitUpdateScrollInfoAfterLayoutTransaction();

    if (remainingSpace > 0 && ((style().isLeftToRightDirection() && style().boxPack() != BoxPack::Start)
        || (!style().isLeftToRightDirection() && style().boxPack() != BoxPack::End))) {
        // Children must be repositioned.
        LayoutUnit offset;
        if (style().boxPack() == BoxPack::Justify) {
            // Determine the total number of children.
            int totalChildren = 0;
            for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                if (childDoesNotAffectWidthOrFlexing(child))
                    continue;
                ++totalChildren;
            }

            // Iterate over the children and space them out according to the
            // justification level.
            if (totalChildren > 1) {
                --totalChildren;
                bool firstChild = true;
                for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                    if (childDoesNotAffectWidthOrFlexing(child))
                        continue;

                    if (firstChild) {
                        firstChild = false;
                        continue;
                    }

                    offset += remainingSpace/totalChildren;
                    remainingSpace -= (remainingSpace/totalChildren);
                    --totalChildren;

                    placeChild(child, child->location() + LayoutSize(offset, 0_lu));
                }
            }
        } else {
            if (style().boxPack() == BoxPack::Center)
                offset += remainingSpace / 2;
            else // BoxPack::End for LTR, BoxPack::Start for RTL
                offset += remainingSpace;
            for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                if (childDoesNotAffectWidthOrFlexing(child))
                    continue;

                placeChild(child, child->location() + LayoutSize(offset, 0_lu));
            }
        }
    }

    // So that the computeLogicalHeight in layoutBlock() knows to relayout positioned objects because of
    // a height change, we revert our height back to the intrinsic height before returning.
    if (heightSpecified)
        setHeight(oldHeight);
}

void RenderDeprecatedFlexibleBox::layoutVerticalBox(bool relayoutChildren)
{
    LayoutUnit yPos = borderTop() + paddingTop();
    LayoutUnit toAdd = borderBottom() + paddingBottom() + horizontalScrollbarHeight();
    bool heightSpecified = false;
    LayoutUnit oldHeight;

    LayoutUnit remainingSpace;

    FlexBoxIterator iterator(this);
    unsigned int highestFlexGroup = 0;
    unsigned int lowestFlexGroup = 0;
    bool haveFlex = false, flexingChildren = false; 
    gatherFlexChildrenInfo(iterator, relayoutChildren, highestFlexGroup, lowestFlexGroup, haveFlex);

    // We confine the line clamp ugliness to vertical flexible boxes (thus keeping it out of
    // mainstream block layout); this is not really part of the XUL box model.
    bool haveLineClamp = !style().lineClamp().isNone();
    if (haveLineClamp)
        applyLineClamp(iterator, relayoutChildren);

    beginUpdateScrollInfoAfterLayoutTransaction();

    ChildLayoutDeltas childLayoutDeltas;
    appendChildLayoutDeltas(this, childLayoutDeltas);

    // We do 2 passes.  The first pass is simply to lay everyone out at
    // their preferred widths.  The second pass handles flexing the children.
    // Our first pass is done without flexing.  We simply lay the children
    // out within the box.
    do {
        setHeight(borderTop() + paddingTop());
        LayoutUnit minHeight = height() + toAdd;

        size_t childIndex = 0;
        for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
            // Make sure we relayout children if we need it.
            if (!haveLineClamp && relayoutChildren)
                child->setChildNeedsLayout(MarkOnlyThis);

            if (child->isOutOfFlowPositioned()) {
                child->containingBlock()->insertPositionedObject(*child);
                RenderLayer* childLayer = child->layer();
                childLayer->setStaticInlinePosition(borderStart() + paddingStart()); // FIXME: Not right for regions.
                if (childLayer->staticBlockPosition() != height()) {
                    childLayer->setStaticBlockPosition(height());
                    if (child->style().hasStaticBlockPosition(style().isHorizontalWritingMode()))
                        child->setChildNeedsLayout(MarkOnlyThis);
                }
                continue;
            }
            
            LayoutSize& childLayoutDelta = childLayoutDeltas[childIndex++];

            if (child->style().visibility() == Visibility::Collapse) {
                // visibility: collapsed children do not participate in our positioning.
                // But we need to lay them down.
                layoutChildIfNeededApplyingDelta(child, childLayoutDelta);
                continue;
            }

            // Compute the child's vertical margins.
            child->computeAndSetBlockDirectionMargins(*this);

            // Add in the child's marginTop to our height.
            setHeight(height() + child->marginTop());

            child->markForPaginationRelayoutIfNeeded();

            // Now do a layout.
            layoutChildIfNeededApplyingDelta(child, childLayoutDelta);

            // We can place the child now, using our value of box-align.
            LayoutUnit childX = borderLeft() + paddingLeft();
            switch (style().boxAlign()) {
            case BoxAlignment::Center:
            case BoxAlignment::Baseline: // Baseline just maps to center for vertical boxes
                childX += child->marginLeft() + std::max<LayoutUnit>(0, (contentWidth() - (child->width() + child->horizontalMarginExtent())) / 2);
                break;
            case BoxAlignment::End:
                if (!style().isLeftToRightDirection())
                    childX += child->marginLeft();
                else
                    childX += contentWidth() - child->marginRight() - child->width();
                break;
            default: // BoxAlignment::Start/BoxAlignment::Stretch
                if (style().isLeftToRightDirection())
                    childX += child->marginLeft();
                else
                    childX += contentWidth() - child->marginRight() - child->width();
                break;
            }

            // Place the child.
            placeChild(child, LayoutPoint(childX, height()), &childLayoutDelta);
            setHeight(height() + child->height() + child->marginBottom());
        }
        ASSERT(childIndex == childLayoutDeltas.size());

        yPos = height();

        if (!iterator.first() && hasLineIfEmpty())
            setHeight(height() + lineHeight(true, style().isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));

        setHeight(height() + toAdd);

        // Negative margins can cause our height to shrink below our minimal height (border/padding).
        // If this happens, ensure that the computed height is increased to the minimal height.
        if (height() < minHeight)
            setHeight(minHeight);

        // Now we have to calc our height, so we know how much space we have remaining.
        oldHeight = height();
        updateLogicalHeight();
        if (oldHeight != height())
            heightSpecified = true;

        remainingSpace = borderTop() + paddingTop() + contentHeight() - yPos;

        if (flexingChildren)
            haveFlex = false; // We're done.
        else if (haveFlex) {
            // We have some flexible objects.  See if we need to grow/shrink them at all.
            if (!remainingSpace)
                break;

            // Allocate the remaining space among the flexible objects.  If we are trying to
            // grow, then we go from the lowest flex group to the highest flex group.  For shrinking,
            // we go from the highest flex group to the lowest group.
            bool expanding = remainingSpace > 0;
            unsigned int start = expanding ? lowestFlexGroup : highestFlexGroup;
            unsigned int end = expanding? highestFlexGroup : lowestFlexGroup;
            for (unsigned int i = start; i <= end && remainingSpace; i++) {
                // Always start off by assuming the group can get all the remaining space.
                LayoutUnit groupRemainingSpace = remainingSpace;
                do {
                    // Flexing consists of multiple passes, since we have to change ratios every time an object hits its max/min-width
                    // For a given pass, we always start off by computing the totalFlex of all objects that can grow/shrink at all, and
                    // computing the allowed growth before an object hits its min/max width (and thus
                    // forces a totalFlex recomputation).
                    LayoutUnit groupRemainingSpaceAtBeginning = groupRemainingSpace;
                    float totalFlex = 0.0f;
                    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                        if (allowedChildFlex(child, expanding, i))
                            totalFlex += child->style().boxFlex();
                    }
                    LayoutUnit spaceAvailableThisPass = groupRemainingSpace;
                    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                        LayoutUnit allowedFlex = allowedChildFlex(child, expanding, i);
                        if (allowedFlex) {
                            LayoutUnit projectedFlex = (allowedFlex == LayoutUnit::max()) ? allowedFlex : LayoutUnit(allowedFlex * (totalFlex / child->style().boxFlex()));
                            spaceAvailableThisPass = expanding ? std::min(spaceAvailableThisPass, projectedFlex) : std::max(spaceAvailableThisPass, projectedFlex);
                        }
                    }

                    // The flex groups may not have any flexible objects this time around.
                    if (!spaceAvailableThisPass || totalFlex == 0.0f) {
                        // If we just couldn't grow/shrink any more, then it's time to transition to the next flex group.
                        groupRemainingSpace = 0;
                        continue;
                    }

                    // Now distribute the space to objects.
                    for (RenderBox* child = iterator.first(); child && spaceAvailableThisPass && totalFlex; child = iterator.next()) {
                        if (allowedChildFlex(child, expanding, i)) {
                            LayoutUnit spaceAdd { spaceAvailableThisPass * (child->style().boxFlex() / totalFlex) };
                            if (spaceAdd) {
                                child->setOverridingLogicalHeight(heightForChild(child) + spaceAdd);
                                flexingChildren = true;
                                relayoutChildren = true;
                            }

                            spaceAvailableThisPass -= spaceAdd;
                            remainingSpace -= spaceAdd;
                            groupRemainingSpace -= spaceAdd;

                            totalFlex -= child->style().boxFlex();
                        }
                    }
                    if (groupRemainingSpace == groupRemainingSpaceAtBeginning) {
                        // This is not advancing, avoid getting stuck by distributing the remaining pixels.
                        LayoutUnit spaceAdd = groupRemainingSpace > 0 ? 1 : -1;
                        for (RenderBox* child = iterator.first(); child && groupRemainingSpace; child = iterator.next()) {
                            if (allowedChildFlex(child, expanding, i)) {
                                child->setOverridingLogicalHeight(heightForChild(child) + spaceAdd);
                                flexingChildren = true;
                                relayoutChildren = true;
                                remainingSpace -= spaceAdd;
                                groupRemainingSpace -= spaceAdd;
                            }
                        }
                    }
                } while (absoluteValue(groupRemainingSpace) >= 1);
            }

            // We didn't find any children that could grow.
            if (haveFlex && !flexingChildren)
                haveFlex = false;
        }
    } while (haveFlex);

    endAndCommitUpdateScrollInfoAfterLayoutTransaction();

    if (style().boxPack() != BoxPack::Start && remainingSpace > 0) {
        // Children must be repositioned.
        LayoutUnit offset;
        if (style().boxPack() == BoxPack::Justify) {
            // Determine the total number of children.
            int totalChildren = 0;
            for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                if (childDoesNotAffectWidthOrFlexing(child))
                    continue;

                ++totalChildren;
            }

            // Iterate over the children and space them out according to the
            // justification level.
            if (totalChildren > 1) {
                --totalChildren;
                bool firstChild = true;
                for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                    if (childDoesNotAffectWidthOrFlexing(child))
                        continue;

                    if (firstChild) {
                        firstChild = false;
                        continue;
                    }

                    offset += remainingSpace/totalChildren;
                    remainingSpace -= (remainingSpace/totalChildren);
                    --totalChildren;
                    placeChild(child, child->location() + LayoutSize(0_lu, offset));
                }
            }
        } else {
            if (style().boxPack() == BoxPack::Center)
                offset += remainingSpace / 2;
            else // BoxPack::End
                offset += remainingSpace;
            for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                if (childDoesNotAffectWidthOrFlexing(child))
                    continue;
                placeChild(child, child->location() + LayoutSize(0_lu, offset));
            }
        }
    }

    // So that the computeLogicalHeight in layoutBlock() knows to relayout positioned objects because of
    // a height change, we revert our height back to the intrinsic height before returning.
    if (heightSpecified)
        setHeight(oldHeight);
}

static LegacyRootInlineBox* lineAtIndex(const RenderBlockFlow& flow, int i)
{
    ASSERT(i >= 0);

    if (flow.style().visibility() != Visibility::Visible)
        return nullptr;

    if (flow.childrenInline()) {
        for (auto* box = flow.firstRootBox(); box; box = box->nextRootBox()) {
            if (!i--)
                return box;
        }
        return nullptr;
    }

    for (auto& blockFlow : childrenOfType<RenderBlockFlow>(flow)) {
        if (!shouldIncludeLinesForParentLineCount(blockFlow))
            continue;
        if (LegacyRootInlineBox* box = lineAtIndex(blockFlow, i))
            return box;
    }

    return nullptr;
}

static int getHeightForLineCount(const RenderBlockFlow& block, int lineCount, bool includeBottom, int& count)
{
    if (block.style().visibility() != Visibility::Visible)
        return -1;

    if (block.childrenInline()) {
        for (auto* box = block.firstRootBox(); box; box = box->nextRootBox()) {
            if (++count == lineCount)
                return box->lineBottom() + (includeBottom ? (block.borderBottom() + block.paddingBottom()) : 0_lu);
        }
    } else {
        RenderBox* normalFlowChildWithoutLines = nullptr;
        for (auto* obj = block.firstChildBox(); obj; obj = obj->nextSiblingBox()) {
            if (is<RenderBlockFlow>(*obj) && shouldIncludeLinesForParentLineCount(downcast<RenderBlockFlow>(*obj))) {
                int result = getHeightForLineCount(downcast<RenderBlockFlow>(*obj), lineCount, false, count);
                if (result != -1)
                    return result + obj->y() + (includeBottom ? (block.borderBottom() + block.paddingBottom()) : 0_lu);
            } else if (!obj->isFloatingOrOutOfFlowPositioned())
                normalFlowChildWithoutLines = obj;
        }
        if (normalFlowChildWithoutLines && !lineCount)
            return normalFlowChildWithoutLines->y() + normalFlowChildWithoutLines->height();
    }

    return -1;
}

static int heightForLineCount(const RenderBlockFlow& flow, int lineCount)
{
    int count = 0;
    return getHeightForLineCount(flow, lineCount, true, count);
}

void RenderDeprecatedFlexibleBox::applyLineClamp(FlexBoxIterator& iterator, bool relayoutChildren)
{
    int maxLineCount = 0;
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (childDoesNotAffectWidthOrFlexing(child))
            continue;

        child->clearOverridingContentSize();
        if (relayoutChildren || (child->isReplaced() && (child->style().width().isPercentOrCalculated() || child->style().height().isPercentOrCalculated()))
            || (child->style().height().isAuto() && is<RenderBlockFlow>(*child))) {
            child->setChildNeedsLayout(MarkOnlyThis);

            // Dirty all the positioned objects.
            if (is<RenderBlockFlow>(*child)) {
                downcast<RenderBlockFlow>(*child).markPositionedObjectsForLayout();
                downcast<RenderBlockFlow>(*child).clearTruncation();
            }
        }
        child->layoutIfNeeded();
        if (child->style().height().isAuto() && is<RenderBlockFlow>(*child))
            maxLineCount = std::max(maxLineCount, downcast<RenderBlockFlow>(*child).lineCount());
    }

    // Get the number of lines and then alter all block flow children with auto height to use the
    // specified height. We always try to leave room for at least one line.
    LineClampValue lineClamp = style().lineClamp();
    int numVisibleLines = lineClamp.isPercentage() ? std::max(1, (maxLineCount + 1) * lineClamp.value() / 100) : lineClamp.value();
    if (numVisibleLines >= maxLineCount)
        return;

    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (childDoesNotAffectWidthOrFlexing(child) || !child->style().height().isAuto() || !is<RenderBlockFlow>(*child))
            continue;

        RenderBlockFlow& blockChild = downcast<RenderBlockFlow>(*child);
        int lineCount = blockChild.lineCount();
        if (lineCount <= numVisibleLines)
            continue;

        LayoutUnit newHeight = heightForLineCount(blockChild, numVisibleLines);
        if (newHeight == child->height())
            continue;

        child->setChildNeedsLayout(MarkOnlyThis);
        child->setOverridingLogicalHeight(newHeight);
        child->layoutIfNeeded();

        // FIXME: For now don't support RTL.
        if (style().direction() != TextDirection::LTR)
            continue;

        // Get the last line
        LegacyRootInlineBox* lastLine = lineAtIndex(blockChild, lineCount - 1);
        if (!lastLine)
            continue;

        LegacyRootInlineBox* lastVisibleLine = lineAtIndex(blockChild, numVisibleLines - 1);
        if (!lastVisibleLine || !lastVisibleLine->firstChild())
            continue;

        const UChar ellipsisAndSpace[2] = { horizontalEllipsis, ' ' };
        static MainThreadNeverDestroyed<const AtomString> ellipsisAndSpaceStr(ellipsisAndSpace, 2);
        static MainThreadNeverDestroyed<const AtomString> ellipsisStr(&horizontalEllipsis, 1);
        const RenderStyle& lineStyle = numVisibleLines == 1 ? firstLineStyle() : style();
        const FontCascade& font = lineStyle.fontCascade();

        // Get ellipsis width, and if the last child is an anchor, it will go after the ellipsis, so add in a space and the anchor width too
        LayoutUnit totalWidth;
        LegacyInlineBox* anchorBox = lastLine->lastChild();
        auto& lastVisibleRenderer = lastVisibleLine->firstChild()->renderer();
        if (anchorBox && anchorBox->renderer().style().isLink() && &lastVisibleRenderer != &anchorBox->renderer())
            totalWidth = anchorBox->logicalWidth() + font.width(constructTextRun(ellipsisAndSpace, 2, style()));
        else {
            anchorBox = nullptr;
            totalWidth = font.width(constructTextRun(&horizontalEllipsis, 1, style()));
        }

        // See if this width can be accommodated on the last visible line
        RenderBlockFlow& destBlock = lastVisibleLine->blockFlow();
        RenderBlockFlow& srcBlock = lastLine->blockFlow();

        // FIXME: Directions of src/destBlock could be different from our direction and from one another.
        if (!srcBlock.style().isLeftToRightDirection())
            continue;

        bool leftToRight = destBlock.style().isLeftToRightDirection();
        if (!leftToRight)
            continue;

        LayoutUnit blockRightEdge = destBlock.logicalRightOffsetForLine(LayoutUnit(lastVisibleLine->y()), DoNotIndentText);
        if (!lastVisibleLine->lineCanAccommodateEllipsis(leftToRight, blockRightEdge, lastVisibleLine->x() + lastVisibleLine->logicalWidth(), totalWidth))
            continue;

        // Let the truncation code kick in.
        // FIXME: the text alignment should be recomputed after the width changes due to truncation.
        LayoutUnit blockLeftEdge = destBlock.logicalLeftOffsetForLine(LayoutUnit(lastVisibleLine->y()), DoNotIndentText);
        lastVisibleLine->placeEllipsis(anchorBox ? ellipsisAndSpaceStr : ellipsisStr, leftToRight, blockLeftEdge, blockRightEdge, totalWidth, anchorBox);
        destBlock.setHasMarkupTruncation(true);
    }
}

void RenderDeprecatedFlexibleBox::clearLineClamp()
{
    FlexBoxIterator iterator(this);
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (childDoesNotAffectWidthOrFlexing(child))
            continue;

        child->clearOverridingContentSize();
        if ((child->isReplaced() && (child->style().width().isPercentOrCalculated() || child->style().height().isPercentOrCalculated()))
            || (child->style().height().isAuto() && is<RenderBlockFlow>(*child))) {
            child->setChildNeedsLayout();

            if (is<RenderBlockFlow>(*child)) {
                downcast<RenderBlockFlow>(*child).markPositionedObjectsForLayout();
                downcast<RenderBlockFlow>(*child).clearTruncation();
            }
        }
    }
}

void RenderDeprecatedFlexibleBox::placeChild(RenderBox* child, const LayoutPoint& location, LayoutSize* childLayoutDelta)
{
    // Place the child and track the layout delta so we can apply it if we do another layout.
    if (childLayoutDelta)
        *childLayoutDelta += LayoutSize(child->x() - location.x(), child->y() - location.y());
    child->setLocation(location);
}

LayoutUnit RenderDeprecatedFlexibleBox::allowedChildFlex(RenderBox* child, bool expanding, unsigned int group)
{
    if (childDoesNotAffectWidthOrFlexing(child) || child->style().boxFlex() == 0.0f || child->style().boxFlexGroup() != group)
        return 0;

    if (expanding) {
        if (isHorizontal()) {
            // FIXME: For now just handle fixed values.
            LayoutUnit maxWidth = LayoutUnit::max();
            LayoutUnit width = contentWidthForChild(child);
            if (!child->style().maxWidth().isUndefined() && child->style().maxWidth().isFixed())
                maxWidth = child->style().maxWidth().value();
            else if (child->style().maxWidth().type() == LengthType::Intrinsic)
                maxWidth = child->maxPreferredLogicalWidth();
            else if (child->style().maxWidth().isMinIntrinsic())
                maxWidth = child->minPreferredLogicalWidth();
            if (maxWidth == LayoutUnit::max())
                return maxWidth;
            return std::max<LayoutUnit>(0, maxWidth - width);
        } else {
            // FIXME: For now just handle fixed values.
            LayoutUnit maxHeight = LayoutUnit::max();
            LayoutUnit height = contentHeightForChild(child);
            if (!child->style().maxHeight().isUndefined() && child->style().maxHeight().isFixed())
                maxHeight = child->style().maxHeight().value();
            if (maxHeight == LayoutUnit::max())
                return maxHeight;
            return std::max<LayoutUnit>(0, maxHeight - height);
        }
    }

    // FIXME: For now just handle fixed values.
    if (isHorizontal()) {
        LayoutUnit minWidth = child->minPreferredLogicalWidth();
        LayoutUnit width = contentWidthForChild(child);
        if (child->style().minWidth().isFixed())
            minWidth = child->style().minWidth().value();
        else if (child->style().minWidth().type() == LengthType::Intrinsic)
            minWidth = child->maxPreferredLogicalWidth();
        else if (child->style().minWidth().isMinIntrinsic())
            minWidth = child->minPreferredLogicalWidth();
        else if (child->style().minWidth().isAuto())
            minWidth = 0;

        LayoutUnit allowedShrinkage = std::min<LayoutUnit>(0, minWidth - width);
        return allowedShrinkage;
    } else {
        Length minHeight = child->style().minHeight();
        if (minHeight.isFixed() || minHeight.isAuto()) {
            LayoutUnit minHeight { child->style().minHeight().value() };
            LayoutUnit height = contentHeightForChild(child);
            LayoutUnit allowedShrinkage = std::min<LayoutUnit>(0, minHeight - height);
            return allowedShrinkage;
        }
    }

    return 0;
}

const char* RenderDeprecatedFlexibleBox::renderName() const
{
    if (isFloating())
        return "RenderDeprecatedFlexibleBox (floating)";
    if (isOutOfFlowPositioned())
        return "RenderDeprecatedFlexibleBox (positioned)";
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderDeprecatedFlexibleBox (generated)";
    if (isAnonymous())
        return "RenderDeprecatedFlexibleBox (generated)";
    if (isRelativelyPositioned())
        return "RenderDeprecatedFlexibleBox (relative positioned)";
    return "RenderDeprecatedFlexibleBox";
}

} // namespace WebCore
