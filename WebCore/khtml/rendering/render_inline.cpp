/*
 * This file is part of the render object implementation for KHTML.
 *
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
#include "render_inline.h"
#include "render_block.h"

using namespace khtml;

RenderInline::RenderInline(DOM::NodeImpl* node)
:RenderFlow(node)
{}

RenderInline::~RenderInline()
{}

void RenderInline::setStyle(RenderStyle* _style)
{
    setInline(true);
    RenderFlow::setStyle(_style);
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
}

void RenderInline::addChildToFlow(RenderObject* newChild, RenderObject* beforeChild)
{
    setLayouted( false );
    
    insertPseudoChild(RenderStyle::BEFORE, newChild, beforeChild);

    if (!newChild->isText() && newChild->style()->position() != STATIC)
        setOverhangingContents();
    
    if (!newChild->isInline() && !newChild->isSpecial() )
    {
        // We are placing a block inside an inline. We have to perform a split of this
        // inline into continuations.  This involves creating an anonymous block box to hold
        // |newChild|.  We then make that block box a continuation of this inline.  We take all of
        // the children after |beforeChild| and put them in a clone of this object.
        RenderStyle *newStyle = new RenderStyle();
        newStyle->inheritFrom(style());
        newStyle->setDisplay(BLOCK);

        RenderBlock *newBox = new (renderArena()) RenderBlock(0 /* anonymous box */);
        newBox->setStyle(newStyle);
        newBox->setIsAnonymousBox(true);
        RenderFlow* oldContinuation = continuation();
        setContinuation(newBox);
        splitFlow(beforeChild, newBox, newChild, oldContinuation);
        return;
    }

    RenderBox::addChild(newChild,beforeChild);
    
    newChild->setLayouted( false );
    newChild->setMinMaxKnown( false );
    insertPseudoChild(RenderStyle::AFTER, newChild, beforeChild);
}

static RenderInline* cloneInline(RenderFlow* src)
{
    RenderInline *o = new (src->renderArena()) RenderInline(src->element());
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
    // then from |this| and place them in the clone.
    RenderObject* o = beforeChild;
    while (o) {
        RenderObject* tmp = o;
        o = tmp->nextSibling();
        clone->appendChildNode(removeChildNode(tmp));
        tmp->setLayouted(false);
        tmp->setMinMaxKnown(false);
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
        clone->appendChildNode(cloneChild);

        // Hook the clone up as a continuation of |curr|.
        RenderFlow* oldCont = curr->continuation();
        curr->setContinuation(clone);
        clone->setContinuation(oldCont);

        // Now we need to take all of the children starting from the first child
        // *after* currChild and append them all to the clone.
        o = currChild->nextSibling();
        while (o) {
            RenderObject* tmp = o;
            o = tmp->nextSibling();
            clone->appendChildNode(curr->removeChildNode(tmp));
            tmp->setLayouted(false);
            tmp->setMinMaxKnown(false);
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
    RenderBlock* block = containingBlock();

    RenderStyle* newStyle = new RenderStyle();
    newStyle->inheritFrom(block->style());
    newStyle->setDisplay(BLOCK);
    RenderBlock *pre = new (renderArena()) RenderBlock(0 /* anonymous box */);
    pre->setStyle(newStyle);
    pre->setIsAnonymousBox(true);
    pre->setChildrenInline(true);

    newStyle = new RenderStyle();
    newStyle->inheritFrom(block->style());
    newStyle->setDisplay(BLOCK);
    RenderBlock *post = new (renderArena()) RenderBlock(0 /* anonymous box */);
    post->setStyle(newStyle);
    post->setIsAnonymousBox(true);
    post->setChildrenInline(true);

    RenderObject* boxFirst = block->firstChild();
    block->insertChildNode(pre, boxFirst);
    block->insertChildNode(newBlockBox, boxFirst);
    block->insertChildNode(post, boxFirst);
    block->setChildrenInline(false);

    RenderObject* o = boxFirst;
    while (o)
    {
        RenderObject* no = o;
        o = no->nextSibling();
        pre->appendChildNode(block->removeChildNode(no));
        no->setLayouted(false);
        no->setMinMaxKnown(false);
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
    pre->setLayouted(false);
    newBlockBox->close();
    newBlockBox->setPos(0, -500000);
    newBlockBox->setLayouted(false);
    post->close();
    post->setPos(0, -500000);
    post->setLayouted(false);

    block->setLayouted(false);
    block->setMinMaxKnown(false);
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

    // FIXME: This function will eventually get much more complicated. :) - dwh
    // paint contents
    RenderObject *child = firstChild();
    while(child != 0)
    {
        if(!child->layer() && !child->isFloating())
            child->paint(p, _x, _y, _w, _h, _tx, _ty, paintAction);
        child = child->nextSibling();
    }
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

short RenderInline::offsetWidth() const
{
    short w = 0;
    RenderObject* object = firstChild();
    while (object) {
        w += object->offsetWidth();
        object = object->nextSibling();
    }
    return w;
}

int RenderInline::offsetHeight() const
{
    if (firstChild())
        return firstChild()->offsetHeight();
    return height();
}

int RenderInline::offsetLeft() const
{
    int x = RenderFlow::offsetLeft();
    RenderObject* textChild = (RenderObject*)this;
    while (textChild && textChild->isInline() && !textChild->isText())
        textChild = textChild->firstChild();
    if (textChild && textChild != this)
        x += textChild->xPos() - textChild->borderLeft() - textChild->paddingLeft();
    return x;
}

int RenderInline::offsetTop() const
{
    RenderObject* textChild = (RenderObject*)this;
    while (textChild && textChild->isInline() && !textChild->isText())
        textChild = textChild->firstChild();
    int y = RenderFlow::offsetTop();
    if (textChild && textChild != this)
        y += textChild->yPos() - textChild->borderTop() - textChild->paddingTop();
    return y;
}

const char *RenderInline::renderName() const
{
    if (isRelPositioned())
        return "RenderInline (relative positioned)";
    if (isAnonymousBox())
        return "RenderInline (anonymous)";
    return "RenderInline";
}
