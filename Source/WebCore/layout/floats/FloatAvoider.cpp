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
#include "LayoutContainerBox.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FloatAvoider);

// Floating boxes intersect their margin box with the other floats in the context,
// while other float avoiders (e.g. non-floating formatting context roots) intersect their border box.
FloatAvoider::FloatAvoider(const Box& layoutBox, LayoutPoint absoluteTopLeft, LayoutUnit borderBoxWidth, const Edges& margin, LayoutPoint containingBlockAbsoluteTopLeft, HorizontalEdges containingBlockAbsoluteContentBox)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_absoluteTopLeft(absoluteTopLeft)
    , m_borderBoxWidth(borderBoxWidth)
    , m_margin(margin)
    , m_containingBlockAbsoluteTopLeft(containingBlockAbsoluteTopLeft)
    , m_containingBlockAbsoluteContentBox(containingBlockAbsoluteContentBox)
{
    ASSERT(m_layoutBox->establishesBlockFormattingContext());
    m_absoluteTopLeft.setX(initialHorizontalPosition());
}

void FloatAvoider::setHorizontalPosition(LayoutUnit horizontalPosition)
{
    if (isLeftAligned() && isFloatingBox())
        horizontalPosition += marginStart();
    if (!isLeftAligned()) {
        horizontalPosition -= borderBoxWidth();
        if (isFloatingBox())
            horizontalPosition -= marginEnd();
    }

    auto constrainedByContainingBlock = [&] {
        // Horizontal position is constrained by the containing block's content box.
        // Compute the horizontal position for the new floating by taking both the contining block and the current left/right floats into account.
        if (isLeftAligned())
            return std::max(m_containingBlockAbsoluteContentBox.left + marginStart(), horizontalPosition);
        // Make sure it does not overflow the containing block on the right.
        return std::min(horizontalPosition, m_containingBlockAbsoluteContentBox.right - marginBoxWidth() + marginStart());
    }();
    m_absoluteTopLeft.setX(constrainedByContainingBlock);
}

void FloatAvoider::setVerticalPosition(LayoutUnit verticalPosition)
{
    if (isFloatingBox())
        verticalPosition += marginBefore();
    m_absoluteTopLeft.setY(verticalPosition);
}

LayoutUnit FloatAvoider::initialHorizontalPosition() const
{
    if (isLeftAligned())
        return { m_containingBlockAbsoluteContentBox.left + marginStart() };
    return { m_containingBlockAbsoluteContentBox.right - marginEnd() - borderBoxWidth() };
}

bool FloatAvoider::overflowsContainingBlock() const
{
    auto left = m_absoluteTopLeft.x() - marginStart();
    if (m_containingBlockAbsoluteContentBox.left > left)
        return true;

    auto right = left + marginBoxWidth();
    return m_containingBlockAbsoluteContentBox.right < right;
}

LayoutPoint FloatAvoider::topLeftInContainingBlock() const
{
    // From formatting root coordinate system back to containing block's.
    if (m_containingBlockAbsoluteTopLeft.isZero())
        return m_absoluteTopLeft;

    return { m_absoluteTopLeft.x() - m_containingBlockAbsoluteTopLeft.x(), m_absoluteTopLeft.y() - m_containingBlockAbsoluteTopLeft.y() };
}

}
}
#endif
