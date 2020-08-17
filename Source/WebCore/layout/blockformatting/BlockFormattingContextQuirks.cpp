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

#include "BlockFormattingState.h"
#include "DisplayBox.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutState.h"

namespace WebCore {
namespace Layout {

static bool isQuirkContainer(const Box& layoutBox)
{
    return layoutBox.isBodyBox() || layoutBox.isDocumentBox() || layoutBox.isTableCell();
}

bool BlockFormattingContext::Quirks::needsStretching(const Box& layoutBox) const
{
    ASSERT(layoutBox.isInFlow());
    // In quirks mode, in-flow body and html stretch to the initial containing block (height: auto only).
    if (!layoutState().inQuirksMode())
        return false;

    if (!layoutBox.isDocumentBox() && !layoutBox.isBodyBox())
        return false;

    return layoutBox.style().logicalHeight().isAuto();
}

LayoutUnit BlockFormattingContext::Quirks::stretchedInFlowHeight(const Box& layoutBox, ContentHeightAndMargin contentHeightAndMargin)
{
    ASSERT(needsStretching(layoutBox));
    auto& formattingContext = this->formattingContext();
    auto nonCollapsedVerticalMargin = contentHeightAndMargin.nonCollapsedMargin.before + contentHeightAndMargin.nonCollapsedMargin.after;

    if (layoutBox.isDocumentBox()) {
        // Let's stretch the inflow document box(<html>) to the height of the initial containing block (view).
        auto documentBoxContentHeight = formattingContext.geometryForBox(layoutBox.initialContainingBlock(), EscapeReason::DocumentBoxStretchesToViewportQuirk).contentBoxHeight();
        // Document box's own vertical margin/border/padding values always shrink the content height.
        auto& documentBoxGeometry = formattingContext.geometryForBox(layoutBox);
        documentBoxContentHeight -= nonCollapsedVerticalMargin + documentBoxGeometry.verticalBorder() + documentBoxGeometry.verticalPadding().valueOr(0);
        return std::max(contentHeightAndMargin.contentHeight,  documentBoxContentHeight);
    }

    // Here is the quirky part for body box when it stretches all the way to the ICB even when the document box does not (e.g. out-of-flow positioned).
    ASSERT(layoutBox.isBodyBox());
    auto& initialContainingBlock = layoutBox.initialContainingBlock();
    auto& initialContainingBlockGeometry = formattingContext.geometryForBox(initialContainingBlock, EscapeReason::BodyStretchesToViewportQuirk);
    // Start the content height with the ICB.
    auto bodyBoxContentHeight = initialContainingBlockGeometry.contentBoxHeight();
    // Body box's own border and padding shrink the content height.
    auto& bodyBoxGeometry = formattingContext.geometryForBox(layoutBox);
    bodyBoxContentHeight -= bodyBoxGeometry.verticalBorder() + bodyBoxGeometry.verticalPadding().valueOr(0);
    // Body box never collapses its vertical margins with the document box but it might collapse its margin with its descendants.
    auto nonCollapsedMargin = contentHeightAndMargin.nonCollapsedMargin;
    auto collapsedMargin = formattingContext.marginCollapse().collapsedVerticalValues(layoutBox, nonCollapsedMargin).collapsedValues;
    auto usedVerticalMargin = collapsedMargin.before.valueOr(nonCollapsedMargin.before);
    usedVerticalMargin += collapsedMargin.isCollapsedThrough ? nonCollapsedMargin.after : collapsedMargin.after.valueOr(nonCollapsedMargin.after);
    bodyBoxContentHeight -= usedVerticalMargin;
    // Document box's padding and border also shrink the body box's content height.
    auto& documentBox = layoutBox.parent();
    auto& documentBoxGeometry = formattingContext.geometryForBox(documentBox, EscapeReason::BodyStretchesToViewportQuirk);
    bodyBoxContentHeight -= documentBoxGeometry.verticalBorder() + documentBoxGeometry.verticalPadding().valueOr(0);
    // However the non-in-flow document box's vertical margins are ignored. They don't affect the body box's content height.
    if (documentBox.isInFlow()) {
        auto geometry = this->geometry();
        auto precomputeDocumentBoxVerticalMargin = geometry.computedVerticalMargin(documentBox, geometry.constraintsForInFlowContent(initialContainingBlock, EscapeReason::BodyStretchesToViewportQuirk).horizontal);
        bodyBoxContentHeight -= precomputeDocumentBoxVerticalMargin.before.valueOr(0) + precomputeDocumentBoxVerticalMargin.after.valueOr(0);
    }
    return std::max(contentHeightAndMargin.contentHeight,  bodyBoxContentHeight);
}

bool BlockFormattingContext::Quirks::shouldIgnoreCollapsedQuirkMargin(const Box& layoutBox) const
{
    return layoutState().inQuirksMode() && isQuirkContainer(layoutBox);
}

bool BlockFormattingContext::Quirks::shouldCollapseMarginBeforeWithParentMarginBefore(const Box& layoutBox) const
{
    return layoutState().inQuirksMode() && layoutBox.style().hasMarginBeforeQuirk() && isQuirkContainer(layoutBox.containingBlock());
}

bool BlockFormattingContext::Quirks::shouldCollapseMarginAfterWithParentMarginAfter(const Box& layoutBox) const
{
    return layoutState().inQuirksMode() && layoutBox.style().hasMarginAfterQuirk() && isQuirkContainer(layoutBox.containingBlock());
}

}
}

#endif
