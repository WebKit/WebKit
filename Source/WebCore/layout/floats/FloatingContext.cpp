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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "FloatAvoider.h"
#include "FormattingContext.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FloatingContext);

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

        Optional<unsigned> left;
        Optional<unsigned> right;
    };

    bool isEmpty() const { return m_floatPair.isEmpty(); }
    const FloatingState::FloatItem* left() const;
    const FloatingState::FloatItem* right() const;
    bool intersects(const FloatAvoider&) const;
    PositionInContextRoot verticalConstraint() const { return m_verticalPosition; }

    struct HorizontalConstraints {
        Optional<PositionInContextRoot> left;
        Optional<PositionInContextRoot> right;
    };
    HorizontalConstraints horizontalConstraints() const;
    PositionInContextRoot bottom() const;
    LeftRightIndex operator*() const { return m_floatPair; };
    bool operator==(const FloatPair&) const;

private:
    friend class Iterator;
    FloatPair(const FloatingState::FloatList&);

    const FloatingState::FloatList& m_floats;
    LeftRightIndex m_floatPair;
    PositionInContextRoot m_verticalPosition;
};

class Iterator {
public:
    Iterator(const FloatingState::FloatList&, Optional<PositionInContextRoot> verticalPosition);

    const FloatPair& operator*() const { return m_current; }
    Iterator& operator++();
    bool operator==(const Iterator&) const;
    bool operator!=(const Iterator&) const;

private:
    void set(PositionInContextRoot verticalPosition);

    const FloatingState::FloatList& m_floats;
    FloatPair m_current;
};

static Iterator begin(const FloatingState::FloatList& floats, PositionInContextRoot initialVerticalPosition)
{
    // Start with the inner-most floating pair for the initial vertical position.
    return Iterator(floats, initialVerticalPosition);
}

static Iterator end(const FloatingState::FloatList& floats)
{
    return Iterator(floats, { });
}

#if ASSERT_ENABLED
static bool areFloatsHorizontallySorted(const FloatingState& floatingState)
{
    auto& floats = floatingState.floats();
    auto rightEdgeOfLeftFloats = LayoutUnit::min();
    auto leftEdgeOfRightFloats = LayoutUnit::max();
    WTF::Optional<LayoutUnit> leftBottom;
    WTF::Optional<LayoutUnit> rightBottom;

    for (auto& floatItem : floats) {
        if (floatItem.isLeftPositioned()) {
            auto rightEdge = floatItem.rectWithMargin().right();
            if (rightEdge < rightEdgeOfLeftFloats) {
                if (leftBottom && floatItem.rectWithMargin().top() < *leftBottom)
                    return false;
            }
            leftBottom = floatItem.rectWithMargin().bottom();
            rightEdgeOfLeftFloats = rightEdge;
        } else {
            auto leftEdge = floatItem.rectWithMargin().left();
            if (leftEdge > leftEdgeOfRightFloats) {
                if (rightBottom && floatItem.rectWithMargin().top() < *rightBottom)
                    return false;
            }
            rightBottom = floatItem.rectWithMargin().bottom();
            leftEdgeOfRightFloats = leftEdge;
        }
    }
    return true;
}
#endif

struct FloatingContext::AbsoluteCoordinateValuesForFloatAvoider {
    Display::Box displayBox;
    LayoutPoint containingBlockTopLeft;
    HorizontalEdges containingBlockContentBox;
};

FloatingContext::FloatingContext(const ContainerBox& floatingContextRoot, const FormattingContext& formattingContext, FloatingState& floatingState)
    : m_root(makeWeakPtr(floatingContextRoot))
    , m_formattingContext(formattingContext)
    , m_floatingState(floatingState)
{
}

Point FloatingContext::positionForFloat(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints) const
{
    ASSERT(layoutBox.isFloatingPositioned());
    ASSERT(areFloatsHorizontallySorted(m_floatingState));

    if (isEmpty()) {
        auto& boxGeometry = formattingContext().geometryForBox(layoutBox);

        auto alignWithContainingBlock = [&]() -> Position {
            // If there is no floating to align with, push the box to the left/right edge of its containing block's content box.
            if (layoutBox.isLeftFloatingPositioned())
                return { horizontalConstraints.logicalLeft + boxGeometry.marginStart() };

            return { horizontalConstraints.logicalRight() - boxGeometry.marginEnd() - boxGeometry.width() };
        };

        // No float box on the context yet -> align it with the containing block's left/right edge.
        return { alignWithContainingBlock(), boxGeometry.top() };
    }

    // Find the top most position where the float box fits.
    auto absoluteDisplayBoxCoordinates = this->absoluteDisplayBoxCoordinates(layoutBox);
    ASSERT(!isEmpty());
    auto previousFloatAbsoluteTop = floatingState().floats().last().rectWithMargin().top();
    auto absoluteDisplayBox = absoluteDisplayBoxCoordinates.displayBox;
    // Incoming float cannot be placed higher than existing floats (margin box of the last float).
    // Take the static position (where the box would go if it wasn't floating) and adjust it with the last float.
    auto adjustedTop = std::max(absoluteDisplayBox.top() - absoluteDisplayBox.marginBefore(), previousFloatAbsoluteTop);
    absoluteDisplayBox.setTop(adjustedTop + absoluteDisplayBox.marginBefore());
    auto margin = Edges { { absoluteDisplayBox.computedMarginStart().valueOr(0), absoluteDisplayBox.computedMarginEnd().valueOr(0) }, { absoluteDisplayBox.marginBefore(), absoluteDisplayBox.marginAfter() } };
    auto floatBox = FloatAvoider { layoutBox, absoluteDisplayBox.topLeft(), absoluteDisplayBox.width(), margin, absoluteDisplayBoxCoordinates.containingBlockTopLeft, absoluteDisplayBoxCoordinates.containingBlockContentBox };
    findPositionForFloatBox(floatBox);
    return floatBox.topLeftInContainingBlock();
}

Optional<Point> FloatingContext::positionForFormattingContextRoot(const Box& layoutBox) const
{
    ASSERT(layoutBox.establishesBlockFormattingContext());
    ASSERT(!layoutBox.isFloatingPositioned());
    ASSERT(!layoutBox.hasFloatClear());
    ASSERT(areFloatsHorizontallySorted(m_floatingState));

    if (isEmpty())
        return { };

    auto absoluteDisplayBoxCoordinates = this->absoluteDisplayBoxCoordinates(layoutBox);
    auto absoluteDisplayBox = absoluteDisplayBoxCoordinates.displayBox;
    auto margin = Edges { { absoluteDisplayBox.computedMarginStart().valueOr(0), absoluteDisplayBox.computedMarginEnd().valueOr(0) }, { absoluteDisplayBox.marginBefore(), absoluteDisplayBox.marginAfter() } };
    auto floatAvoider = FloatAvoider { layoutBox, absoluteDisplayBox.topLeft(), absoluteDisplayBox.width(), margin, absoluteDisplayBoxCoordinates.containingBlockTopLeft, absoluteDisplayBoxCoordinates.containingBlockContentBox };
    findPositionForFormattingContextRoot(floatAvoider);
    return { floatAvoider.topLeftInContainingBlock() };
}

FloatingContext::ClearancePosition FloatingContext::verticalPositionWithClearance(const Box& layoutBox) const
{
    ASSERT(layoutBox.hasFloatClear());
    ASSERT(layoutBox.isBlockLevelBox());
    ASSERT(areFloatsHorizontallySorted(m_floatingState));

    if (isEmpty())
        return { };

    auto bottom = [&](Optional<PositionInContextRoot> floatBottom) -> ClearancePosition {
        // 'bottom' is in the formatting root's coordinate system.
        if (!floatBottom)
            return { };

        // 9.5.2 Controlling flow next to floats: the 'clear' property
        // Then the amount of clearance is set to the greater of:
        //
        // 1. The amount necessary to place the border edge of the block even with the bottom outer edge of the lowest float that is to be cleared.
        // 2. The amount necessary to place the top border edge of the block at its hypothetical position.
        auto rootRelativeTop = mapTopToFloatingStateRoot(layoutBox);
        auto clearance = *floatBottom - rootRelativeTop;
        if (clearance <= 0)
            return { };

        // Clearance inhibits margin collapsing.
        if (auto* previousInFlowSibling = layoutBox.previousInFlowSibling()) {
            // Does this box with clearance actually collapse its margin before with the previous inflow box's margin after? 
            auto verticalMargin = formattingContext().geometryForBox(layoutBox).verticalMargin();
            if (verticalMargin.hasCollapsedValues() && verticalMargin.collapsedValues().before) {
                auto previousVerticalMargin = formattingContext().geometryForBox(*previousInFlowSibling).verticalMargin();
                auto collapsedMargin = *verticalMargin.collapsedValues().before;
                auto nonCollapsedMargin = previousVerticalMargin.after() + verticalMargin.before();
                auto marginDifference = nonCollapsedMargin - collapsedMargin;
                // Move the box to the position where it would be with non-collapsed margins.
                rootRelativeTop += marginDifference;
                // Having negative clearance is also normal. It just means that the box with the non-collapsed margins is now lower than it needs to be.
                clearance -= marginDifference;
            }
        }
        // Now adjust the box's position with the clearance.
        rootRelativeTop += clearance;
        ASSERT(*floatBottom == rootRelativeTop);

        // The return vertical position is in the containing block's coordinate system. Convert it to the formatting root's coordinate system if needed.
        if (&layoutBox.containingBlock() == &m_floatingState.root())
            return { Position { rootRelativeTop }, clearance };

        auto containingBlockRootRelativeTop = mapTopToFloatingStateRoot(layoutBox.containingBlock());
        return { Position { rootRelativeTop - containingBlockRootRelativeTop }, clearance };
    };

    auto clear = layoutBox.style().clear();
    if (clear == Clear::Left)
        return bottom(m_floatingState.leftBottom(root()));

    if (clear == Clear::Right)
        return bottom(m_floatingState.rightBottom(root()));

    if (clear == Clear::Both)
        return bottom(m_floatingState.bottom(root()));

    ASSERT_NOT_REACHED();
    return { };
}

FloatingContext::Constraints FloatingContext::constraints(LayoutUnit candidateTop, LayoutUnit candidateBottom) const
{
    if (isEmpty())
        return { };
    // 1. Convert vertical position if this floating context is inherited.
    // 2. Find the inner left/right floats at candidateTop/candidateBottom.
    // 3. Convert left/right positions back to formattingContextRoot's cooridnate system.
    auto coordinateMappingIsRequired = &floatingState().root() != &root();
    auto adjustedCandidateTop = candidateTop;
    LayoutSize adjustingDelta;
    if (coordinateMappingIsRequired) {
        auto adjustedCandidatePosition = mapPointFromFormattingContextRootToFloatingStateRoot({ 0, candidateTop });
        adjustedCandidateTop = adjustedCandidatePosition.y;
        adjustingDelta = { adjustedCandidatePosition.x, adjustedCandidateTop - candidateTop };
    }
    auto adjustedCandidateBottom = adjustedCandidateTop + (candidateBottom - candidateTop);
    auto isCandidateEmpty = adjustedCandidateTop == adjustedCandidateBottom;

    Constraints constraints;
    auto& floats = floatingState().floats();
    for (auto index = floats.size(); index--;) {
        auto& floatItem = floats[index];

        if (constraints.left && floatItem.isLeftPositioned())
            continue;

        if (constraints.right && !floatItem.isLeftPositioned())
            continue;

        auto floatBoxRect = floatItem.rectWithMargin();
        auto contains = [&] {
            if (isCandidateEmpty)
                return floatBoxRect.top() <= adjustedCandidateTop && floatBoxRect.bottom() > adjustedCandidateTop;
            return floatBoxRect.top() < adjustedCandidateBottom && floatBoxRect.bottom() > adjustedCandidateTop;
        };
        if (!contains())
            continue;

        if (floatItem.isLeftPositioned())
            constraints.left = PointInContextRoot { floatBoxRect.right(), floatBoxRect.bottom() };
        else
            constraints.right = PointInContextRoot { floatBoxRect.left(), floatBoxRect.bottom() };

        if (constraints.left && constraints.right)
            break;
    }

    if (coordinateMappingIsRequired) {
        if (constraints.left)
            constraints.left->move(-adjustingDelta);

        if (constraints.right)
            constraints.right->move(-adjustingDelta);
    }
    return constraints;
}

void FloatingContext::append(const Box& floatBox)
{
    floatingState().append(FloatingState::FloatItem { floatBox, mapToFloatingStateRoot(floatBox) });
}

static FloatPair::LeftRightIndex findAvailablePosition(FloatAvoider& floatAvoider, const FloatingState::FloatList& floats)
{
    Optional<PositionInContextRoot> bottomMost;
    Optional<FloatPair::LeftRightIndex> innerMostLeftAndRight;
    auto end = Layout::end(floats);
    for (auto iterator = begin(floats, { floatAvoider.top() }); iterator != end; ++iterator) {
        ASSERT(!(*iterator).isEmpty());
        auto leftRightFloatPair = *iterator;
        innerMostLeftAndRight = innerMostLeftAndRight.valueOr(*leftRightFloatPair);

        // Move the box horizontally so that it either
        // 1. aligns with the current floating pair
        // 2. or with the containing block's content box if there's no float to align with at this vertical position.
        auto leftRightEdge = leftRightFloatPair.horizontalConstraints();
        if (auto horizontalConstraint = floatAvoider.isLeftAligned() ? leftRightEdge.left : leftRightEdge.right)  
            floatAvoider.setHorizontalPosition(*horizontalConstraint);
        else
            floatAvoider.resetHorizontalPosition();
        floatAvoider.setVerticalPosition(leftRightFloatPair.verticalConstraint());

        // Ensure that the float avoider
        // 1. does not "overflow" its containing block with the current horiztonal constraints. It simply means that the float avoider's
        // containing block could push the candidate position beyond the current float horizontally (too far to the left/right)
        // 2. avoids floats on both sides.
        if (!floatAvoider.overflowsContainingBlock() && !leftRightFloatPair.intersects(floatAvoider))
            return *innerMostLeftAndRight;

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

void FloatingContext::findPositionForFloatBox(FloatAvoider& floatBox) const
{
    findAvailablePosition(floatBox, m_floatingState.floats());
}

void FloatingContext::findPositionForFormattingContextRoot(FloatAvoider& floatAvoider) const
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
    // 2. Check if there's any intersecing float below (forward seaching)
    // 3. Align the box with the intersected float and probe for placement again (#1). 
    auto& floats = m_floatingState.floats();
    while (true) {
        auto innerMostLeftAndRight = findAvailablePosition(floatAvoider, floats);
        if (innerMostLeftAndRight.isEmpty())
            return;

        auto overlappingFloatBox = [&floats](auto startFloatIndex, auto& floatAvoider) -> const FloatingState::FloatItem* {
            for (auto i = startFloatIndex; i < floats.size(); ++i) {
                auto& floatBox = floats[i];

                auto intersects = [&] {
                    auto floatingRect = floatBox.rectWithMargin();
                    if (floatAvoider.left() >= floatingRect.right() || floatAvoider.right() <= floatingRect.left())
                        return false;
                    return floatAvoider.top() >= floatingRect.top() && floatAvoider.top() < floatingRect.bottom();
                }();
                if (intersects)
                    return &floatBox;
            }
            return nullptr;
        };

        auto startIndex = std::max(innerMostLeftAndRight.left.valueOr(0), innerMostLeftAndRight.right.valueOr(0)) + 1;
        auto* intersectedFloatBox = overlappingFloatBox(startIndex, floatAvoider);
        if (!intersectedFloatBox)
            return;
        floatAvoider.setVerticalPosition({ intersectedFloatBox->rectWithMargin().top() });
    }
}

FloatingContext::AbsoluteCoordinateValuesForFloatAvoider FloatingContext::absoluteDisplayBoxCoordinates(const Box& floatAvoider) const
{
    auto& containingBlock = floatAvoider.containingBlock();
    auto displayBox = mapToFloatingStateRoot(floatAvoider);

    if (&containingBlock == &floatingState().root()) {
        auto containingBlockGeometry = formattingContext().geometryForBox(containingBlock, FormattingContext::EscapeReason::FloatBoxNeedsToBeInAbsoluteCoordinates);
        return { displayBox, { }, {  containingBlockGeometry.contentBoxLeft(), containingBlockGeometry.contentBoxRight() } };
    }
    auto containingBlockAbsoluteDisplayBox = mapToFloatingStateRoot(containingBlock);
    auto containingBlockLeft = containingBlockAbsoluteDisplayBox.left();
    return { displayBox, containingBlockAbsoluteDisplayBox.topLeft(), { containingBlockLeft + containingBlockAbsoluteDisplayBox.contentBoxLeft(), containingBlockLeft + containingBlockAbsoluteDisplayBox.contentBoxRight() } };
}

Display::Box FloatingContext::mapToFloatingStateRoot(const Box& floatBox) const
{
    auto& floatingStateRoot = floatingState().root();
    auto& boxGeometry = formattingContext().geometryForBox(floatBox, FormattingContext::EscapeReason::FloatBoxNeedsToBeInAbsoluteCoordinates);
    auto topLeft = boxGeometry.topLeft();
    for (auto* containingBlock = &floatBox.containingBlock(); containingBlock != &floatingStateRoot; containingBlock = &containingBlock->containingBlock())
        topLeft.moveBy(formattingContext().geometryForBox(*containingBlock, FormattingContext::EscapeReason::FloatBoxNeedsToBeInAbsoluteCoordinates).topLeft());

    auto mappedDisplayBox = Display::Box(boxGeometry);
    mappedDisplayBox.setTopLeft(topLeft);
    return mappedDisplayBox;
}

LayoutUnit FloatingContext::mapTopToFloatingStateRoot(const Box& floatBox) const
{
    auto& floatingStateRoot = floatingState().root();
    auto top = formattingContext().geometryForBox(floatBox, FormattingContext::EscapeReason::FloatBoxNeedsToBeInAbsoluteCoordinates).top();
    for (auto* containingBlock = &floatBox.containingBlock(); containingBlock != &floatingStateRoot; containingBlock = &containingBlock->containingBlock())
        top += formattingContext().geometryForBox(*containingBlock, FormattingContext::EscapeReason::FloatBoxNeedsToBeInAbsoluteCoordinates).top();
    return top;
}

Point FloatingContext::mapPointFromFormattingContextRootToFloatingStateRoot(Point position) const
{
    auto& from = root();
    auto& to = floatingState().root();
    if (&from == &to)
        return position;
    auto mappedPosition = position;
    for (auto* containingBlock = &from; containingBlock != &to; containingBlock = &containingBlock->containingBlock())
        mappedPosition.moveBy(formattingContext().geometryForBox(*containingBlock, FormattingContext::EscapeReason::FloatBoxNeedsToBeInAbsoluteCoordinates).topLeft());
    return mappedPosition;
}

FloatPair::FloatPair(const FloatingState::FloatList& floats)
    : m_floats(floats)
{
}

const FloatingState::FloatItem* FloatPair::left() const
{
    if (!m_floatPair.left)
        return nullptr;

    ASSERT(m_floats[*m_floatPair.left].isLeftPositioned());
    return &m_floats[*m_floatPair.left];
}

const FloatingState::FloatItem* FloatPair::right() const
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
        auto floatingRect = floating->rectWithMargin();
        if (floatAvoider.left() >= floatingRect.right() || floatAvoider.right() <= floatingRect.left())
            return false;
        return floatAvoider.top() >= floatingRect.top() && floatAvoider.top() < floatingRect.bottom();
    };

    ASSERT(!m_floatPair.isEmpty());
    return intersects(left()) || intersects(right());
}

bool FloatPair::operator ==(const FloatPair& other) const
{
    return m_floatPair.left == other.m_floatPair.left && m_floatPair.right == other.m_floatPair.right;
}

FloatPair::HorizontalConstraints FloatPair::horizontalConstraints() const
{
    Optional<PositionInContextRoot> leftEdge;
    Optional<PositionInContextRoot> rightEdge;

    if (left())
        leftEdge = PositionInContextRoot { left()->rectWithMargin().right() };

    if (right())
        rightEdge = PositionInContextRoot { right()->rectWithMargin().left() };

    return { leftEdge, rightEdge };
}

PositionInContextRoot FloatPair::bottom() const
{
    auto* left = this->left();
    auto* right = this->right();
    ASSERT(left || right);

    auto leftBottom = left ? Optional<PositionInContextRoot>(PositionInContextRoot { left->rectWithMargin().bottom() }) : WTF::nullopt;
    auto rightBottom = right ? Optional<PositionInContextRoot>(PositionInContextRoot { right->rectWithMargin().bottom() }) : WTF::nullopt;

    if (leftBottom && rightBottom)
        return std::max(*leftBottom, *rightBottom);

    if (leftBottom)
        return *leftBottom;

    return *rightBottom;
}

Iterator::Iterator(const FloatingState::FloatList& floats, Optional<PositionInContextRoot> verticalPosition)
    : m_floats(floats)
    , m_current(floats)
{
    if (verticalPosition)
        set(*verticalPosition);
}

inline static Optional<unsigned> previousFloatingIndex(Float floatingType, const FloatingState::FloatList& floats, unsigned currentIndex)
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

    auto findPreviousFloatingWithLowerBottom = [&](Float floatingType, unsigned currentIndex) -> Optional<unsigned> {

        RELEASE_ASSERT(currentIndex < m_floats.size());

        // Last floating? There's certainly no previous floating at this point.
        if (!currentIndex)
            return { };

        auto currentBottom = m_floats[currentIndex].rectWithMargin().bottom();

        Optional<unsigned> index = currentIndex;
        while (true) {
            index = previousFloatingIndex(floatingType, m_floats, *index);
            if (!index)
                return { };

            if (m_floats[*index].rectWithMargin().bottom() > currentBottom)
                return index;
        }

        ASSERT_NOT_REACHED();
        return { };
    };

    // 1. Take the current floating from left and right and check which one's bottom edge is positioned higher (they could be on the same vertical position too).
    // The current floats from left and right are considered the inner-most pair for the current vertical position.
    // 2. Move away from inner-most pair by picking one of the previous floats in the list(#1)
    // Ensure that the new floating's bottom edge is positioned lower than the current one -which essentially means skipping in-between floats that are positioned higher).
    // 3. Reset the vertical position and align it with the new left-right pair. These floats are now the inner-most boxes for the current vertical position.
    // As the result we have more horizontal space on the current vertical position.
    auto leftBottom = m_current.left() ? Optional<PositionInContextRoot>(m_current.left()->bottom()) : WTF::nullopt;
    auto rightBottom = m_current.right() ? Optional<PositionInContextRoot>(m_current.right()->bottom()) : WTF::nullopt;

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
    // 2. Move outwards from the inner-most pair until the vertical postion intersects.
    m_current.m_verticalPosition = verticalPosition;
    // No floats at all?
    if (m_floats.isEmpty()) {
        ASSERT_NOT_REACHED();
        m_current.m_floatPair = { };
        return;
    }

    auto findFloatingBelow = [&](Float floatingType) -> Optional<unsigned> {

        ASSERT(!m_floats.isEmpty());

        auto index = floatingType == Float::Left ? m_current.m_floatPair.left : m_current.m_floatPair.right;
        // Start from the end if we don't have current yet.
        index = index.valueOr(m_floats.size());
        while (true) {
            index = previousFloatingIndex(floatingType, m_floats, *index);
            if (!index)
                return { };

            // Is this floating intrusive on this position?
            auto rect = m_floats[*index].rectWithMargin();
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

bool Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

}
}
#endif
