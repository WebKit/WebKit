/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderMultiColumnBlock.h"

#include "RenderMultiColumnFlowThread.h"
#include "RenderMultiColumnSet.h"
#include "RenderView.h"
#include "StyleInheritedData.h"

namespace WebCore {

RenderMultiColumnBlock::RenderMultiColumnBlock(Element& element, PassRef<RenderStyle> stylePtr)
    : RenderBlockFlow(element, std::move(stylePtr))
{
    setChildrenInline(false);
    RenderMultiColumnFlowThread* flowThread = new RenderMultiColumnFlowThread(document(), RenderStyle::createAnonymousStyleWithDisplay(&style(), BLOCK));
    flowThread->initializeStyle();
    RenderBlockFlow::addChild(flowThread);
    setMultiColumnFlowThread(flowThread);
}

bool RenderMultiColumnBlock::requiresBalancing() const
{
    return multiColumnFlowThread()->requiresBalancing();
}

LayoutUnit RenderMultiColumnBlock::columnHeightAvailable() const
{
    return multiColumnFlowThread()->columnHeightAvailable();
}

LayoutUnit RenderMultiColumnBlock::columnWidth() const
{
    return multiColumnFlowThread()->columnWidth();
}

unsigned RenderMultiColumnBlock::columnCount() const
{
    return multiColumnFlowThread()->columnCount();
}
    
void RenderMultiColumnBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox())
        child->setStyle(RenderStyle::createAnonymousStyleWithDisplay(&style(), BLOCK));
}

bool RenderMultiColumnBlock::updateLogicalWidthAndColumnWidth()
{
    bool relayoutChildren = RenderBlockFlow::updateLogicalWidthAndColumnWidth();
    if (multiColumnFlowThread()->computeColumnCountAndWidth())
        relayoutChildren = true;
    return relayoutChildren;
}

void RenderMultiColumnBlock::checkForPaginationLogicalHeightChange(LayoutUnit& /*pageLogicalHeight*/, bool& /*pageLogicalHeightChanged*/, bool& /*hasSpecifiedPageLogicalHeight*/)
{
    // We don't actually update any of the variables. We just subclassed to adjust our column height.
    updateLogicalHeight();
    multiColumnFlowThread()->setColumnHeightAvailable(std::max<LayoutUnit>(contentLogicalHeight(), 0));
    setLogicalHeight(0);
}

bool RenderMultiColumnBlock::relayoutForPagination(bool, LayoutUnit, LayoutStateMaintainer& statePusher)
{
    if (!multiColumnFlowThread()->shouldRelayoutForPagination())
        return false;
    
    multiColumnFlowThread()->setNeedsRebalancing(false);
    multiColumnFlowThread()->setInBalancingPass(true); // Prevent re-entering this method (and recursion into layout).

    bool needsRelayout;
    bool neededRelayout = false;
    bool firstPass = true;
    do {
        // Column heights may change here because of balancing. We may have to do multiple layout
        // passes, depending on how the contents is fitted to the changed column heights. In most
        // cases, laying out again twice or even just once will suffice. Sometimes we need more
        // passes than that, though, but the number of retries should not exceed the number of
        // columns, unless we have a bug.
        needsRelayout = false;
        for (RenderBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox())
            if (childBox != multiColumnFlowThread() && childBox->isRenderMultiColumnSet()) {
                RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(childBox);
                if (multicolSet->recalculateBalancedHeight(firstPass)) {
                    multicolSet->setChildNeedsLayout(MarkOnlyThis);
                    needsRelayout = true;
                }
            }

        if (needsRelayout) {
            // Layout again. Column balancing resulted in a new height.
            neededRelayout = true;
            multiColumnFlowThread()->setChildNeedsLayout(MarkOnlyThis);
            setChildNeedsLayout(MarkOnlyThis);
            if (firstPass)
                statePusher.pop();
            layoutBlock(false);
        }
        firstPass = false;
    } while (needsRelayout);
    
    multiColumnFlowThread()->setInBalancingPass(false);
    
    return neededRelayout;
}

void RenderMultiColumnBlock::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    multiColumnFlowThread()->addChild(newChild, beforeChild);
}
    
RenderObject* RenderMultiColumnBlock::layoutSpecialExcludedChild(bool relayoutChildren)
{
    // Update the dimensions of our regions before we lay out the flow thread.
    // FIXME: Eventually this is going to get way more complicated, and we will be destroying regions
    // instead of trying to keep them around.
    bool shouldInvalidateRegions = false;
    for (RenderBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
        if (childBox == multiColumnFlowThread())
            continue;

        if (relayoutChildren || childBox->needsLayout()) {
            if (!multiColumnFlowThread()->inBalancingPass() && childBox->isRenderMultiColumnSet())
                toRenderMultiColumnSet(childBox)->prepareForLayout();
            shouldInvalidateRegions = true;
        }
    }
    
    if (shouldInvalidateRegions)
        multiColumnFlowThread()->invalidateRegions();

    if (relayoutChildren)
        multiColumnFlowThread()->setChildNeedsLayout(MarkOnlyThis);
    
    if (multiColumnFlowThread()->requiresBalancing()) {
        // At the end of multicol layout, relayoutForPagination() is called unconditionally, but if
        // no children are to be laid out (e.g. fixed width with layout already being up-to-date),
        // we want to prevent it from doing any work, so that the column balancing machinery doesn't
        // kick in and trigger additional unnecessary layout passes. Actually, it's not just a good
        // idea in general to not waste time on balancing content that hasn't been re-laid out; we
        // are actually required to guarantee this. The calculation of implicit breaks needs to be
        // preceded by a proper layout pass, since it's layout that sets up content runs, and the
        // runs get deleted right after every pass.
        multiColumnFlowThread()->setNeedsRebalancing(shouldInvalidateRegions || multiColumnFlowThread()->needsLayout());
    }

    setLogicalTopForChild(*multiColumnFlowThread(), borderAndPaddingBefore());
    multiColumnFlowThread()->layoutIfNeeded();
    determineLogicalLeftPositionForChild(*multiColumnFlowThread());
    
    return multiColumnFlowThread();
}

const char* RenderMultiColumnBlock::renderName() const
{
    if (isFloating())
        return "RenderMultiColumnBlock (floating)";
    if (isOutOfFlowPositioned())
        return "RenderMultiColumnBlock (positioned)";
    if (isAnonymousBlock())
        return "RenderMultiColumnBlock (anonymous)";
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderMultiColumnBlock (generated)";
    if (isAnonymous())
        return "RenderMultiColumnBlock (generated)";
    if (isRelPositioned())
        return "RenderMultiColumnBlock (relative positioned)";
    return "RenderMultiColumnBlock";
}

}
