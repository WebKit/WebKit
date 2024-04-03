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
#include "BlockFormattingQuirks.h"

#include "BlockFormattingContext.h"
#include "BlockFormattingGeometry.h"
#include "BlockMarginCollapse.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutContainingBlockChainIterator.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutState.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

static bool isQuirkContainer(const ElementBox& layoutBox)
{
    return layoutBox.isBodyBox() || layoutBox.isDocumentBox() || layoutBox.isTableCell();
}

BlockFormattingQuirks::BlockFormattingQuirks(const BlockFormattingContext& blockFormattingContext)
    : FormattingQuirks(blockFormattingContext)
{
}

static bool needsStretching(const ElementBox& layoutBox)
{
    ASSERT(layoutBox.isInFlow());
    // In quirks mode, in-flow body and html stretch to the initial containing block (height: auto only).
    if (!layoutBox.isDocumentBox() && !layoutBox.isBodyBox())
        return false;
    return layoutBox.style().logicalHeight().isAuto();
}

std::optional<LayoutUnit> BlockFormattingQuirks::stretchedInFlowHeightIfApplicable(const ElementBox& layoutBox, ContentHeightAndMargin contentHeightAndMargin) const
{
    ASSERT(layoutState().inQuirksMode());
    if (!needsStretching(layoutBox))
        return { };
    auto& formattingContext = downcast<BlockFormattingContext>(this->formattingContext());
    auto nonCollapsedVerticalMargin = contentHeightAndMargin.nonCollapsedMargin.before + contentHeightAndMargin.nonCollapsedMargin.after;

    if (layoutBox.isDocumentBox()) {
        // Let's stretch the inflow document box(<html>) to the height of the initial containing block (view).
        auto documentBoxContentHeight = formattingContext.geometryForBox(FormattingContext::initialContainingBlock(layoutBox), FormattingContext::EscapeReason::DocumentBoxStretchesToViewportQuirk).contentBoxHeight();
        // Document box's own vertical margin/border/padding values always shrink the content height.
        auto& documentBoxGeometry = formattingContext.geometryForBox(layoutBox);
        documentBoxContentHeight -= nonCollapsedVerticalMargin + documentBoxGeometry.verticalBorderAndPadding();
        return std::max(contentHeightAndMargin.contentHeight,  documentBoxContentHeight);
    }

    // Here is the quirky part for body box when it stretches all the way to the ICB even when the document box does not (e.g. out-of-flow positioned).
    ASSERT(layoutBox.isBodyBox());
    auto& initialContainingBlock = FormattingContext::initialContainingBlock(layoutBox);
    auto& initialContainingBlockGeometry = formattingContext.geometryForBox(initialContainingBlock, FormattingContext::EscapeReason::BodyStretchesToViewportQuirk);
    // Start the content height with the ICB.
    auto bodyBoxContentHeight = initialContainingBlockGeometry.contentBoxHeight();
    // Body box's own border and padding shrink the content height.
    auto& bodyBoxGeometry = formattingContext.geometryForBox(layoutBox);
    bodyBoxContentHeight -= bodyBoxGeometry.verticalBorderAndPadding();
    // Body box never collapses its vertical margins with the document box but it might collapse its margin with its descendants.
    auto nonCollapsedMargin = contentHeightAndMargin.nonCollapsedMargin;
    auto marginCollapse = BlockMarginCollapse { formattingContext.layoutState(), formattingContext.formattingState() };
    auto collapsedMargin = marginCollapse.collapsedVerticalValues(layoutBox, nonCollapsedMargin).collapsedValues;
    auto usedVerticalMargin = collapsedMargin.before.value_or(nonCollapsedMargin.before);
    usedVerticalMargin += collapsedMargin.isCollapsedThrough ? nonCollapsedMargin.after : collapsedMargin.after.value_or(nonCollapsedMargin.after);
    bodyBoxContentHeight -= usedVerticalMargin;
    // Document box's padding and border also shrink the body box's content height.
    auto& documentBox = layoutBox.parent();
    auto& documentBoxGeometry = formattingContext.geometryForBox(documentBox, FormattingContext::EscapeReason::BodyStretchesToViewportQuirk);
    bodyBoxContentHeight -= documentBoxGeometry.verticalBorderAndPadding();
    // However the non-in-flow document box's vertical margins are ignored. They don't affect the body box's content height.
    if (documentBox.isInFlow()) {
        auto& formattingGeometry = formattingContext.formattingGeometry();
        auto precomputeDocumentBoxVerticalMargin = formattingGeometry.computedVerticalMargin(documentBox, formattingGeometry.constraintsForInFlowContent(initialContainingBlock, FormattingContext::EscapeReason::BodyStretchesToViewportQuirk).horizontal());
        bodyBoxContentHeight -= precomputeDocumentBoxVerticalMargin.before.value_or(0) + precomputeDocumentBoxVerticalMargin.after.value_or(0);
    }
    return std::max(contentHeightAndMargin.contentHeight,  bodyBoxContentHeight);
}

bool BlockFormattingQuirks::shouldIgnoreCollapsedQuirkMargin(const ElementBox& layoutBox)
{
    return isQuirkContainer(layoutBox);
}

enum class VerticalMargin { Before, After };
static inline bool hasQuirkMarginToCollapse(const ElementBox& layoutBox, VerticalMargin verticalMargin)
{
    if (!layoutBox.isInFlow())
        return false;
    auto& style = layoutBox.style();
    return (verticalMargin == VerticalMargin::Before && style.marginBefore().hasQuirk()) || (verticalMargin == VerticalMargin::After && style.marginAfter().hasQuirk());
}

bool BlockFormattingQuirks::shouldCollapseMarginBeforeWithParentMarginBefore(const ElementBox& layoutBox)
{
    return hasQuirkMarginToCollapse(layoutBox, VerticalMargin::Before) && isQuirkContainer(FormattingContext::containingBlock(layoutBox));
}

bool BlockFormattingQuirks::shouldCollapseMarginAfterWithParentMarginAfter(const ElementBox& layoutBox)
{
    return hasQuirkMarginToCollapse(layoutBox, VerticalMargin::After) && isQuirkContainer(FormattingContext::containingBlock(layoutBox));
}

LayoutUnit BlockFormattingQuirks::heightValueOfNearestContainingBlockWithFixedHeight(const Box& layoutBox) const
{
    ASSERT(layoutState().inQuirksMode());
    // In quirks mode, we go and travers the containing block chain to find a block level box with fixed height value, even if it means leaving
    // the current formatting context. FIXME: surely we need to do some tricks here when block direction support is added.
    auto& formattingContext = downcast<BlockFormattingContext>(this->formattingContext());
    auto bodyAndDocumentVerticalMarginPaddingAndBorder = LayoutUnit { };
    for (auto& containingBlock : containingBlockChain(layoutBox)) {
        auto containingBlockHeight = containingBlock.style().logicalHeight();
        if (containingBlockHeight.isFixed())
            return LayoutUnit(containingBlockHeight.value() - bodyAndDocumentVerticalMarginPaddingAndBorder);

        // If the only fixed value box we find is the ICB, then ignore the body and the document (vertical) margin, padding and border. So much quirkiness.
        // -and it's totally insane because now we freely travel across formatting context boundaries and computed margins are nonexistent.
        if (containingBlock.isBodyBox() || containingBlock.isDocumentBox()) {
            auto& formattingGeometry = formattingContext.formattingGeometry();
            auto horizontalConstraints = formattingGeometry.constraintsForInFlowContent(FormattingContext::containingBlock(containingBlock), FormattingContext::EscapeReason::FindFixedHeightAncestorQuirk).horizontal();
            auto verticalMargin = formattingGeometry.computedVerticalMargin(containingBlock, horizontalConstraints);

            auto& boxGeometry = formattingContext.geometryForBox(containingBlock, FormattingContext::EscapeReason::FindFixedHeightAncestorQuirk);
            bodyAndDocumentVerticalMarginPaddingAndBorder += verticalMargin.before.value_or(0) + verticalMargin.after.value_or(0) + boxGeometry.verticalPadding() + boxGeometry.verticalBorder();
        }
    }
    // Initial containing block has to have a height.
    return formattingContext.geometryForBox(FormattingContext::initialContainingBlock(layoutBox), FormattingContext::EscapeReason::FindFixedHeightAncestorQuirk).contentBox().height() - bodyAndDocumentVerticalMarginPaddingAndBorder;
}

}
}

