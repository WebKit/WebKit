/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
// -------------------------------------------------------------------------

#include "config.h"
#include <kdebug.h>
#include <assert.h>
#include <qpainter.h>

#include "rendering/render_flow.h"
#include "InlineTextBox.h"
#include "rendering/render_table.h"
#include "rendering/render_canvas.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "render_inline.h"
#include "render_block.h"
#include "render_arena.h"
#include "render_line.h"
#include "htmlnames.h"

using namespace DOM;
using namespace DOM::HTMLNames;
using namespace khtml;

RenderFlow* RenderFlow::createAnonymousFlow(DOM::DocumentImpl* doc, RenderStyle* style)
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
    RenderFlow* flow = continuationBefore(beforeChild);
    KHTMLAssert(!beforeChild || beforeChild->parent()->isRenderBlock() ||
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

void RenderFlow::addChild(RenderObject *newChild, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderFlow)::addChild( " << newChild->renderName() <<
                       ", " << (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
    kdDebug( 6040 ) << "current height = " << m_height << endl;
#endif

    if (continuation())
        return addChildWithContinuation(newChild, beforeChild);
    return addChildToFlow(newChild, beforeChild);
}

void RenderFlow::extractLineBox(InlineFlowBox* box)
{
    m_lastLineBox = box->prevFlowBox();
    if (box == m_firstLineBox)
        m_firstLineBox = 0;
    if (box->prevLineBox())
        box->prevLineBox()->setNextLineBox(0);
    box->setPreviousLineBox(0);
    for (InlineRunBox* curr = box; curr; curr = curr->nextLineBox())
        curr->setExtracted();
}

void RenderFlow::attachLineBox(InlineFlowBox* box)
{
    if (m_lastLineBox) {
        m_lastLineBox->setNextLineBox(box);
        box->setPreviousLineBox(m_lastLineBox);
    }
    else
        m_firstLineBox = box;
    InlineFlowBox* last = box;
    for (InlineFlowBox* curr = box; curr; curr = curr->nextFlowBox()) {
        curr->setExtracted(false);
        last = curr;
    }
    m_lastLineBox = last;
}

void RenderFlow::removeLineBox(InlineFlowBox* box)
{
    if (box == m_firstLineBox)
        m_firstLineBox = box->nextFlowBox();
    if (box == m_lastLineBox)
        m_lastLineBox = box->prevFlowBox();
    if (box->nextLineBox())
        box->nextLineBox()->setPreviousLineBox(box->prevLineBox());
    if (box->prevLineBox())
        box->prevLineBox()->setNextLineBox(box->nextLineBox());
}

void RenderFlow::deleteLineBoxes()
{
    if (m_firstLineBox) {
        RenderArena* arena = renderArena();
        InlineRunBox *curr=m_firstLineBox, *next=0;
        while (curr) {
            next = curr->nextLineBox();
            curr->destroy(arena);
            curr = next;
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
    RenderContainer::destroyLeftoverAnonymousChildren();
    
    if (!documentBeingDestroyed()) {
        if (m_firstLineBox) {
            // We can't wait for RenderContainer::destroy to clear the selection,
            // because by then we will have nuked the line boxes.
            if (isSelectionBorder())
                canvas()->clearSelection();

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
                    while (InlineBox *childBox = box->firstChild()) {
                        childBox->remove();
                    }
                }
            }
        }
        else if (isInline() && parent())
            parent()->dirtyLinesFromChangedChild(this);
    }

    deleteLineBoxes();

    RenderContainer::destroy();
}

void RenderFlow::dirtyLinesFromChangedChild(RenderObject* child)
{
    if (!parent() || selfNeedsLayout() || isTable())
        return;

    // For an empty inline, go ahead and propagate the check up to our parent.
    if (isInline() && !firstLineBox())
        return parent()->dirtyLinesFromChangedChild(this);
    
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
        }
        else if (curr->isText()) {
            InlineTextBox* textBox = static_cast<RenderText*>(curr)->lastTextBox();
            if (textBox)
                box = textBox->root();
        }
        else if (curr->isInlineFlow()) {
            InlineRunBox* runBox = static_cast<RenderFlow*>(curr)->lastLineBox();
            if (runBox)
                box = runBox->root();
        }
        
        if (box)
            break;
    }
    if (!box && firstLineBox())
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
        if (lh.value < 0) {
            if (s == style()) {
                if (m_lineHeight == -1)
                    m_lineHeight = RenderObject::lineHeight(false);
                return m_lineHeight;
            }
            return s->fontMetrics().lineSpacing();
	}
        if (lh.isPercent())
            return lh.minWidth(s->font().pixelSize());
        return lh.value;
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

    return flowBox;
}

void RenderFlow::paintLines(PaintInfo& i, int _tx, int _ty)
{
    // Only paint during the foreground/selection phases.
    if (i.phase != PaintActionForeground && i.phase != PaintActionSelection && i.phase != PaintActionOutline)
        return;

    bool inlineFlow = isInlineFlow();
    if (inlineFlow)
        KHTMLAssert(m_layer); // The only way a compact/run-in/inline could paint like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!firstLineBox())
        return;

    // We can check the first box and last box and avoid painting if we don't
    // intersect.  This is a quick short-circuit that we can take to avoid walking any lines.
    // FIXME: This check is flawed in two extremely obscure ways.
    // (1) If some line in the middle has a huge overflow, it might actually extend below the last line.
    // (2) The overflow from an inline block on a line is not reported to the line.
    int yPos = firstLineBox()->root()->selectionTop() - maximalOutlineSize(i.phase);
    int h = maximalOutlineSize(i.phase) + lastLineBox()->root()->selectionTop() + lastLineBox()->root()->selectionHeight() - yPos;
    yPos += _ty;
    if ((yPos >= i.r.y() + i.r.height()) || (yPos + h <= i.r.y()))
        return;

    // See if our root lines intersect with the dirty rect.  If so, then we paint
    // them.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    bool isPrinting = (i.p->device()->devType() == QInternal::Printer);
    for (InlineFlowBox* curr = firstLineBox(); curr; curr = curr->nextFlowBox()) {
        if (isPrinting) {
            // FIXME: This is a feeble effort to avoid splitting a line across two pages.
            // It is utterly inadequate, and this should not be done at paint time at all.
            // The whole way objects break across pages needs to be redone.
            RenderCanvas* c = canvas();
            // Try to avoid splitting a line vertically, but only if it's less than the height
            // of the entire page.
            if (curr->root()->bottomOverflow() - curr->root()->topOverflow() <= c->printRect().height()) {
                if (_ty + curr->root()->bottomOverflow() > c->printRect().y() + c->printRect().height()) {
                    if (_ty + curr->root()->topOverflow() < c->truncatedAt())
                        c->setBestTruncatedAt(_ty + curr->root()->topOverflow(), this);
                    // If we were able to truncate, don't paint.
                    if (_ty + curr->root()->topOverflow() >= c->truncatedAt())
                        break;
                }
            }
        }

        int top = kMin(curr->root()->topOverflow(), curr->root()->selectionTop()) - maximalOutlineSize(i.phase);
        int bottom = kMax(curr->root()->selectionTop() + curr->root()->selectionHeight(), curr->root()->bottomOverflow()) + maximalOutlineSize(i.phase);
        h = bottom - top;
        yPos = _ty + top;
        if ((yPos < i.r.y() + i.r.height()) && (yPos + h > i.r.y()))
            curr->paint(i, _tx, _ty);
    }

    if (i.phase == PaintActionOutline && i.outlineObjects) {
        // FIXME: Will the order in which we added objects to the dictionary be preserved? Probably not.
        // This means the paint order of outlines will be wrong, although this is a minor issue.
        QPtrDictIterator<RenderFlow> objects(*i.outlineObjects);
        for (objects.toFirst(); objects.current(); ++objects) {
            if (objects.current()->style()->outlineStyleIsAuto())
                objects.current()->paintFocusRing(i.p, _tx, _ty);
            else
                objects.current()->paintOutlines(i.p, _tx, _ty);
        }
        i.outlineObjects->clear();
    }
}

bool RenderFlow::hitTestLines(NodeInfo& i, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    bool inlineFlow = isInlineFlow();
    if (inlineFlow)
        KHTMLAssert(m_layer); // The only way a compact/run-in/inline could paint like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!firstLineBox())
        return false;

    // We can check the first box and last box and avoid hit testing if we don't
    // contain the point.  This is a quick short-circuit that we can take to avoid walking any lines.
    // FIXME: This check is flawed in two extremely obscure ways.
    // (1) If some line in the middle has a huge overflow, it might actually extend below the last line.
    // (2) The overflow from an inline block on a line is not reported to the line.
    if ((y >= ty + lastLineBox()->root()->bottomOverflow()) || (y < ty + firstLineBox()->root()->topOverflow()))
        return false;

    // See if our root lines contain the point.  If so, then we hit test
    // them further.  Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    for (InlineFlowBox* curr = lastLineBox(); curr; curr = curr->prevFlowBox()) {
        if (y >= ty + curr->root()->topOverflow() && y < ty + curr->root()->bottomOverflow()) {
            bool inside = curr->nodeAtPoint(i, x, y, tx, ty);
            if (inside) {
                setInnerNode(i);
                return true;
            }
        }
    }
    
    return false;
}

QRect RenderFlow::getAbsoluteRepaintRect()
{
    if (isInlineFlow()) {
        // Find our leftmost position.
        int left = 0;
        int top = firstLineBox() ? firstLineBox()->yPos() : 0;
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
            if (curr == firstLineBox() || curr->xPos() < left)
                left = curr->xPos();

        // Now invalidate a rectangle.
        int ow = style() ? style()->outlineSize() : 0;
        if (isCompact())
            left -= m_x;
        
        // We need to add in the relative position offsets of any inlines (including us) up to our
        // containing block.
        RenderBlock* cb = containingBlock();
        for (RenderObject* inlineFlow = this; inlineFlow && inlineFlow->isInlineFlow() && inlineFlow != cb; 
             inlineFlow = inlineFlow->parent()) {
             if (inlineFlow->style()->position() == RELATIVE && inlineFlow->layer())
                inlineFlow->layer()->relativePositionOffset(left, top);
        }

        QRect r(-ow+left, -ow+top, width()+ow*2, height()+ow*2);
        if (cb->hasOverflowClip()) {
            // cb->height() is inaccurate if we're in the middle of a layout of |cb|, so use the
            // layer's size instead.  Even if the layer's size is wrong, the layer itself will repaint
            // anyway if its size does change.
            int x = r.left();
            int y = r.top();
            QRect boxRect(0, 0, cb->layer()->width(), cb->layer()->height());
            cb->layer()->subtractScrollOffset(x,y); // For overflow:auto/scroll/hidden.
            QRect repaintRect(x, y, r.width(), r.height());
            r = repaintRect.intersect(boxRect);
        }
        cb->computeAbsoluteRepaintRect(r);
        
        if (ow) {
            for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
                if (!curr->isText()) {
                    QRect childRect = curr->getAbsoluteRepaintRectWithOutline(ow);
                    r = r.unite(childRect);
                }
            }
            
            if (continuation() && !continuation()->isInline()) {
                QRect contRect = continuation()->getAbsoluteRepaintRectWithOutline(ow);
                r = r.unite(contRect);
            }
        }
        
        return r;
    }
    else {
        if (firstLineBox() && firstLineBox()->topOverflow() < 0) {
            int ow = style() ? style()->outlineSize() : 0;
            QRect r(-ow, -ow+firstLineBox()->topOverflow(),
                    overflowWidth(false)+ow*2,
                    overflowHeight(false)+ow*2-firstLineBox()->topOverflow());
            computeAbsoluteRepaintRect(r);
            return r;
        }
    }

    return RenderContainer::getAbsoluteRepaintRect();
}

int
RenderFlow::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int bottom = RenderContainer::lowestPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return bottom;

    // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
    // For now, we have to descend into all the children, since we may have a huge abs div inside
    // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
    // the abs div.
    for (RenderObject *c = firstChild(); c; c = c->nextSibling()) {
        if (!c->isFloatingOrPositioned() && !c->isText()) {
            int lp = c->yPos() + c->lowestPosition(false);
            bottom = kMax(bottom, lp);
        }
    }
    
    return bottom;
}

int RenderFlow::rightmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int right = RenderContainer::rightmostPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return right;

    // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
    // For now, we have to descend into all the children, since we may have a huge abs div inside
    // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
    // the abs div.
    for (RenderObject *c = firstChild(); c; c = c->nextSibling()) {
        if (!c->isFloatingOrPositioned() && !c->isText()) {
            int rp = c->xPos() + c->rightmostPosition(false);
            right = kMax(right, rp);
        }
    }
    
    return right;
}

int RenderFlow::leftmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int left = RenderContainer::leftmostPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return left;
    
    // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
    // For now, we have to descend into all the children, since we may have a huge abs div inside
    // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
    // the abs div.
    for (RenderObject *c = firstChild(); c; c = c->nextSibling()) {
        if (!c->isFloatingOrPositioned() && !c->isText()) {
            int lp = c->xPos() + c->leftmostPosition(false);
            left = kMin(left, lp);
        }
    }
    
    return left;
}

QRect RenderFlow::caretRect(int offset, EAffinity affinity, int *extraWidthToEndOfLine)
{
    if (firstChild() || style()->display() == INLINE) {
        // Do the normal calculation
        return RenderContainer::caretRect(offset, affinity, extraWidthToEndOfLine);
    }

    // This is a special case:
    // The element is not an inline element, and it's empty. So we have to
    // calculate a fake position to indicate where objects are to be inserted.
    
    int _x, _y, width, height;
    
    // EDIT FIXME: this does neither take into regard :first-line nor :first-letter
    // However, as soon as some content is entered, the line boxes will be
    // constructed properly and this kludge is not called any more. So only
    // the caret size of an empty :first-line'd block is wrong, but I think we
    // can live with that.
    RenderStyle *currentStyle = firstLineStyle();
    //height = currentStyle->fontMetrics().height();
    height = lineHeight(true);
    width = 1;

    // EDIT FIXME: This needs to account for text direction
    int w = this->width();
    switch (currentStyle->textAlign()) {
        case LEFT:
        case KHTML_LEFT:
        case TAAUTO:
        case JUSTIFY:
        default:
            _x = 0;
            break;
        case CENTER:
        case KHTML_CENTER:
            _x = w / 2;
        break;
        case RIGHT:
        case KHTML_RIGHT:
            _x = w;
        break;
    }
    
    _y = 0;
    

    if (extraWidthToEndOfLine) {
        if (isRenderBlock()) {
            *extraWidthToEndOfLine = this->width() - (_x + width);
        } else {
            int myRight = _x + width;
            int ignore;
            absolutePosition(myRight, ignore);
            
            int containerRight = containingBlock()->xPos() + containingBlockWidth();
            absolutePosition(containerRight, ignore);
            
            *extraWidthToEndOfLine = containerRight - myRight;
        }
    }

    int absx, absy;
    absolutePosition(absx, absy, false);
    _x += absx + paddingLeft() + borderLeft();
    _y += absy + paddingTop() + borderTop();

    return QRect(_x, _y, width, height);
}

void RenderFlow::addFocusRingRects(QPainter *p, int _tx, int _ty)
{
    if (isRenderBlock())
       p->addFocusRingRect(_tx, _ty, width(), height());

    if (!hasOverflowClip()) {
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
            p->addFocusRingRect(_tx + curr->xPos(), _ty + curr->yPos(), curr->width(), curr->height());
        
        for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
            if (!curr->isText())
                curr->addFocusRingRects(p, _tx + curr->xPos(), _ty + curr->yPos());
    }
        
    if (continuation())
        continuation()->addFocusRingRects(p, 
                                          _tx - containingBlock()->xPos() + continuation()->xPos(),
                                          _ty - containingBlock()->yPos() + continuation()->yPos());
}

void RenderFlow::paintFocusRing(QPainter *p, int tx, int ty)
{
    int ow = style()->outlineWidth();
    QColor oc = style()->outlineColor();
    if (!oc.isValid())
        oc = style()->color();
    
    p->initFocusRing(ow,  style()->outlineOffset(), oc);
    addFocusRingRects(p, tx, ty);
    p->drawFocusRing();
    p->clearFocusRing();
}

void RenderFlow::paintOutlines(QPainter *p, int _tx, int _ty)
{
    if (style()->outlineStyle() <= BHIDDEN)
        return;
    
    QPtrList <QRect> rects;
    rects.setAutoDelete(true);
    
    rects.append(new QRect(0,0,0,0));
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        rects.append(new QRect(curr->xPos(), curr->yPos(), curr->width(), curr->height()));
    }
    rects.append(new QRect(0,0,0,0));
    
    for (unsigned int i = 1; i < rects.count() - 1; i++)
        paintOutlineForLine(p, _tx, _ty, *rects.at(i-1), *rects.at(i), *rects.at(i+1));
}

void RenderFlow::paintOutlineForLine(QPainter *p, int tx, int ty, const QRect &lastline, const QRect &thisline, const QRect &nextline)
{
    int ow = style()->outlineWidth();
    EBorderStyle os = style()->outlineStyle();
    QColor oc = style()->outlineColor();
    if (!oc.isValid())
        oc = style()->color();
    
    int offset = style()->outlineOffset();
    
    int t = ty + thisline.top() - offset;
    int l = tx + thisline.left() - offset;
    int b = ty + thisline.bottom() + offset + 1;
    int r = tx + thisline.right() + offset + 1;
    
    // left edge
    drawBorder(p,
               l - ow,
               t - (lastline.isEmpty() || thisline.left() < lastline.left() || lastline.right() <= thisline.left() ? ow : 0),
               l,
               b + (nextline.isEmpty() || thisline.left() <= nextline.left() || nextline.right() <= thisline.left() ? ow : 0),
               BSLeft,
               oc, style()->color(), os,
               (lastline.isEmpty() || thisline.left() < lastline.left() || lastline.right() <= thisline.left() ? ow : -ow),
               (nextline.isEmpty() || thisline.left() <= nextline.left() || nextline.right() <= thisline.left() ? ow : -ow),
               true);
    
    // right edge
    drawBorder(p,
               r,
               t - (lastline.isEmpty() || lastline.right() < thisline.right() || thisline.right() <= lastline.left() ? ow : 0),
               r + ow,
               b + (nextline.isEmpty() || nextline.right() <= thisline.right() || thisline.right() <= nextline.left() ? ow : 0),
               BSRight,
               oc, style()->color(), os,
               (lastline.isEmpty() || lastline.right() < thisline.right() || thisline.right() <= lastline.left() ? ow : -ow),
               (nextline.isEmpty() || nextline.right() <= thisline.right() || thisline.right() <= nextline.left() ? ow : -ow),
               true);
    // upper edge
    if ( thisline.left() < lastline.left())
        drawBorder(p,
                   l - ow,
                   t - ow,
                   kMin(r+ow, (lastline.isValid()? tx+lastline.left() : 1000000)),
                   t ,
                   BSTop, oc, style()->color(), os,
                   ow,
                   (lastline.isValid() && tx+lastline.left()+1<r+ow ? -ow : ow),
                   true);
    
    if (lastline.right() < thisline.right())
        drawBorder(p,
                   kMax(lastline.isValid()?tx + lastline.right() + 1:-1000000, l - ow),
                   t - ow,
                   r + ow,
                   t ,
                   BSTop, oc, style()->color(), os,
                   (lastline.isValid() && l-ow < tx+lastline.right()+1 ? -ow : ow),
                   ow,
                   true);
    
    // lower edge
    if ( thisline.left() < nextline.left())
        drawBorder(p,
                   l - ow,
                   b,
                   kMin(r+ow, nextline.isValid()? tx+nextline.left()+1 : 1000000),
                   b + ow,
                   BSBottom, oc, style()->color(), os,
                   ow,
                   (nextline.isValid() && tx+nextline.left()+1<r+ow? -ow : ow),
                   true);
    
    if (nextline.right() < thisline.right())
        drawBorder(p,
                   kMax(nextline.isValid()?tx+nextline.right()+1:-1000000 , l-ow),
                   b,
                   r + ow,
                   b + ow,
                   BSBottom, oc, style()->color(), os,
                   (nextline.isValid() && l-ow < tx+nextline.right()+1? -ow : ow),
                   ow,
                   true);
}
