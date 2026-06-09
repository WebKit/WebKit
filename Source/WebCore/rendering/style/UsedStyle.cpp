/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "UsedStyle.h"

#include "LayoutBox.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObject.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "UsedStyleReferenceSize.h"

namespace WebCore {

UsedStyle::UsedStyle(const Layout::Box& layoutBox)
    : UsedStyleProperties(layoutBox.style(), nullptr)
{
}

LayoutUnit UsedStyleProperties::resolveReferenceSize(ReferenceSize referenceSize) const
{
    switch (referenceSize) {
    case ReferenceSize::ContainingBlockWidth: {
        // The inline size of the containing block, as used to resolve percentage margins/padding.
        // containingBlockLogicalWidthForContent() handles out-of-flow, grid items and table cells
        // correctly (not just containingBlock()->contentBoxLogicalWidth()).
        ASSERT(m_renderer);
        CheckedRef renderBoxModelObject = downcast<RenderBoxModelObject>(*m_renderer);
        return renderBoxModelObject->containingBlockLogicalWidthForContent();
    }
    }

    ASSERT_NOT_REACHED();
    return 0_lu;
}

UsedFloat UsedStyle::floating() const
{
    auto writingMode = m_renderer->containingBlock()->writingMode();
    switch (computedStyle().floating()) {
    case Float::None:
        return UsedFloat::None;
    case Float::Left:
        return writingMode.isLogicalLeftLineLeft() ? UsedFloat::Left : UsedFloat::Right;
    case Float::Right:
        return writingMode.isLogicalLeftLineLeft() ? UsedFloat::Right : UsedFloat::Left;
    case Float::InlineStart:
        return writingMode.isLogicalLeftInlineStart() ? UsedFloat::Left : UsedFloat::Right;
    case Float::InlineEnd:
        return writingMode.isLogicalLeftInlineStart() ? UsedFloat::Right : UsedFloat::Left;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

UsedClear UsedStyle::clear() const
{
    auto writingMode = m_renderer->containingBlock()->writingMode();
    switch (computedStyle().clear()) {
    case Clear::None:
        return UsedClear::None;
    case Clear::Both:
        return UsedClear::Both;
    case Clear::Left:
        return writingMode.isLogicalLeftLineLeft() ? UsedClear::Left : UsedClear::Right;
    case Clear::Right:
        return writingMode.isLogicalLeftLineLeft() ? UsedClear::Right : UsedClear::Left;
    case Clear::InlineStart:
        return writingMode.isLogicalLeftInlineStart() ? UsedClear::Left : UsedClear::Right;
    case Clear::InlineEnd:
        return writingMode.isLogicalLeftInlineStart() ? UsedClear::Right : UsedClear::Left;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore
