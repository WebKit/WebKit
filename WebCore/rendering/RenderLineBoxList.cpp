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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "InlineTextBox.h"
#include "RenderArena.h"
#include "RenderInline.h"
#include "RenderView.h"
#include "RootInlineBox.h"

using namespace std;

namespace WebCore {

#ifndef NDEBUG
RenderLineBoxList::~RenderLineBoxList()
{
    ASSERT(!m_firstLineBox);
    ASSERT(!m_lastLineBox);
}
#endif

void RenderLineBoxList::appendLineBox(InlineFlowBox* box)
{
    checkConsistency();
    
    if (!m_firstLineBox)
        m_firstLineBox = m_lastLineBox = box;
    else {
        m_lastLineBox->setNextLineBox(box);
        box->setPreviousLineBox(m_lastLineBox);
        m_lastLineBox = box;
    }

    checkConsistency();
}

void RenderLineBoxList::deleteLineBoxTree(RenderArena* arena)
{
    InlineFlowBox* line = m_firstLineBox;
    InlineFlowBox* nextLine;
    while (line) {
        nextLine = line->nextFlowBox();
        line->deleteLine(arena);
        line = nextLine;
    }
    m_firstLineBox = m_lastLineBox = 0;
}

void RenderLineBoxList::extractLineBox(InlineFlowBox* box)
{
    checkConsistency();
    
    m_lastLineBox = box->prevFlowBox();
    if (box == m_firstLineBox)
        m_firstLineBox = 0;
    if (box->prevLineBox())
        box->prevLineBox()->setNextLineBox(0);
    box->setPreviousLineBox(0);
    for (InlineRunBox* curr = box; curr; curr = curr->nextLineBox())
        curr->setExtracted();

    checkConsistency();
}

void RenderLineBoxList::attachLineBox(InlineFlowBox* box)
{
    checkConsistency();

    if (m_lastLineBox) {
        m_lastLineBox->setNextLineBox(box);
        box->setPreviousLineBox(m_lastLineBox);
    } else
        m_firstLineBox = box;
    InlineFlowBox* last = box;
    for (InlineFlowBox* curr = box; curr; curr = curr->nextFlowBox()) {
        curr->setExtracted(false);
        last = curr;
    }
    m_lastLineBox = last;

    checkConsistency();
}

void RenderLineBoxList::removeLineBox(InlineFlowBox* box)
{
    checkConsistency();

    if (box == m_firstLineBox)
        m_firstLineBox = box->nextFlowBox();
    if (box == m_lastLineBox)
        m_lastLineBox = box->prevFlowBox();
    if (box->nextLineBox())
        box->nextLineBox()->setPreviousLineBox(box->prevLineBox());
    if (box->prevLineBox())
        box->prevLineBox()->setNextLineBox(box->nextLineBox());

    checkConsistency();
}

void RenderLineBoxList::deleteLineBoxes(RenderArena* arena)
{
    if (m_firstLineBox) {
        InlineRunBox* next;
        for (InlineRunBox* curr = m_firstLineBox; curr; curr = next) {
            next = curr->nextLineBox();
            curr->destroy(arena);
        }
        m_firstLineBox = 0;
        m_lastLineBox = 0;
    }
}

void RenderLineBoxList::dirtyLineBoxes()
{
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
        curr->dirtyLineBoxes();
}

void RenderLineBoxList::paint(RenderBoxModelObject* renderer, RenderObject::PaintInfo& paintInfo, int tx, int ty) const
{
    // Only paint during the foreground/selection phases.
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseOutline 
        && paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines && paintInfo.phase != PaintPhaseTextClip
        && paintInfo.phase != PaintPhaseMask)
        return;

    ASSERT(renderer->isRenderBlock() || (renderer->isRenderInline() && renderer->hasLayer())); // The only way an inline could paint like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!firstLineBox())
        return;

    // We can check the first box and last box and avoid painting if we don't
    // intersect.  This is a quick short-circuit that we can take to avoid walking any lines.
    // FIXME: This check is flawed in the following extremely obscure way:
    // if some line in the middle has a huge overflow, it might actually extend below the last line.
    int yPos = firstLineBox()->topVisibleOverflow() - renderer->maximalOutlineSize(paintInfo.phase);
    int h = renderer->maximalOutlineSize(paintInfo.phase) + lastLineBox()->bottomVisibleOverflow() - yPos;
    yPos += ty;
    if (yPos >= paintInfo.rect.bottom() || yPos + h <= paintInfo.rect.y())
        return;

    RenderObject::PaintInfo info(paintInfo);
    ListHashSet<RenderInline*> outlineObjects;
    info.outlineObjects = &outlineObjects;

    // See if our root lines intersect with the dirty rect.  If so, then we paint
    // them.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    RenderView* v = renderer->view();
    bool usePrintRect = !v->printRect().isEmpty();
    for (InlineFlowBox* curr = firstLineBox(); curr; curr = curr->nextFlowBox()) {
        if (usePrintRect) {
            // FIXME: This is a feeble effort to avoid splitting a line across two pages.
            // It is utterly inadequate, and this should not be done at paint time at all.
            // The whole way objects break across pages needs to be redone.
            // Try to avoid splitting a line vertically, but only if it's less than the height
            // of the entire page.
            if (curr->bottomVisibleOverflow() - curr->topVisibleOverflow() <= v->printRect().height()) {
                if (ty + curr->bottomVisibleOverflow() > v->printRect().bottom()) {
                    if (ty + curr->topVisibleOverflow() < v->truncatedAt())
                        v->setBestTruncatedAt(ty + curr->root()->topVisibleOverflow(), renderer);
                    // If we were able to truncate, don't paint.
                    if (ty + curr->topVisibleOverflow() >= v->truncatedAt())
                        break;
                }
            }
        }

        int top = min(curr->topVisibleOverflow(), curr->root()->selectionTop()) - renderer->maximalOutlineSize(info.phase);
        int bottom = curr->bottomVisibleOverflow() + renderer->maximalOutlineSize(info.phase);
        h = bottom - top;
        yPos = ty + top;
        v->setMinimumColumnHeight(h);
        if (yPos < info.rect.bottom() && yPos + h > info.rect.y())
            curr->paint(info, tx, ty);
    }

    if (info.phase == PaintPhaseOutline || info.phase == PaintPhaseSelfOutline || info.phase == PaintPhaseChildOutlines) {
        ListHashSet<RenderInline*>::iterator end = info.outlineObjects->end();
        for (ListHashSet<RenderInline*>::iterator it = info.outlineObjects->begin(); it != end; ++it) {
            RenderInline* flow = *it;
            flow->paintOutline(info.context, tx, ty);
        }
        info.outlineObjects->clear();
    }
}


bool RenderLineBoxList::hitTest(RenderBoxModelObject* renderer, const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction) const
{
    if (hitTestAction != HitTestForeground)
        return false;

    ASSERT(renderer->isRenderBlock() || (renderer->isRenderInline() && renderer->hasLayer())); // The only way an inline could hit test like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!firstLineBox())
        return false;

    // We can check the first box and last box and avoid hit testing if we don't
    // contain the point.  This is a quick short-circuit that we can take to avoid walking any lines.
    // FIXME: This check is flawed in the following extremely obscure way:
    // if some line in the middle has a huge overflow, it might actually extend below the last line.
    if ((y >= ty + lastLineBox()->root()->bottomVisibleOverflow()) || (y < ty + firstLineBox()->root()->topVisibleOverflow()))
        return false;

    // See if our root lines contain the point.  If so, then we hit test
    // them further.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    for (InlineFlowBox* curr = lastLineBox(); curr; curr = curr->prevFlowBox()) {
        if (y >= ty + curr->root()->topVisibleOverflow() && y < ty + curr->root()->bottomVisibleOverflow()) {
            bool inside = curr->nodeAtPoint(request, result, x, y, tx, ty);
            if (inside) {
                renderer->updateHitTestResult(result, IntPoint(x - tx, y - ty));
                return true;
            }
        }
    }
    
    return false;
}

void RenderLineBoxList::dirtyLinesFromChangedChild(RenderObject* container, RenderObject* child)
{
    if (!container->parent() || (container->isRenderBlock() && (container->selfNeedsLayout() || !container->isBlockFlow())))
        return;

    // If we have no first line box, then just bail early.
    if (!firstLineBox()) {
        // For an empty inline, go ahead and propagate the check up to our parent, unless the parent
        // is already dirty.
        if (container->isInline() && !container->parent()->selfNeedsLayout())
            container->parent()->dirtyLinesFromChangedChild(container);
        return;
    }

    // Try to figure out which line box we belong in.  First try to find a previous
    // line box by examining our siblings.  If we didn't find a line box, then use our 
    // parent's first line box.
    RootInlineBox* box = 0;
    RenderObject* curr = 0;
    for (curr = child->previousSibling(); curr; curr = curr->previousSibling()) {
        if (curr->isFloatingOrPositioned())
            continue;

        if (curr->isReplaced()) {
            InlineBox* wrapper = toRenderBox(curr)->inlineBoxWrapper();
            if (wrapper)
                box = wrapper->root();
        } else if (curr->isText()) {
            InlineTextBox* textBox = toRenderText(curr)->lastTextBox();
            if (textBox)
                box = textBox->root();
        } else if (curr->isRenderInline()) {
            InlineRunBox* runBox = toRenderInline(curr)->lastLineBox();
            if (runBox)
                box = runBox->root();
        }

        if (box)
            break;
    }
    if (!box)
        box = firstLineBox()->root();

    // If we found a line box, then dirty it.
    if (box) {
        RootInlineBox* adjacentBox;
        box->markDirty();

        // dirty the adjacent lines that might be affected
        // NOTE: we dirty the previous line because RootInlineBox objects cache
        // the address of the first object on the next line after a BR, which we may be
        // invalidating here.  For more info, see how RenderBlock::layoutInlineChildren
        // calls setLineBreakInfo with the result of findNextLineBreak.  findNextLineBreak,
        // despite the name, actually returns the first RenderObject after the BR.
        // <rdar://problem/3849947> "Typing after pasting line does not appear until after window resize."
        adjacentBox = box->prevRootBox();
        if (adjacentBox)
            adjacentBox->markDirty();
        if (child->isBR() || (curr && curr->isBR())) {
            adjacentBox = box->nextRootBox();
            if (adjacentBox)
                adjacentBox->markDirty();
        }
    }
}

#ifndef NDEBUG

void RenderLineBoxList::checkConsistency() const
{
#ifdef CHECK_CONSISTENCY
    const InlineFlowBox* prev = 0;
    for (const InlineFlowBox* child = m_firstLineBox; child != 0; child = child->nextFlowBox()) {
        ASSERT(child->prevFlowBox() == prev);
        prev = child;
    }
    ASSERT(prev == m_lastLineBox);
#endif
}

#endif

}
