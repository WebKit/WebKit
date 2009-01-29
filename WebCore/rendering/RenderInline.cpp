/*
 * This file is part of the render object implementation for KHTML.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 *
 */

#include "config.h"
#include "RenderInline.h"

#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "RenderArena.h"
#include "RenderBlock.h"
#include "RenderView.h"
#include "VisiblePosition.h"

using namespace std;

namespace WebCore {

RenderInline::RenderInline(Node* node)
    : RenderFlow(node)
    , m_continuation(0)
    , m_lineHeight(-1)
{
    setChildrenInline(true);
}

RenderInline::~RenderInline()
{
}

void RenderInline::destroy()
{
    // Detach our continuation first.
    if (m_continuation)
        m_continuation->destroy();
    m_continuation = 0;
    
    RenderFlow::destroy();
}

RenderInline* RenderInline::inlineContinuation() const
{
    if (!m_continuation || m_continuation->isInline())
        return static_cast<RenderInline*>(m_continuation);
    return static_cast<RenderBlock*>(m_continuation)->inlineContinuation();
}

void RenderInline::styleDidChange(RenderStyle::Diff diff, const RenderStyle* oldStyle)
{
    RenderFlow::styleDidChange(diff, oldStyle);

    setInline(true);
    setHasReflection(false);

    // Ensure that all of the split inlines pick up the new style. We
    // only do this if we're an inline, since we don't want to propagate
    // a block's style to the other inlines.
    // e.g., <font>foo <h4>goo</h4> moo</font>.  The <font> inlines before
    // and after the block share the same style, but the block doesn't
    // need to pass its style on to anyone else.
    for (RenderInline* currCont = inlineContinuation(); currCont; currCont = currCont->inlineContinuation()) {
        RenderBox* nextCont = currCont->continuation();
        currCont->setContinuation(0);
        currCont->setStyle(style());
        currCont->setContinuation(nextCont);
    }

    m_lineHeight = -1;

    // Update pseudos for :before and :after now.
    if (!isAnonymous() && document()->usesBeforeAfterRules()) {
        updateBeforeAfterContent(RenderStyle::BEFORE);
        updateBeforeAfterContent(RenderStyle::AFTER);
    }
}

static inline bool isAfterContent(RenderObject* child)
{
    if (!child)
        return false;
    if (child->style()->styleType() != RenderStyle::AFTER)
        return false;
    // Text nodes don't have their own styles, so ignore the style on a text node.
    if (child->isText() && !child->isBR())
        return false;
    return true;
}

void RenderInline::addChildToFlow(RenderObject* newChild, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild && isAfterContent(lastChild()))
        beforeChild = lastChild();

    if (!newChild->isInline() && !newChild->isFloatingOrPositioned()) {
        // We are placing a block inside an inline. We have to perform a split of this
        // inline into continuations.  This involves creating an anonymous block box to hold
        // |newChild|.  We then make that block box a continuation of this inline.  We take all of
        // the children after |beforeChild| and put them in a clone of this object.
        RefPtr<RenderStyle> newStyle = RenderStyle::create();
        newStyle->inheritFrom(style());
        newStyle->setDisplay(BLOCK);

        RenderBlock* newBox = new (renderArena()) RenderBlock(document() /* anonymous box */);
        newBox->setStyle(newStyle.release());
        RenderBox* oldContinuation = continuation();
        setContinuation(newBox);

        // Someone may have put a <p> inside a <q>, causing a split.  When this happens, the :after content
        // has to move into the inline continuation.  Call updateBeforeAfterContent to ensure that our :after
        // content gets properly destroyed.
        bool isLastChild = (beforeChild == lastChild());
        if (document()->usesBeforeAfterRules())
            updateBeforeAfterContent(RenderStyle::AFTER);
        if (isLastChild && beforeChild != lastChild())
            beforeChild = 0; // We destroyed the last child, so now we need to update our insertion
                             // point to be 0.  It's just a straight append now.

        splitFlow(beforeChild, newBox, newChild, oldContinuation);
        return;
    }

    RenderContainer::addChild(newChild, beforeChild);

    newChild->setNeedsLayoutAndPrefWidthsRecalc();
}

RenderInline* RenderInline::cloneInline(RenderFlow* src)
{
    RenderInline* o = new (src->renderArena()) RenderInline(src->element());
    o->setStyle(src->style());
    return o;
}

void RenderInline::splitInlines(RenderBlock* fromBlock, RenderBlock* toBlock,
                                RenderBlock* middleBlock,
                                RenderObject* beforeChild, RenderBox* oldCont)
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
        tmp->setNeedsLayoutAndPrefWidthsRecalc();
    }

    // Hook |clone| up as the continuation of the middle block.
    middleBlock->setInlineContinuation(clone);

    // We have been reparented and are now under the fromBlock.  We need
    // to walk up our inline parent chain until we hit the containing block.
    // Once we hit the containing block we're done.
    RenderFlow* curr = static_cast<RenderFlow*>(parent());
    RenderFlow* currChild = this;
    
    // FIXME: Because splitting is O(n^2) as tags nest pathologically, we cap the depth at which we're willing to clone.
    // There will eventually be a better approach to this problem that will let us nest to a much
    // greater depth (see bugzilla bug 13430) but for now we have a limit.  This *will* result in
    // incorrect rendering, but the alternative is to hang forever.
    unsigned splitDepth = 1;
    const unsigned cMaxSplitDepth = 200; 
    while (curr && curr != fromBlock) {
        if (splitDepth < cMaxSplitDepth) {
            // Create a new clone.
            RenderInline* cloneChild = clone;
            clone = cloneInline(curr);

            // Insert our child clone as the first child.
            clone->addChildToFlow(cloneChild, 0);

            // Hook the clone up as a continuation of |curr|.
            RenderInline* inlineCurr = static_cast<RenderInline*>(curr);
            RenderBox* oldCont = inlineCurr->continuation();
            inlineCurr->setContinuation(clone);
            clone->setContinuation(oldCont);

            // Someone may have indirectly caused a <q> to split.  When this happens, the :after content
            // has to move into the inline continuation.  Call updateBeforeAfterContent to ensure that the inline's :after
            // content gets properly destroyed.
            if (document()->usesBeforeAfterRules())
                curr->updateBeforeAfterContent(RenderStyle::AFTER);

            // Now we need to take all of the children starting from the first child
            // *after* currChild and append them all to the clone.
            o = currChild->nextSibling();
            while (o) {
                RenderObject* tmp = o;
                o = tmp->nextSibling();
                clone->addChildToFlow(curr->removeChildNode(tmp), 0);
                tmp->setNeedsLayoutAndPrefWidthsRecalc();
            }
        }
        
        // Keep walking up the chain.
        currChild = curr;
        curr = static_cast<RenderFlow*>(curr->parent());
        splitDepth++;
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
                             RenderObject* newChild, RenderBox* oldCont)
{
    RenderBlock* pre = 0;
    RenderBlock* block = containingBlock();
    
    // Delete our line boxes before we do the inline split into continuations.
    block->deleteLineBoxTree();
    
    bool madeNewBeforeBlock = false;
    if (block->isAnonymousBlock() && (!block->parent() || !block->parent()->createsAnonymousWrapper())) {
        // We can reuse this block and make it the preBlock of the next continuation.
        pre = block;
        block = block->containingBlock();
    } else {
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
        while (o) {
            RenderObject* no = o;
            o = no->nextSibling();
            pre->appendChildNode(block->removeChildNode(no));
            no->setNeedsLayoutAndPrefWidthsRecalc();
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

    // Always just do a full layout in order to ensure that line boxes (especially wrappers for images)
    // get deleted properly.  Because objects moves from the pre block into the post block, we want to
    // make new line boxes instead of leaving the old line boxes around.
    pre->setNeedsLayoutAndPrefWidthsRecalc();
    block->setNeedsLayoutAndPrefWidthsRecalc();
    post->setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderInline::paint(PaintInfo& paintInfo, int tx, int ty)
{
    paintLines(paintInfo, tx, ty);
}

void RenderInline::absoluteRects(Vector<IntRect>& rects, int tx, int ty, bool topLevel)
{
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
        rects.append(IntRect(tx + curr->xPos(), ty + curr->yPos(), curr->width(), curr->height()));

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (curr->isBox()) {
            RenderBox* box = toRenderBox(curr);
            curr->absoluteRects(rects, tx + box->x(), ty + box->y(), false);
        }
    }

    if (continuation() && topLevel)
        continuation()->absoluteRects(rects, 
                                      tx - containingBlock()->x() + continuation()->x(),
                                      ty - containingBlock()->y() + continuation()->y(),
                                      topLevel);
}

void RenderInline::absoluteQuads(Vector<FloatQuad>& quads, bool topLevel)
{
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
        FloatRect localRect(curr->xPos(), curr->yPos(), curr->width(), curr->height());
        quads.append(localToAbsoluteQuad(localRect));
    }
    
    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isText())
            curr->absoluteQuads(quads, false);
    }

    if (continuation() && topLevel)
        continuation()->absoluteQuads(quads, topLevel);
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

const char* RenderInline::renderName() const
{
    if (isRelPositioned())
        return "RenderInline (relative positioned)";
    if (isAnonymous())
        return "RenderInline (generated)";
    if (isRunIn())
        return "RenderInline (run-in)";
    return "RenderInline";
}

bool RenderInline::nodeAtPoint(const HitTestRequest& request, HitTestResult& result,
                                int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    return hitTestLines(request, result, x, y, tx, ty, hitTestAction);
}

VisiblePosition RenderInline::positionForCoordinates(int x, int y)
{
    // Translate the coords from the pre-anonymous block to the post-anonymous block.
    RenderBlock* cb = containingBlock();
    int parentBlockX = cb->x() + x;
    int parentBlockY = cb->y() + y;
    RenderBox* c = continuation();
    while (c) {
        RenderBox* contBlock = c;
        if (c->isInline() || c->firstChild())
            return c->positionForCoordinates(parentBlockX - contBlock->x(), parentBlockY - contBlock->y());
        c = static_cast<RenderBlock*>(c)->inlineContinuation();
    }
    
    return RenderFlow::positionForCoordinates(x, y);
}

IntRect RenderInline::linesBoundingBox() const
{
    IntRect result;
    
    // See <rdar://problem/5289721>, for an unknown reason the linked list here is sometimes inconsistent, first is non-zero and last is zero.  We have been
    // unable to reproduce this at all (and consequently unable to figure ot why this is happening).  The assert will hopefully catch the problem in debug
    // builds and help us someday figure out why.  We also put in a redundant check of lastLineBox() to avoid the crash for now.
    ASSERT(!firstLineBox() == !lastLineBox());  // Either both are null or both exist.
    if (firstLineBox() && lastLineBox()) {
        // Return the width of the minimal left side and the maximal right side.
        int leftSide = 0;
        int rightSide = 0;
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
            if (curr == firstLineBox() || curr->xPos() < leftSide)
                leftSide = curr->xPos();
            if (curr == firstLineBox() || curr->xPos() + curr->width() > rightSide)
                rightSide = curr->xPos() + curr->width();
        }
        result.setWidth(rightSide - leftSide);
        result.setX(leftSide);
        result.setHeight(lastLineBox()->yPos() + lastLineBox()->height() - firstLineBox()->yPos());
        result.setY(firstLineBox()->yPos());
    }

    return result;
}

IntRect RenderInline::clippedOverflowRectForRepaint(RenderBox* repaintContainer)
{
    // Only run-ins are allowed in here during layout.
    ASSERT(!view() || !view()->layoutStateEnabled() || isRunIn());

    if (!firstLineBox() && !continuation())
        return IntRect();

    // Find our leftmost position.
    IntRect boundingBox(linesBoundingBox());
    int left = boundingBox.x();
    int top = boundingBox.y();

    // Now invalidate a rectangle.
    int ow = style() ? style()->outlineSize() : 0;
    
    // We need to add in the relative position offsets of any inlines (including us) up to our
    // containing block.
    RenderBlock* cb = containingBlock();
    for (RenderObject* inlineFlow = this; inlineFlow && inlineFlow->isRenderInline() && inlineFlow != cb; 
         inlineFlow = inlineFlow->parent()) {
         if (inlineFlow->style()->position() == RelativePosition && inlineFlow->hasLayer())
            toRenderBox(inlineFlow)->layer()->relativePositionOffset(left, top);
    }

    IntRect r(-ow + left, -ow + top, boundingBox.width() + ow * 2, boundingBox.height() + ow * 2);
    if (cb->hasColumns())
        cb->adjustRectForColumns(r);

    if (cb->hasOverflowClip()) {
        // cb->height() is inaccurate if we're in the middle of a layout of |cb|, so use the
        // layer's size instead.  Even if the layer's size is wrong, the layer itself will repaint
        // anyway if its size does change.
        int x = r.x();
        int y = r.y();
        IntRect boxRect(0, 0, cb->layer()->width(), cb->layer()->height());
        cb->layer()->subtractScrolledContentOffset(x, y); // For overflow:auto/scroll/hidden.
        IntRect repaintRect(x, y, r.width(), r.height());
        r = intersection(repaintRect, boxRect);
    }
    ASSERT(repaintContainer != this);
    cb->computeRectForRepaint(repaintContainer, r);

    if (ow) {
        for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
            if (!curr->isText()) {
                IntRect childRect = curr->rectWithOutlineForRepaint(repaintContainer, ow);
                r.unite(childRect);
            }
        }

        if (continuation() && !continuation()->isInline()) {
            IntRect contRect = continuation()->rectWithOutlineForRepaint(repaintContainer, ow);
            r.unite(contRect);
        }
    }

    return r;
}

IntRect RenderInline::rectWithOutlineForRepaint(RenderBox* repaintContainer, int outlineWidth)
{
    IntRect r(RenderFlow::rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isText())
            r.unite(curr->rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    }
    return r;
}

void RenderInline::updateDragState(bool dragOn)
{
    RenderFlow::updateDragState(dragOn);
    if (continuation())
        continuation()->updateDragState(dragOn);
}

void RenderInline::childBecameNonInline(RenderObject* child)
{
    // We have to split the parent flow.
    RenderBlock* newBox = createAnonymousBlock();
    RenderBox* oldContinuation = continuation();
    setContinuation(newBox);
    RenderObject* beforeChild = child->nextSibling();
    removeChildNode(child);
    splitFlow(beforeChild, newBox, child, oldContinuation);
}

void RenderInline::updateHitTestResult(HitTestResult& result, const IntPoint& point)
{
    if (result.innerNode())
        return;

    Node* node = element();
    IntPoint localPoint(point);
    if (node) {
        if (isInlineContinuation()) {
            // We're in the continuation of a split inline.  Adjust our local point to be in the coordinate space
            // of the principal renderer's containing block.  This will end up being the innerNonSharedNode.
            RenderBlock* firstBlock = node->renderer()->containingBlock();
            
            // Get our containing block.
            RenderBox* block = toRenderBox(this);
            if (isInline())
                block = containingBlock();
        
            localPoint.move(block->x() - firstBlock->x(), block->y() - firstBlock->y());
        }

        result.setInnerNode(node);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node);
        result.setLocalPoint(localPoint);
    }
}

InlineBox* RenderInline::createInlineBox(bool, bool, bool)
{
    InlineFlowBox* flowBox = new (renderArena()) InlineFlowBox(this);
    m_lineBoxes.appendLineBox(flowBox);
    return flowBox;
}

int RenderInline::lineHeight(bool firstLine, bool /*isRootLineBox*/) const
{
    if (firstLine && document()->usesFirstLineRules()) {
        RenderStyle* s = style(firstLine);
        if (s != style())
            return s->computedLineHeight();
    }
    
    if (m_lineHeight == -1)
        m_lineHeight = style()->computedLineHeight();
    
    return m_lineHeight;
}

void RenderInline::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
        graphicsContext->addFocusRingRect(IntRect(tx + curr->xPos(), ty + curr->yPos(), curr->width(), curr->height()));

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isText() && !curr->isListMarker() && curr->isBox()) {
            RenderBox* box = toRenderBox(curr);
            FloatPoint pos;
            // FIXME: This doesn't work correctly with transforms.
            if (box->layer()) 
                pos = curr->localToAbsolute();
            else
                pos = FloatPoint(tx + box->x(), ty + box->y());
            box->addFocusRingRects(graphicsContext, pos.x(), pos.y());
        }
    }

    if (continuation())
        continuation()->addFocusRingRects(graphicsContext, 
                                          tx - containingBlock()->x() + continuation()->x(),
                                          ty - containingBlock()->y() + continuation()->y());
}

void RenderInline::paintOutline(GraphicsContext* graphicsContext, int tx, int ty)
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

    if (style()->outlineStyleIsAuto() || style()->outlineStyle() == BNONE)
        return;

    Vector<IntRect> rects;

    rects.append(IntRect());
    for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
        rects.append(IntRect(curr->xPos(), curr->yPos(), curr->width(), curr->height()));

    rects.append(IntRect());

    for (unsigned i = 1; i < rects.size() - 1; i++)
        paintOutlineForLine(graphicsContext, tx, ty, rects.at(i - 1), rects.at(i), rects.at(i + 1));
}

void RenderInline::paintOutlineForLine(GraphicsContext* graphicsContext, int tx, int ty,
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

} // namespace WebCore
