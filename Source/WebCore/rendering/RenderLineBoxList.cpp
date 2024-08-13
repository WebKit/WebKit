/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderLineBoxList.h"

#include "HitTestResult.h"
#include "LegacyInlineTextBox.h"
#include "LegacyRootInlineBox.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderSVGInline.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"

namespace WebCore {

#ifndef NDEBUG
RenderLineBoxList::~RenderLineBoxList()
{
    ASSERT(!m_firstLineBox);
    ASSERT(!m_lastLineBox);
}
#endif

void RenderLineBoxList::appendLineBox(std::unique_ptr<LegacyInlineFlowBox> box)
{
    checkConsistency();

    LegacyInlineFlowBox* boxPtr = box.release();

    if (!m_firstLineBox) {
        m_firstLineBox = boxPtr;
        m_lastLineBox = boxPtr;
    } else {
        m_lastLineBox->setNextLineBox(boxPtr);
        boxPtr->setPreviousLineBox(m_lastLineBox);
        m_lastLineBox = boxPtr;
    }

    checkConsistency();
}

void RenderLineBoxList::deleteLineBoxTree()
{
    LegacyInlineFlowBox* line = m_firstLineBox;
    LegacyInlineFlowBox* nextLine;
    while (line) {
        nextLine = line->nextLineBox();
        line->deleteLine();
        line = nextLine;
    }
    m_firstLineBox = m_lastLineBox = nullptr;
}

void RenderLineBoxList::extractLineBox(LegacyInlineFlowBox* box)
{
    checkConsistency();
    
    m_lastLineBox = box->prevLineBox();
    if (box == m_firstLineBox)
        m_firstLineBox = 0;
    if (box->prevLineBox())
        box->prevLineBox()->setNextLineBox(nullptr);
    box->setPreviousLineBox(nullptr);
    for (auto* curr = box; curr; curr = curr->nextLineBox())
        curr->setExtracted();

    checkConsistency();
}

void RenderLineBoxList::attachLineBox(LegacyInlineFlowBox* box)
{
    checkConsistency();

    if (m_lastLineBox) {
        m_lastLineBox->setNextLineBox(box);
        box->setPreviousLineBox(m_lastLineBox);
    } else
        m_firstLineBox = box;
    LegacyInlineFlowBox* last = box;
    for (auto* curr = box; curr; curr = curr->nextLineBox()) {
        curr->setExtracted(false);
        last = curr;
    }
    m_lastLineBox = last;

    checkConsistency();
}

void RenderLineBoxList::removeLineBox(LegacyInlineFlowBox* box)
{
    checkConsistency();

    if (box == m_firstLineBox)
        m_firstLineBox = box->nextLineBox();
    if (box == m_lastLineBox)
        m_lastLineBox = box->prevLineBox();
    if (box->nextLineBox())
        box->nextLineBox()->setPreviousLineBox(box->prevLineBox());
    if (box->prevLineBox())
        box->prevLineBox()->setNextLineBox(box->nextLineBox());

    checkConsistency();
}

void RenderLineBoxList::deleteLineBoxes()
{
    if (m_firstLineBox) {
        LegacyInlineFlowBox* next;
        for (auto* curr = m_firstLineBox; curr; curr = next) {
            next = curr->nextLineBox();
            delete curr;
        }
        m_firstLineBox = nullptr;
        m_lastLineBox = nullptr;
    }
}

void RenderLineBoxList::dirtyLineBoxes()
{
    for (auto* curr = firstLegacyLineBox(); curr; curr = curr->nextLineBox())
        curr->dirtyLineBoxes();
}

void RenderLineBoxList::shiftLinesBy(LayoutUnit shiftX, LayoutUnit shiftY)
{
    for (auto* box = firstLegacyLineBox(); box; box = box->nextLineBox())
        box->adjustPosition(shiftX, shiftY);
}

// FIXME: This should take a RenderBoxModelObject&.
bool RenderLineBoxList::rangeIntersectsRect(RenderBoxModelObject* renderer, LayoutUnit logicalTop, LayoutUnit logicalBottom, const LayoutRect& rect, const LayoutPoint& offset) const
{
    LayoutUnit physicalStart = logicalTop;
    LayoutUnit physicalEnd = logicalBottom;
    if (renderer->view().frameView().hasFlippedBlockRenderers()) {
        RenderBox* block;
        if (auto* box = dynamicDowncast<RenderBox>(*renderer))
            block = box;
        else
            block = renderer->containingBlock();
        physicalStart = block->flipForWritingMode(logicalTop);
        physicalEnd = block->flipForWritingMode(logicalBottom);
    }

    LayoutUnit physicalExtent = absoluteValue(physicalEnd - physicalStart);
    physicalStart = std::min(physicalStart, physicalEnd);
    
    if (renderer->style().isHorizontalWritingMode()) {
        physicalStart += offset.y();
        if (physicalStart >= rect.maxY() || physicalStart + physicalExtent <= rect.y())
            return false;
    } else {
        physicalStart += offset.x();
        if (physicalStart >= rect.maxX() || physicalStart + physicalExtent <= rect.x())
            return false;
    }
    
    return true;
}

bool RenderLineBoxList::anyLineIntersectsRect(RenderBoxModelObject* renderer, const LayoutRect& rect, const LayoutPoint& offset, bool usePrintRect) const
{
    // We can check the first box and last box and avoid painting/hit testing if we don't
    // intersect.  This is a quick short-circuit that we can take to avoid walking any lines.
    // FIXME: This check is flawed in the following extremely obscure way:
    // if some line in the middle has a huge overflow, it might actually extend below the last line.
    const LegacyRootInlineBox& firstRootBox = firstLegacyLineBox()->root();
    const LegacyRootInlineBox& lastRootBox = lastLegacyLineBox()->root();
    LayoutUnit firstLineTop = firstLegacyLineBox()->logicalTopVisualOverflow(firstRootBox.lineTop());
    if (usePrintRect && !firstLegacyLineBox()->parent())
        firstLineTop = std::min(firstLineTop, firstRootBox.lineTop());
    LayoutUnit lastLineBottom = lastLegacyLineBox()->logicalBottomVisualOverflow(lastRootBox.lineBottom());
    if (usePrintRect && !lastLegacyLineBox()->parent())
        lastLineBottom = std::max(lastLineBottom, lastRootBox.lineBottom());
    return rangeIntersectsRect(renderer, firstLineTop, lastLineBottom, rect, offset);
}

bool RenderLineBoxList::lineIntersectsDirtyRect(RenderBoxModelObject* renderer, LegacyInlineFlowBox* box, const PaintInfo& paintInfo, const LayoutPoint& offset) const
{
    const LegacyRootInlineBox& rootBox = box->root();
    LayoutUnit logicalTop = std::min(box->logicalTopVisualOverflow(rootBox.lineTop()), rootBox.selectionTop());
    LayoutUnit logicalBottom = box->logicalBottomVisualOverflow(rootBox.lineBottom());
    return rangeIntersectsRect(renderer, logicalTop, logicalBottom, paintInfo.rect, offset);
}

void RenderLineBoxList::paint(RenderBoxModelObject* renderer, PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    ASSERT(renderer->isRenderBlock() || (renderer->isRenderInline() && renderer->hasLayer())); // The only way an inline could paint like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!firstLegacyLineBox())
        return;

    // FIXME: Paint-time pagination is obsolete and is now only used by embedded WebViews inside AppKit
    // NSViews.  Do not add any more code for this.
    RenderView& v = renderer->view();
    bool usePrintRect = !v.printRect().isEmpty();
    if (!anyLineIntersectsRect(renderer, paintInfo.rect, paintOffset, usePrintRect))
        return;

    PaintInfo info(paintInfo);
    SingleThreadWeakListHashSet<RenderInline> outlineObjects;
    info.outlineObjects = &outlineObjects;

    // See if our root lines intersect with the dirty rect.  If so, then we paint
    // them.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    for (auto* curr = firstLegacyLineBox(); curr; curr = curr->nextLineBox()) {
        if (usePrintRect) {
            // FIXME: This is the deprecated pagination model that is still needed
            // for embedded views inside AppKit.  AppKit is incapable of paginating vertical
            // text pages, so we don't have to deal with vertical lines at all here.
            const LegacyRootInlineBox& rootBox = curr->root();
            LayoutUnit topForPaginationCheck = curr->logicalTopVisualOverflow(rootBox.lineTop());
            LayoutUnit bottomForPaginationCheck = curr->logicalLeftVisualOverflow();
            if (!curr->parent()) {
                // We're a root box.  Use lineTop and lineBottom as well here.
                topForPaginationCheck = std::min(topForPaginationCheck, rootBox.lineTop());
                bottomForPaginationCheck = std::max(bottomForPaginationCheck, rootBox.lineBottom());
            }
            if (bottomForPaginationCheck - topForPaginationCheck <= v.printRect().height()) {
                if (paintOffset.y() + bottomForPaginationCheck > v.printRect().maxY()) {
                    if (LegacyRootInlineBox* nextRootBox = rootBox.nextRootBox())
                        bottomForPaginationCheck = std::min(bottomForPaginationCheck, std::min<LayoutUnit>(nextRootBox->logicalTopVisualOverflow(), nextRootBox->lineTop()));
                }
                if (paintOffset.y() + bottomForPaginationCheck > v.printRect().maxY()) {
                    if (paintOffset.y() + topForPaginationCheck < v.truncatedAt())
                        v.setBestTruncatedAt(paintOffset.y() + topForPaginationCheck, renderer);
                    // If we were able to truncate, don't paint.
                    if (paintOffset.y() + topForPaginationCheck >= v.truncatedAt())
                        break;
                }
            }
        }

        if (lineIntersectsDirtyRect(renderer, curr, info, paintOffset)) {
            const LegacyRootInlineBox& rootBox = curr->root();
            curr->paint(info, paintOffset, rootBox.lineTop(), rootBox.lineBottom());
        }
    }

    if (info.phase == PaintPhase::Outline || info.phase == PaintPhase::SelfOutline || info.phase == PaintPhase::ChildOutlines) {
        auto end = info.outlineObjects->end();
        for (auto it = info.outlineObjects->begin(); it != end; ++it) {
            RenderInline& flow = *it;
            flow.paintOutline(info, paintOffset);
        }
        info.outlineObjects->clear();
    }
}

bool RenderLineBoxList::hitTest(RenderBoxModelObject* renderer, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction) const
{
    ASSERT(renderer->isRenderBlock() || (renderer->isRenderInline() && renderer->hasLayer())); // The only way an inline could hit test like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!firstLegacyLineBox())
        return false;

    LayoutPoint point = locationInContainer.point();
    LayoutRect rect = firstLegacyLineBox()->isHorizontal() ?
        IntRect(point.x(), point.y() - locationInContainer.topPadding(), 1, locationInContainer.topPadding() + locationInContainer.bottomPadding() + 1) :
        IntRect(point.x() - locationInContainer.leftPadding(), point.y(), locationInContainer.rightPadding() + locationInContainer.leftPadding() + 1, 1);

    if (!anyLineIntersectsRect(renderer, rect, accumulatedOffset))
        return false;

    // See if our root lines contain the point.  If so, then we hit test
    // them further.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    for (auto* curr = lastLegacyLineBox(); curr; curr = curr->prevLineBox()) {
        const LegacyRootInlineBox& rootBox = curr->root();
        if (rangeIntersectsRect(renderer, curr->logicalTopVisualOverflow(rootBox.lineTop()), curr->logicalBottomVisualOverflow(rootBox.lineBottom()), rect, accumulatedOffset)) {
            bool inside = curr->nodeAtPoint(request, result, locationInContainer, accumulatedOffset, rootBox.lineTop(), rootBox.lineBottom(), hitTestAction);
            if (inside) {
                renderer->updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
                return true;
            }
        }
    }

    return false;
}

void RenderLineBoxList::dirtyLineFromChangedChild(RenderBoxModelObject& container)
{
    ASSERT(is<RenderInline>(container) || is<RenderBlockFlow>(container));
    if (!container.isSVGRenderer())
        return;

    if (!container.parent() || (is<RenderBlockFlow>(container) && container.selfNeedsLayout()))
        return;

    auto* inlineContainer = dynamicDowncast<RenderSVGInline>(container);
    if (auto* lineBox = inlineContainer ? inlineContainer->firstLegacyInlineBox() : firstLegacyLineBox()) {
        lineBox->root().markDirty();
        return;
    }
    // For an empty inline, propagate the check up to our parent.
    if (inlineContainer && inlineContainer->everHadLayout()) {
        auto* parent = inlineContainer->parent();
        parent->dirtyLineFromChangedChild();
        parent->setNeedsLayout();
    }
}

#ifndef NDEBUG

void RenderLineBoxList::checkConsistency() const
{
#ifdef CHECK_CONSISTENCY
    const LegacyInlineFlowBox* prev = nullptr;
    for (const LegacyInlineFlowBox* child = m_firstLineBox; child != nullptr; child = child->nextLineBox()) {
        ASSERT(child->prevLineBox() == prev);
        prev = child;
    }
    ASSERT(prev == m_lastLineBox);
#endif
}

#endif

}
