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

#include "LayoutBox.h"
#include "LayoutElementBox.h"
#include "RenderObject.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(FloatAvoider);

// Floating boxes intersect their margin box with the other floats in the context,
// while other float avoiders (e.g. non-floating formatting context roots) intersect their border box.
FloatAvoider::FloatAvoider(LayoutPoint absoluteTopLeft, LayoutUnit borderBoxWidth, const BoxGeometry::Edges& margin, BoxGeometry::HorizontalEdges containingBlockAbsoluteContentBox, bool isFloatingPositioned, bool isStartAligned)
    : m_absoluteTopLeft(absoluteTopLeft)
    , m_borderBoxWidth(borderBoxWidth)
    , m_margin(margin)
    , m_containingBlockAbsoluteContentBox(containingBlockAbsoluteContentBox)
    , m_isFloatingPositioned(isFloatingPositioned)
    , m_isStartAligned(isStartAligned)
{
    m_absoluteTopLeft.setX(initialInlineStart());
}

void FloatAvoider::setInlineStart(LayoutUnit inlineStart)
{
    if (isStartAligned() && isFloatingBox())
        inlineStart += marginStart();
    if (!isStartAligned()) {
        inlineStart -= borderBoxWidth();
        if (isFloatingBox())
            inlineStart -= marginEnd();
    }

    auto constrainedByContainingBlock = [&] {
        // Horizontal position is constrained by the containing block's content box.
        // Compute the horizontal position for the new floating by taking both the contining block and the current left/right floats into account.
        if (isStartAligned())
            return std::max(m_containingBlockAbsoluteContentBox.start + marginStart(), inlineStart);
        // Make sure it does not overflow the containing block on the right.
        return std::min(inlineStart, m_containingBlockAbsoluteContentBox.end - marginBoxWidth() + marginStart());
    }();
    m_absoluteTopLeft.setX(constrainedByContainingBlock);
}

void FloatAvoider::setBlockStart(LayoutUnit blockStart)
{
    if (isFloatingBox())
        blockStart += marginBefore();
    m_absoluteTopLeft.setY(blockStart);
}

LayoutUnit FloatAvoider::initialInlineStart() const
{
    if (isStartAligned())
        return { m_containingBlockAbsoluteContentBox.start + marginStart() };
    return { m_containingBlockAbsoluteContentBox.end - marginEnd() - borderBoxWidth() };
}

bool FloatAvoider::overflowsContainingBlock() const
{
    auto left = m_absoluteTopLeft.x() - marginStart();
    if (m_containingBlockAbsoluteContentBox.start > left)
        return true;

    auto right = left + marginBoxWidth();
    return m_containingBlockAbsoluteContentBox.end < right;
}

}
}
