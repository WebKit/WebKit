/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "FloatingContext.h"

#include "FloatAvoider.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutElementBox.h"
#include "LayoutShape.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <ranges>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore::Layout {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FloatingContext);

// Finding the top/left position for a new floating(F)
//  ____  ____  _____               _______
// |    || L2 ||     | <-----1---->|       |
// |    ||____||  L3 |             |   R1  |
// | L1 |      |_____|             |       |
// |____| <-------------2--------->|       |
//                                 |       |
//                                 |_______|
//
// 1. Compute the initial vertical position for (F) -> (1)
// 2. Find the corresponding floating pair (L3-R1)
// 3. Align (F) horizontally with (L3-R1) depending whether (F) is left/right positioned
// 4. Intersect (F) with (L3-R1)
// 5. If (F) does not fit, find the next floating pair (L1-R1)
// 6. Repeat until either (F) fits/no more floats.
// Note that all coordinates are in the coordinate system of the formatting root.
// The formatting root here is always the one that establishes the floating context (see inherited floating context).
// (It simply means that the float box's formatting root is not necessarily the same as the FormattingContext's root.)

class Iterator;

class FloatPair {
public:
    struct InlineStartEndIndex {
        NODELETE bool isEmpty() const { return !inlineStart && !inlineEnd; }

        std::optional<size_t> inlineStart;
        std::optional<size_t> inlineEnd;
    };

    NODELETE bool isEmpty() const { return m_floatPair.isEmpty(); }
    const PlacedFloats::Item* inlineStart() const;
    const PlacedFloats::Item* inlineEnd() const;
    bool intersects(const FloatAvoider&) const;
    bool intersects(AxisRange<ParentFormattingContextRoot>) const;
    bool containsFloatFromFormattingContext(const ElementBox& formattingContextRoot) const;
    NODELETE AxisPosition<ParentFormattingContextRoot> highestBlockAxisPosition() const { return m_highestBlockAxisPosition; }
    NODELETE AxisPosition<ParentFormattingContextRoot> lowestBlockAxisPosition() const;

    struct InlineAxisConstraints {
        std::optional<AxisPosition<ParentFormattingContextRoot>> start;
        std::optional<AxisPosition<ParentFormattingContextRoot>> end;
    };
    InlineAxisConstraints inlineAxisConstraints() const;
    NODELETE InlineStartEndIndex operator*() const { return m_floatPair; };
    bool operator==(const FloatPair&) const;

private:
    friend class Iterator;
    NODELETE FloatPair(const PlacedFloats::List&);

    const PlacedFloats::List& m_floats;
    InlineStartEndIndex m_floatPair;
    AxisPosition<ParentFormattingContextRoot> m_highestBlockAxisPosition;
};

class Iterator {
public:
    Iterator(const PlacedFloats&, std::optional<AxisPosition<ParentFormattingContextRoot>> blockStart);

    NODELETE const FloatPair& operator*() const { return m_current; }
    Iterator& operator++();
    bool operator==(const Iterator&) const;

private:
    void set(AxisPosition<ParentFormattingContextRoot> blockAxisPosition);

    const PlacedFloats& m_placedFloats;
    const PlacedFloats::List& m_floats;
    FloatPair m_current;
};

static Iterator begin(const PlacedFloats& placedFloats, AxisPosition<ParentFormattingContextRoot> initialBlockStart)
{
    // Start with the inner-most floating pair for the initial vertical position.
    return Iterator(placedFloats, initialBlockStart);
}

static Iterator end(const PlacedFloats& placedFloats)
{
    return Iterator(placedFloats, { });
}

#if ASSERT_ENABLED
static bool areFloatsHorizontallySorted(const PlacedFloats& placedFloats)
{
    auto& floats = placedFloats.list();
    auto inlineEndEdgeOfInlineStartFloats = AxisPosition<ParentFormattingContextRoot> { LayoutUnit::min() };
    auto inlineStartEdgeOfInlineEndFloats = AxisPosition<ParentFormattingContextRoot> { LayoutUnit::max() };
    auto lowestBlockAxisPositionAtInlineStart = std::optional<AxisPosition<ParentFormattingContextRoot>> { };
    auto lowestBlockAxisPositionAtInlineEnd = std::optional<AxisPosition<ParentFormattingContextRoot>> { };

    for (auto& floatItem : floats) {
        if (floatItem.isStartPositioned()) {
            auto inlineEndEdge = floatItem.marginBoxRight();
            if (inlineEndEdge < inlineEndEdgeOfInlineStartFloats) {
                if (lowestBlockAxisPositionAtInlineStart && floatItem.marginBoxTop() < *lowestBlockAxisPositionAtInlineStart)
                    return false;
            }
            lowestBlockAxisPositionAtInlineStart = floatItem.marginBoxBottom();
            inlineEndEdgeOfInlineStartFloats = inlineEndEdge;
        } else {
            auto inlineStarEdge = floatItem.marginBoxLeft();
            if (inlineStarEdge > inlineStartEdgeOfInlineEndFloats) {
                if (lowestBlockAxisPositionAtInlineEnd && floatItem.marginBoxTop() < *lowestBlockAxisPositionAtInlineEnd)
                    return false;
            }
            lowestBlockAxisPositionAtInlineEnd = floatItem.marginBoxBottom();
            inlineStartEdgeOfInlineEndFloats = inlineStarEdge;
        }
    }
    return true;
}
#endif

static FloatPair::InlineStartEndIndex findAvailablePosition(FloatAvoider& floatAvoider, const PlacedFloats& placedFloats, AxisRange<ParentFormattingContextRoot> containingBlockContentBoxEdges, const ElementBox& formattingContextRoot)
{
    auto lowestBlockAxisPosition = std::optional<AxisPosition<ParentFormattingContextRoot>> { };
    auto innerMostInlineStartAndEnd = std::optional<FloatPair::InlineStartEndIndex> { };
    auto end = Layout::end(placedFloats);
    for (auto iterator = begin(placedFloats, floatAvoider.blockStart()); iterator != end; ++iterator) {
        ASSERT(!(*iterator).isEmpty());
        auto inlineStartEndFloatPair = *iterator;
        innerMostInlineStartAndEnd = innerMostInlineStartAndEnd.value_or(*inlineStartEndFloatPair);

        // Move the box horizontally so that it either
        // 1. aligns with the current floating pair (always constrained by containing block e.g. when current float on this position is outside of containing block i.e. not intrusive).
        // 2. or with the containing block's content box if there's no float to align with at this vertical position.
        auto inlineStartEndEdge = inlineStartEndFloatPair.inlineAxisConstraints();

        // Ensure that the float avoider
        // 1. avoids floats on both sides (with the exception of non-intrusive floats from other FCs)
        // 2. does not overflow its containing block if the horizontal position is constrained by other floats
        // (i.e. a float avoider may overflow its containing block just fine unless this overflow is the result of getting it pushed by other floats on this vertical position -out of available space)
        // 3. Move to the next floating pair if this vertical position is over-constrained.
        if (auto inlineAxisConstraints = floatAvoider.isStartAligned() ? inlineStartEndEdge.start : inlineStartEndEdge.end)
            floatAvoider.setInlineStart(*inlineAxisConstraints);
        else
            floatAvoider.resetInlineStart();
        floatAvoider.setBlockStart(inlineStartEndFloatPair.highestBlockAxisPosition());

        if (!inlineStartEndFloatPair.intersects(floatAvoider) && !floatAvoider.overflowsContainingBlockWhileConstrainedByFloats())
            return *innerMostInlineStartAndEnd;

        // Is this float pair is outside of our containing block's content box? In some cases we _may_ overlap them.
        if (!inlineStartEndFloatPair.intersects(containingBlockContentBoxEdges) && !inlineStartEndFloatPair.containsFloatFromFormattingContext(formattingContextRoot)) {
            // Surprisingly floats do overlap each other on the non-floating side (e.g. float: left may overlap a float: right)
            // when they are not considered intrusive (i.e. they are outside of our containing block's content box) and coming from outside of the formatting context.
            return *innerMostInlineStartAndEnd;
        }

        lowestBlockAxisPosition = inlineStartEndFloatPair.lowestBlockAxisPosition();
        // Move to the next floating pair.
    }

    // The candidate box is already below of all the floats.
    if (!lowestBlockAxisPosition)
        return { };

    // Passed all the floats and still does not fit? Push it below the last float.
    floatAvoider.setBlockStart(*lowestBlockAxisPosition);
    floatAvoider.resetInlineStart();
    ASSERT(innerMostInlineStartAndEnd);
    return *innerMostInlineStartAndEnd;
}

struct FloatingContext::AbsoluteCoordinateValuesForFloatAvoider {
    Point<ParentFormattingContextRoot> topLeft;
    Point<ParentFormattingContextRoot> containingBlockTopLeft;
    AxisRange<ParentFormattingContextRoot> containingBlockContentBox;
};

FloatingContext::FloatingContext(const ElementBox& formattingContextRoot, const PlacedFloats& placedFloats, Point<ParentFormattingContextRoot> rootTopLeft, LayoutUnit rootBorderBoxInlineSize)
    : m_formattingContextRoot(formattingContextRoot)
    , m_placedFloats(placedFloats)
    , m_rootTopLeft(rootTopLeft)
    , m_rootBorderBoxInlineSize(rootBorderBoxInlineSize)
{
}

LayoutPoint FloatingContext::positionForFloat(const Box& layoutBox, const BoxGeometry& boxGeometry, const ContainingBlock& containingBlock) const
{
    ASSERT(layoutBox.isFloatingPositioned());
    ASSERT(areFloatsHorizontallySorted(m_placedFloats));
    auto borderBoxTopLeft = BoxGeometry::borderBoxTopLeft(boxGeometry);

    if (isEmpty()) {
        auto alignWithContainingBlock = [&] -> LayoutUnit {
            // If there is no floating to align with, push the box to the left/right edge of its containing block's content box.
            if (isFloatingCandidateStartPositionedInBlockFormattingContext(layoutBox))
                return { containingBlock.contentBox.logicalLeft + boxGeometry.marginStart() };
            return { containingBlock.contentBox.logicalRight() - boxGeometry.marginEnd() - boxGeometry.borderBoxWidth() };
        };
        // No float box on the context yet -> align it with the containing block's left/right edge.
        return { alignWithContainingBlock(), borderBoxTopLeft.y() };
    }

    // Find the top most position where the float box fits.
    ASSERT(!isEmpty());
    auto clearPosition = [&] -> std::optional<AxisPosition<ParentFormattingContextRoot>> {
        if (!layoutBox.hasFloatClear())
            return { };
        // The vertical position candidate needs to clear the existing floats in this context.
        switch (clearInBlockFormattingContext(layoutBox)) {
        case Clear::Left:
            return placedFloats().lowestPositionOnBlockAxis(Clear::InlineStart);
        case Clear::Right:
            return placedFloats().lowestPositionOnBlockAxis(Clear::InlineEnd);
        case Clear::Both:
            return placedFloats().lowestPositionOnBlockAxis();
        default:
            ASSERT_NOT_REACHED();
        }
        return { };
    };

    auto absoluteCoordinates = this->absoluteCoordinates(borderBoxTopLeft, containingBlock);
    auto absoluteTopLeft = absoluteCoordinates.topLeft;
    auto blockStartCandidate = absoluteTopLeft.y();
    // Incoming float cannot be placed higher than existing floats (margin box of the lowest float).
    // Take the static position (where the box would go if it wasn't floating) and adjust it with the lowest float.
    auto lowestFloatTop = *placedFloats().lowestTopPositionOnBlockAxis();
    auto lastOrClearedFloatPosition = std::max(clearPosition().value_or(lowestFloatTop), lowestFloatTop);
    if (blockStartCandidate - boxGeometry.marginBefore() < lastOrClearedFloatPosition)
        blockStartCandidate = lastOrClearedFloatPosition + boxGeometry.marginBefore();

    absoluteTopLeft.setY(blockStartCandidate);
    auto margins = BoxGeometry::Edges { { boxGeometry.marginStart(), boxGeometry.marginEnd() }, { boxGeometry.marginBefore(), boxGeometry.marginAfter() } };
    auto floatBox = FloatAvoider { absoluteTopLeft, boxGeometry.borderBoxWidth(), margins, absoluteCoordinates.containingBlockContentBox, true, isFloatingCandidateStartPositionedInBlockFormattingContext(layoutBox) };
    findAvailablePosition(floatBox, placedFloats(), absoluteCoordinates.containingBlockContentBox, protect(root()));
    // Convert box coordinates from formatting root back to containing block.
    auto containingBlockTopLeft = absoluteCoordinates.containingBlockTopLeft;
    return { floatBox.inlineStart() + margins.horizontal.start - containingBlockTopLeft.x(), floatBox.blockStart() + margins.vertical.before - containingBlockTopLeft.y() };
}

LayoutPoint FloatingContext::positionForNonFloatingFloatAvoider(const Box& layoutBox, const BoxGeometry& boxGeometry, const ContainingBlock& containingBlock) const
{
    ASSERT(layoutBox.establishesBlockFormattingContext());
    ASSERT(!layoutBox.isFloatingPositioned());
    ASSERT(!layoutBox.hasFloatClear());
    ASSERT(areFloatsHorizontallySorted(m_placedFloats));

    auto borderBoxTopLeft = BoxGeometry::borderBoxTopLeft(boxGeometry);
    if (isEmpty())
        return borderBoxTopLeft;

    auto absoluteCoordinates = this->absoluteCoordinates(borderBoxTopLeft, containingBlock);
    auto margins = BoxGeometry::Edges { { boxGeometry.marginStart(), boxGeometry.marginEnd() }, { boxGeometry.marginBefore(), boxGeometry.marginAfter() } };
    auto floatAvoider = FloatAvoider { absoluteCoordinates.topLeft, boxGeometry.borderBoxWidth(), margins, absoluteCoordinates.containingBlockContentBox, false, layoutBox.writingMode().isBidiLTR() };
    findPositionForFormattingContextRoot(floatAvoider, absoluteCoordinates.containingBlockContentBox);
    auto containingBlockTopLeft = absoluteCoordinates.containingBlockTopLeft;
    return { floatAvoider.inlineStart() - containingBlockTopLeft.x(), floatAvoider.blockStart() - containingBlockTopLeft.y() };
}

std::optional<FloatingContext::BlockAxisPositionWithClearance> FloatingContext::blockAxisPositionWithClearance(const Box& layoutBox, const BoxGeometry& boxGeometry, const ContainingBlock& containingBlock) const
{
    ASSERT(layoutBox.hasFloatClear());
    ASSERT(areFloatsHorizontallySorted(m_placedFloats));

    if (isEmpty())
        return { };

    auto lowestPositionOnBlockAxis = [&](auto blockAxisPosition) -> std::optional<BlockAxisPositionWithClearance> {
        if (!blockAxisPosition)
            return { };
        // 9.5.2 Controlling flow next to floats: the 'clear' property
        // Then the amount of clearance is set to the greater of:
        //
        // 1. The amount necessary to place the border edge of the block even with the bottom outer edge of the lowest float that is to be cleared.
        // 2. The amount necessary to place the top border edge of the block at its hypothetical position.
        auto logicalTopRelativeToBlockFormattingContextRoot = mapTopLeftToParentFormattingContextRoot(BoxGeometry::borderBoxTopLeft(boxGeometry), containingBlock).y();
        auto clearance = *blockAxisPosition - logicalTopRelativeToBlockFormattingContextRoot;
        if (clearance <= 0)
            return { };

        if (layoutBox.isBlockLevelBox()) {
            // Clearance inhibits margin collapsing in block formatting context.
            ASSERT_NOT_IMPLEMENTED_YET();
            // FIXME: This needs to go to BFC.
        }
        // Now adjust the box's position with the clearance.
        logicalTopRelativeToBlockFormattingContextRoot += clearance;
        ASSERT(*blockAxisPosition == logicalTopRelativeToBlockFormattingContextRoot);

        // The return vertical position needs to be in the containing block's coordinate system.
        auto containingBlockRootRelativeTop = mapTopLeftToParentFormattingContextRoot({ }, containingBlock).y();
        return BlockAxisPositionWithClearance { logicalTopRelativeToBlockFormattingContextRoot - containingBlockRootRelativeTop, clearance };
    };

    auto clear = clearInBlockFormattingContext(layoutBox);
    if (clear == Clear::Left)
        return lowestPositionOnBlockAxis(placedFloats().lowestPositionOnBlockAxis(Clear::InlineStart));

    if (clear == Clear::Right)
        return lowestPositionOnBlockAxis(placedFloats().lowestPositionOnBlockAxis(Clear::InlineEnd));

    if (clear == Clear::Both)
        return lowestPositionOnBlockAxis(placedFloats().lowestPositionOnBlockAxis());

    ASSERT_NOT_REACHED();
    return { };
}

NODELETE static bool floatContainsLine(const PlacedFloats::Item& floatItem, AxisPosition<ParentFormattingContextRoot> candidateTop, AxisPosition<ParentFormattingContextRoot> candidateBottom, LayoutUnit candidateHeight)
{
    if (floatItem.marginBoxSize().isEmpty())
        return false;
    if (!candidateHeight)
        return floatItem.marginBoxTop() <= candidateTop && floatItem.marginBoxBottom() > candidateTop;
    return floatItem.marginBoxTop() < candidateBottom && floatItem.marginBoxBottom() > candidateTop;
}

static std::optional<std::pair<AxisPosition<ParentFormattingContextRoot>, AxisPosition<ParentFormattingContextRoot>>> computeShapeEdge(const PlacedFloats::Item& floatItem, AxisPosition<ParentFormattingContextRoot> candidateTop, LayoutUnit candidateHeight)
{
    RefPtr shape = floatItem.shape();
    if (!shape)
        return { };

    auto positionInShape = candidateTop - floatItem.borderBoxTop();

    if (!shape->lineOverlapsShapeMarginBounds(positionInShape, candidateHeight))
        return { };

    // PolygonShape gets confused when passing in 0px height interval at vertices.
    auto segment = shape->getExcludedInterval(positionInShape, std::max(candidateHeight, 1_lu));
    if (!segment.isValid)
        return { };

    // FIXME: This is potentially slow.
    auto bottom = candidateTop + 1_lu;

    if (floatItem.isStartPositioned()) {
        auto shapeRight = floatItem.borderBoxLeft() + LayoutUnit { segment.logicalRight };
        return std::pair { std::min(shapeRight, floatItem.marginBoxRight()), bottom };
    }
    auto shapeLeft = floatItem.borderBoxLeft() + LayoutUnit { segment.logicalLeft };
    return std::pair { std::max(shapeLeft, floatItem.marginBoxLeft()), bottom };
}

static std::optional<std::pair<AxisPosition<ParentFormattingContextRoot>, AxisPosition<ParentFormattingContextRoot>>> computeFloatEdgeAndBottom(const PlacedFloats::Item& floatItem, AxisPosition<ParentFormattingContextRoot> candidateTop, AxisPosition<ParentFormattingContextRoot> candidateBottom, LayoutUnit candidateHeight)
{
    if (!floatContainsLine(floatItem, candidateTop, candidateBottom, candidateHeight))
        return { };

    if (floatItem.shape())
        return computeShapeEdge(floatItem, candidateTop, candidateHeight);

    auto edge = floatItem.isStartPositioned() ? floatItem.marginBoxRight() : floatItem.marginBoxLeft();
    return std::pair { edge, floatItem.marginBoxBottom() };
}

FloatingContext::Constraints FloatingContext::constraints(AxisPosition<FormattingContextRoot> candidateTop, AxisPosition<FormattingContextRoot> candidateBottom, MayBeAboveLastFloat mayBeAboveLastFloat) const
{
    if (isEmpty())
        return { };
    // 1. Convert vertical position if this floating context is inherited.
    // 2. Find the inner left/right floats at candidateTop/candidateBottom. Note when MayBeAboveLastFloat is 'no', we can just stop at the inner most (last) float (block vs. inline case).
    // 3. Convert left/right positions back to formattingContextRoot's coordinate system.
    auto& placedFloats = this->placedFloats();
    auto adjustedCandidateTop = m_rootTopLeft.y() + candidateTop.value();
    auto adjustedCandidateBottom = adjustedCandidateTop + (candidateBottom - candidateTop);
    auto candidateHeight = adjustedCandidateBottom - adjustedCandidateTop;

    auto start = std::optional<Point<ParentFormattingContextRoot>> { };
    auto end = std::optional<Point<ParentFormattingContextRoot>> { };
    if (mayBeAboveLastFloat == MayBeAboveLastFloat::No) {
        for (auto& floatItem : placedFloats.list() | std::views::reverse) {
            if ((start && floatItem.isStartPositioned()) || (end && !floatItem.isStartPositioned()))
                continue;

            auto edgeAndBottom = computeFloatEdgeAndBottom(floatItem, adjustedCandidateTop, adjustedCandidateBottom, candidateHeight);
            if (!edgeAndBottom)
                continue;

            auto [edge, bottom] = *edgeAndBottom;

            if (floatItem.isStartPositioned())
                start = Point<ParentFormattingContextRoot> { edge, bottom };
            else
                end = Point<ParentFormattingContextRoot> { edge, bottom };

            if ((start && end) || (start && !placedFloats.hasEndPositioned()) || (end && !placedFloats.hasStartPositioned()))
                break;
        }
    } else {
        for (auto& floatItem : placedFloats.list() | std::views::reverse) {
            auto edgeAndBottom = computeFloatEdgeAndBottom(floatItem, adjustedCandidateTop, adjustedCandidateBottom, candidateHeight);
            if (!edgeAndBottom)
                continue;

            auto [edge, bottom] = *edgeAndBottom;

            if (floatItem.isStartPositioned()) {
                if (!start || start->x() < edge)
                    start = Point<ParentFormattingContextRoot> { edge, bottom };
            } else {
                if (!end || end->x() > edge)
                    end = Point<ParentFormattingContextRoot> { edge, bottom };
            }
            // FIXME: Bail out when floats are way above.
        }
    }

    auto mapToRoot = [&](Point<ParentFormattingContextRoot> point) -> Point<FormattingContextRoot> {
        return { AxisPosition<FormattingContextRoot> { point.x() - m_rootTopLeft.x() }, AxisPosition<FormattingContextRoot> { point.y() - m_rootTopLeft.y() } };
    };
    auto constraints = Constraints { };
    if (start)
        constraints.start = mapToRoot(*start);
    if (end)
        constraints.end = mapToRoot(*end);

    if (placedFloats.blockFormattingContextRoot().style().writingMode().isInlineOpposing(root().writingMode())) {
        // FIXME: Move it under a &placedFloats.blockFormattingContextRoot() != &root() check when the integration codepath starts initiating the floating state with the
        // correct containing block (i.e. when the float comes from the parent BFC).

        // Flip to logical in inline direction.
        auto logicalConstraints = Constraints { };
        auto borderBoxWidth = m_rootBorderBoxInlineSize;
        if (constraints.start)
            logicalConstraints.end = Point<FormattingContextRoot> { AxisPosition<FormattingContextRoot> { borderBoxWidth - constraints.start->x().value() }, constraints.start->y() };
        if (constraints.end)
            logicalConstraints.start = Point<FormattingContextRoot> { AxisPosition<FormattingContextRoot> { borderBoxWidth - constraints.end->x().value() }, constraints.end->y() };
        constraints = logicalConstraints;
    }
    return constraints;
}

PlacedFloats::Item FloatingContext::makeFloatItem(const Box& floatBox, const BoxGeometry& boxGeometry, const ContainingBlock& containingBlock, std::optional<size_t> line) const
{
    auto borderBoxTopLeft = BoxGeometry::borderBoxTopLeft(boxGeometry);
    auto absoluteBoxGeometry = BoxGeometry { boxGeometry };
    auto absoluteTopLeft = mapTopLeftToParentFormattingContextRoot(borderBoxTopLeft, containingBlock);
    absoluteBoxGeometry.setTopLeft({ absoluteTopLeft.x().value(), absoluteTopLeft.y().value() });
    auto position = isFloatingCandidateStartPositionedInBlockFormattingContext(floatBox) ? PlacedFloats::Item::Position::Start : PlacedFloats::Item::Position::End;
    return { floatBox, position, absoluteBoxGeometry, borderBoxTopLeft, line };
}

void FloatingContext::findPositionForFormattingContextRoot(FloatAvoider& floatAvoider, AxisRange<ParentFormattingContextRoot> containingBlockContentBoxEdges) const
{
    // A non-floating formatting root's initial vertical position is its static position.
    // It means that such boxes can end up vertically placed in-between existing floats (which is
    // never the case for floats, since they cannot be placed above existing floats).
    //  ____  ____
    // |    || F1 |
    // | L1 | ----
    // |    |  ________
    //  ----  |   R1   |
    //         --------
    // Document order: 1. float: left (L1) 2. float: right (R1) 3. formatting root (F1)
    //
    // 1. Probe for available placement at initial position (note it runs a backward probing algorithm at a specific vertical position)
    // 2. Check if there's any intersecting float below (forward search)
    // 3. Align the box with the intersected float and probe for placement again (#1). 
    auto& floats = m_placedFloats.list();
    while (true) {
        auto innerMostInlineStartAndEnd = findAvailablePosition(floatAvoider, m_placedFloats, containingBlockContentBoxEdges, protect(root()));
        if (innerMostInlineStartAndEnd.isEmpty())
            return;

        auto overlappingFloatBox = [&floats](auto startFloatIndex, auto& floatAvoider) -> const PlacedFloats::Item* {
            for (auto i = startFloatIndex; i < floats.size(); ++i) {
                auto& floatBox = floats[i];

                auto intersects = [&] {
                    if (floatAvoider.inlineStart() >= floatBox.marginBoxRight() || floatAvoider.inlineEnd() <= floatBox.marginBoxLeft())
                        return false;
                    return floatAvoider.blockStart() >= floatBox.marginBoxTop() && floatAvoider.blockStart() < floatBox.marginBoxBottom();
                }();
                if (intersects)
                    return &floatBox;
            }
            return nullptr;
        };

        auto startIndex = std::max(innerMostInlineStartAndEnd.inlineStart.value_or(0), innerMostInlineStartAndEnd.inlineEnd.value_or(0)) + 1;
        auto* intersectedFloatBox = overlappingFloatBox(startIndex, floatAvoider);
        if (!intersectedFloatBox)
            return;
        floatAvoider.setBlockStart(intersectedFloatBox->marginBoxTop());
    }
}

FloatingContext::AbsoluteCoordinateValuesForFloatAvoider FloatingContext::absoluteCoordinates(LayoutPoint borderBoxTopLeft, const ContainingBlock& containingBlock) const
{
    auto containingBlockAbsoluteTopLeft = mapTopLeftToParentFormattingContextRoot({ }, containingBlock);
    auto& contentBox = containingBlock.contentBox;
    return { mapTopLeftToParentFormattingContextRoot(borderBoxTopLeft, containingBlock), containingBlockAbsoluteTopLeft, { containingBlockAbsoluteTopLeft.x() + contentBox.logicalLeft, containingBlockAbsoluteTopLeft.x() + contentBox.logicalRight() } };
}

Point<ParentFormattingContextRoot> FloatingContext::mapTopLeftToParentFormattingContextRoot(LayoutPoint borderBoxTopLeft, const ContainingBlock& containingBlock) const
{
    return { m_rootTopLeft.x() + containingBlock.borderBoxTopLeft.x().value() + borderBoxTopLeft.x(), m_rootTopLeft.y() + containingBlock.borderBoxTopLeft.y().value() + borderBoxTopLeft.y() };
}

bool FloatingContext::isStartPositioned(const Box& floatBox) const
{
    ASSERT(floatBox.isFloatingPositioned());
    // Note that this returns true relative to the root of this FloatingContext and not to the PlacedFloats's block formatting context root.
    // PlacedFloats's root may be an ancestor block container with mismatching inline direction.
    auto floatingBoxIsInLeftToRightDirection = root().writingMode().isBidiLTR();
    auto floatingValue = floatBox.style().floating();
    return floatingValue == Float::InlineStart
        || (floatingBoxIsInLeftToRightDirection && floatingValue == Float::Left)
        || (!floatingBoxIsInLeftToRightDirection && floatingValue == Float::Right);
}

bool FloatingContext::isFloatingCandidateStartPositionedInBlockFormattingContext(const Box& floatBox) const
{
    ASSERT(floatBox.isFloatingPositioned());
    // A floating candidate is start positioned when:
    // - "float: inline-start or left" in left-to-right floating context with matching floating state.
    // - "float: inline-end or right" in mismatching floating state where floating state is right-to-left.
    // (FloatingContext's direction may not be the same as the PlacedFloats's direction when dealing with inherited PlacedFloats across nested IFCs).
    auto floatingContextIsLeftToRight = root().writingMode().isBidiLTR();
    auto blockFormattingContextRootIsLeftToRight = placedFloats().blockFormattingContextRoot().style().writingMode().isBidiLTR();
    if (floatingContextIsLeftToRight == blockFormattingContextRootIsLeftToRight)
        return isStartPositioned(floatBox);

    auto floatingValue = floatBox.style().floating();
    if (floatingValue == Float::InlineStart)
        floatingValue = floatingContextIsLeftToRight ? Float::Left : Float::Right;
    else if (floatingValue == Float::InlineEnd)
        floatingValue = floatingContextIsLeftToRight ? Float::Right : Float::Left;

    return (blockFormattingContextRootIsLeftToRight && floatingValue == Float::Left) || (!blockFormattingContextRootIsLeftToRight && floatingValue == Float::Right);
}

Clear FloatingContext::clearInBlockFormattingContext(const Box& clearBox) const
{
    // See isFloatingCandidateStartPositionedInBlockFormattingContext for details.
    ASSERT(clearBox.hasFloatClear());
    auto clearBoxIsInLeftToRightDirection = root().writingMode().isBidiLTR();
    auto clearValue = clearBox.style().clear();
    if (clearValue == Clear::Both)
        return clearValue;

    if (clearValue == Clear::InlineStart)
        clearValue = clearBoxIsInLeftToRightDirection ? Clear::Left : Clear::Right;
    else if (clearValue == Clear::InlineEnd)
        clearValue = clearBoxIsInLeftToRightDirection ? Clear::Right : Clear::Left;

    auto blockFormattingContextRootIsLeftToRight = m_placedFloats.blockFormattingContextRoot().style().writingMode().isBidiLTR();
    return (blockFormattingContextRootIsLeftToRight && clearValue == Clear::Left) || (!blockFormattingContextRootIsLeftToRight && clearValue == Clear::Right) ? Clear::Left : Clear::Right;
}

FloatPair::FloatPair(const PlacedFloats::List& floats)
    : m_floats(floats)
{
}

NODELETE const PlacedFloats::Item* FloatPair::inlineStart() const
{
    if (!m_floatPair.inlineStart)
        return { };

    ASSERT(m_floats[*m_floatPair.inlineStart].isStartPositioned());
    return &m_floats[*m_floatPair.inlineStart];
}

NODELETE const PlacedFloats::Item* FloatPair::inlineEnd() const
{
    if (!m_floatPair.inlineEnd)
        return { };

    ASSERT(!m_floats[*m_floatPair.inlineEnd].isStartPositioned());
    return &m_floats[*m_floatPair.inlineEnd];
}

bool FloatPair::intersects(const FloatAvoider& floatAvoider) const
{
    auto intersects = [&](auto* floating) {
        if (!floating)
            return false;
        if (floatAvoider.inlineStart() >= floating->marginBoxRight() || floatAvoider.inlineEnd() <= floating->marginBoxLeft())
            return false;
        return floatAvoider.blockStart() >= floating->marginBoxTop() && floatAvoider.blockStart() < floating->marginBoxBottom();
    };

    ASSERT(!m_floatPair.isEmpty());
    return intersects(inlineStart()) || intersects(inlineEnd());
}

NODELETE bool FloatPair::intersects(AxisRange<ParentFormattingContextRoot> containingBlockContentBoxEdges) const
{
    ASSERT(!m_floatPair.isEmpty());

    auto inlineAxisConstraints = this->inlineAxisConstraints();
    if (inlineAxisConstraints.start && *inlineAxisConstraints.start > containingBlockContentBoxEdges.start())
        return true;

    if (inlineAxisConstraints.end && *inlineAxisConstraints.end < containingBlockContentBoxEdges.end())
        return true;

    return false;
}

bool FloatPair::containsFloatFromFormattingContext(const ElementBox& formattingContextRoot) const
{
    ASSERT(!m_floatPair.isEmpty());

    auto isInsideCurrentFormattingContext = [&](auto* floatBox) {
        return floatBox && floatBox->isInFormattingContextOf(formattingContextRoot);
    };
    return isInsideCurrentFormattingContext(inlineStart()) || isInsideCurrentFormattingContext(inlineEnd());
}

NODELETE bool FloatPair::operator ==(const FloatPair& other) const
{
    return m_floatPair.inlineStart == other.m_floatPair.inlineStart && m_floatPair.inlineEnd == other.m_floatPair.inlineEnd;
}

NODELETE FloatPair::InlineAxisConstraints FloatPair::inlineAxisConstraints() const
{
    auto startEdge = std::optional<AxisPosition<ParentFormattingContextRoot>> { };
    auto endEdge = std::optional<AxisPosition<ParentFormattingContextRoot>> { };

    if (auto* inlineStart = this->inlineStart())
        startEdge = inlineStart->marginBoxRight();

    if (auto* inlineEnd = this->inlineEnd())
        endEdge = inlineEnd->marginBoxLeft();

    return { startEdge, endEdge };
}

AxisPosition<ParentFormattingContextRoot> FloatPair::lowestBlockAxisPosition() const
{
    auto* inlineStart = this->inlineStart();
    auto* inlineEnd = this->inlineEnd();
    ASSERT(inlineStart || inlineEnd);

    auto lowestBlockAxisPositionAtInlineStart = inlineStart ? std::optional<AxisPosition<ParentFormattingContextRoot>>(inlineStart->marginBoxBottom()) : std::nullopt;
    auto lowestBlockAxisPositionAtInlineEnd = inlineEnd ? std::optional<AxisPosition<ParentFormattingContextRoot>>(inlineEnd->marginBoxBottom()) : std::nullopt;

    if (lowestBlockAxisPositionAtInlineStart && lowestBlockAxisPositionAtInlineEnd)
        return std::max(*lowestBlockAxisPositionAtInlineStart, *lowestBlockAxisPositionAtInlineEnd);

    if (lowestBlockAxisPositionAtInlineStart)
        return *lowestBlockAxisPositionAtInlineStart;

    return *lowestBlockAxisPositionAtInlineEnd;
}

Iterator::Iterator(const PlacedFloats& placedFloats, std::optional<AxisPosition<ParentFormattingContextRoot>> blockStart)
    : m_placedFloats(placedFloats)
    , m_floats(placedFloats.list())
    , m_current(placedFloats.list())
{
    if (blockStart)
        set(*blockStart);
}

NODELETE inline static std::optional<size_t> previousFloatingIndex(Float floatingType, const PlacedFloats::List& floats, size_t currentIndex)
{
    ASSERT(floatingType == Float::InlineStart || floatingType == Float::InlineEnd);
    RELEASE_ASSERT(currentIndex <= floats.size());

    while (currentIndex) {
        auto& floating = floats[--currentIndex];
        if ((floatingType == Float::InlineStart && floating.isStartPositioned()) || (floatingType == Float::InlineEnd && !floating.isStartPositioned()))
            return currentIndex;
    }

    return { };
}

Iterator& Iterator::operator++()
{
    if (m_current.isEmpty()) {
        ASSERT_NOT_REACHED();
        return *this;
    }

    auto findPreviousFloatingWithLowerOnBlockAxis = [&](auto floatingType, auto currentIndex) -> std::optional<size_t> {
        ASSERT(floatingType == Float::InlineStart || floatingType == Float::InlineEnd);
        RELEASE_ASSERT(currentIndex < m_floats.size());

        // Last floating? There's certainly no previous floating at this point.
        if (!currentIndex)
            return { };

        auto currentBlockAxisPosition = m_floats[currentIndex].marginBoxBottom();

        std::optional<size_t> index = currentIndex;
        while (true) {
            index = previousFloatingIndex(floatingType, m_floats, *index);
            if (!index)
                return { };

            if (m_floats[*index].marginBoxBottom() > currentBlockAxisPosition)
                return index;
        }

        ASSERT_NOT_REACHED();
        return { };
    };

    // 1. Take the current floating from left and right and check which one's bottom edge is positioned higher (they could be on the same vertical position too).
    // The current floats from left and right are considered the inner-most pair for the current vertical position.
    // 2. Move away from inner-most pair by picking one of the previous floats in the list(#1)
    // Ensure that the new floating bottom edge is positioned lower than the current one -which essentially means skipping in-between floats that are positioned higher).
    // 3. Reset the vertical position and align it with the new left-right pair. These floats are now the inner-most boxes for the current vertical position.
    // As the result we have more horizontal space on the current vertical position.
    auto lowestBlockAxisPositionAtInlineStart = m_current.inlineStart() ? std::optional<AxisPosition<ParentFormattingContextRoot>>(m_current.inlineStart()->marginBoxBottom()) : std::nullopt;
    auto lowestBlockAxisPositionAtInlineEnd = m_current.inlineEnd() ? std::optional<AxisPosition<ParentFormattingContextRoot>>(m_current.inlineEnd()->marginBoxBottom()) : std::nullopt;

    auto updateInlineStartSide = (lowestBlockAxisPositionAtInlineStart == lowestBlockAxisPositionAtInlineEnd) || (!lowestBlockAxisPositionAtInlineEnd || (lowestBlockAxisPositionAtInlineStart && *lowestBlockAxisPositionAtInlineStart < *lowestBlockAxisPositionAtInlineEnd));
    auto updateInlineEndSide = (lowestBlockAxisPositionAtInlineStart == lowestBlockAxisPositionAtInlineEnd) || (!lowestBlockAxisPositionAtInlineStart || (lowestBlockAxisPositionAtInlineEnd && *lowestBlockAxisPositionAtInlineStart > *lowestBlockAxisPositionAtInlineEnd));

    if (updateInlineStartSide) {
        ASSERT(m_current.m_floatPair.inlineStart);
        m_current.m_highestBlockAxisPosition = *lowestBlockAxisPositionAtInlineStart;
        m_current.m_floatPair.inlineStart = findPreviousFloatingWithLowerOnBlockAxis(Float::InlineStart, *m_current.m_floatPair.inlineStart);
    }
    
    if (updateInlineEndSide) {
        ASSERT(m_current.m_floatPair.inlineEnd);
        m_current.m_highestBlockAxisPosition = *lowestBlockAxisPositionAtInlineEnd;
        m_current.m_floatPair.inlineEnd = findPreviousFloatingWithLowerOnBlockAxis(Float::InlineEnd, *m_current.m_floatPair.inlineEnd);
    }

    return *this;
}

void Iterator::set(AxisPosition<ParentFormattingContextRoot> blockAxisPosition)
{
    // Move the iterator to the initial vertical position by starting at the inner-most floating pair (last floats on left/right).
    // 1. Check if the inner-most pair covers the vertical position.
    // 2. Move outwards from the inner-most pair until the vertical position intersects.
    m_current.m_highestBlockAxisPosition = blockAxisPosition;
    // No floats at all?
    if (m_floats.isEmpty()) {
        ASSERT_NOT_REACHED();
        m_current.m_floatPair = { };
        return;
    }

    auto findFloatingBelow = [&](auto floatingType) -> std::optional<size_t> {
        ASSERT(floatingType == Float::InlineStart || floatingType == Float::InlineEnd);

        ASSERT(!m_floats.isEmpty());
        // When no float on this side reaches down to this position, the search below would walk the whole list.
        auto lowestPositionOnBlockAxis = m_placedFloats.lowestPositionOnBlockAxis(floatingType == Float::InlineStart ? Clear::InlineStart : Clear::InlineEnd);
        if (!lowestPositionOnBlockAxis || *lowestPositionOnBlockAxis <= blockAxisPosition)
            return { };

        auto index = floatingType == Float::InlineStart ? m_current.m_floatPair.inlineStart : m_current.m_floatPair.inlineEnd;
        // Start from the end if we don't have current yet.
        index = index.value_or(m_floats.size());
        while (true) {
            index = previousFloatingIndex(floatingType, m_floats, *index);
            if (!index)
                return { };

            // Is this floating intrusive on this position?
            auto& floatItem = m_floats[*index];
            if (floatItem.marginBoxTop() <= blockAxisPosition && floatItem.marginBoxBottom() > blockAxisPosition)
                return index;
        }

        return { };
    };

    m_current.m_floatPair.inlineStart = findFloatingBelow(Float::InlineStart);
    m_current.m_floatPair.inlineEnd = findFloatingBelow(Float::InlineEnd);

    ASSERT(!m_current.m_floatPair.inlineStart || (*m_current.m_floatPair.inlineStart < m_floats.size() && m_floats[*m_current.m_floatPair.inlineStart].isStartPositioned()));
    ASSERT(!m_current.m_floatPair.inlineEnd || (*m_current.m_floatPair.inlineEnd < m_floats.size() && !m_floats[*m_current.m_floatPair.inlineEnd].isStartPositioned()));
}

NODELETE bool Iterator::operator==(const Iterator& other) const
{
    return m_current == other.m_current;
}

}
