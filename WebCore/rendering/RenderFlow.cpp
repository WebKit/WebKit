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

#ifndef NDEBUG

RenderFlow::~RenderFlow()
{
    ASSERT(!m_firstLineBox);
    ASSERT(!m_lastLineBox);
}

#endif

RenderFlow* RenderFlow::createAnonymousFlow(Document* doc, RenderStyle* style)
{
    RenderFlow* result;
    if (style->display() == INLINE)
        result = new (doc->renderArena()) RenderInline(doc);
    else
        result = new (doc->renderArena()) RenderBlock(doc);
    result->setStyle(style);
    return result;
}

RenderFlow* RenderFlow::continuationBefore(RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == this)
        return this;

    RenderFlow* curr = continuation();
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
        curr = curr->continuation();
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
    ASSERT(!beforeChild || beforeChild->parent()->isRenderBlock() ||
                beforeChild->parent()->isRenderInline());
    RenderFlow* beforeChildParent = beforeChild ? static_cast<RenderFlow*>(beforeChild->parent()) : 
                                    (flow->continuation() ? flow->continuation() : flow);

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
    if (continuation())
        return addChildWithContinuation(newChild, beforeChild);
    return addChildToFlow(newChild, beforeChild);
}

void RenderFlow::extractLineBox(InlineFlowBox* box)
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

void RenderFlow::attachLineBox(InlineFlowBox* box)
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

void RenderFlow::removeLineBox(InlineFlowBox* box)
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

void RenderFlow::deleteLineBoxes()
{
    if (m_firstLineBox) {
        RenderArena* arena = renderArena();
        InlineRunBox* next;
        for (InlineRunBox* curr = m_firstLineBox; curr; curr = next) {
            next = curr->nextLineBox();
            curr->destroy(arena);
        }
        m_firstLineBox = 0;
        m_lastLineBox = 0;
    }
}

void RenderFlow::destroy()
{
    // Detach our continuation first.
    if (m_continuation)
        m_continuation->destroy();
    m_continuation = 0;

    // Make sure to destroy anonymous children first while they are still connected to the rest of the tree, so that they will
    // properly dirty line boxes that they are removed from.  Effects that do :before/:after only on hover could crash otherwise.
    RenderContainer::destroyLeftoverChildren();

    if (!documentBeingDestroyed()) {
        if (m_firstLineBox) {
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
            if (m_firstLineBox->parent()) {
                for (InlineRunBox* box = m_firstLineBox; box; box = box->nextLineBox())
                    box->remove();
            }

            // If we are an anonymous block, then our line boxes might have children
            // that will outlast this block. In the non-anonymous block case those
            // children will be destroyed by the time we return from this function.
            if (isAnonymousBlock()) {
                for (InlineFlowBox* box = m_firstLineBox; box; box = box->nextFlowBox()) {
                    while (InlineBox* childBox = box->firstChild())
                        childBox->remove();
                }
            }
        } else if (isInline() && parent())
            parent()->dirtyLinesFromChangedChild(this);
    }

    deleteLineBoxes();

    RenderContainer::destroy();
}

void RenderFlow::dirtyLinesFromChangedChild(RenderObject* child)
{
    if (!parent() || (selfNeedsLayout() && !isInlineFlow()) || isTable())
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
            InlineTextBox* textBox = static_cast<RenderText*>(curr)->lastTextBox();
            if (textBox)
                box = textBox->root();
        } else if (curr->isInlineFlow()) {
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

short RenderFlow::lineHeight(bool firstLine, bool isRootLineBox) const
{
    if (firstLine) {
        RenderStyle* s = style(firstLine);
        Length lh = s->lineHeight();
        if (lh.isNegative()) {
            if (s == style()) {
                if (m_lineHeight == -1)
                    m_lineHeight = RenderObject::lineHeight(false);
                return m_lineHeight;
            }
            return s->font().lineSpacing();
        }
        if (lh.isPercent())
            return lh.calcMinValue(s->fontSize());
        return lh.value();
    }

    if (m_lineHeight == -1)
        m_lineHeight = RenderObject::lineHeight(false);
    return m_lineHeight;
}

void RenderFlow::dirtyLineBoxes(bool fullLayout, bool isRootLineBox)
{
    if (!isRootLineBox && isReplaced())
        return RenderContainer::dirtyLineBoxes(fullLayout, isRootLineBox);

    if (fullLayout)
        deleteLineBoxes();
    else {
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
            curr->dirtyLineBoxes();
    }
}

InlineBox* RenderFlow::createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun)
{
    checkConsistency();

    if (!isRootLineBox &&
        (isReplaced() || makePlaceHolderBox))                     // Inline tables and inline blocks
        return RenderContainer::createInlineBox(false, isRootLineBox);  // (or positioned element placeholders).

    InlineFlowBox* flowBox = 0;
    if (isInlineFlow())
        flowBox = new (renderArena()) InlineFlowBox(this);
    else
        flowBox = new (renderArena()) RootInlineBox(this);

    if (!m_firstLineBox)
        m_firstLineBox = m_lastLineBox = flowBox;
    else {
        m_lastLineBox->setNextLineBox(flowBox);
        flowBox->setPreviousLineBox(m_lastLineBox);
        m_lastLineBox = flowBox;
    }

    checkConsistency();

    return flowBox;
}

void RenderFlow::paintLines(PaintInfo& paintInfo, int tx, int ty)
{
    // Only paint during the foreground/selection phases.
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseOutline 
        && paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines)
        return;

    bool inlineFlow = isInlineFlow();
    if (inlineFlow)
        ASSERT(m_layer); // The only way a compact/run-in/inline could paint like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!firstLineBox())
        return;

    // We can check the first box and last box and avoid painting if we don't
    // intersect.  This is a quick short-circuit that we can take to avoid walking any lines.
    // FIXME: This check is flawed in the following extremely obscure way:
    // if some line in the middle has a huge overflow, it might actually extend below the last line.
    int yPos = firstLineBox()->root()->topOverflow() - maximalOutlineSize(paintInfo.phase);
    int h = maximalOutlineSize(paintInfo.phase) + lastLineBox()->root()->bottomOverflow() - yPos;
    yPos += ty;
    if (yPos >= paintInfo.rect.bottom() || yPos + h <= paintInfo.rect.y())
        return;

    PaintInfo info(paintInfo);
    RenderFlowSequencedSet outlineObjects;
    info.outlineObjects = &outlineObjects;

    // See if our root lines intersect with the dirty rect.  If so, then we paint
    // them.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    RenderView* v = view();
    bool usePrintRect = !v->printRect().isEmpty();
    for (InlineFlowBox* curr = firstLineBox(); curr; curr = curr->nextFlowBox()) {
        if (usePrintRect) {
            // FIXME: This is a feeble effort to avoid splitting a line across two pages.
            // It is utterly inadequate, and this should not be done at paint time at all.
            // The whole way objects break across pages needs to be redone.
            // Try to avoid splitting a line vertically, but only if it's less than the height
            // of the entire page.
            if (curr->root()->bottomOverflow() - curr->root()->topOverflow() <= v->printRect().height()) {
                if (ty + curr->root()->bottomOverflow() > v->printRect().bottom()) {
                    if (ty + curr->root()->topOverflow() < v->truncatedAt())
                        v->setBestTruncatedAt(ty + curr->root()->topOverflow(), this);
                    // If we were able to truncate, don't paint.
                    if (ty + curr->root()->topOverflow() >= v->truncatedAt())
                        break;
                }
            }
        }

        int top = min(curr->root()->topOverflow(), curr->root()->selectionTop()) - maximalOutlineSize(info.phase);
        int bottom = curr->root()->bottomOverflow() + maximalOutlineSize(info.phase);
        h = bottom - top;
        yPos = ty + top;
        if (yPos < info.rect.bottom() && yPos + h > info.rect.y())
            curr->paint(info, tx, ty);
    }

    if (info.phase == PaintPhaseOutline || info.phase == PaintPhaseSelfOutline || info.phase == PaintPhaseChildOutlines) {
        RenderFlowSequencedSet::iterator end = info.outlineObjects->end();
        for (RenderFlowSequencedSet::iterator it = info.outlineObjects->begin(); it != end; ++it) {
            RenderFlow* flow = *it;
            flow->paintOutline(info.context, tx, ty);
        }
        info.outlineObjects->clear();
    }
}

bool RenderFlow::hitTestLines(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    bool inlineFlow = isInlineFlow();
    if (inlineFlow)
        ASSERT(m_layer); // The only way a compact/run-in/inline could paint like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!firstLineBox())
        return false;

    // We can check the first box and last box and avoid hit testing if we don't
    // contain the point.  This is a quick short-circuit that we can take to avoid walking any lines.
    // FIXME: This check is flawed in the following extremely obscure way:
    // if some line in the middle has a huge overflow, it might actually extend below the last line.
    if ((y >= ty + lastLineBox()->root()->bottomOverflow()) || (y < ty + firstLineBox()->root()->topOverflow()))
        return false;

    // See if our root lines contain the point.  If so, then we hit test
    // them further.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    for (InlineFlowBox* curr = lastLineBox(); curr; curr = curr->prevFlowBox()) {
        if (y >= ty + curr->root()->topOverflow() && y < ty + curr->root()->bottomOverflow()) {
            bool inside = curr->nodeAtPoint(request, result, x, y, tx, ty);
            if (inside) {
                updateHitTestResult(result, IntPoint(x - tx, y - ty));
                return true;
            }
        }
    }
    
    return false;
}

IntRect RenderFlow::absoluteClippedOverflowRect()
{
    if (isInlineFlow()) {
        // Only compacts and run-ins are allowed in here during layout.
        ASSERT(!view() || !view()->layoutState() || isCompact() || isRunIn());

        if (!firstLineBox() && !continuation())
            return IntRect();

        // Find our leftmost position.
        int left = 0;
        int top = firstLineBox() ? firstLineBox()->yPos() : 0;
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
            if (curr == firstLineBox() || curr->xPos() < left)
                left = curr->xPos();
        }

        // Now invalidate a rectangle.
        int ow = style() ? style()->outlineSize() : 0;
        if (isCompact())
            left -= m_x;
        
        // We need to add in the relative position offsets of any inlines (including us) up to our
        // containing block.
        RenderBlock* cb = containingBlock();
        for (RenderObject* inlineFlow = this; inlineFlow && inlineFlow->isInlineFlow() && inlineFlow != cb; 
             inlineFlow = inlineFlow->parent()) {
             if (inlineFlow->style()->position() == RelativePosition && inlineFlow->hasLayer())
                inlineFlow->layer()->relativePositionOffset(left, top);
        }

        IntRect r(-ow + left, -ow + top, width() + ow * 2, height() + ow * 2);
        if (cb->hasColumns())
            cb->adjustRectForColumns(r);

        if (cb->hasOverflowClip()) {
            // cb->height() is inaccurate if we're in the middle of a layout of |cb|, so use the
            // layer's size instead.  Even if the layer's size is wrong, the layer itself will repaint
            // anyway if its size does change.
            int x = r.x();
            int y = r.y();
            IntRect boxRect(0, 0, cb->layer()->width(), cb->layer()->height());
            cb->layer()->subtractScrollOffset(x, y); // For overflow:auto/scroll/hidden.
            IntRect repaintRect(x, y, r.width(), r.height());
            r = intersection(repaintRect, boxRect);
        }
        cb->computeAbsoluteRepaintRect(r);

        if (ow) {
            for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
                if (!curr->isText()) {
                    IntRect childRect = curr->getAbsoluteRepaintRectWithOutline(ow);
                    r.unite(childRect);
                }
            }

            if (continuation() && !continuation()->isInline()) {
                IntRect contRect = continuation()->getAbsoluteRepaintRectWithOutline(ow);
                r.unite(contRect);
            }
        }

        return r;
    }

    return RenderContainer::absoluteClippedOverflowRect();
}

int RenderFlow::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    ASSERT(!isInlineFlow());
    if (!includeOverflowInterior && hasOverflowClip())
        return includeSelf && m_width > 0 ? overflowHeight(false) : 0;

    int bottom = includeSelf && m_width > 0 ? m_height : 0;
    if (!hasColumns()) {
        // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
        // For now, we have to descend into all the children, since we may have a huge abs div inside
        // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
        // the abs div.
        for (RenderObject* c = firstChild(); c; c = c->nextSibling()) {
            if (!c->isFloatingOrPositioned() && !c->isText() && !c->isInlineFlow())
                bottom = max(bottom, c->yPos() + c->lowestPosition(false));
        }
    }

    if (includeSelf && isRelPositioned())
        bottom += relativePositionOffsetY();         

    return bottom;
}

int RenderFlow::rightmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    ASSERT(!isInlineFlow());
    if (!includeOverflowInterior && hasOverflowClip())
        return includeSelf && m_height > 0 ? overflowWidth(false) : 0;

    int right = includeSelf && m_height > 0 ? m_width : 0;
    if (!hasColumns()) {
        // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
        // For now, we have to descend into all the children, since we may have a huge abs div inside
        // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
        // the abs div.
        for (RenderObject* c = firstChild(); c; c = c->nextSibling()) {
            if (!c->isFloatingOrPositioned() && !c->isText() && !c->isInlineFlow())
                right = max(right, c->xPos() + c->rightmostPosition(false));
        }
    }

    if (includeSelf && isRelPositioned())
        right += relativePositionOffsetX();

    return right;
}

int RenderFlow::leftmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    ASSERT(!isInlineFlow());
    if (!includeOverflowInterior && hasOverflowClip())
        return includeSelf && m_height > 0 ? overflowLeft(false) : m_width;

    int left = includeSelf && m_height > 0 ? 0 : m_width;
    if (!hasColumns()) {
        // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
        // For now, we have to descend into all the children, since we may have a huge abs div inside
        // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
        // the abs div.
        for (RenderObject* c = firstChild(); c; c = c->nextSibling()) {
            if (!c->isFloatingOrPositioned() && !c->isText() && !c->isInlineFlow())
                left = min(left, c->xPos() + c->leftmostPosition(false));
        }
    }

    if (includeSelf && isRelPositioned())
        left += relativePositionOffsetX(); 

    return left;
}

IntRect RenderFlow::caretRect(int offset, EAffinity affinity, int* extraWidthToEndOfLine)
{
    // Do the normal calculation in most cases.
    if (firstChild() || style()->display() == INLINE)
        return RenderContainer::caretRect(offset, affinity, extraWidthToEndOfLine);

    // This is a special case:
    // The element is not an inline element, and it's empty. So we have to
    // calculate a fake position to indicate where objects are to be inserted.
    
    // FIXME: This does not take into account either :first-line or :first-letter
    // However, as soon as some content is entered, the line boxes will be
    // constructed and this kludge is not called any more. So only the caret size
    // of an empty :first-line'd block is wrong. I think we can live with that.
    RenderStyle* currentStyle = firstLineStyle();
    int height = lineHeight(true);
    const int caretWidth = 1;

    enum CaretAlignment { alignLeft, alignRight, alignCenter };

    CaretAlignment alignment = alignLeft;

    switch (currentStyle->textAlign()) {
        case TAAUTO:
        case JUSTIFY:
            if (currentStyle->direction() == RTL)
                alignment = alignRight;
            break;
        case LEFT:
        case WEBKIT_LEFT:
            break;
        case CENTER:
        case WEBKIT_CENTER:
            alignment = alignCenter;
            break;
        case RIGHT:
        case WEBKIT_RIGHT:
            alignment = alignRight;
            break;
    }

    int x = borderLeft() + paddingLeft();
    int w = width();

    switch (alignment) {
        case alignLeft:
            break;
        case alignCenter:
            x = (x + w - (borderRight() + paddingRight())) / 2;
            break;
        case alignRight:
            x = w - (borderRight() + paddingRight());
            break;
    }

    if (extraWidthToEndOfLine) {
        if (isRenderBlock()) {
            *extraWidthToEndOfLine = w - (x + caretWidth);
        } else {
            // FIXME: This code looks wrong.
            // myRight and containerRight are set up, but then clobbered.
            // So *extraWidthToEndOfLine will always be 0 here.

            int myRight = x + caretWidth;
            int ignore;
            absolutePositionForContent(myRight, ignore);

            int containerRight = containingBlock()->xPos() + containingBlockWidth();
            absolutePositionForContent(containerRight, ignore);

            *extraWidthToEndOfLine = containerRight - myRight;
        }
    }

    int absx, absy;
    absolutePositionForContent(absx, absy);
    x += absx;
    int y = absy + paddingTop() + borderTop();

    return IntRect(x, y, caretWidth, height);
}

void RenderFlow::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    if (isRenderBlock()) {
        // Continuations should include their margins in the outline rect.
        if (continuation()) {
            bool nextInlineHasLineBox = continuation()->firstLineBox();
            bool prevInlineHasLineBox = static_cast<RenderFlow*>(continuation()->element()->renderer())->firstLineBox();
            int topMargin = prevInlineHasLineBox ? collapsedMarginTop() : 0;
            int bottomMargin = nextInlineHasLineBox ? collapsedMarginBottom() : 0;
            graphicsContext->addFocusRingRect(IntRect(tx, ty - topMargin, 
                                                      width(), height() + topMargin + bottomMargin));
        } else
            graphicsContext->addFocusRingRect(IntRect(tx, ty, width(), height()));
    }

    if (!hasOverflowClip() && !hasControlClip()) {
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
            graphicsContext->addFocusRingRect(IntRect(tx + curr->xPos(), ty + curr->yPos(), curr->width(), curr->height()));

        for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
            if (!curr->isText() && !curr->isListMarker()) {
                int x = 0;
                int y = 0;
                if (curr->layer()) 
                    curr->absolutePosition(x, y);
                else {
                    x = tx + curr->xPos();
                    y = ty + curr->yPos();
                }
                curr->addFocusRingRects(graphicsContext, x, y);
            }
    }

    if (continuation()) {
        if (isInline())
            continuation()->addFocusRingRects(graphicsContext, 
                                              tx - containingBlock()->xPos() + continuation()->xPos(),
                                              ty - containingBlock()->yPos() + continuation()->yPos());
        else
            continuation()->addFocusRingRects(graphicsContext, 
                                              tx - xPos() + continuation()->containingBlock()->xPos(),
                                              ty - yPos() + continuation()->containingBlock()->yPos());
    }
}

void RenderFlow::paintOutline(GraphicsContext* graphicsContext, int tx, int ty)
{
    if (!hasOutline())
        return;
    
    if (style()->outlineStyleIsAuto() || hasOutlineAnnotation()) {
        int ow = style()->outlineWidth();
        Color oc = style()->outlineColor();
        if (!oc.isValid())
            oc = style()->color();

        graphicsContext->initFocusRing(ow, style()->outlineOffset());
        addFocusRingRects(graphicsContext, tx, ty);
        if (style()->outlineStyleIsAuto())
            graphicsContext->drawFocusRing(oc);
        else
            addPDFURLRect(graphicsContext, graphicsContext->focusRingBoundingRect());
        graphicsContext->clearFocusRing();
    }

    if (style()->outlineStyleIsAuto() || style()->outlineStyle() <= BHIDDEN)
        return;

    Vector<IntRect> rects;

    rects.append(IntRect());
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
        rects.append(IntRect(curr->xPos(), curr->yPos(), curr->width(), curr->height()));

    rects.append(IntRect());

    for (unsigned i = 1; i < rects.size() - 1; i++)
        paintOutlineForLine(graphicsContext, tx, ty, rects.at(i - 1), rects.at(i), rects.at(i + 1));
}

void RenderFlow::paintOutlineForLine(GraphicsContext* graphicsContext, int tx, int ty,
                                     const IntRect& lastline, const IntRect& thisline, const IntRect& nextline)
{
    int ow = style()->outlineWidth();
    EBorderStyle os = style()->outlineStyle();
    Color oc = style()->outlineColor();
    if (!oc.isValid())
        oc = style()->color();

    int offset = style()->outlineOffset();

    int t = ty + thisline.y() - offset;
    int l = tx + thisline.x() - offset;
    int b = ty + thisline.bottom() + offset;
    int r = tx + thisline.right() + offset;
    
    // left edge
    drawBorder(graphicsContext,
               l - ow,
               t - (lastline.isEmpty() || thisline.x() < lastline.x() || (lastline.right() - 1) <= thisline.x() ? ow : 0),
               l,
               b + (nextline.isEmpty() || thisline.x() <= nextline.x() || (nextline.right() - 1) <= thisline.x() ? ow : 0),
               BSLeft,
               oc, style()->color(), os,
               (lastline.isEmpty() || thisline.x() < lastline.x() || (lastline.right() - 1) <= thisline.x() ? ow : -ow),
               (nextline.isEmpty() || thisline.x() <= nextline.x() || (nextline.right() - 1) <= thisline.x() ? ow : -ow));
    
    // right edge
    drawBorder(graphicsContext,
               r,
               t - (lastline.isEmpty() || lastline.right() < thisline.right() || (thisline.right() - 1) <= lastline.x() ? ow : 0),
               r + ow,
               b + (nextline.isEmpty() || nextline.right() <= thisline.right() || (thisline.right() - 1) <= nextline.x() ? ow : 0),
               BSRight,
               oc, style()->color(), os,
               (lastline.isEmpty() || lastline.right() < thisline.right() || (thisline.right() - 1) <= lastline.x() ? ow : -ow),
               (nextline.isEmpty() || nextline.right() <= thisline.right() || (thisline.right() - 1) <= nextline.x() ? ow : -ow));
    // upper edge
    if (thisline.x() < lastline.x())
        drawBorder(graphicsContext,
                   l - ow,
                   t - ow,
                   min(r+ow, (lastline.isEmpty() ? 1000000 : tx + lastline.x())),
                   t ,
                   BSTop, oc, style()->color(), os,
                   ow,
                   (!lastline.isEmpty() && tx + lastline.x() + 1 < r + ow) ? -ow : ow);
    
    if (lastline.right() < thisline.right())
        drawBorder(graphicsContext,
                   max(lastline.isEmpty() ? -1000000 : tx + lastline.right(), l - ow),
                   t - ow,
                   r + ow,
                   t ,
                   BSTop, oc, style()->color(), os,
                   (!lastline.isEmpty() && l - ow < tx + lastline.right()) ? -ow : ow,
                   ow);
    
    // lower edge
    if (thisline.x() < nextline.x())
        drawBorder(graphicsContext,
                   l - ow,
                   b,
                   min(r + ow, !nextline.isEmpty() ? tx + nextline.x() + 1 : 1000000),
                   b + ow,
                   BSBottom, oc, style()->color(), os,
                   ow,
                   (!nextline.isEmpty() && tx + nextline.x() + 1 < r + ow) ? -ow : ow);
    
    if (nextline.right() < thisline.right())
        drawBorder(graphicsContext,
                   max(!nextline.isEmpty() ? tx + nextline.right() : -1000000, l - ow),
                   b,
                   r + ow,
                   b + ow,
                   BSBottom, oc, style()->color(), os,
                   (!nextline.isEmpty() && l - ow < tx + nextline.right()) ? -ow : ow,
                   ow);
}

#ifndef NDEBUG

void RenderFlow::checkConsistency() const
{
#ifdef CHECK_CONSISTENCY
    const InlineFlowBox* prev = 0;
    for (const InlineFlowBox* child = m_firstLineBox; child != 0; child = child->nextFlowBox()) {
        ASSERT(child->object() == this);
        ASSERT(child->prevFlowBox() == prev);
        prev = child;
    }
    ASSERT(prev == m_lastLineBox);
#endif
}

#endif

} // namespace WebCore
