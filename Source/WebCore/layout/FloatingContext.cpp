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
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
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
// 6. Repeat until either (F) fits/no more floatings.  

class Iterator;

class FloatingPair {
public:
    bool isEmpty() const { return !m_leftIndex && !m_rightIndex; }
    const Display::Box* left() const;
    const Display::Box* right() const;
    bool intersects(const Display::Box::Rect&) const;
    LayoutUnit verticalPosition() const { return m_verticalPosition; }
    LayoutUnit bottom() const;
    bool operator==(const FloatingPair&) const;

private:
    friend class Iterator;
    FloatingPair(const LayoutContext&, const FloatingState::FloatingList&);

    const LayoutContext& m_layoutContext;
    const FloatingState::FloatingList& m_floatings;

    std::optional<unsigned> m_leftIndex;
    std::optional<unsigned> m_rightIndex;
    LayoutUnit m_verticalPosition;
};

class Iterator {
public:
    Iterator(const LayoutContext&, const FloatingState::FloatingList&, std::optional<LayoutUnit> verticalPosition);

    const FloatingPair& operator*() const { return m_current; }
    Iterator& operator++();
    bool operator==(const Iterator&) const;
    bool operator!=(const Iterator&) const;

private:
    void set(LayoutUnit verticalPosition);

    const LayoutContext& m_layoutContext;
    const FloatingState::FloatingList& m_floatings;
    FloatingPair m_current;
};

static Iterator begin(const LayoutContext& layoutContext, const FloatingState& floatingState, LayoutUnit initialVerticalPosition)
{
    // Start with the inner-most floating pair for the initial vertical position.
    return Iterator(layoutContext, floatingState.floatings(), initialVerticalPosition);
}

static Iterator end(const LayoutContext& layoutContext, const FloatingState& floatingState)
{
    return Iterator(layoutContext, floatingState.floatings(), std::nullopt);
}

FloatingContext::FloatingContext(const Container& formattingContextRoot, FloatingState& floatingState)
    : m_formattingContextRoot(formattingContextRoot)
    , m_floatingState(floatingState)
{
    ASSERT(m_formattingContextRoot.establishesFormattingContext());
}

Position FloatingContext::computePosition(const Box& layoutBox) const
{
    ASSERT(layoutBox.isFloatingPositioned());

    // 1. No floating box on the context yet -> align it with the containing block's left/right edge. 
    if (m_floatingState.isEmpty()) {
        auto* displayBox = layoutContext().displayBoxForLayoutBox(layoutBox);
        return { alignWithContainingBlock(layoutBox) + displayBox->marginLeft(), displayBox->top() };
    }

    // 2. Find the top most position where the floating fits.
    return floatingPosition(layoutBox); 
}

Position FloatingContext::floatingPosition(const Box& layoutBox) const
{
    auto initialVerticalPosition = this->initialVerticalPosition(layoutBox);
    auto* displayBox = layoutContext().displayBoxForLayoutBox(layoutBox);
    auto marginBoxSize = displayBox->marginBox().size();

    auto end = Layout::end(layoutContext(), m_floatingState);
    auto top = initialVerticalPosition;
    auto bottomMost = top;
    for (auto iterator = begin(layoutContext(), m_floatingState, initialVerticalPosition); iterator != end; ++iterator) {
        ASSERT(!(*iterator).isEmpty());

        auto floatings = *iterator;
        top = floatings.verticalPosition();

        // Move the box horizontally so that it aligns with the current floating pair.
        auto left = alignWithFloatings(floatings, layoutBox);
        // Check if the box fits at this vertical position.
        if (!floatings.intersects({ top, left, marginBoxSize.width(), marginBoxSize.height() }))
            return { left + displayBox->marginLeft(), top + displayBox->marginTop() };

        bottomMost = floatings.bottom();
        // Move to the next floating pair.
    }

    // Passed all the floatings and still does not fit? 
    return { alignWithContainingBlock(layoutBox) + displayBox->marginLeft(), bottomMost + displayBox->marginTop() };
}

LayoutUnit FloatingContext::initialVerticalPosition(const Box& layoutBox) const
{
    // Incoming floating cannot be placed higher than existing floatings.
    // Take the static position (where the box would go if it wasn't floating) and adjust it with the last floating.
    auto marginBoxTop = layoutContext().displayBoxForLayoutBox(layoutBox)->rectWithMargin().top();

    if (auto lastFloating = m_floatingState.last())
        return std::max(marginBoxTop, layoutContext().displayBoxForLayoutBox(*lastFloating)->rectWithMargin().top());

    return marginBoxTop;
}

LayoutUnit FloatingContext::alignWithContainingBlock(const Box& layoutBox) const
{
    // If there is no floating to align with, push the box to the left/right edge of its containing block's content box.
    // (Either there's no floatings at all or this box does not fit at any vertical positions where the floatings are.)
    auto* displayBox = layoutContext().displayBoxForLayoutBox(layoutBox);
    auto* containingBlock = layoutBox.containingBlock();
    ASSERT(containingBlock == &m_formattingContextRoot || containingBlock->isDescendantOf(m_formattingContextRoot));

    auto* containgBlockDisplayBox = layoutContext().displayBoxForLayoutBox(*containingBlock);

    if (layoutBox.isLeftFloatingPositioned())
        return containgBlockDisplayBox->contentBoxLeft();

    return containgBlockDisplayBox->contentBoxRight() - displayBox->marginBox().width();
}

LayoutUnit FloatingContext::alignWithFloatings(const FloatingPair& floatingPair, const Box& layoutBox) const
{
    // Compute the horizontal position for the new floating by taking both the contining block and the current left/right floatings into account.
    auto* displayBox = layoutContext().displayBoxForLayoutBox(layoutBox);
    auto& containingBlock = *layoutContext().displayBoxForLayoutBox(*layoutBox.containingBlock());
    auto containingBlockContentBoxLeft = containingBlock.contentBoxLeft();
    auto containingBlockContentBoxRight = containingBlock.contentBoxRight();
    auto marginBoxWidth = displayBox->marginBox().width();

    auto leftAlignedBoxLeft = containingBlockContentBoxLeft;
    auto rightAlignedBoxLeft = containingBlockContentBoxRight - displayBox->marginBox().width();

    if (floatingPair.isEmpty()) {
        ASSERT_NOT_REACHED();
        return layoutBox.isLeftFloatingPositioned() ? leftAlignedBoxLeft : rightAlignedBoxLeft;
    }

    if (layoutBox.isLeftFloatingPositioned()) {
        if (auto* leftDisplayBox = floatingPair.left()) {
            auto leftFloatingBoxRight = leftDisplayBox->rectWithMargin().right();
            return std::min(std::max(leftAlignedBoxLeft, leftFloatingBoxRight), rightAlignedBoxLeft);
        }
        
        return leftAlignedBoxLeft;
    }

    ASSERT(layoutBox.isRightFloatingPositioned());

    if (auto* rightDisplayBox = floatingPair.right()) {
        auto rightFloatingBoxLeft = rightDisplayBox->rectWithMargin().left();
        return std::max(std::min(rightAlignedBoxLeft, rightFloatingBoxLeft) - marginBoxWidth, leftAlignedBoxLeft);
    }

    return rightAlignedBoxLeft;
}

FloatingPair::FloatingPair(const LayoutContext& layoutContext, const FloatingState::FloatingList& floatings)
    : m_layoutContext(layoutContext)
    , m_floatings(floatings)
{
}

const Display::Box* FloatingPair::left() const
{
    if (!m_leftIndex)
        return nullptr;

    ASSERT(m_floatings[*m_leftIndex]->isLeftFloatingPositioned());
    return m_layoutContext.displayBoxForLayoutBox(*m_floatings[*m_leftIndex]);
}

const Display::Box* FloatingPair::right() const
{
    if (!m_rightIndex)
        return nullptr;

    ASSERT(m_floatings[*m_rightIndex]->isRightFloatingPositioned());
    return m_layoutContext.displayBoxForLayoutBox(*m_floatings[*m_rightIndex]);
}

bool FloatingPair::intersects(const Display::Box::Rect& rect) const
{
    auto intersects = [&](const Display::Box* floating, const Display::Box::Rect& rect) {
        if (!floating)
            return false;

        return floating->rectWithMargin().intersects(rect);
    };

    if (!m_leftIndex && !m_rightIndex) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (intersects(left(), rect))
        return true;

    if (intersects(right(), rect))
        return true;

    return false;
}

bool FloatingPair::operator ==(const FloatingPair& other) const
{
    return m_leftIndex == other.m_leftIndex && m_rightIndex == other.m_rightIndex;
}

LayoutUnit FloatingPair::bottom() const
{
    auto* left = this->left();
    auto* right = this->right();
    ASSERT(left || right);

    auto leftBottom = left ? std::optional<LayoutUnit>(left->bottom()) : std::nullopt;
    auto rightBottom = right ? std::optional<LayoutUnit>(right->bottom()) : std::nullopt;

    if (leftBottom && rightBottom)
        return std::max(*leftBottom, *rightBottom);

    if (leftBottom)
        return *leftBottom;

    return *rightBottom;
}

Iterator::Iterator(const LayoutContext& layoutContext, const FloatingState::FloatingList& floatings, std::optional<LayoutUnit> verticalPosition)
    : m_layoutContext(layoutContext)
    , m_floatings(floatings)
    , m_current(layoutContext, floatings)
{
    if (verticalPosition)
        set(*verticalPosition);
}

inline static std::optional<unsigned> previousFloatingIndex(Float floatingType, const FloatingState::FloatingList& floatings, unsigned currentIndex)
{
    RELEASE_ASSERT(currentIndex <= floatings.size());

    while (currentIndex) {
        auto* floating = floatings[--currentIndex].get();
        if ((floatingType == Float::Left && floating->isLeftFloatingPositioned()) || (floatingType == Float::Right && floating->isRightFloatingPositioned()))
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

        RELEASE_ASSERT(currentIndex < m_floatings.size());

        // Last floating? There's certainly no previous floating at this point.
        if (!currentIndex)
            return { };

        auto currentBottom = m_layoutContext.displayBoxForLayoutBox(*m_floatings[currentIndex])->bottom();

        std::optional<unsigned> index = currentIndex;
        while (true) {
            index = previousFloatingIndex(floatingType, m_floatings, *index);
            if (!index)
                return { };

            if (m_layoutContext.displayBoxForLayoutBox(*m_floatings[*index])->bottom() > currentBottom)
                return index;
        }

        ASSERT_NOT_REACHED();
        return { };
    };

    // 1. Take the current floating from left and right and check which one's bottom edge is positioned higher (they could be on the same vertical position too).
    // The current floatings from left and right are considered the inner-most pair for the current vertical position.
    // 2. Move away from inner-most pair by picking one of the previous floatings in the list(#1)
    // Ensure that the new floating's bottom edge is positioned lower than the current one -which essentially means skipping in-between floats that are positioned higher).
    // 3. Reset the vertical position and align it with the new left-right pair. These floatings are now the inner-most boxes for the current vertical position.
    // As the result we have more horizontal space on the current vertical position.
    auto leftBottom = m_current.left() ? std::optional<LayoutUnit>(m_current.left()->bottom()) : std::nullopt;
    auto rightBottom = m_current.right() ? std::optional<LayoutUnit>(m_current.right()->bottom()) : std::nullopt;

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

void Iterator::set(LayoutUnit verticalPosition)
{
    // Move the iterator to the initial vertical position by starting at the inner-most floating pair (last floats on left/right).
    // 1. Check if the inner-most pair covers the vertical position.
    // 2. Move outwards from the inner-most pair until the vertical postion intersects.
    // (Note that verticalPosition has already been adjusted with the top of the last float.)

    m_current.m_verticalPosition = verticalPosition;
    // No floatings at all?
    if (m_floatings.isEmpty()) {
        ASSERT_NOT_REACHED();

        m_current.m_leftIndex = { };
        m_current.m_rightIndex = { };
        return;
    }

    auto findFloatingBelow = [&](Float floatingType) -> std::optional<unsigned> {

        ASSERT(!m_floatings.isEmpty());

        auto index = floatingType == Float::Left ? m_current.m_leftIndex : m_current.m_rightIndex;
        // Start from the end if we don't have current yet.
        index = index.value_or(m_floatings.size());
        while (true) {
            index = previousFloatingIndex(floatingType, m_floatings, *index);
            if (!index)
                return { };

            auto bottom = m_layoutContext.displayBoxForLayoutBox(*m_floatings[*index])->bottom();
            // Is this floating intrusive on this position?
            if (bottom > verticalPosition)
                return index;
        }

        return { };
    };

    m_current.m_leftIndex = findFloatingBelow(Float::Left);
    m_current.m_rightIndex = findFloatingBelow(Float::Right);

    ASSERT(!m_current.m_leftIndex || (*m_current.m_leftIndex < m_floatings.size() && m_floatings[*m_current.m_leftIndex]->isLeftFloatingPositioned()));
    ASSERT(!m_current.m_rightIndex || (*m_current.m_rightIndex < m_floatings.size() && m_floatings[*m_current.m_rightIndex]->isRightFloatingPositioned()));
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
