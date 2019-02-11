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
#include "LayoutState.h"

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

bool BlockFormattingContext::Quirks::needsStretching(const LayoutState& layoutState, const Box& layoutBox)
{
    // In quirks mode, body stretches to html and html to the initial containing block (height: auto only).
    if (!layoutState.inQuirksMode())
        return false;

    if (!layoutBox.isDocumentBox() && !layoutBox.isBodyBox())
        return false;

    return layoutBox.style().logicalHeight().isAuto();
}

HeightAndMargin BlockFormattingContext::Quirks::stretchedInFlowHeight(const LayoutState& layoutState, const Box& layoutBox, HeightAndMargin heightAndMargin)
{
    ASSERT(layoutBox.isInFlow());
    ASSERT(layoutBox.isDocumentBox() || layoutBox.isBodyBox());

    auto& documentBox = layoutBox.isDocumentBox() ? layoutBox : *layoutBox.parent();
    auto& documentBoxDisplayBox = layoutState.displayBoxForLayoutBox(documentBox);

    auto& initialContainingBlockDisplayBox = layoutState.displayBoxForLayoutBox(initialContainingBlock(layoutBox));
    auto strechedHeight = initialContainingBlockDisplayBox.contentBoxHeight();
    strechedHeight -= documentBoxDisplayBox.verticalBorder() + documentBoxDisplayBox.verticalPadding().valueOr(0);

    LayoutUnit totalVerticalMargin;
    if (layoutBox.isDocumentBox()) {
        // Document box's margins do not collapse.
        auto verticalMargin = heightAndMargin.nonCollapsedMargin;
        totalVerticalMargin = verticalMargin.before + verticalMargin.after;
    } else if (layoutBox.isBodyBox()) {
        // Here is the quirky part for body box:
        // Stretch the body using the initial containing block's height and shrink it with document box's margin/border/padding.
        // This looks extremely odd when html has non-auto height.
        auto documentBoxVerticalMargin = Geometry::computedVerticalMargin(documentBox, UsedHorizontalValues { initialContainingBlockDisplayBox.contentBoxWidth() });
        strechedHeight -= (documentBoxVerticalMargin.before.valueOr(0) + documentBoxVerticalMargin.after.valueOr(0));

        auto& bodyBoxDisplayBox = layoutState.displayBoxForLayoutBox(layoutBox);
        strechedHeight -= bodyBoxDisplayBox.verticalBorder() + bodyBoxDisplayBox.verticalPadding().valueOr(0);

        auto nonCollapsedMargin = heightAndMargin.nonCollapsedMargin;
        auto collapsedMargin = MarginCollapse::collapsedVerticalValues(layoutState, layoutBox, nonCollapsedMargin);
        totalVerticalMargin = collapsedMargin.before.valueOr(nonCollapsedMargin.before);
        totalVerticalMargin += collapsedMargin.isCollapsedThrough ? nonCollapsedMargin.after : collapsedMargin.after.valueOr(nonCollapsedMargin.after);
    }

    // Stretch but never overstretch with the margins.
    if (heightAndMargin.height + totalVerticalMargin < strechedHeight)
        heightAndMargin.height = strechedHeight - totalVerticalMargin;

    return heightAndMargin;
}

bool BlockFormattingContext::Quirks::shouldIgnoreCollapsedQuirkMargin(const LayoutState& layoutState, const Box& layoutBox)
{
    return layoutState.inQuirksMode() && isQuirkContainer(layoutBox);
}

}
}

#endif
