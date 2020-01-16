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

bool BlockFormattingContext::Quirks::needsStretching(const Box& layoutBox) const
{
    // In quirks mode, body stretches to html and html to the initial containing block (height: auto only).
    if (!layoutState().inQuirksMode())
        return false;

    if (!layoutBox.isDocumentBox() && !layoutBox.isBodyBox())
        return false;

    return layoutBox.style().logicalHeight().isAuto();
}

ContentHeightAndMargin BlockFormattingContext::Quirks::stretchedInFlowHeight(const Box& layoutBox, ContentHeightAndMargin contentHeightAndMargin)
{
    ASSERT(layoutBox.isInFlow());
    ASSERT(layoutBox.isDocumentBox() || layoutBox.isBodyBox());

    auto& formattingContext = this->formattingContext();
    auto& documentBox = layoutBox.isDocumentBox() ? layoutBox : *layoutBox.parent();
    auto& documentBoxGeometry = formattingContext.geometryForBox(documentBox, EscapeReason::BodyStrechesToViewportQuirk);

    auto& initialContainingBlockGeometry = formattingContext.geometryForBox(initialContainingBlock(layoutBox), EscapeReason::BodyStrechesToViewportQuirk);
    auto strechedHeight = initialContainingBlockGeometry.contentBoxHeight();
    strechedHeight -= documentBoxGeometry.verticalBorder() + documentBoxGeometry.verticalPadding().valueOr(0);

    LayoutUnit totalVerticalMargin;
    if (layoutBox.isDocumentBox()) {
        // Document box's margins do not collapse.
        auto verticalMargin = contentHeightAndMargin.nonCollapsedMargin;
        totalVerticalMargin = verticalMargin.before + verticalMargin.after;
    } else if (layoutBox.isBodyBox()) {
        // Here is the quirky part for body box:
        // Stretch the body using the initial containing block's height and shrink it with document box's margin/border/padding.
        // This looks extremely odd when html has non-auto height.
        auto documentBoxVerticalMargin = formattingContext.geometry().computedVerticalMargin(documentBox, Geometry::horizontalConstraintsForInFlow(initialContainingBlockGeometry));
        strechedHeight -= (documentBoxVerticalMargin.before.valueOr(0) + documentBoxVerticalMargin.after.valueOr(0));

        auto& bodyBoxGeometry = formattingContext.geometryForBox(layoutBox);
        strechedHeight -= bodyBoxGeometry.verticalBorder() + bodyBoxGeometry.verticalPadding().valueOr(0);

        auto nonCollapsedMargin = contentHeightAndMargin.nonCollapsedMargin;
        auto collapsedMargin = formattingContext.marginCollapse().collapsedVerticalValues(layoutBox, nonCollapsedMargin);
        totalVerticalMargin = collapsedMargin.before.valueOr(nonCollapsedMargin.before);
        totalVerticalMargin += collapsedMargin.isCollapsedThrough ? nonCollapsedMargin.after : collapsedMargin.after.valueOr(nonCollapsedMargin.after);
    }

    // Stretch but never overstretch with the margins.
    if (contentHeightAndMargin.contentHeight + totalVerticalMargin < strechedHeight)
        contentHeightAndMargin.contentHeight = strechedHeight - totalVerticalMargin;

    return contentHeightAndMargin;
}

bool BlockFormattingContext::Quirks::shouldIgnoreCollapsedQuirkMargin(const Box& layoutBox) const
{
    return layoutState().inQuirksMode() && isQuirkContainer(layoutBox);
}

}
}

#endif
