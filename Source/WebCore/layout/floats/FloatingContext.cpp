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
#include "FloatBox.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutState.h"
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

class FloatingPair {
public:
    bool isEmpty() const { return !m_leftIndex && !m_rightIndex; }
    const FloatingState::FloatItem* left() const;
    const FloatingState::FloatItem* right() const;
    bool intersects(const Display::Box::Rect&) const;
    PositionInContextRoot verticalConstraint() const { return m_verticalPosition; }
    FloatAvoider::HorizontalConstraints horizontalConstraints() const;
    PositionInContextRoot bottom() const;
    bool operator==(const FloatingPair&) const;

private:
    friend class Iterator;
    FloatingPair(const FloatingState::FloatList&);

    const FloatingState::FloatList& m_floats;

    Optional<unsigned> m_leftIndex;
    Optional<unsigned> m_rightIndex;
    PositionInContextRoot m_verticalPosition;
};

class Iterator {
public:
    Iterator(const FloatingState::FloatList&, Optional<PositionInContextRoot> verticalPosition);

    const FloatingPair& operator*() const { return m_current; }
    Iterator& operator++();
    bool operator==(const Iterator&) const;
    bool operator!=(const Iterator&) const;

private:
    void set(PositionInContextRoot verticalPosition);

    const FloatingState::FloatList& m_floats;
    FloatingPair m_current;
};

static Iterator begin(const FloatingState& floatingState, PositionInContextRoot initialVerticalPosition)
{
    // Start with the inner-most floating pair for the initial vertical position.
    return Iterator(floatingState.floats(), initialVerticalPosition);
}

static Iterator end(const FloatingState& floatingState)
{
    return Iterator(floatingState.floats(), WTF::nullopt);
}

#ifndef NDEBUG
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

FloatingContext::FloatingContext(FloatingState& floatingState)
    : m_floatingState(floatingState)
{
}

Point FloatingContext::positionForFloat(const Box& layoutBox) const
{
    ASSERT(layoutBox.isFloatingPositioned());
    ASSERT(areFloatsHorizontallySorted(m_floatingState));

    if (m_floatingState.isEmpty()) {
        auto& displayBox = layoutState().displayBoxForLayoutBox(layoutBox);

        auto alignWithContainingBlock = [&]() -> Position {
            // If there is no floating to align with, push the box to the left/right edge of its containing block's content box.
            auto& containingBlockDisplayBox = layoutState().displayBoxForLayoutBox(*layoutBox.containingBlock());

            if (layoutBox.isLeftFloatingPositioned())
                return Position { containingBlockDisplayBox.contentBoxLeft() + displayBox.marginStart() };

            return Position { containingBlockDisplayBox.contentBoxRight() - displayBox.marginEnd() - displayBox.width() };
        };

        // No float box on the context yet -> align it with the containing block's left/right edge.
        return { alignWithContainingBlock(), displayBox.top() };
    }

    // Find the top most position where the float box fits.
    FloatBox floatBox = { layoutBox, m_floatingState, layoutState() };
    floatingPosition(floatBox);
    return floatBox.rectInContainingBlock().topLeft();
}

Optional<Point> FloatingContext::positionForFloatAvoiding(const Box& layoutBox) const
{
    ASSERT(layoutBox.establishesBlockFormattingContext());
    ASSERT(!layoutBox.isFloatingPositioned());
    ASSERT(!layoutBox.hasFloatClear());
    ASSERT(areFloatsHorizontallySorted(m_floatingState));

    if (m_floatingState.isEmpty())
        return { };

    FloatAvoider floatAvoider = { layoutBox, m_floatingState, layoutState() };
    floatingPosition(floatAvoider);
    return { floatAvoider.rectInContainingBlock().topLeft() };
}

Optional<Position> FloatingContext::verticalPositionWithClearance(const Box& layoutBox) const
{
    ASSERT(layoutBox.hasFloatClear());
    ASSERT(layoutBox.isBlockLevelBox());
    ASSERT(areFloatsHorizontallySorted(m_floatingState));

    if (m_floatingState.isEmpty())
        return { };

    auto bottom = [&](Optional<PositionInContextRoot> floatBottom) -> Optional<Position> {
        // 'bottom' is in the formatting root's coordinate system.
        if (!floatBottom)
            return { };

        // 9.5.2 Controlling flow next to floats: the 'clear' property
        // Then the amount of clearance is set to the greater of:
        //
        // 1. The amount necessary to place the border edge of the block even with the bottom outer edge of the lowest float that is to be cleared.
        // 2. The amount necessary to place the top border edge of the block at its hypothetical position.

        auto& layoutState = this->layoutState();
        auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
        auto rootRelativeTop = FormattingContext::mapTopLeftToAncestor(layoutState, layoutBox, downcast<Container>(m_floatingState.root())).y;
        auto clearance = *floatBottom - rootRelativeTop;
        if (clearance <= 0)
            return { };

        displayBox.setHasClearance();
        // Clearance inhibits margin collapsing. Let's reset the relevant adjoining margins.
        if (auto* previousInFlowSibling = layoutBox.previousInFlowSibling()) {
            auto& previousInFlowDisplayBox = layoutState.displayBoxForLayoutBox(*previousInFlowSibling);
            // Does this box with clearance actually collapse its margin before with the previous inflow box's margin after? 
            auto verticalMargin = displayBox.verticalMargin();
            if (verticalMargin.hasCollapsedValues() && verticalMargin.collapsedValues().before) {
                // Reset previous bottom after and current margin before to non-collapsing.
                auto previousVerticalMargin = previousInFlowDisplayBox.verticalMargin();
                ASSERT(previousVerticalMargin.hasCollapsedValues() && previousVerticalMargin.collapsedValues().after);

                auto collapsedMargin = *verticalMargin.collapsedValues().before;
                previousVerticalMargin.setCollapsedValues({ previousVerticalMargin.collapsedValues().before, { } });
                previousInFlowDisplayBox.setVerticalMargin(previousVerticalMargin);

                verticalMargin.setCollapsedValues({ { }, verticalMargin.collapsedValues().after });
                displayBox.setVerticalMargin(verticalMargin);

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

        // The return vertical position is in the containing block's coordinate system.
        auto containingBlockRootRelativeTop = FormattingContext::mapTopLeftToAncestor(layoutState, *layoutBox.containingBlock(), downcast<Container>(m_floatingState.root())).y;
        return Position { rootRelativeTop - containingBlockRootRelativeTop };
    };

    auto clear = layoutBox.style().clear();
    auto& formattingContextRoot = layoutBox.formattingContextRoot();

    if (clear == Clear::Left)
        return bottom(m_floatingState.leftBottom(formattingContextRoot));

    if (clear == Clear::Right)
        return bottom(m_floatingState.rightBottom(formattingContextRoot));

    if (clear == Clear::Both)
        return bottom(m_floatingState.bottom(formattingContextRoot));

    ASSERT_NOT_REACHED();
    return { };
}

void FloatingContext::floatingPosition(FloatAvoider& floatAvoider) const
{
    // Ensure the float avoider starts with no constraints.
    floatAvoider.resetPosition();

    Optional<PositionInContextRoot> bottomMost;
    auto end = Layout::end(m_floatingState);
    for (auto iterator = begin(m_floatingState, { floatAvoider.rect().top() }); iterator != end; ++iterator) {
        ASSERT(!(*iterator).isEmpty());
        auto floats = *iterator;

        // Move the box horizontally so that it either
        // 1. aligns with the current floating pair
        // 2. or with the containing block's content box if there's no float to align with at this vertical position.
        floatAvoider.setHorizontalConstraints(floats.horizontalConstraints());
        floatAvoider.setVerticalConstraint(floats.verticalConstraint());

        // Ensure that the float avoider
        // 1. does not overflow its containing block with the current horiztonal constraints
        // 2. avoids floats on both sides.
        if (!floatAvoider.overflowsContainingBlock() && !floats.intersects(floatAvoider.rect()))
            return;

        bottomMost = floats.bottom();
        // Move to the next floating pair.
    }

    // The candidate box is already below of all the floats.
    if (!bottomMost)
        return;

    // Passed all the floats and still does not fit? Push it below the last float.
    floatAvoider.setVerticalConstraint(*bottomMost);
    floatAvoider.setHorizontalConstraints({ });
}

FloatingPair::FloatingPair(const FloatingState::FloatList& floats)
    : m_floats(floats)
{
}

const FloatingState::FloatItem* FloatingPair::left() const
{
    if (!m_leftIndex)
        return nullptr;

    ASSERT(m_floats[*m_leftIndex].isLeftPositioned());
    return &m_floats[*m_leftIndex];
}

const FloatingState::FloatItem* FloatingPair::right() const
{
    if (!m_rightIndex)
        return nullptr;

    ASSERT(!m_floats[*m_rightIndex].isLeftPositioned());
    return &m_floats[*m_rightIndex];
}

bool FloatingPair::intersects(const Display::Box::Rect& candidateRect) const
{
    auto intersects = [&](const FloatingState::FloatItem* floating, Float floatingType) {
        if (!floating)
            return false;

        auto marginRect = floating->rectWithMargin();
        // Before intersecting, check if the candidate position is too far to the left/right.
        // The new float's containing block could push the candidate position beyond the current float horizontally.
        if ((floatingType == Float::Left && candidateRect.left() < marginRect.right())
            || (floatingType == Float::Right && candidateRect.right() > marginRect.left()))
            return true;
        return marginRect.intersects(candidateRect);
    };

    if (!m_leftIndex && !m_rightIndex) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (intersects(left(), Float::Left))
        return true;

    if (intersects(right(), Float::Right))
        return true;

    return false;
}

bool FloatingPair::operator ==(const FloatingPair& other) const
{
    return m_leftIndex == other.m_leftIndex && m_rightIndex == other.m_rightIndex;
}

FloatAvoider::HorizontalConstraints FloatingPair::horizontalConstraints() const
{
    Optional<PositionInContextRoot> leftEdge;
    Optional<PositionInContextRoot> rightEdge;

    if (left())
        leftEdge = PositionInContextRoot { left()->rectWithMargin().right() };

    if (right())
        rightEdge = PositionInContextRoot { right()->rectWithMargin().left() };

    return { leftEdge, rightEdge };
}

PositionInContextRoot FloatingPair::bottom() const
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

    auto updateLeft = (leftBottom == rightBottom) || (!rightBottom || (leftBottom && leftBottom < rightBottom)); 
    auto updateRight = (leftBottom == rightBottom) || (!leftBottom || (rightBottom && leftBottom > rightBottom)); 

    if (updateLeft) {
        ASSERT(m_current.m_leftIndex);
        m_current.m_verticalPosition = *leftBottom;
        m_current.m_leftIndex = findPreviousFloatingWithLowerBottom(Float::Left, *m_current.m_leftIndex);
    }
    
    if (updateRight) {
        ASSERT(m_current.m_rightIndex);
        m_current.m_verticalPosition = *rightBottom;
        m_current.m_rightIndex = findPreviousFloatingWithLowerBottom(Float::Right, *m_current.m_rightIndex);
    }

    return *this;
}

void Iterator::set(PositionInContextRoot verticalPosition)
{
    // Move the iterator to the initial vertical position by starting at the inner-most floating pair (last floats on left/right).
    // 1. Check if the inner-most pair covers the vertical position.
    // 2. Move outwards from the inner-most pair until the vertical postion intersects.
    // (Note that verticalPosition has already been adjusted with the top of the last float.)

    m_current.m_verticalPosition = verticalPosition;
    // No floats at all?
    if (m_floats.isEmpty()) {
        ASSERT_NOT_REACHED();

        m_current.m_leftIndex = { };
        m_current.m_rightIndex = { };
        return;
    }

    auto findFloatingBelow = [&](Float floatingType) -> Optional<unsigned> {

        ASSERT(!m_floats.isEmpty());

        auto index = floatingType == Float::Left ? m_current.m_leftIndex : m_current.m_rightIndex;
        // Start from the end if we don't have current yet.
        index = index.valueOr(m_floats.size());
        while (true) {
            index = previousFloatingIndex(floatingType, m_floats, *index);
            if (!index)
                return { };

            auto bottom = m_floats[*index].rectWithMargin().bottom();
            // Is this floating intrusive on this position?
            if (bottom > verticalPosition)
                return index;
        }

        return { };
    };

    m_current.m_leftIndex = findFloatingBelow(Float::Left);
    m_current.m_rightIndex = findFloatingBelow(Float::Right);

    ASSERT(!m_current.m_leftIndex || (*m_current.m_leftIndex < m_floats.size() && m_floats[*m_current.m_leftIndex].isLeftPositioned()));
    ASSERT(!m_current.m_rightIndex || (*m_current.m_rightIndex < m_floats.size() && !m_floats[*m_current.m_rightIndex].isLeftPositioned()));
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
