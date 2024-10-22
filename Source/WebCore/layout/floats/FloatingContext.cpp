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

#include "BlockFormattingContext.h"
#include "BlockFormattingState.h"
#include "FloatAvoider.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutContainingBlockChainIterator.h"
#include "LayoutElementBox.h"
#include "RenderStyleInlines.h"
#include "Shape.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(FloatingContext);

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
    struct LeftRightIndex {
        bool isEmpty() const { return !left && !right;}

        std::optional<unsigned> left;
        std::optional<unsigned> right;
    };

    bool isEmpty() const { return m_floatPair.isEmpty(); }
    const PlacedFloats::Item* left() const;
    const PlacedFloats::Item* right() const;
    bool intersects(const FloatAvoider&) const;
    bool intersects(BoxGeometry::HorizontalEdges) const;
    bool containsFloatFromFormattingContext() const;
    PositionInContextRoot verticalConstraint() const { return m_verticalPosition; }

    struct HorizontalConstraints {
        std::optional<PositionInContextRoot> left;
        std::optional<PositionInContextRoot> right;
    };
    HorizontalConstraints horizontalConstraints() const;
    PositionInContextRoot bottom() const;
    LeftRightIndex operator*() const { return m_floatPair; };
    bool operator==(const FloatPair&) const;

private:
    friend class Iterator;
    FloatPair(const PlacedFloats::List&);

    const PlacedFloats::List& m_floats;
    LeftRightIndex m_floatPair;
    PositionInContextRoot m_verticalPosition;
};

class Iterator {
public:
    Iterator(const PlacedFloats::List&, std::optional<PositionInContextRoot> verticalPosition);

    const FloatPair& operator*() const { return m_current; }
    Iterator& operator++();
    bool operator==(const Iterator&) const;

private:
    void set(PositionInContextRoot verticalPosition);

    const PlacedFloats::List& m_floats;
    FloatPair m_current;
};

static Iterator begin(const PlacedFloats::List& floats, PositionInContextRoot initialVerticalPosition)
{
    // Start with the inner-most floating pair for the initial vertical position.
    return Iterator(floats, initialVerticalPosition);
}

static Iterator end(const PlacedFloats::List& floats)
{
    return Iterator(floats, { });
}

#if ASSERT_ENABLED
static bool areFloatsHorizontallySorted(const PlacedFloats& placedFloats)
{
    auto& floats = placedFloats.list();
    auto rightEdgeOfLeftFloats = LayoutUnit::min();
    auto leftEdgeOfRightFloats = LayoutUnit::max();
    std::optional<LayoutUnit> leftBottom;
    std::optional<LayoutUnit> rightBottom;

    for (auto& floatItem : floats) {
        if (floatItem.isLeftPositioned()) {
            auto rightEdge = floatItem.absoluteRectWithMargin().right();
            if (rightEdge < rightEdgeOfLeftFloats) {
                if (leftBottom && floatItem.absoluteRectWithMargin().top() < *leftBottom)
                    return false;
            }
            leftBottom = floatItem.absoluteRectWithMargin().bottom();
            rightEdgeOfLeftFloats = rightEdge;
        } else {
            auto leftEdge = floatItem.absoluteRectWithMargin().left();
            if (leftEdge > leftEdgeOfRightFloats) {
                if (rightBottom && floatItem.absoluteRectWithMargin().top() < *rightBottom)
                    return false;
            }
            rightBottom = floatItem.absoluteRectWithMargin().bottom();
            leftEdgeOfRightFloats = leftEdge;
        }
    }
    return true;
}
#endif

static FloatPair::LeftRightIndex findAvailablePosition(FloatAvoider& floatAvoider, const PlacedFloats::List& floats, BoxGeometry::HorizontalEdges containingBlockContentBoxEdges)
{
    std::optional<PositionInContextRoot> bottomMost;
    std::optional<FloatPair::LeftRightIndex> innerMostLeftAndRight;
    auto end = Layout::end(floats);
    for (auto iterator = begin(floats, { floatAvoider.top() }); iterator != end; ++iterator) {
        ASSERT(!(*iterator).isEmpty());
        auto leftRightFloatPair = *iterator;
        innerMostLeftAndRight = innerMostLeftAndRight.value_or(*leftRightFloatPair);

        // Move the box horizontally so that it either
        // 1. aligns with the current floating pair (always constrained by containing block e.g. when current float on this position is outside of containing block i.e. not intrusive).
        // 2. or with the containing block's content box if there's no float to align with at this vertical position.
        auto leftRightEdge = leftRightFloatPair.horizontalConstraints();

        // Ensure that the float avoider
        // 1. avoids floats on both sides (with the exception of non-intrusive floats from other FCs)
        // 2. does not overflow its containing block if the horizontal position is constrained by other floats
        // (i.e. a float avoider may overflow its containing block just fine unless this overflow is the result of getting it pushed by other floats on this vertical position -out of available space)
        // 3. Move to the next floating pair if this vertical position is over-constrained.
        if (auto horizontalConstraint = floatAvoider.isLeftAligned() ? leftRightEdge.left : leftRightEdge.right)
            floatAvoider.setHorizontalPosition(*horizontalConstraint);
        else
            floatAvoider.resetHorizontalPosition();
        floatAvoider.setVerticalPosition(leftRightFloatPair.verticalConstraint());

        if (!leftRightFloatPair.intersects(floatAvoider) && !floatAvoider.overflowsContainingBlock())
            return *innerMostLeftAndRight;

        // Is this float pair is outside of our containing block's content box? In some cases we _may_ overlap them.
        if (!leftRightFloatPair.intersects(containingBlockContentBoxEdges) && !leftRightFloatPair.containsFloatFromFormattingContext()) {
            // Surprisingly floats do overlap each other on the non-floating side (e.g. float: left may overlap a float: right)
            // when they are not considered intrusive (i.e. they are outside of our containing block's content box) and coming from outside of the formatting context.
            return *innerMostLeftAndRight;
        }

        bottomMost = leftRightFloatPair.bottom();
        // Move to the next floating pair.
    }

    // The candidate box is already below of all the floats.
    if (!bottomMost)
        return { };

    // Passed all the floats and still does not fit? Push it below the last float.
    floatAvoider.setVerticalPosition(*bottomMost);
    floatAvoider.resetHorizontalPosition();
    ASSERT(innerMostLeftAndRight);
    return *innerMostLeftAndRight;
}

struct FloatingContext::AbsoluteCoordinateValuesForFloatAvoider {
    LayoutPoint topLeft;
    LayoutPoint containingBlockTopLeft;
    BoxGeometry::HorizontalEdges containingBlockContentBox;
};

FloatingContext::FloatingContext(const ElementBox& formattingContextRoot, const LayoutState& layoutState, const PlacedFloats& placedFloats)
    : m_formattingContextRoot(formattingContextRoot)
    , m_layoutState(layoutState)
    , m_placedFloats(placedFloats)
{
}

LayoutPoint FloatingContext::positionForFloat(const Box& layoutBox, const BoxGeometry& boxGeometry, const HorizontalConstraints& horizontalConstraints) const
{
    ASSERT(layoutBox.isFloatingPositioned());
    ASSERT(areFloatsHorizontallySorted(m_placedFloats));
    auto borderBoxTopLeft = BoxGeometry::borderBoxTopLeft(boxGeometry);

    if (isEmpty()) {
        auto alignWithContainingBlock = [&]() -> Position {
            // If there is no floating to align with, push the box to the left/right edge of its containing block's content box.
            if (isFloatingCandidateLeftPositionedInPlacedFloats(layoutBox))
                return { horizontalConstraints.logicalLeft + boxGeometry.marginStart() };
            return { horizontalConstraints.logicalRight() - boxGeometry.marginEnd() - boxGeometry.borderBoxWidth() };
        };
        // No float box on the context yet -> align it with the containing block's left/right edge.
        return { alignWithContainingBlock(), borderBoxTopLeft.y() };
    }

    // Find the top most position where the float box fits.
    ASSERT(!isEmpty());
    auto clearPosition = [&]() -> std::optional<LayoutUnit> {
        if (!layoutBox.hasFloatClear())
            return { };
        // The vertical position candidate needs to clear the existing floats in this context.
        switch (clearInPlacedFloats(layoutBox)) {
        case Clear::Left:
            return leftBottom();
        case Clear::Right:
            return rightBottom();
        case Clear::Both:
            return bottom();
        default:
            ASSERT_NOT_REACHED();
        }
        return { };
    };

    auto absoluteCoordinates = this->absoluteCoordinates(layoutBox, borderBoxTopLeft);
    auto absoluteTopLeft = absoluteCoordinates.topLeft;
    auto verticalPositionCandidate = absoluteTopLeft.y();
    // Incoming float cannot be placed higher than existing floats (margin box of the last float).
    // Take the static position (where the box would go if it wasn't floating) and adjust it with the last float.
    auto lastFloatAbsoluteTop = placedFloats().last()->absoluteRectWithMargin().top();
    auto lastOrClearedFloatPosition = std::max(clearPosition().value_or(lastFloatAbsoluteTop), lastFloatAbsoluteTop);
    if (verticalPositionCandidate - boxGeometry.marginBefore() < lastOrClearedFloatPosition)
        verticalPositionCandidate = lastOrClearedFloatPosition + boxGeometry.marginBefore();

    absoluteTopLeft.setY(verticalPositionCandidate);
    auto margins = BoxGeometry::Edges { { boxGeometry.marginStart(), boxGeometry.marginEnd() }, { boxGeometry.marginBefore(), boxGeometry.marginAfter() } };
    auto floatBox = FloatAvoider { absoluteTopLeft, boxGeometry.borderBoxWidth(), margins, absoluteCoordinates.containingBlockContentBox, true, isFloatingCandidateLeftPositionedInPlacedFloats(layoutBox) };
    findAvailablePosition(floatBox, m_placedFloats.list(), absoluteCoordinates.containingBlockContentBox);
    // Convert box coordinates from formatting root back to containing block.
    auto containingBlockTopLeft = absoluteCoordinates.containingBlockTopLeft;
    return { floatBox.left() + margins.horizontal.start - containingBlockTopLeft.x(), floatBox.top() + margins.vertical.before - containingBlockTopLeft.y() };
}

LayoutPoint FloatingContext::positionForNonFloatingFloatAvoider(const Box& layoutBox, const BoxGeometry& boxGeometry) const
{
    ASSERT(layoutBox.establishesBlockFormattingContext());
    ASSERT(!layoutBox.isFloatingPositioned());
    ASSERT(!layoutBox.hasFloatClear());
    ASSERT(areFloatsHorizontallySorted(m_placedFloats));

    auto borderBoxTopLeft = BoxGeometry::borderBoxTopLeft(boxGeometry);
    if (isEmpty())
        return borderBoxTopLeft;

    auto absoluteCoordinates = this->absoluteCoordinates(layoutBox, borderBoxTopLeft);
    auto margins = BoxGeometry::Edges { { boxGeometry.marginStart(), boxGeometry.marginEnd() }, { boxGeometry.marginBefore(), boxGeometry.marginAfter() } };
    auto floatAvoider = FloatAvoider { absoluteCoordinates.topLeft, boxGeometry.borderBoxWidth(), margins, absoluteCoordinates.containingBlockContentBox, false, layoutBox.writingMode().isBidiLTR() };
    findPositionForFormattingContextRoot(floatAvoider, absoluteCoordinates.containingBlockContentBox);
    auto containingBlockTopLeft = absoluteCoordinates.containingBlockTopLeft;
    return { floatAvoider.left() - containingBlockTopLeft.x(), floatAvoider.top() - containingBlockTopLeft.y() };
}

std::optional<FloatingContext::PositionWithClearance> FloatingContext::verticalPositionWithClearance(const Box& layoutBox, const BoxGeometry& boxGeometry) const
{
    ASSERT(layoutBox.hasFloatClear());
    ASSERT(areFloatsHorizontallySorted(m_placedFloats));

    if (isEmpty())
        return { };

    auto bottom = [&](auto floatBottom) -> std::optional<PositionWithClearance> {
        if (!floatBottom)
            return { };
        // 9.5.2 Controlling flow next to floats: the 'clear' property
        // Then the amount of clearance is set to the greater of:
        //
        // 1. The amount necessary to place the border edge of the block even with the bottom outer edge of the lowest float that is to be cleared.
        // 2. The amount necessary to place the top border edge of the block at its hypothetical position.
        auto logicalTopRelativeToPlacedFloatsRoot = mapTopLeftToPlacedFloatsRoot(layoutBox, BoxGeometry::borderBoxTopLeft(boxGeometry)).y();
        auto clearance = *floatBottom - logicalTopRelativeToPlacedFloatsRoot;
        if (clearance <= 0)
            return { };

        if (layoutBox.isBlockLevelBox()) {
            // Clearance inhibits margin collapsing in block formatting context.
            ASSERT_NOT_IMPLEMENTED_YET();
            // FIXME: This needs to go to BFC.
        }
        // Now adjust the box's position with the clearance.
        logicalTopRelativeToPlacedFloatsRoot += clearance;
        ASSERT(*floatBottom == logicalTopRelativeToPlacedFloatsRoot);

        // The return vertical position needs to be in the containing block's coordinate system.
        auto& containingBlock = FormattingContext::containingBlock(layoutBox);
        if (&containingBlock == &m_placedFloats.formattingContextRoot())
            return PositionWithClearance { logicalTopRelativeToPlacedFloatsRoot, clearance };

        auto containingBlockTopLeft = BoxGeometry::borderBoxTopLeft(containingBlockGeometries().geometryForBox(containingBlock));
        auto containingBlockRootRelativeTop = mapTopLeftToPlacedFloatsRoot(containingBlock, containingBlockTopLeft).y();
        return PositionWithClearance { logicalTopRelativeToPlacedFloatsRoot - containingBlockRootRelativeTop, clearance };
    };

    auto clear = clearInPlacedFloats(layoutBox);
    if (clear == Clear::Left)
        return bottom(leftBottom());

    if (clear == Clear::Right)
        return bottom(rightBottom());

    if (clear == Clear::Both)
        return bottom(this->bottom());

    ASSERT_NOT_REACHED();
    return { };
}

std::optional<LayoutUnit> FloatingContext::bottom(Clear type) const
{
    // TODO: Currently this is only called once for each formatting context root with floats per layout.
    // Cache the value if we end up calling it more frequently (and update it at append/remove).
    auto bottom = std::optional<LayoutUnit> { };
    for (auto& floatItem : placedFloats().list()) {
        if ((type == Clear::Left && !floatItem.isLeftPositioned()) || (type == Clear::Right && floatItem.isLeftPositioned()))
            continue;
        bottom = !bottom ? floatItem.absoluteRectWithMargin().bottom() : std::max(*bottom, floatItem.absoluteRectWithMargin().bottom());
    }
    return bottom;
}

std::optional<LayoutUnit> FloatingContext::top() const
{
    auto top = std::optional<LayoutUnit> { };
    for (auto& floatItem : placedFloats().list())
        top = !top ? floatItem.absoluteRectWithMargin().top() : std::min(*top, floatItem.absoluteRectWithMargin().top());
    return top;
}

FloatingContext::Constraints FloatingContext::constraints(LayoutUnit candidateTop, LayoutUnit candidateBottom, MayBeAboveLastFloat mayBeAboveLastFloat) const
{
    if (isEmpty())
        return { };
    // 1. Convert vertical position if this floating context is inherited.
    // 2. Find the inner left/right floats at candidateTop/candidateBottom. Note when MayBeAboveLastFloat is 'no', we can just stop at the inner most (last) float (block vs. inline case).
    // 3. Convert left/right positions back to formattingContextRoot's coordinate system.
    auto& placedFloats = this->placedFloats();
    auto coordinateMappingIsRequired = &placedFloats.formattingContextRoot() != &root();
    auto adjustedCandidateTop = candidateTop;
    LayoutSize adjustingDelta;
    if (coordinateMappingIsRequired) {
        auto adjustedCandidatePosition = mapPointFromFormattingContextRootToPlacedFloatsRoot({ 0, candidateTop });
        adjustedCandidateTop = adjustedCandidatePosition.y;
        adjustingDelta = { adjustedCandidatePosition.x, adjustedCandidateTop - candidateTop };
    }
    auto adjustedCandidateBottom = adjustedCandidateTop + (candidateBottom - candidateTop);
    auto candidateHeight = adjustedCandidateBottom - adjustedCandidateTop;

    auto contains = [&] (auto& floatBoxRect) {
        if (floatBoxRect.isEmpty())
            return false;
        if (!candidateHeight)
            return floatBoxRect.top() <= adjustedCandidateTop && floatBoxRect.bottom() > adjustedCandidateTop;
        return floatBoxRect.top() < adjustedCandidateBottom && floatBoxRect.bottom() > adjustedCandidateTop;
    };

    auto computeFloatEdgeAndBottom = [&](auto& floatItem) -> std::optional<std::pair<LayoutUnit, LayoutUnit>> {
        auto marginRect = floatItem.absoluteRectWithMargin();
        if (!contains(marginRect))
            return { };

        if (auto* shape = floatItem.shape()) {
            // Shapes are relative to the border box.
            auto borderRect = floatItem.absoluteBorderBoxRect();
            auto positionInShape = adjustedCandidateTop - borderRect.top();

            if (!shape->lineOverlapsShapeMarginBounds(positionInShape, candidateHeight))
                return { };

            // PolygonShape gets confused when passing in 0px height interval at vertices.
            auto segment = shape->getExcludedInterval(positionInShape, std::max(candidateHeight, 1_lu));
            if (!segment.isValid)
                return { };

            // Bottom is used to decide the next line top if nothing fits. With shape we'll just sample one pixel down.
            // FIXME: This is potentially slow.
            auto bottom = adjustedCandidateTop + 1_lu;

            if (floatItem.isLeftPositioned()) {
                auto shapeRight = borderRect.left() + LayoutUnit { segment.logicalRight };
                // Shape can't extend beyond the margin box.
                return std::pair { std::min(shapeRight, marginRect.right()), bottom };
            }
            auto shapeLeft = borderRect.left() + LayoutUnit { segment.logicalLeft };
            return std::pair { std::max(shapeLeft, marginRect.left()), bottom };
        }

        auto edge = floatItem.isLeftPositioned() ? marginRect.right() : marginRect.left();
        return std::pair { edge, marginRect.bottom() };
    };

    auto constraints = Constraints { };
    if (mayBeAboveLastFloat == MayBeAboveLastFloat::No) {
        for (auto& floatItem : makeReversedRange(placedFloats.list())) {
            if ((constraints.left && floatItem.isLeftPositioned()) || (constraints.right && !floatItem.isLeftPositioned()))
                continue;

            auto edgeAndBottom = computeFloatEdgeAndBottom(floatItem);
            if (!edgeAndBottom)
                continue;

            auto [edge, bottom] = *edgeAndBottom;

            if (floatItem.isLeftPositioned())
                constraints.left = PointInContextRoot { edge, bottom };
            else
                constraints.right = PointInContextRoot { edge, bottom };

            if ((constraints.left && constraints.right)
                || (constraints.left && !placedFloats.hasRightPositioned())
                || (constraints.right && !placedFloats.hasLeftPositioned()))
                break;
        }
    } else {
        for (auto& floatItem : makeReversedRange(placedFloats.list())) {
            auto edgeAndBottom = computeFloatEdgeAndBottom(floatItem);
            if (!edgeAndBottom)
                continue;

            auto [edge, bottom] = *edgeAndBottom;

            if (floatItem.isLeftPositioned()) {
                if (!constraints.left || constraints.left->x < edge)
                    constraints.left = PointInContextRoot { edge, bottom };
            } else {
                if (!constraints.right || constraints.right->x > edge)
                    constraints.right = PointInContextRoot { edge, bottom };
            }
            // FIXME: Bail out when floats are way above.
        }
    }

    if (coordinateMappingIsRequired) {
        if (constraints.left)
            constraints.left->move(-adjustingDelta);

        if (constraints.right)
            constraints.right->move(-adjustingDelta);
    }

    if (placedFloats.writingMode().isInlineOpposing(root().writingMode())) {
        // FIXME: Move it under coordinateMappingIsRequired when the integration codepath starts initiating the floating state with the
        // correct containing block (i.e. when the float comes from the parent BFC).

        // Flip to logical in inline direction.
        auto logicalConstraints = Constraints { };
        auto borderBoxWidth = containingBlockGeometries().geometryForBox(root()).borderBoxWidth();
        if (constraints.left)
            logicalConstraints.right = PointInContextRoot { borderBoxWidth - constraints.left->x, constraints.left->y };
        if (constraints.right)
            logicalConstraints.left = PointInContextRoot { borderBoxWidth - constraints.right->x, constraints.right->y };
        constraints = logicalConstraints;
    }
    return constraints;
}

PlacedFloats::Item FloatingContext::makeFloatItem(const Box& floatBox, const BoxGeometry& boxGeometry, std::optional<size_t> line) const
{
    auto borderBoxTopLeft = BoxGeometry::borderBoxTopLeft(boxGeometry);
    auto absoluteBoxGeometry = BoxGeometry { boxGeometry };
    absoluteBoxGeometry.setTopLeft(mapTopLeftToPlacedFloatsRoot(floatBox, borderBoxTopLeft));
    auto position = isFloatingCandidateLeftPositionedInPlacedFloats(floatBox) ? PlacedFloats::Item::Position::Left : PlacedFloats::Item::Position::Right;
    return { floatBox, position, absoluteBoxGeometry, borderBoxTopLeft, line };
}

void FloatingContext::findPositionForFormattingContextRoot(FloatAvoider& floatAvoider, BoxGeometry::HorizontalEdges containingBlockContentBoxEdges) const
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
        auto innerMostLeftAndRight = findAvailablePosition(floatAvoider, floats, containingBlockContentBoxEdges);
        if (innerMostLeftAndRight.isEmpty())
            return;

        auto overlappingFloatBox = [&floats](auto startFloatIndex, auto& floatAvoider) -> const PlacedFloats::Item* {
            for (auto i = startFloatIndex; i < floats.size(); ++i) {
                auto& floatBox = floats[i];

                auto intersects = [&] {
                    auto floatingRect = floatBox.absoluteRectWithMargin();
                    if (floatAvoider.left() >= floatingRect.right() || floatAvoider.right() <= floatingRect.left())
                        return false;
                    return floatAvoider.top() >= floatingRect.top() && floatAvoider.top() < floatingRect.bottom();
                }();
                if (intersects)
                    return &floatBox;
            }
            return nullptr;
        };

        auto startIndex = std::max(innerMostLeftAndRight.left.value_or(0), innerMostLeftAndRight.right.value_or(0)) + 1;
        auto* intersectedFloatBox = overlappingFloatBox(startIndex, floatAvoider);
        if (!intersectedFloatBox)
            return;
        floatAvoider.setVerticalPosition({ intersectedFloatBox->absoluteRectWithMargin().top() });
    }
}

FloatingContext::AbsoluteCoordinateValuesForFloatAvoider FloatingContext::absoluteCoordinates(const Box& floatAvoider, LayoutPoint borderBoxTopLeft) const
{
    auto& containingBlock = FormattingContext::containingBlock(floatAvoider);
    auto& containingBlockGeometry = containingBlockGeometries().geometryForBox(containingBlock);
    auto absoluteTopLeft = mapTopLeftToPlacedFloatsRoot(floatAvoider, borderBoxTopLeft);

    if (&containingBlock == &placedFloats().formattingContextRoot())
        return { absoluteTopLeft, { }, { containingBlockGeometry.contentBoxLeft(), containingBlockGeometry.contentBoxRight() } };

    auto containingBlockAbsoluteTopLeft = mapTopLeftToPlacedFloatsRoot(containingBlock, BoxGeometry::borderBoxTopLeft(containingBlockGeometry));
    return { absoluteTopLeft, containingBlockAbsoluteTopLeft, { containingBlockAbsoluteTopLeft.x() + containingBlockGeometry.contentBoxLeft(), containingBlockAbsoluteTopLeft.x() + containingBlockGeometry.contentBoxRight() } };
}

LayoutPoint FloatingContext::mapTopLeftToPlacedFloatsRoot(const Box& layoutBox, LayoutPoint borderBoxTopLeft) const
{
    ASSERT(layoutBox.isFloatingPositioned() || layoutBox.isInFlow());
    auto& placedFloatsRoot = placedFloats().formattingContextRoot();
    for (auto& containingBlock : containingBlockChain(layoutBox, placedFloatsRoot))
        borderBoxTopLeft.moveBy(BoxGeometry::borderBoxTopLeft(containingBlockGeometries().geometryForBox(containingBlock)));
    return borderBoxTopLeft;
}

Point FloatingContext::mapPointFromFormattingContextRootToPlacedFloatsRoot(Point position) const
{
    auto& from = root();
    auto& to = placedFloats().formattingContextRoot();
    if (&from == &to)
        return position;
    auto mappedPosition = position;
    for (auto* containingBlock = &from; containingBlock != &to; containingBlock = &FormattingContext::containingBlock(*containingBlock))
        mappedPosition.moveBy(BoxGeometry::borderBoxTopLeft(containingBlockGeometries().geometryForBox(*containingBlock)));
    return mappedPosition;
}

bool FloatingContext::isLogicalLeftPositioned(const Box& floatBox) const
{
    ASSERT(floatBox.isFloatingPositioned());
    // Note that this returns true relative to the root of this FloatingContext and not to the PlacedFloats
    // PlacedFloats's root may be an ancestor block container with mismatching inline direction.
    auto floatingBoxIsInLeftToRightDirection = root().writingMode().isBidiLTR();
    auto floatingValue = floatBox.style().floating();
    return floatingValue == Float::InlineStart
        || (floatingBoxIsInLeftToRightDirection && floatingValue == Float::Left)
        || (!floatingBoxIsInLeftToRightDirection && floatingValue == Float::Right);
}

bool FloatingContext::isFloatingCandidateLeftPositionedInPlacedFloats(const Box& floatBox) const
{
    ASSERT(floatBox.isFloatingPositioned());
    // A floating candidate is logically left positioned when:
    // - "float: left" in left-to-right floating state
    // - "float: inline-start" inline left-to-right floating state
    // If the floating state is right-to-left (meaning that the PlacedFloats is constructed by a BFC root with "direction: rtl")
    // visually left positioned floats are logically right (Note that FloatingContext's direction may not be the same as the PlacedFloats's direction
    // when dealing with inherited PlacedFloatss across nested IFCs).
    auto floatingContextIsLeftToRight = root().writingMode().isBidiLTR();
    auto placedFloatsIsLeftToRight = m_placedFloats.writingMode().isBidiLTR();
    if (floatingContextIsLeftToRight == placedFloatsIsLeftToRight)
        return isLogicalLeftPositioned(floatBox);

    auto floatingValue = floatBox.style().floating();
    if (floatingValue == Float::InlineStart)
        floatingValue = floatingContextIsLeftToRight ? Float::Left : Float::Right;
    else if (floatingValue == Float::InlineEnd)
        floatingValue = floatingContextIsLeftToRight ? Float::Right : Float::Left;
    return (placedFloatsIsLeftToRight && floatingValue == Float::Left)
        || (!placedFloatsIsLeftToRight && floatingValue == Float::Right);
}

Clear FloatingContext::clearInPlacedFloats(const Box& clearBox) const
{
    // See isFloatingCandidateLeftPositionedInPlacedFloats for details.
    ASSERT(clearBox.hasFloatClear());
    auto clearBoxIsInLeftToRightDirection = root().writingMode().isBidiLTR();
    auto clearValue = clearBox.style().clear();
    if (clearValue == Clear::Both)
        return clearValue;

    if (clearValue == Clear::InlineStart)
        clearValue = clearBoxIsInLeftToRightDirection ? Clear::Left : Clear::Right;
    else if (clearValue == Clear::InlineEnd)
        clearValue = clearBoxIsInLeftToRightDirection ? Clear::Right : Clear::Left;

    auto floatsAreInLeftToRightDirection = m_placedFloats.writingMode().isBidiLTR();
    return (floatsAreInLeftToRightDirection && clearValue == Clear::Left)
        || (!floatsAreInLeftToRightDirection && clearValue == Clear::Right) ? Clear::Left : Clear::Right;
}

FloatPair::FloatPair(const PlacedFloats::List& floats)
    : m_floats(floats)
{
}

const PlacedFloats::Item* FloatPair::left() const
{
    if (!m_floatPair.left)
        return nullptr;

    ASSERT(m_floats[*m_floatPair.left].isLeftPositioned());
    return &m_floats[*m_floatPair.left];
}

const PlacedFloats::Item* FloatPair::right() const
{
    if (!m_floatPair.right)
        return nullptr;

    ASSERT(!m_floats[*m_floatPair.right].isLeftPositioned());
    return &m_floats[*m_floatPair.right];
}

bool FloatPair::intersects(const FloatAvoider& floatAvoider) const
{
    auto intersects = [&](auto* floating) {
        if (!floating)
            return false;
        auto floatingRect = floating->absoluteRectWithMargin();
        if (floatAvoider.left() >= floatingRect.right() || floatAvoider.right() <= floatingRect.left())
            return false;
        return floatAvoider.top() >= floatingRect.top() && floatAvoider.top() < floatingRect.bottom();
    };

    ASSERT(!m_floatPair.isEmpty());
    return intersects(left()) || intersects(right());
}

bool FloatPair::intersects(BoxGeometry::HorizontalEdges containingBlockContentBoxEdges) const
{
    ASSERT(!m_floatPair.isEmpty());

    auto leftRightEdge = horizontalConstraints();
    if (leftRightEdge.left && *leftRightEdge.left > containingBlockContentBoxEdges.start)
        return true;

    if (leftRightEdge.right && *leftRightEdge.right < containingBlockContentBoxEdges.end)
        return true;

    return false;
}

bool FloatPair::containsFloatFromFormattingContext() const
{
    ASSERT(!m_floatPair.isEmpty());

    auto isInsideCurrentFormattingContext = [&](auto* floatBox) {
        // FIXME: This should be a tree traversal on the ancestor chain to see if this belongs to the
        // FloatingContext's formatting context (e.g. float may come from parent and sibling formatting contexts)
        return floatBox && floatBox->layoutBox();
    };
    return isInsideCurrentFormattingContext(left()) || isInsideCurrentFormattingContext(right());
}

bool FloatPair::operator ==(const FloatPair& other) const
{
    return m_floatPair.left == other.m_floatPair.left && m_floatPair.right == other.m_floatPair.right;
}

FloatPair::HorizontalConstraints FloatPair::horizontalConstraints() const
{
    std::optional<PositionInContextRoot> leftEdge;
    std::optional<PositionInContextRoot> rightEdge;

    if (left())
        leftEdge = PositionInContextRoot { left()->absoluteRectWithMargin().right() };

    if (right())
        rightEdge = PositionInContextRoot { right()->absoluteRectWithMargin().left() };

    return { leftEdge, rightEdge };
}

PositionInContextRoot FloatPair::bottom() const
{
    auto* left = this->left();
    auto* right = this->right();
    ASSERT(left || right);

    auto leftBottom = left ? std::optional<PositionInContextRoot>(PositionInContextRoot { left->absoluteRectWithMargin().bottom() }) : std::nullopt;
    auto rightBottom = right ? std::optional<PositionInContextRoot>(PositionInContextRoot { right->absoluteRectWithMargin().bottom() }) : std::nullopt;

    if (leftBottom && rightBottom)
        return std::max(*leftBottom, *rightBottom);

    if (leftBottom)
        return *leftBottom;

    return *rightBottom;
}

Iterator::Iterator(const PlacedFloats::List& floats, std::optional<PositionInContextRoot> verticalPosition)
    : m_floats(floats)
    , m_current(floats)
{
    if (verticalPosition)
        set(*verticalPosition);
}

inline static std::optional<unsigned> previousFloatingIndex(Float floatingType, const PlacedFloats::List& floats, unsigned currentIndex)
{
    RELEASE_ASSERT(currentIndex <= floats.size());

    while (currentIndex) {
        auto& floating = floats[--currentIndex];
        if ((floatingType == Float::Left && floating.isLeftPositioned()) || (floatingType == Float::Right && !floating.isLeftPositioned()))
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

    auto findPreviousFloatingWithLowerBottom = [&](Float floatingType, unsigned currentIndex) -> std::optional<unsigned> {

        RELEASE_ASSERT(currentIndex < m_floats.size());

        // Last floating? There's certainly no previous floating at this point.
        if (!currentIndex)
            return { };

        auto currentBottom = m_floats[currentIndex].absoluteRectWithMargin().bottom();

        std::optional<unsigned> index = currentIndex;
        while (true) {
            index = previousFloatingIndex(floatingType, m_floats, *index);
            if (!index)
                return { };

            if (m_floats[*index].absoluteRectWithMargin().bottom() > currentBottom)
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
    auto leftBottom = m_current.left() ? std::optional<PositionInContextRoot>(m_current.left()->absoluteBottom()) : std::nullopt;
    auto rightBottom = m_current.right() ? std::optional<PositionInContextRoot>(m_current.right()->absoluteBottom()) : std::nullopt;

    auto updateLeft = (leftBottom == rightBottom) || (!rightBottom || (leftBottom && *leftBottom < *rightBottom));
    auto updateRight = (leftBottom == rightBottom) || (!leftBottom || (rightBottom && *leftBottom > *rightBottom));

    if (updateLeft) {
        ASSERT(m_current.m_floatPair.left);
        m_current.m_verticalPosition = *leftBottom;
        m_current.m_floatPair.left = findPreviousFloatingWithLowerBottom(Float::Left, *m_current.m_floatPair.left);
    }
    
    if (updateRight) {
        ASSERT(m_current.m_floatPair.right);
        m_current.m_verticalPosition = *rightBottom;
        m_current.m_floatPair.right = findPreviousFloatingWithLowerBottom(Float::Right, *m_current.m_floatPair.right);
    }

    return *this;
}

void Iterator::set(PositionInContextRoot verticalPosition)
{
    // Move the iterator to the initial vertical position by starting at the inner-most floating pair (last floats on left/right).
    // 1. Check if the inner-most pair covers the vertical position.
    // 2. Move outwards from the inner-most pair until the vertical position intersects.
    m_current.m_verticalPosition = verticalPosition;
    // No floats at all?
    if (m_floats.isEmpty()) {
        ASSERT_NOT_REACHED();
        m_current.m_floatPair = { };
        return;
    }

    auto findFloatingBelow = [&](Float floatingType) -> std::optional<unsigned> {

        ASSERT(!m_floats.isEmpty());

        auto index = floatingType == Float::Left ? m_current.m_floatPair.left : m_current.m_floatPair.right;
        // Start from the end if we don't have current yet.
        index = index.value_or(m_floats.size());
        while (true) {
            index = previousFloatingIndex(floatingType, m_floats, *index);
            if (!index)
                return { };

            // Is this floating intrusive on this position?
            auto rect = m_floats[*index].absoluteRectWithMargin();
            if (rect.top() <= verticalPosition && rect.bottom() > verticalPosition)
                return index;
        }

        return { };
    };

    m_current.m_floatPair.left = findFloatingBelow(Float::Left);
    m_current.m_floatPair.right = findFloatingBelow(Float::Right);

    ASSERT(!m_current.m_floatPair.left || (*m_current.m_floatPair.left < m_floats.size() && m_floats[*m_current.m_floatPair.left].isLeftPositioned()));
    ASSERT(!m_current.m_floatPair.right || (*m_current.m_floatPair.right < m_floats.size() && !m_floats[*m_current.m_floatPair.right].isLeftPositioned()));
}

bool Iterator::operator==(const Iterator& other) const
{
    return m_current == other.m_current;
}

}
}
