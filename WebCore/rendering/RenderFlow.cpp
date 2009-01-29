/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderFlow.h"

#include "Document.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "RenderArena.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderView.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderFlow* RenderFlow::createAnonymousFlow(Document* doc, PassRefPtr<RenderStyle> style)
{
    RenderFlow* result;
    if (style->display() == INLINE)
        result = new (doc->renderArena()) RenderInline(doc);
    else
        result = new (doc->renderArena()) RenderBlock(doc);
    result->setStyle(style);
    return result;
}

// FIXME: This is temporary and will go away once we finish pushing all the continuation code down into RenderBlock and RenderInline.
static RenderFlow* nextContinuation(RenderObject* renderer)
{
    if (renderer->isInline() && !renderer->isReplaced())
        return static_cast<RenderFlow*>(static_cast<RenderInline*>(renderer)->continuation());
    return static_cast<RenderBlock*>(renderer)->inlineContinuation();
}

RenderFlow* RenderFlow::continuationBefore(RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == this)
        return this;

    RenderFlow* curr = nextContinuation(this);
    RenderFlow* nextToLast = this;
    RenderFlow* last = this;
    while (curr) {
        if (beforeChild && beforeChild->parent() == curr) {
            if (curr->firstChild() == beforeChild)
                return last;
            return curr;
        }

        nextToLast = last;
        last = curr;
        curr = nextContinuation(curr);
    }

    if (!beforeChild && !last->firstChild())
        return nextToLast;
    return last;
}

void RenderFlow::addChildWithContinuation(RenderObject* newChild, RenderObject* beforeChild)
{
    if (beforeChild && (beforeChild->parent()->isTableRow() || beforeChild->parent()->isTableSection() || beforeChild->parent()->isTable())) {
        RenderObject* anonymousTablePart = beforeChild->parent();
        ASSERT(anonymousTablePart->isAnonymous());
        while (!anonymousTablePart->isTable()) {
            anonymousTablePart = anonymousTablePart->parent();
            ASSERT(anonymousTablePart->isAnonymous());
        }
        return anonymousTablePart->addChild(newChild, beforeChild);
    }

    RenderFlow* flow = continuationBefore(beforeChild);
    ASSERT(!beforeChild || beforeChild->parent()->isRenderBlock() || beforeChild->parent()->isRenderInline());
    RenderFlow* beforeChildParent = 0;
    if (beforeChild)
        beforeChildParent = static_cast<RenderFlow*>(beforeChild->parent());
    else {
        RenderFlow* cont = nextContinuation(flow);
        if (cont)
            beforeChildParent = cont;
        else
            beforeChildParent = flow;
    }

    if (newChild->isFloatingOrPositioned())
        return beforeChildParent->addChildToFlow(newChild, beforeChild);

    // A continuation always consists of two potential candidates: an inline or an anonymous
    // block box holding block children.
    bool childInline = newChild->isInline();
    bool bcpInline = beforeChildParent->isInline();
    bool flowInline = flow->isInline();

    if (flow == beforeChildParent)
        return flow->addChildToFlow(newChild, beforeChild);
    else {
        // The goal here is to match up if we can, so that we can coalesce and create the
        // minimal # of continuations needed for the inline.
        if (childInline == bcpInline)
            return beforeChildParent->addChildToFlow(newChild, beforeChild);
        else if (flowInline == childInline)
            return flow->addChildToFlow(newChild, 0); // Just treat like an append.
        else
            return beforeChildParent->addChildToFlow(newChild, beforeChild);
    }
}

void RenderFlow::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (nextContinuation(this))
        return addChildWithContinuation(newChild, beforeChild);
    return addChildToFlow(newChild, beforeChild);
}

void RenderFlow::destroy()
{
    // Make sure to destroy anonymous children first while they are still connected to the rest of the tree, so that they will
    // properly dirty line boxes that they are removed from.  Effects that do :before/:after only on hover could crash otherwise.
    RenderContainer::destroyLeftoverChildren();

    if (!documentBeingDestroyed()) {
        if (firstLineBox()) {
            // We can't wait for RenderContainer::destroy to clear the selection,
            // because by then we will have nuked the line boxes.
            // FIXME: The SelectionController should be responsible for this when it
            // is notified of DOM mutations.
            if (isSelectionBorder())
                view()->clearSelection();

            // If line boxes are contained inside a root, that means we're an inline.
            // In that case, we need to remove all the line boxes so that the parent
            // lines aren't pointing to deleted children. If the first line box does
            // not have a parent that means they are either already disconnected or
            // root lines that can just be destroyed without disconnecting.
            if (firstLineBox()->parent()) {
                for (InlineRunBox* box = firstLineBox(); box; box = box->nextLineBox())
                    box->remove();
            }

            // If we are an anonymous block, then our line boxes might have children
            // that will outlast this block. In the non-anonymous block case those
            // children will be destroyed by the time we return from this function.
            if (isAnonymousBlock()) {
                for (InlineFlowBox* box = firstLineBox(); box; box = box->nextFlowBox()) {
                    while (InlineBox* childBox = box->firstChild())
                        childBox->remove();
                }
            }
        } else if (isInline() && parent())
            parent()->dirtyLinesFromChangedChild(this);
    }

    m_lineBoxes.deleteLineBoxes(renderArena());

    RenderContainer::destroy();
}

void RenderFlow::dirtyLinesFromChangedChild(RenderObject* child)
{
    if (!parent() || (selfNeedsLayout() && !isRenderInline()) || isTable())
        return;

    // If we have no first line box, then just bail early.
    if (!firstLineBox()) {
        // For an empty inline, go ahead and propagate the check up to our parent, unless the parent
        // is already dirty.
        if (isInline() && !parent()->selfNeedsLayout())
            parent()->dirtyLinesFromChangedChild(this);
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
            InlineBox* wrapper = curr->inlineBoxWrapper();
            if (wrapper)
                box = wrapper->root();
        } else if (curr->isText()) {
            InlineTextBox* textBox = toRenderText(curr)->lastTextBox();
            if (textBox)
                box = textBox->root();
        } else if (curr->isRenderInline()) {
            InlineRunBox* runBox = static_cast<RenderFlow*>(curr)->lastLineBox();
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

} // namespace WebCore
