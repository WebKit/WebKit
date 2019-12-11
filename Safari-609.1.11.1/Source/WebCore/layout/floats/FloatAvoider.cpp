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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FloatAvoider);

FloatAvoider::FloatAvoider(const Box& layoutBox, Display::Box absoluteDisplayBox, LayoutPoint containingBlockAbsoluteTopLeft, HorizontalEdges containingBlockAbsoluteContentBox)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_absoluteDisplayBox(absoluteDisplayBox)
    , m_containingBlockAbsoluteTopLeft(containingBlockAbsoluteTopLeft)
    , m_containingBlockAbsoluteContentBox(containingBlockAbsoluteContentBox)
{
    ASSERT(m_layoutBox->establishesBlockFormattingContext());
    m_absoluteDisplayBox.setLeft({ initialHorizontalPosition() });
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
        if (isLeftAligned())
            return std::max<PositionInContextRoot>({ m_containingBlockAbsoluteContentBox.left + marginStart() }, left);

        // Make sure it does not overflow the containing block on the right.
        return std::min<PositionInContextRoot>(left, { m_containingBlockAbsoluteContentBox.right - marginBoxWidth() + marginStart() });
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
    return { isLeftAligned() ? *horizontalConstraints.left : *horizontalConstraints.right - rect().width() };
}

PositionInContextRoot FloatAvoider::verticalPositionCandidate(PositionInContextRoot verticalConstraint)
{
    return verticalConstraint;
}

PositionInContextRoot FloatAvoider::initialHorizontalPosition() const
{
    // Align the box with the containing block's content box.
    auto left = isLeftAligned() ? m_containingBlockAbsoluteContentBox.left : m_containingBlockAbsoluteContentBox.right - marginBoxWidth();
    left += marginStart();
    return { left };
}

bool FloatAvoider::overflowsContainingBlock() const
{
    auto left = displayBox().left() - marginStart();
    if (m_containingBlockAbsoluteContentBox.left > left)
        return true;

    auto right = displayBox().right() + marginEnd();
    return m_containingBlockAbsoluteContentBox.right < right;
}

Display::Rect FloatAvoider::rectInContainingBlock() const
{
    // From formatting root coordinate system back to containing block's.
    if (m_containingBlockAbsoluteTopLeft.isZero())
        return m_absoluteDisplayBox.rect();

    return {
        m_absoluteDisplayBox.top() - m_containingBlockAbsoluteTopLeft.y(),
        m_absoluteDisplayBox.left() - m_containingBlockAbsoluteTopLeft.x(),
        m_absoluteDisplayBox.width(),
        m_absoluteDisplayBox.height()
    };
}

}
}
#endif
