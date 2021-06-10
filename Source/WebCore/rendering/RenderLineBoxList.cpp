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
#include "LegacyInlineElementBox.h"
#include "LegacyInlineTextBox.h"
#include "LegacyRootInlineBox.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
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
    for (auto* curr = firstLineBox(); curr; curr = curr->nextLineBox())
        curr->dirtyLineBoxes();
}

// FIXME: This should take a RenderBoxModelObject&.
bool RenderLineBoxList::rangeIntersectsRect(RenderBoxModelObject* renderer, LayoutUnit logicalTop, LayoutUnit logicalBottom, const LayoutRect& rect, const LayoutPoint& offset) const
{
    LayoutUnit physicalStart = logicalTop;
    LayoutUnit physicalEnd = logicalBottom;
    if (renderer->view().frameView().hasFlippedBlockRenderers()) {
        RenderBox* block;
        if (is<RenderBox>(*renderer))
            block = downcast<RenderBox>(renderer);
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
    const LegacyRootInlineBox& firstRootBox = firstLineBox()->root();
    const LegacyRootInlineBox& lastRootBox = lastLineBox()->root();
    LayoutUnit firstLineTop = firstLineBox()->logicalTopVisualOverflow(firstRootBox.lineTop());
    if (usePrintRect && !firstLineBox()->parent())
        firstLineTop = std::min(firstLineTop, firstRootBox.lineTop());
    LayoutUnit lastLineBottom = lastLineBox()->logicalBottomVisualOverflow(lastRootBox.lineBottom());
    if (usePrintRect && !lastLineBox()->parent())
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
    if (!firstLineBox())
        return;

    // FIXME: Paint-time pagination is obsolete and is now only used by embedded WebViews inside AppKit
    // NSViews.  Do not add any more code for this.
    RenderView& v = renderer->view();
    bool usePrintRect = !v.printRect().isEmpty();
    if (!anyLineIntersectsRect(renderer, paintInfo.rect, paintOffset, usePrintRect))
        return;

    PaintInfo info(paintInfo);
    ListHashSet<RenderInline*> outlineObjects;
    info.outlineObjects = &outlineObjects;

    // See if our root lines intersect with the dirty rect.  If so, then we paint
    // them.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    for (auto* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
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
        ListHashSet<RenderInline*>::iterator end = info.outlineObjects->end();
        for (ListHashSet<RenderInline*>::iterator it = info.outlineObjects->begin(); it != end; ++it) {
            RenderInline* flow = *it;
            flow->paintOutline(info, paintOffset);
        }
        info.outlineObjects->clear();
    }
}

bool RenderLineBoxList::hitTest(RenderBoxModelObject* renderer, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction) const
{
    ASSERT(renderer->isRenderBlock() || (renderer->isRenderInline() && renderer->hasLayer())); // The only way an inline could hit test like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!firstLineBox())
        return false;

    LayoutPoint point = locationInContainer.point();
    LayoutRect rect = firstLineBox()->isHorizontal() ?
        IntRect(point.x(), point.y() - locationInContainer.topPadding(), 1, locationInContainer.topPadding() + locationInContainer.bottomPadding() + 1) :
        IntRect(point.x() - locationInContainer.leftPadding(), point.y(), locationInContainer.rightPadding() + locationInContainer.leftPadding() + 1, 1);

    if (!anyLineIntersectsRect(renderer, rect, accumulatedOffset))
        return false;

    // See if our root lines contain the point.  If so, then we hit test
    // them further.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    for (auto* curr = lastLineBox(); curr; curr = curr->prevLineBox()) {
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

void RenderLineBoxList::dirtyLinesFromChangedChild(RenderBoxModelObject& container, RenderObject& child)
{
    ASSERT(is<RenderInline>(container) || is<RenderBlockFlow>(container));
    if (!container.parent() || (is<RenderBlockFlow>(container) && container.selfNeedsLayout()))
        return;

    RenderInline* inlineContainer = is<RenderInline>(container) ? &downcast<RenderInline>(container) : nullptr;
    LegacyInlineBox* firstBox = inlineContainer ? inlineContainer->firstLineBoxIncludingCulling() : firstLineBox();

    // If we have no first line box, then just bail early.
    if (!firstBox) {
        // For an empty inline, propagate the check up to our parent, unless the parent is already dirty.
        if (container.isInline() && !container.ancestorLineBoxDirty()) {
            container.parent()->dirtyLinesFromChangedChild(container);
            container.setAncestorLineBoxDirty(); // Mark the container to avoid dirtying the same lines again across multiple destroy() calls of the same subtree.
        }
        return;
    }

    // Try to figure out which line box we belong in. First try to find a previous
    // line box by examining our siblings. If we didn't find a line box, then use our
    // parent's first line box.
    LegacyRootInlineBox* box = nullptr;
    RenderObject* current;
    for (current = child.previousSibling(); current; current = current->previousSibling()) {
        if (current->isFloatingOrOutOfFlowPositioned())
            continue;

        if (current->isReplaced()) {
            if (auto wrapper = downcast<RenderBox>(*current).inlineBoxWrapper())
                box = &wrapper->root();
        } if (is<RenderLineBreak>(*current)) {
            if (auto wrapper = downcast<RenderLineBreak>(*current).inlineBoxWrapper())
                box = &wrapper->root();
        } else if (is<RenderText>(*current)) {
            if (LegacyInlineTextBox* textBox = downcast<RenderText>(*current).lastTextBox())
                box = &textBox->root();
        } else if (is<RenderInline>(*current)) {
            LegacyInlineBox* lastSiblingBox = downcast<RenderInline>(*current).lastLineBoxIncludingCulling();
            if (lastSiblingBox)
                box = &lastSiblingBox->root();
        }

        if (box)
            break;
    }
    if (!box) {
        if (inlineContainer && !inlineContainer->alwaysCreateLineBoxes()) {
            // https://bugs.webkit.org/show_bug.cgi?id=60778
            // We may have just removed a <br> with no line box that was our first child. In this case
            // we won't find a previous sibling, but firstBox can be pointing to a following sibling.
            // This isn't good enough, since we won't locate the root line box that encloses the removed
            // <br>. We have to just over-invalidate a bit and go up to our parent.
            if (!inlineContainer->ancestorLineBoxDirty()) {
                inlineContainer->parent()->dirtyLinesFromChangedChild(*inlineContainer);
                inlineContainer->setAncestorLineBoxDirty(); // Mark the container to avoid dirtying the same lines again across multiple destroy() calls of the same subtree.
            }
            return;
        }
        box = &firstBox->root();
    }

    // If we found a line box, then dirty it.
    if (box) {
        box->markDirty();

        // Dirty the adjacent lines that might be affected.
        // NOTE: we dirty the previous line because RootInlineBox objects cache
        // the address of the first object on the next line after a BR, which we may be
        // invalidating here. For more info, see how RenderBlock::layoutInlineChildren
        // calls setLineBreakInfo with the result of findNextLineBreak. findNextLineBreak,
        // despite the name, actually returns the first RenderObject after the BR.
        // <rdar://problem/3849947> "Typing after pasting line does not appear until after window resize."
        if (LegacyRootInlineBox* prevBox = box->prevRootBox())
            prevBox->markDirty();

        // FIXME: We shouldn't need to always dirty the next line. This is only strictly 
        // necessary some of the time, in situations involving BRs.
        if (LegacyRootInlineBox* nextBox = box->nextRootBox()) {
            nextBox->markDirty();
            // Dedicated linebox for floats may be added as the last rootbox. If this occurs with BRs inside inlines that propagte their lineboxes to
            // the parent flow, we need to invalidate it explicitly.
            // FIXME: We should be able to figure out the actual "changed child" even when we are calling through empty inlines recursively.
            if (is<RenderInline>(child) && !downcast<RenderInline>(child).firstLineBoxIncludingCulling()) {
                auto* lastRootBox = nextBox->blockFlow().lastRootBox();
                if (lastRootBox->isForTrailingFloats() && !lastRootBox->isDirty())
                    lastRootBox->markDirty();
            }
        }
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
