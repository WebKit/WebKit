/*
 * This file is part of the render object implementation for KHTML.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#include "LayoutRepainter.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include <wtf/StdLibExtras.h>
#include <wtf/unicode/CharacterNames.h>

using namespace std;

namespace WebCore {

class FlexBoxIterator {
public:
    FlexBoxIterator(RenderDeprecatedFlexibleBox* parent)
        : m_box(parent)
        , m_largestOrdinal(1)
    {
        if (m_box->style()->boxOrient() == HORIZONTAL && !m_box->style()->isLeftToRightDirection())
            m_forward = m_box->style()->boxDirection() != BNORMAL;
        else
            m_forward = m_box->style()->boxDirection() == BNORMAL;
        if (!m_forward) {
            // No choice, since we're going backwards, we have to find out the highest ordinal up front.
            RenderBox* child = m_box->firstChildBox();
            while (child) {
                if (child->style()->boxOrdinalGroup() > m_largestOrdinal)
                    m_largestOrdinal = child->style()->boxOrdinalGroup();
                child = child->nextSiblingBox();
            }
        }

        reset();
    }

    void reset()
    {
        m_currentChild = 0;
        m_ordinalIteration = -1;
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
                    if (m_ordinalIteration >= m_ordinalValues.size() + 1)
                        return 0;

                    // Only copy+sort the values once per layout even if the iterator is reset.
                    if (static_cast<size_t>(m_ordinalValues.size()) != m_sortedOrdinalValues.size()) {
                        copyToVector(m_ordinalValues, m_sortedOrdinalValues);
                        sort(m_sortedOrdinalValues.begin(), m_sortedOrdinalValues.end());
                    }
                    m_currentOrdinal = m_forward ? m_sortedOrdinalValues[m_ordinalIteration - 1] : m_sortedOrdinalValues[m_sortedOrdinalValues.size() - m_ordinalIteration];
                }

                m_currentChild = m_forward ? m_box->firstChildBox() : m_box->lastChildBox();
            } else
                m_currentChild = m_forward ? m_currentChild->nextSiblingBox() : m_currentChild->previousSiblingBox();

            if (m_currentChild && notFirstOrdinalValue())
                m_ordinalValues.add(m_currentChild->style()->boxOrdinalGroup());
        } while (!m_currentChild || (!m_currentChild->isAnonymous()
                 && m_currentChild->style()->boxOrdinalGroup() != m_currentOrdinal));
        return m_currentChild;
    }

private:
    bool notFirstOrdinalValue()
    {
        unsigned int firstOrdinalValue = m_forward ? 1 : m_largestOrdinal;
        return m_currentOrdinal == firstOrdinalValue && m_currentChild->style()->boxOrdinalGroup() != firstOrdinalValue;
    }

    RenderDeprecatedFlexibleBox* m_box;
    RenderBox* m_currentChild;
    bool m_forward;
    unsigned int m_currentOrdinal;
    unsigned int m_largestOrdinal;
    HashSet<unsigned int> m_ordinalValues;
    Vector<unsigned int> m_sortedOrdinalValues;
    int m_ordinalIteration;
};

RenderDeprecatedFlexibleBox::RenderDeprecatedFlexibleBox(Node* node)
    : RenderBlock(node)
{
    setChildrenInline(false); // All of our children must be block-level
    m_flexingChildren = m_stretchingChildren = false;
}

RenderDeprecatedFlexibleBox::~RenderDeprecatedFlexibleBox()
{
}

static LayoutUnit marginWidthForChild(RenderBox* child)
{
    // A margin basically has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins simply become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length marginLeft = child->style()->marginLeft();
    Length marginRight = child->style()->marginRight();
    LayoutUnit margin = 0;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

static bool childDoesNotAffectWidthOrFlexing(RenderObject* child)
{
    // Positioned children and collapsed children don't affect the min/max width.
    return child->isPositioned() || child->style()->visibility() == COLLAPSE;
}

void RenderDeprecatedFlexibleBox::calcHorizontalPrefWidths()
{
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (childDoesNotAffectWidthOrFlexing(child))
            continue;

        LayoutUnit margin = marginWidthForChild(child);
        m_minPreferredLogicalWidth += child->minPreferredLogicalWidth() + margin;
        m_maxPreferredLogicalWidth += child->maxPreferredLogicalWidth() + margin;
    }
}

void RenderDeprecatedFlexibleBox::calcVerticalPrefWidths()
{
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (childDoesNotAffectWidthOrFlexing(child))
            continue;

        LayoutUnit margin = marginWidthForChild(child);
        LayoutUnit width = child->minPreferredLogicalWidth() + margin;
        m_minPreferredLogicalWidth = max(width, m_minPreferredLogicalWidth);

        width = child->maxPreferredLogicalWidth() + margin;
        m_maxPreferredLogicalWidth = max(width, m_maxPreferredLogicalWidth);
    }
}

void RenderDeprecatedFlexibleBox::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeContentBoxLogicalWidth(style()->width().value());
    else {
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

        if (hasMultipleLines() || isVertical())
            calcVerticalPrefWidths();
        else
            calcHorizontalPrefWidths();

        m_maxPreferredLogicalWidth = max(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);
    }

    if (hasOverflowClip() && style()->overflowY() == OSCROLL) {
        layer()->setHasVerticalScrollbar(true);
        LayoutUnit scrollbarWidth = verticalScrollbarWidth();
        m_maxPreferredLogicalWidth += scrollbarWidth;
        m_minPreferredLogicalWidth += scrollbarWidth;
    }

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
    }

    if (style()->maxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
    }

    LayoutUnit borderAndPadding = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPadding;
    m_maxPreferredLogicalWidth += borderAndPadding;

    setPreferredLogicalWidthsDirty(false);
}

void RenderDeprecatedFlexibleBox::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight, BlockLayoutPass layoutPass)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), this, LayoutSize(x(), y()), hasTransform() || hasReflection() || style()->isFlippedBlocksWritingMode());

    LayoutSize previousSize = size();

    computeLogicalWidth();
    computeLogicalHeight();

    m_overflow.clear();

    if (previousSize != size()
        || (parent()->isDeprecatedFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL
        && parent()->style()->boxAlign() == BSTRETCH))
        relayoutChildren = true;

    setHeight(0);

    m_flexingChildren = m_stretchingChildren = false;

    initMaxMarginValues();

    // For overflow:scroll blocks, ensure we have both scrollbars in place always.
    if (scrollsOverflow()) {
        if (style()->overflowX() == OSCROLL)
            layer()->setHasHorizontalScrollbar(true);
        if (style()->overflowY() == OSCROLL)
            layer()->setHasVerticalScrollbar(true);
    }

    if (isHorizontal())
        layoutHorizontalBox(relayoutChildren);
    else
        layoutVerticalBox(relayoutChildren);

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    computeLogicalHeight();

    if (previousSize.height() != height())
        relayoutChildren = true;

    bool needAnotherLayoutPass = layoutPositionedObjects(relayoutChildren || isRoot());

    if (!isFloatingOrPositioned() && height() == 0) {
        // We are a block with no border and padding and a computed height
        // of 0.  The CSS spec states that zero-height blocks collapse their margins
        // together.
        // When blocks are self-collapsing, we just use the top margin values and set the
        // bottom margin max values to 0.  This way we don't factor in the values
        // twice when we collapse with our previous vertically adjacent and
        // following vertically adjacent blocks.
        LayoutUnit pos = maxPositiveMarginBefore();
        LayoutUnit neg = maxNegativeMarginBefore();
        if (maxPositiveMarginAfter() > pos)
            pos = maxPositiveMarginAfter();
        if (maxNegativeMarginAfter() > neg)
            neg = maxNegativeMarginAfter();
        setMaxMarginBeforeValues(pos, neg);
        setMaxMarginAfterValues(0, 0);
    }

    computeOverflow(oldClientAfterEdge);

    statePusher.pop();

    updateLayerTransform();

    if (view()->layoutState()->pageLogicalHeight())
        setPageLogicalOffset(view()->layoutState()->pageLogicalOffset(logicalTop()));

    // Update our scrollbars if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        layer()->updateScrollInfoAfterLayout();

    // Repaint with our new bounds if they are different from our old bounds.
    repainter.repaintAfterLayout();

    if (needAnotherLayoutPass && layoutPass == NormalLayoutPass) {
        setChildNeedsLayout(true, false);
        layoutBlock(false, pageLogicalHeight);
    } else
        setNeedsLayout(false);
}

// The first walk over our kids is to find out if we have any flexible children.
static void gatherFlexChildrenInfo(FlexBoxIterator& iterator, bool relayoutChildren, unsigned int& highestFlexGroup, unsigned int& lowestFlexGroup, bool& haveFlex)
{
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        // Check to see if this child flexes.
        if (!childDoesNotAffectWidthOrFlexing(child) && child->style()->boxFlex() > 0.0f) {
            // We always have to lay out flexible objects again, since the flex distribution
            // may have changed, and we need to reallocate space.
            child->clearOverrideSize();
            if (!relayoutChildren)
                child->setChildNeedsLayout(true, false);
            haveFlex = true;
            unsigned int flexGroup = child->style()->boxFlexGroup();
            if (lowestFlexGroup == 0)
                lowestFlexGroup = flexGroup;
            if (flexGroup < lowestFlexGroup)
                lowestFlexGroup = flexGroup;
            if (flexGroup > highestFlexGroup)
                highestFlexGroup = flexGroup;
        }
    }
}

void RenderDeprecatedFlexibleBox::layoutHorizontalBox(bool relayoutChildren)
{
    LayoutUnit toAdd = borderBottom() + paddingBottom() + horizontalScrollbarHeight();
    LayoutUnit yPos = borderTop() + paddingTop();
    LayoutUnit xPos = borderLeft() + paddingLeft();
    bool heightSpecified = false;
    LayoutUnit oldHeight = 0;

    LayoutUnit remainingSpace = 0;


    FlexBoxIterator iterator(this);
    unsigned int highestFlexGroup = 0;
    unsigned int lowestFlexGroup = 0;
    bool haveFlex = false;
    gatherFlexChildrenInfo(iterator, relayoutChildren, highestFlexGroup, lowestFlexGroup, haveFlex);

    RenderBlock::startDelayUpdateScrollInfo();

    // We do 2 passes.  The first pass is simply to lay everyone out at
    // their preferred widths.  The second pass handles flexing the children.
    do {
        // Reset our height.
        setHeight(yPos);

        xPos = borderLeft() + paddingLeft();

        // Our first pass is done without flexing.  We simply lay the children
        // out within the box.  We have to do a layout first in order to determine
        // our box's intrinsic height.
        LayoutUnit maxAscent = 0, maxDescent = 0;
        for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
            // make sure we relayout children if we need it.
            if (relayoutChildren || (child->isReplaced() && (child->style()->width().isPercent() || child->style()->height().isPercent())))
                child->setChildNeedsLayout(true, false);

            if (child->isPositioned())
                continue;

            // Compute the child's vertical margins.
            child->computeBlockDirectionMargins(this);

            if (!child->needsLayout())
                child->markForPaginationRelayoutIfNeeded();

            // Now do the layout.
            child->layoutIfNeeded();

            // Update our height and overflow height.
            if (style()->boxAlign() == BBASELINE) {
                LayoutUnit ascent = child->firstLineBoxBaseline();
                if (ascent == -1)
                    ascent = child->height() + child->marginBottom();
                ascent += child->marginTop();
                LayoutUnit descent = (child->marginTop() + child->height() + child->marginBottom()) - ascent;

                // Update our maximum ascent.
                maxAscent = max(maxAscent, ascent);

                // Update our maximum descent.
                maxDescent = max(maxDescent, descent);

                // Now update our height.
                setHeight(max(yPos + maxAscent + maxDescent, height()));
            }
            else
                setHeight(max(height(), yPos + child->marginTop() + child->height() + child->marginBottom()));
        }

        if (!iterator.first() && hasLineIfEmpty())
            setHeight(height() + lineHeight(true, style()->isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));

        setHeight(height() + toAdd);

        oldHeight = height();
        computeLogicalHeight();

        relayoutChildren = false;
        if (oldHeight != height())
            heightSpecified = true;

        // Now that our height is actually known, we can place our boxes.
        m_stretchingChildren = (style()->boxAlign() == BSTRETCH);
        for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
            if (child->isPositioned()) {
                child->containingBlock()->insertPositionedObject(child);
                RenderLayer* childLayer = child->layer();
                childLayer->setStaticInlinePosition(xPos); // FIXME: Not right for regions.
                if (childLayer->staticBlockPosition() != yPos) {
                    childLayer->setStaticBlockPosition(yPos);
                    if (child->style()->hasStaticBlockPosition(style()->isHorizontalWritingMode()))
                        child->setChildNeedsLayout(true, false);
                }
                continue;
            } else if (child->style()->visibility() == COLLAPSE) {
                // visibility: collapsed children do not participate in our positioning.
                // But we need to lay them down.
                child->layoutIfNeeded();
                continue;
            }


            // We need to see if this child's height has changed, since we make block elements
            // fill the height of a containing box by default.
            // Now do a layout.
            LayoutUnit oldChildHeight = child->height();
            child->computeLogicalHeight();
            if (oldChildHeight != child->height())
                child->setChildNeedsLayout(true, false);

            if (!child->needsLayout())
                child->markForPaginationRelayoutIfNeeded();

            child->layoutIfNeeded();

            // We can place the child now, using our value of box-align.
            xPos += child->marginLeft();
            LayoutUnit childY = yPos;
            switch (style()->boxAlign()) {
                case BCENTER:
                    childY += child->marginTop() + max<LayoutUnit>(0, (contentHeight() - (child->height() + child->marginTop() + child->marginBottom())) / 2);
                    break;
                case BBASELINE: {
                    LayoutUnit ascent = child->firstLineBoxBaseline();
                    if (ascent == -1)
                        ascent = child->height() + child->marginBottom();
                    ascent += child->marginTop();
                    childY += child->marginTop() + (maxAscent - ascent);
                    break;
                }
                case BEND:
                    childY += contentHeight() - child->marginBottom() - child->height();
                    break;
                default: // BSTART
                    childY += child->marginTop();
                    break;
            }

            placeChild(child, LayoutPoint(xPos, childY));

            xPos += child->width() + child->marginRight();
        }

        remainingSpace = borderLeft() + paddingLeft() + contentWidth() - xPos;

        m_stretchingChildren = false;
        if (m_flexingChildren)
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
                            totalFlex += child->style()->boxFlex();
                    }
                    LayoutUnit spaceAvailableThisPass = groupRemainingSpace;
                    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                        LayoutUnit allowedFlex = allowedChildFlex(child, expanding, i);
                        if (allowedFlex) {
                            LayoutUnit projectedFlex = (allowedFlex == numeric_limits<LayoutUnit>::max()) ? allowedFlex : LayoutUnit(allowedFlex * (totalFlex / child->style()->boxFlex()));
                            spaceAvailableThisPass = expanding ? min(spaceAvailableThisPass, projectedFlex) : max(spaceAvailableThisPass, projectedFlex);
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
                        if (child->style()->visibility() == COLLAPSE)
                            continue;

                        if (allowedChildFlex(child, expanding, i)) {
                            LayoutUnit spaceAdd = LayoutUnit(spaceAvailableThisPass * (child->style()->boxFlex() / totalFlex));
                            if (spaceAdd) {
                                child->setOverrideWidth(child->overrideWidth() + spaceAdd);
                                m_flexingChildren = true;
                                relayoutChildren = true;
                            }

                            spaceAvailableThisPass -= spaceAdd;
                            remainingSpace -= spaceAdd;
                            groupRemainingSpace -= spaceAdd;

                            totalFlex -= child->style()->boxFlex();
                        }
                    }
                    if (groupRemainingSpace == groupRemainingSpaceAtBeginning) {
                        // This is not advancing, avoid getting stuck by distributing the remaining pixels.
                        LayoutUnit spaceAdd = groupRemainingSpace > 0 ? 1 : -1;
                        for (RenderBox* child = iterator.first(); child && groupRemainingSpace; child = iterator.next()) {
                            if (allowedChildFlex(child, expanding, i)) {
                                child->setOverrideWidth(child->overrideWidth() + spaceAdd);
                                m_flexingChildren = true;
                                relayoutChildren = true;
                                remainingSpace -= spaceAdd;
                                groupRemainingSpace -= spaceAdd;
                            }
                        }
                    }
                } while (groupRemainingSpace);
            }

            // We didn't find any children that could grow.
            if (haveFlex && !m_flexingChildren)
                haveFlex = false;
        }
    } while (haveFlex);

    m_flexingChildren = false;

    RenderBlock::finishDelayUpdateScrollInfo();

    if (remainingSpace > 0 && ((style()->isLeftToRightDirection() && style()->boxPack() != BSTART)
        || (!style()->isLeftToRightDirection() && style()->boxPack() != BEND))) {
        // Children must be repositioned.
        LayoutUnit offset = 0;
        if (style()->boxPack() == BJUSTIFY) {
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

                    placeChild(child, child->location() + LayoutSize(offset, 0));
                }
            }
        } else {
            if (style()->boxPack() == BCENTER)
                offset += remainingSpace / 2;
            else // END for LTR, START for RTL
                offset += remainingSpace;
            for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                if (childDoesNotAffectWidthOrFlexing(child))
                    continue;

                placeChild(child, child->location() + LayoutSize(offset, 0));
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
    LayoutUnit oldHeight = 0;

    LayoutUnit remainingSpace = 0;

    FlexBoxIterator iterator(this);
    unsigned int highestFlexGroup = 0;
    unsigned int lowestFlexGroup = 0;
    bool haveFlex = false;
    gatherFlexChildrenInfo(iterator, relayoutChildren, highestFlexGroup, lowestFlexGroup, haveFlex);

    // We confine the line clamp ugliness to vertical flexible boxes (thus keeping it out of
    // mainstream block layout); this is not really part of the XUL box model.
    bool haveLineClamp = !style()->lineClamp().isNone();
    if (haveLineClamp)
        applyLineClamp(iterator, relayoutChildren);

    RenderBlock::startDelayUpdateScrollInfo();

    // We do 2 passes.  The first pass is simply to lay everyone out at
    // their preferred widths.  The second pass handles flexing the children.
    // Our first pass is done without flexing.  We simply lay the children
    // out within the box.
    do {
        setHeight(borderTop() + paddingTop());
        LayoutUnit minHeight = height() + toAdd;

        for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
            // Make sure we relayout children if we need it.
            if (!haveLineClamp && (relayoutChildren || (child->isReplaced() && (child->style()->width().isPercent() || child->style()->height().isPercent()))))
                child->setChildNeedsLayout(true, false);

            if (child->isPositioned()) {
                child->containingBlock()->insertPositionedObject(child);
                RenderLayer* childLayer = child->layer();
                childLayer->setStaticInlinePosition(borderStart() + paddingStart()); // FIXME: Not right for regions.
                if (childLayer->staticBlockPosition() != height()) {
                    childLayer->setStaticBlockPosition(height());
                    if (child->style()->hasStaticBlockPosition(style()->isHorizontalWritingMode()))
                        child->setChildNeedsLayout(true, false);
                }
                continue;
            } else if (child->style()->visibility() == COLLAPSE) {
                // visibility: collapsed children do not participate in our positioning.
                // But we need to lay them down.
                child->layoutIfNeeded();
                continue;
            }

            // Compute the child's vertical margins.
            child->computeBlockDirectionMargins(this);

            // Add in the child's marginTop to our height.
            setHeight(height() + child->marginTop());

            if (!child->needsLayout())
                child->markForPaginationRelayoutIfNeeded();

            // Now do a layout.
            child->layoutIfNeeded();

            // We can place the child now, using our value of box-align.
            LayoutUnit childX = borderLeft() + paddingLeft();
            switch (style()->boxAlign()) {
                case BCENTER:
                case BBASELINE: // Baseline just maps to center for vertical boxes
                    childX += child->marginLeft() + max<LayoutUnit>(0, (contentWidth() - (child->width() + child->marginLeft() + child->marginRight())) / 2);
                    break;
                case BEND:
                    if (!style()->isLeftToRightDirection())
                        childX += child->marginLeft();
                    else
                        childX += contentWidth() - child->marginRight() - child->width();
                    break;
                default: // BSTART/BSTRETCH
                    if (style()->isLeftToRightDirection())
                        childX += child->marginLeft();
                    else
                        childX += contentWidth() - child->marginRight() - child->width();
                    break;
            }

            // Place the child.
            placeChild(child, LayoutPoint(childX, height()));
            setHeight(height() + child->height() + child->marginBottom());
        }

        yPos = height();

        if (!iterator.first() && hasLineIfEmpty())
            setHeight(height() + lineHeight(true, style()->isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));

        setHeight(height() + toAdd);

        // Negative margins can cause our height to shrink below our minimal height (border/padding).
        // If this happens, ensure that the computed height is increased to the minimal height.
        if (height() < minHeight)
            setHeight(minHeight);

        // Now we have to calc our height, so we know how much space we have remaining.
        oldHeight = height();
        computeLogicalHeight();
        if (oldHeight != height())
            heightSpecified = true;

        remainingSpace = borderTop() + paddingTop() + contentHeight() - yPos;

        if (m_flexingChildren)
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
                            totalFlex += child->style()->boxFlex();
                    }
                    LayoutUnit spaceAvailableThisPass = groupRemainingSpace;
                    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                        LayoutUnit allowedFlex = allowedChildFlex(child, expanding, i);
                        if (allowedFlex) {
                            LayoutUnit projectedFlex = (allowedFlex == numeric_limits<LayoutUnit>::max()) ? allowedFlex : static_cast<LayoutUnit>(allowedFlex * (totalFlex / child->style()->boxFlex()));
                            spaceAvailableThisPass = expanding ? min(spaceAvailableThisPass, projectedFlex) : max(spaceAvailableThisPass, projectedFlex);
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
                            LayoutUnit spaceAdd = static_cast<LayoutUnit>(spaceAvailableThisPass * (child->style()->boxFlex() / totalFlex));
                            if (spaceAdd) {
                                child->setOverrideHeight(child->overrideHeight() + spaceAdd);
                                m_flexingChildren = true;
                                relayoutChildren = true;
                            }

                            spaceAvailableThisPass -= spaceAdd;
                            remainingSpace -= spaceAdd;
                            groupRemainingSpace -= spaceAdd;

                            totalFlex -= child->style()->boxFlex();
                        }
                    }
                    if (groupRemainingSpace == groupRemainingSpaceAtBeginning) {
                        // This is not advancing, avoid getting stuck by distributing the remaining pixels.
                        LayoutUnit spaceAdd = groupRemainingSpace > 0 ? 1 : -1;
                        for (RenderBox* child = iterator.first(); child && groupRemainingSpace; child = iterator.next()) {
                            if (allowedChildFlex(child, expanding, i)) {
                                child->setOverrideHeight(child->overrideHeight() + spaceAdd);
                                m_flexingChildren = true;
                                relayoutChildren = true;
                                remainingSpace -= spaceAdd;
                                groupRemainingSpace -= spaceAdd;
                            }
                        }
                    }
                } while (groupRemainingSpace);
            }

            // We didn't find any children that could grow.
            if (haveFlex && !m_flexingChildren)
                haveFlex = false;
        }
    } while (haveFlex);

    RenderBlock::finishDelayUpdateScrollInfo();

    if (style()->boxPack() != BSTART && remainingSpace > 0) {
        // Children must be repositioned.
        LayoutUnit offset = 0;
        if (style()->boxPack() == BJUSTIFY) {
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
                    placeChild(child, child->location() + LayoutSize(0, offset));
                }
            }
        } else {
            if (style()->boxPack() == BCENTER)
                offset += remainingSpace / 2;
            else // END
                offset += remainingSpace;
            for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
                if (childDoesNotAffectWidthOrFlexing(child))
                    continue;
                placeChild(child, child->location() + LayoutSize(0, offset));
            }
        }
    }

    // So that the computeLogicalHeight in layoutBlock() knows to relayout positioned objects because of
    // a height change, we revert our height back to the intrinsic height before returning.
    if (heightSpecified)
        setHeight(oldHeight);
}

void RenderDeprecatedFlexibleBox::applyLineClamp(FlexBoxIterator& iterator, bool relayoutChildren)
{
    int maxLineCount = 0;
    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (childDoesNotAffectWidthOrFlexing(child))
            continue;

        if (relayoutChildren || (child->isReplaced() && (child->style()->width().isPercent() || child->style()->height().isPercent()))
            || (child->style()->height().isAuto() && child->isBlockFlow())) {
            child->setChildNeedsLayout(true, false);

            // Dirty all the positioned objects.
            if (child->isRenderBlock()) {
                toRenderBlock(child)->markPositionedObjectsForLayout();
                toRenderBlock(child)->clearTruncation();
            }
        }
        child->layoutIfNeeded();
        if (child->style()->height().isAuto() && child->isBlockFlow())
            maxLineCount = max(maxLineCount, toRenderBlock(child)->lineCount());
    }

    // Get the number of lines and then alter all block flow children with auto height to use the
    // specified height. We always try to leave room for at least one line.
    LineClampValue lineClamp = style()->lineClamp();
    int numVisibleLines = lineClamp.isPercentage() ? max(1, (maxLineCount + 1) * lineClamp.value() / 100) : lineClamp.value();
    if (numVisibleLines >= maxLineCount)
        return;

    for (RenderBox* child = iterator.first(); child; child = iterator.next()) {
        if (childDoesNotAffectWidthOrFlexing(child) || !child->style()->height().isAuto() || !child->isBlockFlow())
            continue;

        RenderBlock* blockChild = toRenderBlock(child);
        int lineCount = blockChild->lineCount();
        if (lineCount <= numVisibleLines)
            continue;

        LayoutUnit newHeight = blockChild->heightForLineCount(numVisibleLines);
        if (newHeight == child->height())
            continue;

        child->setChildNeedsLayout(true, false);
        child->setOverrideHeight(newHeight);
        m_flexingChildren = true;
        child->layoutIfNeeded();
        m_flexingChildren = false;
        child->clearOverrideSize();

        // FIXME: For now don't support RTL.
        if (style()->direction() != LTR)
            continue;

        // Get the last line
        RootInlineBox* lastLine = blockChild->lineAtIndex(lineCount - 1);
        if (!lastLine)
            continue;

        RootInlineBox* lastVisibleLine = blockChild->lineAtIndex(numVisibleLines - 1);
        if (!lastVisibleLine)
            continue;

        const UChar ellipsisAndSpace[2] = { horizontalEllipsis, ' ' };
        DEFINE_STATIC_LOCAL(AtomicString, ellipsisAndSpaceStr, (ellipsisAndSpace, 2));
        DEFINE_STATIC_LOCAL(AtomicString, ellipsisStr, (&horizontalEllipsis, 1));
        const Font& font = style(numVisibleLines == 1)->font();

        // Get ellipsis width, and if the last child is an anchor, it will go after the ellipsis, so add in a space and the anchor width too
        LayoutUnit totalWidth;
        InlineBox* anchorBox = lastLine->lastChild();
        if (anchorBox && anchorBox->renderer()->style()->isLink())
            totalWidth = anchorBox->logicalWidth() + font.width(constructTextRun(this, font, ellipsisAndSpace, 2, style()));
        else {
            anchorBox = 0;
            totalWidth = font.width(constructTextRun(this, font, &horizontalEllipsis, 1, style()));
        }

        // See if this width can be accommodated on the last visible line
        RenderBlock* destBlock = toRenderBlock(lastVisibleLine->renderer());
        RenderBlock* srcBlock = toRenderBlock(lastLine->renderer());

        // FIXME: Directions of src/destBlock could be different from our direction and from one another.
        if (!srcBlock->style()->isLeftToRightDirection())
            continue;

        bool leftToRight = destBlock->style()->isLeftToRightDirection();
        if (!leftToRight)
            continue;

        LayoutUnit blockRightEdge = destBlock->logicalRightOffsetForLine(lastVisibleLine->y(), false);
        LayoutUnit blockLeftEdge = destBlock->logicalLeftOffsetForLine(lastVisibleLine->y(), false);

        LayoutUnit blockEdge = leftToRight ? blockRightEdge : blockLeftEdge;
        if (!lastVisibleLine->lineCanAccommodateEllipsis(leftToRight, blockEdge, lastVisibleLine->x() + lastVisibleLine->logicalWidth(), totalWidth))
            continue;

        // Let the truncation code kick in.
        lastVisibleLine->placeEllipsis(anchorBox ? ellipsisAndSpaceStr : ellipsisStr, leftToRight, blockLeftEdge, blockRightEdge, totalWidth, anchorBox);
        destBlock->setHasMarkupTruncation(true);
    }
}

void RenderDeprecatedFlexibleBox::placeChild(RenderBox* child, const LayoutPoint& location)
{
    LayoutRect oldRect = child->frameRect();

    // Place the child.
    child->setLocation(location);

    // If the child moved, we have to repaint it as well as any floating/positioned
    // descendants.  An exception is if we need a layout.  In this case, we know we're going to
    // repaint ourselves (and the child) anyway.
    if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
        child->repaintDuringLayoutIfMoved(oldRect);
}

LayoutUnit RenderDeprecatedFlexibleBox::allowedChildFlex(RenderBox* child, bool expanding, unsigned int group)
{
    if (childDoesNotAffectWidthOrFlexing(child) || child->style()->boxFlex() == 0.0f || child->style()->boxFlexGroup() != group)
        return 0;

    if (expanding) {
        if (isHorizontal()) {
            // FIXME: For now just handle fixed values.
            LayoutUnit maxWidth = numeric_limits<LayoutUnit>::max();
            LayoutUnit width = child->overrideWidth() - child->borderAndPaddingWidth();
            if (!child->style()->maxWidth().isUndefined() && child->style()->maxWidth().isFixed())
                maxWidth = child->style()->maxWidth().value();
            else if (child->style()->maxWidth().type() == Intrinsic)
                maxWidth = child->maxPreferredLogicalWidth();
            else if (child->style()->maxWidth().type() == MinIntrinsic)
                maxWidth = child->minPreferredLogicalWidth();
            if (maxWidth == numeric_limits<LayoutUnit>::max())
                return maxWidth;
            return max<LayoutUnit>(0, maxWidth - width);
        } else {
            // FIXME: For now just handle fixed values.
            LayoutUnit maxHeight = numeric_limits<LayoutUnit>::max();
            LayoutUnit height = child->overrideHeight() - child->borderAndPaddingHeight();
            if (!child->style()->maxHeight().isUndefined() && child->style()->maxHeight().isFixed())
                maxHeight = child->style()->maxHeight().value();
            if (maxHeight == numeric_limits<LayoutUnit>::max())
                return maxHeight;
            return max<LayoutUnit>(0, maxHeight - height);
        }
    }

    // FIXME: For now just handle fixed values.
    if (isHorizontal()) {
        LayoutUnit minWidth = child->minPreferredLogicalWidth();
        LayoutUnit width = child->overrideWidth() - child->borderAndPaddingWidth();
        if (child->style()->minWidth().isFixed())
            minWidth = child->style()->minWidth().value();
        else if (child->style()->minWidth().type() == Intrinsic)
            minWidth = child->maxPreferredLogicalWidth();
        else if (child->style()->minWidth().type() == MinIntrinsic)
            minWidth = child->minPreferredLogicalWidth();

        LayoutUnit allowedShrinkage = min<LayoutUnit>(0, minWidth - width);
        return allowedShrinkage;
    } else {
        if (child->style()->minHeight().isFixed()) {
            LayoutUnit minHeight = child->style()->minHeight().value();
            LayoutUnit height = child->overrideHeight() - child->borderAndPaddingHeight();
            LayoutUnit allowedShrinkage = min<LayoutUnit>(0, minHeight - height);
            return allowedShrinkage;
        }
    }

    return 0;
}

const char *RenderDeprecatedFlexibleBox::renderName() const
{
    if (isFloating())
        return "RenderDeprecatedFlexibleBox (floating)";
    if (isPositioned())
        return "RenderDeprecatedFlexibleBox (positioned)";
    if (isAnonymous())
        return "RenderDeprecatedFlexibleBox (generated)";
    if (isRelPositioned())
        return "RenderDeprecatedFlexibleBox (relative positioned)";
    return "RenderDeprecatedFlexibleBox";
}

} // namespace WebCore
