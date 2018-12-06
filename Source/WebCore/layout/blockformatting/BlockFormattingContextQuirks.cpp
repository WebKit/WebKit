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
#include "BlockFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutFormattingState.h"

namespace WebCore {
namespace Layout {

static const Container& initialContainingBlock(const Box& layoutBox)
{
    auto* containingBlock = layoutBox.containingBlock();
    while (containingBlock->containingBlock())
        containingBlock = containingBlock->containingBlock();
    return *containingBlock;
}

static bool isQuirkContainer(const Box& layoutBox)
{
    return layoutBox.isBodyBox() || layoutBox.isDocumentBox() || layoutBox.isTableCell();
}

static bool hasMarginTopQuirkValue(const Box& layoutBox)
{
    return layoutBox.style().hasMarginBeforeQuirk();
}

bool BlockFormattingContext::Quirks::needsStretching(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow());
    // In quirks mode, body stretches to html and html to the initial containing block (height: auto only).
    if (!layoutState.inQuirksMode())
        return false;

    if (!layoutBox.isDocumentBox() && !layoutBox.isBodyBox())
        return false;

    return layoutBox.style().logicalHeight().isAuto();
}

HeightAndMargin BlockFormattingContext::Quirks::stretchedHeight(const LayoutState& layoutState, const Box& layoutBox, HeightAndMargin heightAndMargin)
{
    ASSERT(layoutBox.isDocumentBox() || layoutBox.isBodyBox());

    auto& documentBox = layoutBox.isDocumentBox() ? layoutBox : *layoutBox.parent();
    auto& documentBoxDisplayBox = layoutState.displayBoxForLayoutBox(documentBox);
    auto documentBoxVerticalBorders = documentBoxDisplayBox.borderTop() + documentBoxDisplayBox.borderBottom();
    auto documentBoxVerticalPaddings = documentBoxDisplayBox.paddingTop().value_or(0) + documentBoxDisplayBox.paddingBottom().value_or(0);

    auto strechedHeight = layoutState.displayBoxForLayoutBox(initialContainingBlock(layoutBox)).contentBoxHeight();
    strechedHeight -= documentBoxVerticalBorders + documentBoxVerticalPaddings;

    LayoutUnit totalVerticalMargins;
    if (layoutBox.isDocumentBox()) {
        auto verticalMargins = heightAndMargin.usedMarginValues();
        // Document box's margins do not collapse.
        ASSERT(!heightAndMargin.collapsedMargin);
        totalVerticalMargins = verticalMargins.top + verticalMargins.bottom;
    } else if (layoutBox.isBodyBox()) {
        // Here is the quirky part for body box:
        // Stretch the body using the initial containing block's height and shrink it with document box's margin/border/padding.
        // This looks extremely odd when html has non-auto height.
        auto verticalMargins = Geometry::estimatedMarginTop(layoutState, documentBox) + Geometry::estimatedMarginBottom(layoutState, documentBox);
        strechedHeight -= verticalMargins;

        // This quirk happens when the body height is 0 which means its vertical margins collapse through (top and bottom margins are adjoining).
        // However now that we stretch the body they don't collapse through anymore, so we need to use the non-collapsed values instead.
        auto bodyBoxVerticalMargins = heightAndMargin.height ? heightAndMargin.usedMarginValues() : heightAndMargin.nonCollapsedMargin;
        totalVerticalMargins = bodyBoxVerticalMargins.top + bodyBoxVerticalMargins.bottom;
    }

    // Stretch but never overstretch with the margins.
    if (heightAndMargin.height + totalVerticalMargins < strechedHeight)
        heightAndMargin.height = strechedHeight - totalVerticalMargins;

    return heightAndMargin;
}

bool BlockFormattingContext::Quirks::shouldIgnoreMarginTop(const LayoutState& layoutState, const Box& layoutBox)
{
    if (!layoutBox.parent())
        return false;

    return layoutState.inQuirksMode() && isQuirkContainer(*layoutBox.parent()) && hasMarginTopQuirkValue(layoutBox);
}

}
}

#endif
