/*
 * This file is part of the render object implementation for KHTML.
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
 *
 */

#include <kglobal.h>
#include "render_arena.h"
#include "render_inline.h"
#include "render_block.h"
#include "xml/dom_docimpl.h"

using namespace khtml;

RenderInline::RenderInline(DOM::NodeImpl* node)
:RenderFlow(node), m_isContinuation(false)
{}

RenderInline::~RenderInline()
{}

void RenderInline::setStyle(RenderStyle* _style)
{
    RenderFlow::setStyle(_style);
    setInline(true);

    // Ensure that all of the split inlines pick up the new style. We
    // only do this if we're an inline, since we don't want to propagate
    // a block's style to the other inlines.
    // e.g., <font>foo <h4>goo</h4> moo</font>.  The <font> inlines before
    // and after the block share the same style, but the block doesn't
    // need to pass its style on to anyone else.
    RenderFlow* currCont = continuation();
    while (currCont) {
        if (currCont->isInline()) {
            RenderFlow* nextCont = currCont->continuation();
            currCont->setContinuation(0);
            currCont->setStyle(style());
            currCont->setContinuation(nextCont);
        }
        currCont = currCont->continuation();
    }

    // Update pseudos for :before and :after now.
    updatePseudoChild(RenderStyle::BEFORE, firstChild());
    updatePseudoChild(RenderStyle::AFTER, lastChild());
}

bool RenderInline::isInlineContinuation() const
{
    return m_isContinuation;
}

void RenderInline::addChildToFlow(RenderObject* newChild, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild && lastChild() && lastChild()->style()->styleType() == RenderStyle::AFTER)
        beforeChild = lastChild();
    
    if (!newChild->isText() && newChild->style()->position() != STATIC)
        setOverhangingContents();
    
    if (!newChild->isInline() && !newChild->isFloatingOrPositioned() )
    {
        // We are placing a block inside an inline. We have to perform a split of this
        // inline into continuations.  This involves creating an anonymous block box to hold
        // |newChild|.  We then make that block box a continuation of this inline.  We take all of
        // the children after |beforeChild| and put them in a clone of this object.
        RenderStyle *newStyle = new RenderStyle();
        newStyle->inheritFrom(style());
        newStyle->setDisplay(BLOCK);

        RenderBlock *newBox = new (renderArena()) RenderBlock(document() /* anonymous box */);
        newBox->setStyle(newStyle);
        RenderFlow* oldContinuation = continuation();
        setContinuation(newBox);

        // Someone may have put a <p> inside a <q>, causing a split.  When this happens, the :after content
        // has to move into the inline continuation.  Call updatePseudoChild to ensure that our :after
        // content gets properly destroyed.
        bool isLastChild = (beforeChild == lastChild());
        updatePseudoChild(RenderStyle::AFTER, lastChild());
        if (isLastChild && beforeChild != lastChild())
            beforeChild = 0; // We destroyed the last child, so now we need to update our insertion
                             // point to be 0.  It's just a straight append now.
        
        splitFlow(beforeChild, newBox, newChild, oldContinuation);
        return;
    }

    RenderBox::addChild(newChild,beforeChild);

    newChild->setNeedsLayoutAndMinMaxRecalc();
}

RenderInline* RenderInline::cloneInline(RenderFlow* src)
{
    RenderInline *o = new (src->renderArena()) RenderInline(src->element());
    o->m_isContinuation = true;
    o->setStyle(src->style());
    return o;
}

void RenderInline::splitInlines(RenderBlock* fromBlock, RenderBlock* toBlock,
                                RenderBlock* middleBlock,
                                RenderObject* beforeChild, RenderFlow* oldCont)
{
    // Create a clone of this inline.
    RenderInline* clone = cloneInline(this);
    clone->setContinuation(oldCont);
    
    // Now take all of the children from beforeChild to the end and remove
    // them from |this| and place them in the clone.
    RenderObject* o = beforeChild;
    while (o) {
        RenderObject* tmp = o;
        o = tmp->nextSibling();
        clone->addChildToFlow(removeChildNode(tmp), 0);
        tmp->setNeedsLayoutAndMinMaxRecalc();
    }

    // Hook |clone| up as the continuation of the middle block.
    middleBlock->setContinuation(clone);

    // We have been reparented and are now under the fromBlock.  We need
    // to walk up our inline parent chain until we hit the containing block.
    // Once we hit the containing block we're done.
    RenderFlow* curr = static_cast<RenderFlow*>(parent());
    RenderFlow* currChild = this;
    while (curr && curr != fromBlock) {
        // Create a new clone.
        RenderInline* cloneChild = clone;
        clone = cloneInline(curr);

        // Insert our child clone as the first child.
        clone->addChildToFlow(cloneChild, 0);

        // Hook the clone up as a continuation of |curr|.
        RenderFlow* oldCont = curr->continuation();
        curr->setContinuation(clone);
        clone->setContinuation(oldCont);

        // Someone may have indirectly caused a <q> to split.  When this happens, the :after content
        // has to move into the inline continuation.  Call updatePseudoChild to ensure that the inline's :after
        // content gets properly destroyed.
        curr->updatePseudoChild(RenderStyle::AFTER, curr->lastChild());
        
        // Now we need to take all of the children starting from the first child
        // *after* currChild and append them all to the clone.
        o = currChild->nextSibling();
        while (o) {
            RenderObject* tmp = o;
            o = tmp->nextSibling();
            clone->addChildToFlow(curr->removeChildNode(tmp), 0);
            tmp->setNeedsLayoutAndMinMaxRecalc();
        }

        // Keep walking up the chain.
        currChild = curr;
        curr = static_cast<RenderFlow*>(curr->parent());
    }

    // Now we are at the block level. We need to put the clone into the toBlock.
    toBlock->appendChildNode(clone);

    // Now take all the children after currChild and remove them from the fromBlock
    // and put them in the toBlock.
    o = currChild->nextSibling();
    while (o) {
        RenderObject* tmp = o;
        o = tmp->nextSibling();
        toBlock->appendChildNode(fromBlock->removeChildNode(tmp));
    }
}

void RenderInline::splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                             RenderObject* newChild, RenderFlow* oldCont)
{
    RenderBlock* pre = 0;
    RenderBlock* block = containingBlock();
    bool madeNewBeforeBlock = false;
    if (block->isAnonymous()) {
        // We can reuse this block and make it the preBlock of the next continuation.
        pre = block;
        block = block->containingBlock();
    }
    else {
        // No anonymous block available for use.  Make one.
        pre = block->createAnonymousBlock();
        madeNewBeforeBlock = true;
    }

    RenderBlock* post = block->createAnonymousBlock();
    
    RenderObject* boxFirst = madeNewBeforeBlock ? block->firstChild() : pre->nextSibling();
    if (madeNewBeforeBlock)
        block->insertChildNode(pre, boxFirst);
    block->insertChildNode(newBlockBox, boxFirst);
    block->insertChildNode(post, boxFirst);
    block->setChildrenInline(false);

    if (madeNewBeforeBlock) {
        RenderObject* o = boxFirst;
        while (o)
        {
            RenderObject* no = o;
            o = no->nextSibling();
            pre->appendChildNode(block->removeChildNode(no));
            no->setNeedsLayoutAndMinMaxRecalc();
        }
    }

    splitInlines(pre, post, newBlockBox, beforeChild, oldCont);

    // We already know the newBlockBox isn't going to contain inline kids, so avoid wasting
    // time in makeChildrenNonInline by just setting this explicitly up front.
    newBlockBox->setChildrenInline(false);

    // We don't just call addChild, since it would pass things off to the
    // continuation, so we call addChildToFlow explicitly instead.  We delayed
    // adding the newChild until now so that the |newBlockBox| would be fully
    // connected, thus allowing newChild access to a renderArena should it need
    // to wrap itself in additional boxes (e.g., table construction).
    newBlockBox->addChildToFlow(newChild, 0);

    // XXXdwh is any of this even necessary? I don't think it is.
    pre->close();
    pre->setPos(0, -500000);
    pre->setNeedsLayout(true);
    newBlockBox->close();
    newBlockBox->setPos(0, -500000);
    newBlockBox->setNeedsLayout(true);
    post->close();
    post->setPos(0, -500000);
    post->setNeedsLayout(true);

    block->setNeedsLayoutAndMinMaxRecalc();
}

void RenderInline::paint(QPainter *p, int _x, int _y, int _w, int _h,
                      int _tx, int _ty, PaintAction paintAction)
{
    paintObject(p, _x, _y, _w, _h, _tx, _ty, paintAction);
}

void RenderInline::paintObject(QPainter *p, int _x, int _y,
                             int _w, int _h, int _tx, int _ty, PaintAction paintAction)
{
#ifdef DEBUG_LAYOUT
    //    kdDebug( 6040 ) << renderName() << "(RenderInline) " << this << " ::paintObject() w/h = (" << width() << "/" << height() << ")" << endl;
#endif

    // If we have an inline root, it has to call the special root box decoration painting
    // function.
    if (isRoot() &&
        (paintAction == PaintActionElementBackground || paintAction == PaintActionChildBackground) &&
        shouldPaintBackgroundOrBorder() && style()->visibility() == VISIBLE) {
        paintRootBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);
    }
    
    // We're done.  We don't bother painting any children.
    if (paintAction == PaintActionElementBackground)
        return;
    // We don't paint our own background, but we do let the kids paint their backgrounds.
    if (paintAction == PaintActionChildBackgrounds)
        paintAction = PaintActionChildBackground;

    paintLineBoxBackgroundBorder(p, _x, _y, _w, _h, _tx, _ty, paintAction);
    
    RenderObject *child = firstChild();
    while(child != 0)
    {
        if(!child->layer() && !child->isFloating())
            child->paint(p, _x, _y, _w, _h, _tx, _ty, paintAction);
        child = child->nextSibling();
    }

    paintLineBoxDecorations(p, _x, _y, _w, _h, _tx, _ty, paintAction);
    if (style()->visibility() == VISIBLE && paintAction == PaintActionOutline) {
#if APPLE_CHANGES
        if (style()->outlineStyleIsAuto())
            paintFocusRing(p, _tx, _ty);
        else
#endif
        paintOutlines(p, _tx, _ty);
    }
}

void RenderInline::absoluteRects(QValueList<QRect>& rects, int _tx, int _ty)
{
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
        rects.append(QRect(_tx + curr->xPos(), _ty + curr->yPos(), curr->width(), curr->height()));
    
    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        if (!curr->isText())
            curr->absoluteRects(rects, _tx + curr->xPos(), _ty + curr->yPos());
    
    if (continuation())
        continuation()->absoluteRects(rects, 
                                      _tx - containingBlock()->xPos() + continuation()->xPos(),
                                      _ty - containingBlock()->yPos() + continuation()->yPos());
}

#if APPLE_CHANGES
void RenderInline::addFocusRingRects(QPainter *p, int _tx, int _ty)
{
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        p->addFocusRingRect(_tx + curr->xPos(), 
                            _ty + curr->yPos(), 
                            curr->width(), 
                            curr->height());
    }
    
    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isText())
            curr->addFocusRingRects(p, _tx + curr->xPos(), _ty + curr->yPos());
    }
    
    if (continuation())
        continuation()->addFocusRingRects(p, 
                                          _tx - containingBlock()->xPos() + continuation()->xPos(),
                                          _ty - containingBlock()->yPos() + continuation()->yPos());
}

void RenderInline::paintFocusRing(QPainter *p, int tx, int ty)
{
    int ow = style()->outlineWidth();
    if (ow == 0 || m_isContinuation) // Continuations get painted by the original inline.
        return;

    QColor oc = style()->outlineColor();
    if (!oc.isValid())
        oc = style()->color();

    p->initFocusRing(ow,  style()->outlineOffset(), oc);
    addFocusRingRects(p, tx, ty);
    p->drawFocusRing();
    p->clearFocusRing();
}
#endif

void RenderInline::paintOutlines(QPainter *p, int _tx, int _ty)
{
    if (style()->outlineWidth() == 0 || style()->outlineStyle() <= BHIDDEN)
        return;
    
    QPtrList <QRect> rects;
    rects.setAutoDelete(true);

    rects.append(new QRect(0,0,0,0));
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        rects.append(new QRect(curr->xPos(), curr->yPos(), curr->width(), curr->height()));
    }
    rects.append(new QRect(0,0,0,0));

    for (unsigned int i = 1; i < rects.count() - 1; i++)
        paintOutline(p, _tx, _ty, *rects.at(i-1), *rects.at(i), *rects.at(i+1));
}

void RenderInline::paintOutline(QPainter *p, int tx, int ty, const QRect &lastline, const QRect &thisline, const QRect &nextline)
{
    int ow = style()->outlineWidth();
    if (ow == 0 || m_isContinuation) // Continuations get painted by the original inline.
        return;
    
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
                   QMIN(r+ow, (lastline.isValid()? tx+lastline.left() : 1000000)),
                   t ,
                   BSTop, oc, style()->color(), os,
                   ow,
                   (lastline.isValid() && tx+lastline.left()+1<r+ow ? -ow : ow),
                   true);
    
    if (lastline.right() < thisline.right())
        drawBorder(p,
                   QMAX(lastline.isValid()?tx + lastline.right() + 1:-1000000, l - ow),
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
                   QMIN(r+ow, nextline.isValid()? tx+nextline.left()+1 : 1000000),
                   b + ow,
                   BSBottom, oc, style()->color(), os,
                   ow,
                   (nextline.isValid() && tx+nextline.left()+1<r+ow? -ow : ow),
                   true);
    
    if (nextline.right() < thisline.right())
        drawBorder(p,
                   QMAX(nextline.isValid()?tx+nextline.right()+1:-1000000 , l-ow),
                   b,
                   r + ow,
                   b + ow,
                   BSBottom, oc, style()->color(), os,
                   (nextline.isValid() && l-ow < tx+nextline.right()+1? -ow : ow),
                   ow,
                   true);
}

void RenderInline::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderInline)::calcMinMaxWidth() this=" << this << endl;
#endif

    // Irrelevant, since some enclosing block will actually measure us and our children.
    m_minWidth = 0;
    m_maxWidth = 0;

    setMinMaxKnown();
}

bool RenderInline::requiresLayer() {
    return isRoot() || isRelPositioned() || style()->opacity() < 1.0f;
}

short RenderInline::width() const
{
    // Return the width of the minimal left side and the maximal right side.
    short leftSide = 0;
    short rightSide = 0;
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        if (curr == firstLineBox() || curr->xPos() < leftSide)
            leftSide = curr->xPos();
        if (curr == firstLineBox() || curr->xPos() + curr->width() > rightSide)
            rightSide = curr->xPos() + curr->width();
    }
    
    return rightSide - leftSide;
}

int RenderInline::height() const
{
    int h = 0;
    if (firstLineBox())
        h = lastLineBox()->yPos() + lastLineBox()->height() - firstLineBox()->yPos();
    return h;
}

int RenderInline::offsetLeft() const
{
    int x = RenderFlow::offsetLeft();
    if (firstLineBox())
        x += firstLineBox()->xPos();
    return x;
}

int RenderInline::offsetTop() const
{
    int y = RenderFlow::offsetTop();
    if (firstLineBox())
        y += firstLineBox()->yPos();
    return y;
}

const char *RenderInline::renderName() const
{
    if (isRelPositioned())
        return "RenderInline (relative positioned)";
    if (isAnonymous())
        return "RenderInline (anonymous)";
    return "RenderInline";
}

bool RenderInline::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty,
                               HitTestAction hitTestAction, bool inside)
{
    // Check our kids if our HitTestAction says to.
    if (hitTestAction != HitTestSelfOnly) {
        for (RenderObject* child = lastChild(); child; child = child->previousSibling())
            if (!child->layer() && !child->isFloating() && child->nodeAtPoint(info, _x, _y, _tx, _ty))
                inside = true;
    }
    
    // Check our line boxes if we're still not inside.
    if (hitTestAction != HitTestChildrenOnly && !inside && style()->visibility() != HIDDEN) {
        // See if we're inside one of our line boxes.
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
            if((_y >=_ty + curr->m_y) && (_y < _ty + curr->m_y + curr->m_height) &&
               (_x >= _tx + curr->m_x) && (_x <_tx + curr->m_x + curr->m_width) ) {
                inside = true;
                break;
            }
        }
    }

    if (inside && element()) {
        if (info.innerNode() && info.innerNode()->renderer() &&
            !info.innerNode()->renderer()->isInline()) {
            // Within the same layer, inlines are ALWAYS fully above blocks.  Change inner node.
            info.setInnerNode(element());

            // Clear everything else.
            info.setInnerNonSharedNode(0);
            info.setURLElement(0);
        }

        if (!info.innerNode())
            info.setInnerNode(element());

        if(!info.innerNonSharedNode())
            info.setInnerNonSharedNode(element());
    }

    return inside;
}

