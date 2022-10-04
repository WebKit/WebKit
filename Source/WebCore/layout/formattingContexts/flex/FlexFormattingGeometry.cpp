/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "FlexFormattingGeometry.h"

#include "FlexFormattingContext.h"
#include "LayoutContext.h"

namespace WebCore {
namespace Layout {

FlexFormattingGeometry::FlexFormattingGeometry(const FlexFormattingContext& flexFormattingContext)
    : FormattingGeometry(flexFormattingContext)
{
}

bool FlexFormattingGeometry::isMainAxisParallelWithInlineAxis(const ElementBox& flexBox)
{
    ASSERT(flexBox.isFlexBox());
    auto flexDirection = flexBox.style().flexDirection();
    return flexDirection == FlexDirection::Row || flexBox.style().flexDirection() == FlexDirection::RowReverse;
}

bool FlexFormattingGeometry::isReversedToContentDirection(const ElementBox& flexBox)
{
    ASSERT(flexBox.isFlexBox());
    auto flexDirection = flexBox.style().flexDirection();
    return flexDirection == FlexDirection::RowReverse || flexDirection == FlexDirection::ColumnReverse;
}

IntrinsicWidthConstraints FlexFormattingGeometry::intrinsicWidthConstraints(const ElementBox& flexItem) const
{
    auto fixedMarginBorderAndPadding = [&](auto& layoutBox) {
        auto& style = layoutBox.style();
        return fixedValue(style.marginStart()).value_or(0)
            + LayoutUnit { style.borderLeftWidth() }
            + fixedValue(style.paddingLeft()).value_or(0)
            + fixedValue(style.paddingRight()).value_or(0)
            + LayoutUnit { style.borderRightWidth() }
            + fixedValue(style.marginEnd()).value_or(0);
    };

    auto computedIntrinsicWidthConstraints = [&]() -> IntrinsicWidthConstraints {
        auto logicalWidth = flexItem.style().logicalWidth();
        // Minimum/maximum width can't be depending on the containing block's width.
        auto needsResolvedContainingBlockWidth = logicalWidth.isCalculated() || logicalWidth.isPercent() || logicalWidth.isRelative();
        if (needsResolvedContainingBlockWidth)
            return { };

        if (auto width = fixedValue(logicalWidth))
            return { *width, *width };

        ASSERT(flexItem.establishesFormattingContext());
        auto& layoutState = this->layoutState();
        auto intrinsicWidthConstraints = LayoutContext::createFormattingContext(flexItem, const_cast<LayoutState&>(layoutState))->computedIntrinsicWidthConstraints();
        if (logicalWidth.isMinContent())
            return { intrinsicWidthConstraints.minimum, intrinsicWidthConstraints.minimum };
        if (logicalWidth.isMaxContent())
            return { intrinsicWidthConstraints.maximum, intrinsicWidthConstraints.maximum };
        return intrinsicWidthConstraints;
    };
    auto intrinsicWidthConstraints = constrainByMinMaxWidth(flexItem, computedIntrinsicWidthConstraints());
    intrinsicWidthConstraints.expand(fixedMarginBorderAndPadding(flexItem));
    return intrinsicWidthConstraints;
}

}
}

