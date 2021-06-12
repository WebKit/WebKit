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
#include "FlexFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FlexFormattingGeometry.h"
#include "FlexFormattingState.h"
#include "InlineRect.h"
#include "InvalidationState.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "LayoutContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FlexFormattingContext);

FlexFormattingContext::FlexFormattingContext(const ContainerBox& formattingContextRoot, FlexFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
    , m_flexFormattingGeometry(*this)
    , m_flexFormattingQuirks(*this)
{
}

void FlexFormattingContext::layoutInFlowContent(InvalidationState&, const ConstraintsForInFlowContent& constraints)
{
    computeIntrinsicWidthConstraintsForFlexItems();
    sizeAndPlaceFlexItems(constraints);
}

LayoutUnit FlexFormattingContext::usedContentHeight() const
{
    auto& lines = formattingState().lines();
    return LayoutUnit { lines.last().bottom() - lines.first().top() };
}

IntrinsicWidthConstraints FlexFormattingContext::computedIntrinsicWidthConstraints()
{
    return { };
}

void FlexFormattingContext::sizeAndPlaceFlexItems(const ConstraintsForInFlowContent& constraints)
{
    auto& formattingState = this->formattingState();
    auto& formattingGeometry = this->formattingGeometry();
    auto flexItemMainAxisStart = constraints.horizontal().logicalLeft;
    auto flexItemMainAxisEnd = flexItemMainAxisStart;
    auto flexItemCrosAxisStart = constraints.logicalTop();
    auto flexItemCrosAxisEnd = flexItemCrosAxisStart;
    for (auto& flexItem : childrenOfType<ContainerBox>(root())) {
        ASSERT(flexItem.establishesFormattingContext());
        // FIXME: This is just a simple, let's layout the flex items and place them next to each other setup.
        auto intrinsicWidths = formattingState.intrinsicWidthConstraintsForBox(flexItem);
        auto flexItemLogicalWidth = std::min(std::max(intrinsicWidths->minimum, constraints.horizontal().logicalWidth), intrinsicWidths->maximum);
        auto flexItemConstraints = ConstraintsForInFlowContent { { { }, flexItemLogicalWidth }, { } };

        auto invalidationState = InvalidationState { };
        LayoutContext::createFormattingContext(flexItem, layoutState())->layoutInFlowContent(invalidationState, flexItemConstraints);

        auto computeFlexItemGeometry = [&] {
            auto& flexItemGeometry = formattingState.boxGeometry(flexItem);

            flexItemGeometry.setLogicalTopLeft(LayoutPoint { flexItemMainAxisEnd, flexItemCrosAxisStart });

            flexItemGeometry.setBorder(formattingGeometry.computedBorder(flexItem));
            flexItemGeometry.setPadding(formattingGeometry.computedPadding(flexItem, constraints.horizontal().logicalWidth));

            auto computedHorizontalMargin = formattingGeometry.computedHorizontalMargin(flexItem, constraints.horizontal());
            flexItemGeometry.setHorizontalMargin({ computedHorizontalMargin.start.value_or(0_lu), computedHorizontalMargin.end.value_or(0_lu) });

            auto computedVerticalMargin = formattingGeometry.computedVerticalMargin(flexItem, constraints.horizontal());
            flexItemGeometry.setVerticalMargin({ computedVerticalMargin.before.value_or(0_lu), computedVerticalMargin.after.value_or(0_lu) });

            flexItemGeometry.setContentBoxHeight(formattingGeometry.contentHeightForFormattingContextRoot(flexItem));
            flexItemGeometry.setContentBoxWidth(flexItemLogicalWidth);
            flexItemMainAxisEnd= BoxGeometry::borderBoxRect(flexItemGeometry).right();
            flexItemCrosAxisEnd = std::max(flexItemCrosAxisEnd, BoxGeometry::borderBoxRect(flexItemGeometry).bottom());
        };
        computeFlexItemGeometry();
    }
    auto flexLine = InlineRect { flexItemCrosAxisStart, flexItemMainAxisStart, flexItemMainAxisEnd - flexItemMainAxisStart, flexItemCrosAxisEnd - flexItemCrosAxisStart };
    formattingState.addLine(flexLine);
}

void FlexFormattingContext::computeIntrinsicWidthConstraintsForFlexItems()
{
    auto& formattingState = this->formattingState();
    auto& formattingGeometry = this->formattingGeometry();
    for (auto& flexItem : childrenOfType<ContainerBox>(root())) {
        if (formattingState.intrinsicWidthConstraintsForBox(flexItem))
            continue;
        formattingState.setIntrinsicWidthConstraintsForBox(flexItem, formattingGeometry.intrinsicWidthConstraints(flexItem));
    }
}

}
}

#endif
