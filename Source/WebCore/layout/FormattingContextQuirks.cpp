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
#include "FormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "LayoutBox.h"
#include "LayoutInitialContainingBlock.h"

namespace WebCore {
namespace Layout {

LayoutUnit FormattingContext::Quirks::heightValueOfNearestContainingBlockWithFixedHeight(const Box& layoutBox)
{
    // In quirks mode, we go and travers the containing block chain to find a block level box with fixed height value, even if it means leaving
    // the current formatting context. FIXME: surely we need to do some tricks here when block direction support is added.
    auto& formattingContext = this->formattingContext();
    auto* containingBlock = &layoutBox.containingBlock();
    LayoutUnit bodyAndDocumentVerticalMarginPaddingAndBorder;
    while (containingBlock) {
        auto containingBlockHeight = containingBlock->style().logicalHeight();
        if (containingBlockHeight.isFixed())
            return LayoutUnit(containingBlockHeight.value() - bodyAndDocumentVerticalMarginPaddingAndBorder);

        // If the only fixed value box we find is the ICB, then ignore the body and the document (vertical) margin, padding and border. So much quirkiness.
        // -and it's totally insane because now we freely travel across formatting context boundaries and computed margins are nonexistent.
        if (containingBlock->isBodyBox() || containingBlock->isDocumentBox()) {

            auto geometry = formattingContext.geometry();
            auto horizontalConstraints = geometry.constraintsForInFlowContent(containingBlock->containingBlock(), FormattingContext::EscapeReason::FindFixedHeightAncestorQuirk).horizontal;
            auto verticalMargin = geometry.computedVerticalMargin(*containingBlock, horizontalConstraints);

            auto& boxGeometry = formattingContext.geometryForBox(*containingBlock, FormattingContext::EscapeReason::FindFixedHeightAncestorQuirk);
            auto verticalPadding = boxGeometry.paddingTop().valueOr(0) + boxGeometry.paddingBottom().valueOr(0);
            auto verticalBorder = boxGeometry.borderTop() + boxGeometry.borderBottom();
            bodyAndDocumentVerticalMarginPaddingAndBorder += verticalMargin.before.valueOr(0) + verticalMargin.after.valueOr(0) + verticalPadding + verticalBorder;
        }

        if (is<InitialContainingBlock>(*containingBlock))
            break;
        containingBlock = &containingBlock->containingBlock();
    }
    // Initial containing block has to have a height.
    return formattingContext.geometryForBox(layoutBox.initialContainingBlock(), FormattingContext::EscapeReason::FindFixedHeightAncestorQuirk).contentBox().height() - bodyAndDocumentVerticalMarginPaddingAndBorder;
}

}
}

#endif
