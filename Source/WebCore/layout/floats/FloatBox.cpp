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
#include "FloatBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FloatBox);

FloatBox::FloatBox(const Box& layoutBox, const FloatingState& floatingState, const LayoutContext& layoutContext)
    : m_layoutBox(makeWeakPtr(const_cast<Box&>(layoutBox)))
    , m_floatingState(floatingState)
    , m_absoluteDisplayBox(FormattingContext::mapBoxToAncestor(layoutContext, layoutBox, downcast<Container>(floatingState.root())))
    , m_containingBlockAbsoluteDisplayBox(FormattingContext::mapBoxToAncestor(layoutContext, *layoutBox.containingBlock(), downcast<Container>(floatingState.root())))
{
    initializePosition();
}

void FloatBox::initializePosition()
{
    resetVertically();
    resetHorizontally();
}

bool FloatBox::isLeftAligned() const
{
    return m_layoutBox->isLeftFloatingPositioned();
}

void FloatBox::setLeft(PositionInContextRoot left)
{
    // Horizontal position is constrained by the containing block's content box.
    // Compute the horizontal position for the new floating by taking both the contining block and the current left/right floats into account.
    auto containingBlockContentBoxLeft = m_containingBlockAbsoluteDisplayBox.left() + m_containingBlockAbsoluteDisplayBox.contentBoxLeft();
    auto containingBlockContentBoxRight = containingBlockContentBoxLeft + m_containingBlockAbsoluteDisplayBox.contentBoxWidth();

    // Align it with the containing block's left edge first.
    left = std::max(containingBlockContentBoxLeft + marginLeft(), left);
    // Make sure it does not overflow the containing block on the right.
    auto marginBoxSize = m_absoluteDisplayBox.marginBox().width();
    left = std::min(left, containingBlockContentBoxRight - marginBoxSize + marginLeft());

    m_absoluteDisplayBox.setLeft(left);
}

void FloatBox::setTopLeft(PointInContextRoot topLeft)
{
    setTop(topLeft.y);
    setLeft(topLeft.x);
}

void FloatBox::resetVertically()
{
    // Incoming float cannot be placed higher than existing floats (margin box of the last float).
    // Take the static position (where the box would go if it wasn't floating) and adjust it with the last float.
    auto top = m_absoluteDisplayBox.rectWithMargin().top();
    if (auto lastFloat = m_floatingState.last())
        top = std::max(top, lastFloat->displayBox().rectWithMargin().top());
    top += m_absoluteDisplayBox.marginTop();

    m_absoluteDisplayBox.setTop(top);
}

void FloatBox::resetHorizontally()
{
    // Align the box with the containing block's content box.
    auto containingBlockContentBoxLeft = m_containingBlockAbsoluteDisplayBox.left() + m_containingBlockAbsoluteDisplayBox.contentBoxLeft();
    auto containingBlockContentBoxRight = containingBlockContentBoxLeft + m_containingBlockAbsoluteDisplayBox.contentBoxWidth();

    auto left = isLeftAligned() ? containingBlockContentBoxLeft : containingBlockContentBoxRight - m_absoluteDisplayBox.marginBox().width();
    left += m_absoluteDisplayBox.marginLeft();

    m_absoluteDisplayBox.setLeft(left);
}

PointInContainingBlock FloatBox::topLeftInContainingBlock() const
{
    // From formatting root coordinate system back to containing block's.
    if (m_layoutBox->containingBlock() == &m_floatingState.root())
        return m_absoluteDisplayBox.topLeft();

    return { m_absoluteDisplayBox.left() - m_containingBlockAbsoluteDisplayBox.left(), m_absoluteDisplayBox.top() - m_containingBlockAbsoluteDisplayBox.top() };
}

}
}
#endif
