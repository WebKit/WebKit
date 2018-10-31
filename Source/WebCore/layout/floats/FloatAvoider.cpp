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
#include "FloatAvoider.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutFormattingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FloatAvoider);

FloatAvoider::FloatAvoider(const Box& layoutBox, const FloatingState& floatingState, const LayoutState& layoutState)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_floatingState(floatingState)
    , m_absoluteDisplayBox(FormattingContext::mapBoxToAncestor(layoutState, layoutBox, downcast<Container>(floatingState.root())))
    , m_containingBlockAbsoluteDisplayBox(layoutBox.containingBlock() == &floatingState.root() ? Display::Box(layoutState.displayBoxForLayoutBox(*layoutBox.containingBlock())) : FormattingContext::mapBoxToAncestor(layoutState, *layoutBox.containingBlock(), downcast<Container>(floatingState.root())))
    , m_initialVerticalPosition(m_absoluteDisplayBox.top())
{
    ASSERT(m_layoutBox->establishesBlockFormattingContext());
}

void FloatAvoider::setHorizontalConstraints(HorizontalConstraints horizontalConstraints)
{
    if ((isLeftAligned() && !horizontalConstraints.left) || (!isLeftAligned() && !horizontalConstraints.right)) {
        // No constraints? Set horizontal position back to the inital value.
        m_absoluteDisplayBox.setLeft(initialHorizontalPosition());
        return;
    }

    auto constrainWithContainingBlock = [&](auto left) -> PositionInContextRoot {
        // Horizontal position is constrained by the containing block's content box.
        // Compute the horizontal position for the new floating by taking both the contining block and the current left/right floats into account.
        auto containingBlockContentBoxLeft = m_containingBlockAbsoluteDisplayBox.left() + m_containingBlockAbsoluteDisplayBox.contentBoxLeft();
        if (isLeftAligned())
            return std::max(containingBlockContentBoxLeft + marginLeft(), left);

        // Make sure it does not overflow the containing block on the right.
        auto containingBlockContentBoxRight = containingBlockContentBoxLeft + m_containingBlockAbsoluteDisplayBox.contentBoxWidth();
        return std::min(left, containingBlockContentBoxRight - marginBoxWidth() + marginLeft());
    };

    auto positionCandidate = horizontalPositionCandidate(horizontalConstraints);
    m_absoluteDisplayBox.setLeft(constrainWithContainingBlock(positionCandidate));
}

void FloatAvoider::setVerticalConstraint(PositionInContextRoot verticalConstraint)
{
    m_absoluteDisplayBox.setTop(verticalPositionCandidate(verticalConstraint));
}

PositionInContextRoot FloatAvoider::horizontalPositionCandidate(HorizontalConstraints horizontalConstraints)
{
    return isLeftAligned() ? *horizontalConstraints.left : *horizontalConstraints.right - rect().width();
}

PositionInContextRoot FloatAvoider::verticalPositionCandidate(PositionInContextRoot verticalConstraint)
{
    return verticalConstraint;
}

void FloatAvoider::resetPosition()
{
    m_absoluteDisplayBox.setTopLeft({ initialHorizontalPosition(), initialVerticalPosition() });
}

PositionInContextRoot FloatAvoider::initialHorizontalPosition() const
{
    // Align the box with the containing block's content box.
    auto containingBlockContentBoxLeft = m_containingBlockAbsoluteDisplayBox.left() + m_containingBlockAbsoluteDisplayBox.contentBoxLeft();
    auto containingBlockContentBoxRight = containingBlockContentBoxLeft + m_containingBlockAbsoluteDisplayBox.contentBoxWidth();

    auto left = isLeftAligned() ? containingBlockContentBoxLeft : containingBlockContentBoxRight - marginBoxWidth();
    left += marginLeft();

    return left;
}

bool FloatAvoider::overflowsContainingBlock() const
{
    auto containingBlockContentBoxLeft = m_containingBlockAbsoluteDisplayBox.left() + m_containingBlockAbsoluteDisplayBox.contentBoxLeft();
    auto left = displayBox().left() - marginLeft();

    if (containingBlockContentBoxLeft > left)
        return true;

    auto containingBlockContentBoxRight = containingBlockContentBoxLeft + m_containingBlockAbsoluteDisplayBox.contentBoxWidth();
    auto right = displayBox().right() + marginRight();

    return containingBlockContentBoxRight < right;
}

Display::Box::Rect FloatAvoider::rectInContainingBlock() const
{
    // From formatting root coordinate system back to containing block's.
    if (layoutBox().containingBlock() == &floatingState().root())
        return m_absoluteDisplayBox.rect();

    return {
        m_absoluteDisplayBox.top() - m_containingBlockAbsoluteDisplayBox.top(),
        m_absoluteDisplayBox.left() - m_containingBlockAbsoluteDisplayBox.left(),
        m_absoluteDisplayBox.width(),
        m_absoluteDisplayBox.height()
    };
}

}
}
#endif
