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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "RenderBlock.h"

#include "DocumentImpl.h"
#include "Frame.h"
#include "FrameView.h"
#include "InlineTextBox.h"
#include "RenderTextFragment.h"
#include "SelectionController.h"
#include "htmlnames.h"
#include "render_canvas.h"
#include "RenderTableCell.h"
#include "render_theme.h"
#include "visible_position.h"
#include <kdebug.h>

namespace WebCore {

using namespace HTMLNames;

// Our MarginInfo state used when laying out block children.
RenderBlock::MarginInfo::MarginInfo(RenderBlock* block, int top, int bottom)
{
    // Whether or not we can collapse our own margins with our children.  We don't do this
    // if we had any border/padding (obviously), if we're the root or HTML elements, or if
    // we're positioned, floating, a table cell.
    m_canCollapseWithChildren = !block->isCanvas() && !block->isRoot() && !block->isPositioned() &&
        !block->isFloating() && !block->isTableCell() && !block->hasOverflowClip() && !block->isInlineBlockOrInlineTable();

    m_canCollapseTopWithChildren = m_canCollapseWithChildren && (top == 0) && block->style()->marginTopCollapse() != MSEPARATE;

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins.  To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it.  We also don't collapse if we have any bottom border/padding.
    m_canCollapseBottomWithChildren = m_canCollapseWithChildren && (bottom == 0) &&
        (block->style()->height().isAuto() && block->style()->height().value == 0) && block->style()->marginBottomCollapse() != MSEPARATE;
    
    m_quirkContainer = block->isTableCell() || block->isBody() || block->style()->marginTopCollapse() == MDISCARD || 
        block->style()->marginBottomCollapse() == MDISCARD;

    m_atTopOfBlock = true;
    m_atBottomOfBlock = false;

    m_posMargin = m_canCollapseTopWithChildren ? block->maxTopMargin(true) : 0;
    m_negMargin = m_canCollapseTopWithChildren ? block->maxTopMargin(false) : 0;

    m_selfCollapsingBlockClearedFloat = false;
    
    m_topQuirk = m_bottomQuirk = m_determinedTopQuirk = false;
}

// -------------------------------------------------------------------------------------------------------

RenderBlock::RenderBlock(DOM::NodeImpl* node)
:RenderFlow(node)
{
    m_childrenInline = true;
    m_floatingObjects = 0;
    m_positionedObjects = 0;
    m_firstLine = false;
    m_hasMarkupTruncation = false;
    m_selectionState = SelectionNone;
    m_clearStatus = CNONE;
    m_maxTopPosMargin = m_maxTopNegMargin = m_maxBottomPosMargin = m_maxBottomNegMargin = 0;
    m_topMarginQuirk = m_bottomMarginQuirk = false;
    m_overflowHeight = m_overflowWidth = 0;
    m_overflowLeft = m_overflowTop = 0;
    m_tabWidth = 0;
}

RenderBlock::~RenderBlock()
{
    delete m_floatingObjects;
    delete m_positionedObjects;
}

void RenderBlock::setStyle(RenderStyle* _style)
{
    setReplaced(_style->isDisplayReplacedType());

    RenderFlow::setStyle(_style);

    // ### we could save this call when the change only affected
    // non inherited properties
    RenderObject *child = firstChild();
    while (child != 0)
    {   
        if (child->isAnonymousBlock())
        {
            RenderStyle* newStyle = new (renderArena()) RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);
            child->setStyle(newStyle);
        }
        child = child->nextSibling();
    }

    m_lineHeight = -1;
    m_tabWidth = 0;

    // Update pseudos for :before and :after now.
    RenderObject* first = firstChild();
    while (first && first->isListMarker()) first = first->nextSibling();
    updatePseudoChild(RenderStyle::BEFORE, first);
    updatePseudoChild(RenderStyle::AFTER, lastChild());
}

void RenderBlock::addChildToFlow(RenderObject* newChild, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild && lastChild() && lastChild()->style()->styleType() == RenderStyle::AFTER)
        beforeChild = lastChild();
    
    bool madeBoxesNonInline = false;

    // If the requested beforeChild is not one of our children, then this is most likely because
    // there is an anonymous block box within this object that contains the beforeChild. So
    // just insert the child into the anonymous block box instead of here.
    if (beforeChild && beforeChild->parent() != this) {

        KHTMLAssert(beforeChild->parent());
        KHTMLAssert(beforeChild->parent()->isAnonymousBlock());

        if (newChild->isInline()) {
            beforeChild->parent()->addChild(newChild,beforeChild);
            return;
        }
        else if (beforeChild->parent()->firstChild() != beforeChild)
            return beforeChild->parent()->addChild(newChild, beforeChild);
        else
            return addChildToFlow(newChild, beforeChild->parent());
    }

    // A block has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes
    if ( m_childrenInline && !newChild->isInline() && !newChild->isFloatingOrPositioned() )
    {
        // This is a block with inline content. Wrap the inline content in anonymous blocks.
        makeChildrenNonInline(beforeChild);
        madeBoxesNonInline = true;
        
        if (beforeChild && beforeChild->parent() != this) {
            beforeChild = beforeChild->parent();
            KHTMLAssert(beforeChild->isAnonymousBlock());
            KHTMLAssert(beforeChild->parent() == this);
        }
    }
    else if (!m_childrenInline && !newChild->isFloatingOrPositioned())
    {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        if (newChild->isInline()) {
            if (beforeChild) {
                if (beforeChild->previousSibling() && beforeChild->previousSibling()->isAnonymousBlock()) {
                    beforeChild->previousSibling()->addChild(newChild);
                    return;
                }
            }
            else {
                if (m_last && m_last->isAnonymousBlock()) {
                    m_last->addChild(newChild);
                    return;
                }
            }

            // no suitable existing anonymous box - create a new one
            RenderBlock* newBox = createAnonymousBlock();
            RenderContainer::addChild(newBox,beforeChild);
            newBox->addChild(newChild);
            return;
        }
    }

    RenderContainer::addChild(newChild,beforeChild);
    // ### care about aligned stuff

    if ( madeBoxesNonInline )
        removeLeftoverAnonymousBoxes();
}

static void getInlineRun(RenderObject* start, RenderObject* boundary,
                         RenderObject*& inlineRunStart,
                         RenderObject*& inlineRunEnd)
{
    // Beginning at |start| we find the largest contiguous run of inlines that
    // we can.  We denote the run with start and end points, |inlineRunStart|
    // and |inlineRunEnd|.  Note that these two values may be the same if
    // we encounter only one inline.
    //
    // We skip any non-inlines we encounter as long as we haven't found any
    // inlines yet.
    //
    // |boundary| indicates a non-inclusive boundary point.  Regardless of whether |boundary|
    // is inline or not, we will not include it in a run with inlines before it.  It's as though we encountered
    // a non-inline.
    
    // Start by skipping as many non-inlines as we can.
    RenderObject * curr = start;
    bool sawInline;
    do {
        while (curr && !(curr->isInline() || curr->isFloatingOrPositioned()))
            curr = curr->nextSibling();
        
        inlineRunStart = inlineRunEnd = curr;
        
        if (!curr)
            return; // No more inline children to be found.
        
        sawInline = curr->isInline();
        
        curr = curr->nextSibling();
        while (curr && (curr->isInline() || curr->isFloatingOrPositioned()) && (curr != boundary)) {
            inlineRunEnd = curr;
            if (curr->isInline())
                sawInline = true;
            curr = curr->nextSibling();
        }
    } while (!sawInline);
}

void RenderBlock::makeChildrenNonInline(RenderObject *insertionPoint)
{    
    // makeChildrenNonInline takes a block whose children are *all* inline and it
    // makes sure that inline children are coalesced under anonymous
    // blocks.  If |insertionPoint| is defined, then it represents the insertion point for
    // the new block child that is causing us to have to wrap all the inlines.  This
    // means that we cannot coalesce inlines before |insertionPoint| with inlines following
    // |insertionPoint|, because the new child is going to be inserted in between the inlines,
    // splitting them.
    KHTMLAssert(isInlineBlockOrInlineTable() || !isInline());
    KHTMLAssert(!insertionPoint || insertionPoint->parent() == this);

    m_childrenInline = false;

    RenderObject *child = firstChild();

    while (child) {
        RenderObject *inlineRunStart, *inlineRunEnd;
        getInlineRun(child, insertionPoint, inlineRunStart, inlineRunEnd);

        if (!inlineRunStart)
            break;

        child = inlineRunEnd->nextSibling();

        RenderBlock* box = createAnonymousBlock();
        insertChildNode(box, inlineRunStart);
        RenderObject* o = inlineRunStart;
        while(o != inlineRunEnd)
        {
            RenderObject* no = o;
            o = no->nextSibling();
            box->appendChildNode(removeChildNode(no));
        }
        box->appendChildNode(removeChildNode(inlineRunEnd));
    }

#ifndef NDEBUG
    for (RenderObject *c = firstChild(); c; c = c->nextSibling())
        KHTMLAssert(!c->isInline());
#endif
}

void RenderBlock::removeChild(RenderObject *oldChild)
{
    // If this child is a block, and if our previous and next siblings are
    // both anonymous blocks with inline content, then we can go ahead and
    // fold the inline content back together.
    RenderObject* prev = oldChild->previousSibling();
    RenderObject* next = oldChild->nextSibling();
    bool canDeleteAnonymousBlocks = !documentBeingDestroyed() && !isInline() && !oldChild->isInline() && 
                                    !oldChild->continuation() && 
                                    (!prev || (prev->isAnonymousBlock() && prev->childrenInline())) &&
                                    (!next || (next->isAnonymousBlock() && next->childrenInline()));
    if (canDeleteAnonymousBlocks && prev && next) {
        // Take all the children out of the |next| block and put them in
        // the |prev| block.
        prev->setNeedsLayoutAndMinMaxRecalc();
        RenderObject* o = next->firstChild();
        while (o) {
            RenderObject* no = o;
            o = no->nextSibling();
            prev->appendChildNode(next->removeChildNode(no));
            no->setNeedsLayoutAndMinMaxRecalc();
        }
        // Nuke the now-empty block.
        next->destroy();
    }

    RenderFlow::removeChild(oldChild);

    RenderObject* child = prev ? prev : next;
    if (canDeleteAnonymousBlocks && child && !child->previousSibling() && !child->nextSibling()) {
        // The removal has knocked us down to containing only a single anonymous
        // box.  We can go ahead and pull the content right back up into our
        // box.
        setNeedsLayoutAndMinMaxRecalc();
        RenderObject* anonBlock = removeChildNode(child);
        m_childrenInline = true;
        RenderObject* o = anonBlock->firstChild();
        while (o) {
            RenderObject* no = o;
            o = no->nextSibling();
            appendChildNode(anonBlock->removeChildNode(no));
            no->setNeedsLayoutAndMinMaxRecalc();
        }

        // Nuke the now-empty block.
        anonBlock->destroy();
    }
}

int RenderBlock::overflowHeight(bool includeInterior) const
{
    return (!includeInterior && hasOverflowClip()) ? m_height : m_overflowHeight;
}

int RenderBlock::overflowWidth(bool includeInterior) const
{
    return (!includeInterior && hasOverflowClip()) ? m_width : m_overflowWidth;
}
int RenderBlock::overflowLeft(bool includeInterior) const
{
    return (!includeInterior && hasOverflowClip()) ? 0 : m_overflowLeft;
}

int RenderBlock::overflowTop(bool includeInterior) const
{
    return (!includeInterior && hasOverflowClip()) ? 0 : m_overflowTop;
}

IntRect RenderBlock::overflowRect(bool includeInterior) const
{
    if (!includeInterior && hasOverflowClip())
        return borderBox();
    int l = overflowLeft(includeInterior);
    int t = kMin(overflowTop(includeInterior), -borderTopExtra());
    return IntRect(l, t, m_overflowWidth - 2*l, m_overflowHeight + borderTopExtra() + borderBottomExtra() - 2*t);
}

bool RenderBlock::isSelfCollapsingBlock() const
{
    // We are not self-collapsing if we
    // (a) have a non-zero height according to layout (an optimization to avoid wasting time)
    // (b) are a table,
    // (c) have border/padding,
    // (d) have a min-height
    // (e) have specified that one of our margins can't collapse using a CSS extension
    if (m_height > 0 ||
        isTable() || (borderBottom() + paddingBottom() + borderTop() + paddingTop()) != 0 ||
        style()->minHeight().value > 0 || 
        style()->marginTopCollapse() == MSEPARATE || style()->marginBottomCollapse() == MSEPARATE)
        return false;

    bool hasAutoHeight = style()->height().isAuto();
    if (style()->height().isPercent() && !style()->htmlHacks()) {
        hasAutoHeight = true;
        for (RenderBlock* cb = containingBlock(); !cb->isCanvas(); cb = cb->containingBlock()) {
            if (cb->style()->height().isFixed() || cb->isTableCell())
                hasAutoHeight = false;
        }
    }

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    if (hasAutoHeight || ((style()->height().isFixed() || style()->height().isPercent()) && style()->height().value == 0)) {
        // If the block has inline children, see if we generated any line boxes.  If we have any
        // line boxes, then we can't be self-collapsing, since we have content.
        if (childrenInline())
            return !firstLineBox();
        
        // Whether or not we collapse is dependent on whether all our normal flow children
        // are also self-collapsing.
        for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
            if (child->isFloatingOrPositioned())
                continue;
            if (!child->isSelfCollapsingBlock())
                return false;
        }
        return true;
    }
    return false;
}

void RenderBlock::layout()
{
    // Table cells call layoutBlock directly, so don't add any logic here.  Put code into
    // layoutBlock().
    layoutBlock(false);
}

void RenderBlock::layoutBlock(bool relayoutChildren)
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    if (isInline() && !isInlineBlockOrInlineTable()) // Inline <form>s inside various table elements can
        return;                                      // cause us to come in here.  Just bail.

    if (!relayoutChildren && posChildNeedsLayout() && !normalChildNeedsLayout() && !selfNeedsLayout()) {
        // All we have to is lay out our positioned objects.
        layoutPositionedObjects(relayoutChildren || isRoot());
        if (hasOverflowClip())
            m_layer->updateScrollInfoAfterLayout();
        setNeedsLayout(false);
        return;
    }
    
    IntRect oldBounds, oldFullBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        getAbsoluteRepaintRectIncludingFloats(oldBounds, oldFullBounds);

    int oldWidth = m_width;
    
    calcWidth();
    m_overflowWidth = m_width;

    if (oldWidth != m_width)
        relayoutChildren = true;

    clearFloats();

    int previousHeight = m_height;
    m_height = 0;
    m_overflowHeight = 0;
    m_clearStatus = CNONE;

    // We use four values, maxTopPos, maxPosNeg, maxBottomPos, and maxBottomNeg, to track
    // our current maximal positive and negative margins.  These values are used when we
    // are collapsed with adjacent blocks, so for example, if you have block A and B
    // collapsing together, then you'd take the maximal positive margin from both A and B
    // and subtract it from the maximal negative margin from both A and B to get the
    // true collapsed margin.  This algorithm is recursive, so when we finish layout()
    // our block knows its current maximal positive/negative values.
    //
    // Start out by setting our margin values to our current margins.  Table cells have
    // no margins, so we don't fill in the values for table cells.
    if (!isTableCell()) {
        initMaxMarginValues();

        m_topMarginQuirk = style()->marginTop().quirk;
        m_bottomMarginQuirk = style()->marginBottom().quirk;

        if (element() && element()->hasTagName(formTag) && element()->isMalformed())
            // See if this form is malformed (i.e., unclosed). If so, don't give the form
            // a bottom margin.
            m_maxBottomPosMargin = m_maxBottomNegMargin = 0;
    }

    if (scrollsOverflow()) {
        // For overflow:scroll blocks, ensure we have both scrollbars in place always.
        if (style()->overflow() == OSCROLL) {
            m_layer->setHasHorizontalScrollbar(true);
            m_layer->setHasVerticalScrollbar(true);
        }
        
        // Move the scrollbars aside during layout.  The layer will move them back when it
        // does painting or event handling.
        m_layer->moveScrollbarsAside();
    }
   
    IntRect repaintRect;
    if (childrenInline())
        repaintRect = layoutInlineChildren(relayoutChildren);
    else
        layoutBlockChildren(relayoutChildren);

    // Expand our intrinsic height to encompass floats.
    int toAdd = borderBottom() + paddingBottom();
    if (includeScrollbarSize())
        toAdd += m_layer->horizontalScrollbarHeight();
    if (floatBottom() > (m_height - toAdd) && (isInlineBlockOrInlineTable() || isFloatingOrPositioned() || hasOverflowClip() ||
                                    (parent() && parent()->isFlexibleBox())))
        m_height = floatBottom() + toAdd;
           
    int oldHeight = m_height;
    calcHeight();
    if (oldHeight != m_height) {
        // If the block got expanded in size, then increase our overflowheight to match.
        if (m_overflowHeight > m_height)
            m_overflowHeight -= paddingBottom() + borderBottom();
        if (m_overflowHeight < m_height)
            m_overflowHeight = m_height;
    }
    if (previousHeight != m_height)
        relayoutChildren = true;

    if (isTableCell()) {
        // Table cells need to grow to accommodate both overhanging floats and
        // blocks that have overflowed content.
        // Check for an overhanging float first.
        // FIXME: This needs to look at the last flow, not the last child.
        if (lastChild() && lastChild()->hasOverhangingFloats()) {
            KHTMLAssert(lastChild()->isRenderBlock());
            m_height = lastChild()->yPos() + static_cast<RenderBlock*>(lastChild())->floatBottom();
            m_height += borderBottom() + paddingBottom();
        }

        if (m_overflowHeight > m_height && !hasOverflowClip())
            m_height = m_overflowHeight + borderBottom() + paddingBottom();
    }

    if (hasOverhangingFloats() && ((isFloating() && style()->height().isAuto()) || isTableCell())) {
        m_height = floatBottom();
        m_height += borderBottom() + paddingBottom();
    }

    layoutPositionedObjects(relayoutChildren || isRoot());

    // Always ensure our overflow width/height are at least as large as our width/height.
    m_overflowWidth = kMax(m_overflowWidth, m_width);
    m_overflowHeight = kMax(m_overflowHeight, m_height);

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        m_layer->updateScrollInfoAfterLayout();

    // Repaint with our new bounds if they are different from our old bounds.
    bool didFullRepaint = false;
    if (checkForRepaint)
        didFullRepaint = repaintAfterLayoutIfNeeded(oldBounds, oldFullBounds);
    if (!didFullRepaint && !repaintRect.isEmpty()) {
        RenderCanvas* c = canvas();
        if (c && c->view())
            c->view()->addRepaintInfo(this, repaintRect); // We need to do a partial repaint of our content.
    }
    setNeedsLayout(false);
}

void RenderBlock::adjustPositionedBlock(RenderObject* child, const MarginInfo& marginInfo)
{
    if (child->hasStaticX()) {
        if (style()->direction() == LTR)
            child->setStaticX(borderLeft() + paddingLeft());
        else
            child->setStaticX(borderRight() + paddingRight());
    }

    if (child->hasStaticY()) {
        int y = m_height;
        if (!marginInfo.canCollapseWithTop()) {
            child->calcVerticalMargins();
            int marginTop = child->marginTop();
            int collapsedTopPos = marginInfo.posMargin();
            int collapsedTopNeg = marginInfo.negMargin();
            if (marginTop > 0) {
                if (marginTop > collapsedTopPos)
                    collapsedTopPos = marginTop;
            } else {
                if (-marginTop > collapsedTopNeg)
                    collapsedTopNeg = -marginTop;
            }
            y += (collapsedTopPos - collapsedTopNeg) - marginTop;
        }
        child->setStaticY(y);
    }
}

void RenderBlock::adjustFloatingBlock(const MarginInfo& marginInfo)
{
    // The float should be positioned taking into account the bottom margin
    // of the previous flow.  We add that margin into the height, get the
    // float positioned properly, and then subtract the margin out of the
    // height again.  In the case of self-collapsing blocks, we always just
    // use the top margins, since the self-collapsing block collapsed its
    // own bottom margin into its top margin.
    //
    // Note also that the previous flow may collapse its margin into the top of
    // our block.  If this is the case, then we do not add the margin in to our
    // height when computing the position of the float.   This condition can be tested
    // for by simply calling canCollapseWithTop.  See
    // http://www.hixie.ch/tests/adhoc/css/box/block/margin-collapse/046.html for
    // an example of this scenario.
    int marginOffset = marginInfo.canCollapseWithTop() ? 0 : marginInfo.margin();
    m_height += marginOffset;
    positionNewFloats();
    m_height -= marginOffset;
}

RenderObject* RenderBlock::handleSpecialChild(RenderObject* child, const MarginInfo& marginInfo, CompactInfo& compactInfo, bool& handled)
{
    // Handle positioned children first.
    RenderObject* next = handlePositionedChild(child, marginInfo, handled);
    if (handled) return next;
    
    // Handle floating children next.
    next = handleFloatingChild(child, marginInfo, handled);
    if (handled) return next;

    // See if we have a compact element.  If we do, then try to tuck the compact element into the margin space of the next block.
    next = handleCompactChild(child, compactInfo, handled);
    if (handled) return next;

    // Finally, see if we have a run-in element.
    return handleRunInChild(child, handled);
}


RenderObject* RenderBlock::handlePositionedChild(RenderObject* child, const MarginInfo& marginInfo, bool& handled)
{
    if (child->isPositioned()) {
        handled = true;
        child->containingBlock()->insertPositionedObject(child);
        adjustPositionedBlock(child, marginInfo);
        return child->nextSibling();
    }

    return 0;
}

RenderObject* RenderBlock::handleFloatingChild(RenderObject* child, const MarginInfo& marginInfo, bool& handled)
{
    if (child->isFloating()) {
        handled = true;
        insertFloatingObject(child);
        adjustFloatingBlock(marginInfo);
        return child->nextSibling();
    }
    
    return 0;
}

RenderObject* RenderBlock::handleCompactChild(RenderObject* child, CompactInfo& compactInfo, bool& handled)
{
    // FIXME: We only deal with one compact at a time.  It is unclear what should be
    // done if multiple contiguous compacts are encountered.  For now we assume that
    // compact A followed by another compact B should simply be treated as block A.
    if (child->isCompact() && !compactInfo.compact() && (child->childrenInline() || child->isReplaced())) {
        // Get the next non-positioned/non-floating RenderBlock.
        RenderObject* next = child->nextSibling();
        RenderObject* curr = next;
        while (curr && curr->isFloatingOrPositioned())
            curr = curr->nextSibling();
        if (curr && curr->isRenderBlock() && !curr->isCompact() && !curr->isRunIn()) {
            curr->calcWidth(); // So that horizontal margins are correct.
                               
            child->setInline(true); // Need to compute the margins/width for the child as though it is an inline, so that it won't try to puff up the margins to
                                    // fill the containing block width.
            child->calcWidth();
            int childMargins = child->marginLeft() + child->marginRight();
            int margin = style()->direction() == LTR ? curr->marginLeft() : curr->marginRight();
            if (margin >= (childMargins + child->maxWidth())) {
                // The compact will fit in the margin.
                handled = true;
                compactInfo.set(child, curr);
                child->setPos(0,0); // This position will be updated to reflect the compact's
                                    // desired position and the line box for the compact will
                                    // pick that position up.
                
                // Remove the child.
                RenderObject* next = child->nextSibling();
                removeChildNode(child);
                
                // Now insert the child under |curr|.
                curr->insertChildNode(child, curr->firstChild());
                return next;
            }
            else
                child->setInline(false); // We didn't fit, so we remain a block-level element.
        }
    }
    return 0;
}

void RenderBlock::insertCompactIfNeeded(RenderObject* child, CompactInfo& compactInfo)
{
    if (compactInfo.matches(child)) {
        // We have a compact child to squeeze in.
        RenderObject* compactChild = compactInfo.compact();
        int compactXPos = borderLeft() + paddingLeft() + compactChild->marginLeft();
        if (style()->direction() == RTL) {
            compactChild->calcWidth(); // have to do this because of the capped maxwidth
            compactXPos = width() - borderRight() - paddingRight() - marginRight() -
                compactChild->width() - compactChild->marginRight();
        }
        compactXPos -= child->xPos(); // Put compactXPos into the child's coordinate space.
        compactChild->setPos(compactXPos, compactChild->yPos()); // Set the x position.
        compactInfo.clear();
    }
}

RenderObject* RenderBlock::handleRunInChild(RenderObject* child, bool& handled)
{
    // See if we have a run-in element with inline children.  If the
    // children aren't inline, then just treat the run-in as a normal
    // block.
    if (child->isRunIn() && (child->childrenInline() || child->isReplaced())) {
        // Get the next non-positioned/non-floating RenderBlock.
        RenderObject* curr = child->nextSibling();
        while (curr && curr->isFloatingOrPositioned())
            curr = curr->nextSibling();
        if (curr && (curr->isRenderBlock() && curr->childrenInline() && !curr->isCompact() && !curr->isRunIn())) {
            // The block acts like an inline, so just null out its
            // position.
            handled = true;
            child->setInline(true);
            child->setPos(0,0);
            
            // Remove the child.
            RenderObject* next = child->nextSibling();
            removeChildNode(child);
            
            // Now insert the child under |curr|.
            curr->insertChildNode(child, curr->firstChild());
            return next;
        }
    }
    return 0;
}

void RenderBlock::collapseMargins(RenderObject* child, MarginInfo& marginInfo, int yPosEstimate)
{
    // Get our max pos and neg top margins.
    int posTop = child->maxTopMargin(true);
    int negTop = child->maxTopMargin(false);

    // For self-collapsing blocks, collapse our bottom margins into our
    // top to get new posTop and negTop values.
    if (child->isSelfCollapsingBlock()) {
        posTop = kMax(posTop, child->maxBottomMargin(true));
        negTop = kMax(negTop, child->maxBottomMargin(false));
    }
    
    // See if the top margin is quirky. We only care if this child has
    // margins that will collapse with us.
    bool topQuirk = child->isTopMarginQuirk() || style()->marginTopCollapse() == MDISCARD;

    if (marginInfo.canCollapseWithTop()) {
        // This child is collapsing with the top of the
        // block.  If it has larger margin values, then we need to update
        // our own maximal values.
        if (!style()->htmlHacks() || !marginInfo.quirkContainer() || !topQuirk) {
            m_maxTopPosMargin = kMax(posTop, m_maxTopPosMargin);
            m_maxTopNegMargin = kMax(negTop, m_maxTopNegMargin);
        }

        // The minute any of the margins involved isn't a quirk, don't
        // collapse it away, even if the margin is smaller (www.webreference.com
        // has an example of this, a <dt> with 0.8em author-specified inside
        // a <dl> inside a <td>.
        if (!marginInfo.determinedTopQuirk() && !topQuirk && (posTop-negTop)) {
            m_topMarginQuirk = false;
            marginInfo.setDeterminedTopQuirk(true);
        }

        if (!marginInfo.determinedTopQuirk() && topQuirk && marginTop() == 0)
            // We have no top margin and our top child has a quirky margin.
            // We will pick up this quirky margin and pass it through.
            // This deals with the <td><div><p> case.
            // Don't do this for a block that split two inlines though.  You do
            // still apply margins in this case.
            m_topMarginQuirk = true;
    }

    if (marginInfo.quirkContainer() && marginInfo.atTopOfBlock() && (posTop - negTop))
        marginInfo.setTopQuirk(topQuirk);

    int ypos = m_height;
    if (child->isSelfCollapsingBlock()) {
        // This child has no height.  We need to compute our
        // position before we collapse the child's margins together,
        // so that we can get an accurate position for the zero-height block.
        int collapsedTopPos = kMax(marginInfo.posMargin(), child->maxTopMargin(true));
        int collapsedTopNeg = kMax(marginInfo.negMargin(), child->maxTopMargin(false));
        marginInfo.setMargin(collapsedTopPos, collapsedTopNeg);
        
        // Now collapse the child's margins together, which means examining our
        // bottom margin values as well. 
        marginInfo.setPosMarginIfLarger(child->maxBottomMargin(true));
        marginInfo.setNegMarginIfLarger(child->maxBottomMargin(false));

        if (!marginInfo.canCollapseWithTop())
            // We need to make sure that the position of the self-collapsing block
            // is correct, since it could have overflowing content
            // that needs to be positioned correctly (e.g., a block that
            // had a specified height of 0 but that actually had subcontent).
            ypos = m_height + collapsedTopPos - collapsedTopNeg;
    }
    else {
        if (child->style()->marginTopCollapse() == MSEPARATE) {
            m_height += marginInfo.margin() + child->marginTop();
            ypos = m_height;
        }
        else if (!marginInfo.atTopOfBlock() ||
            (!marginInfo.canCollapseTopWithChildren()
             && (!style()->htmlHacks() || !marginInfo.quirkContainer() || !marginInfo.topQuirk()))) {
            // We're collapsing with a previous sibling's margins and not
            // with the top of the block.
            m_height += kMax(marginInfo.posMargin(), posTop) - kMax(marginInfo.negMargin(), negTop);
            ypos = m_height;
        }

        marginInfo.setPosMargin(child->maxBottomMargin(true));
        marginInfo.setNegMargin(child->maxBottomMargin(false));

        if (marginInfo.margin())
            marginInfo.setBottomQuirk(child->isBottomMarginQuirk() || style()->marginBottomCollapse() == MDISCARD);

        marginInfo.setSelfCollapsingBlockClearedFloat(false);
    }

    child->setPos(child->xPos(), ypos);
    if (ypos != yPosEstimate) {
        if (child->style()->width().isPercent() && child->usesLineWidth())
            // The child's width is a percentage of the line width.
            // When the child shifts to clear an item, its width can
            // change (because it has more available line width).
            // So go ahead and mark the item as dirty.
            child->setChildNeedsLayout(true);

        if (!child->avoidsFloats() && child->containsFloats())
            child->markAllDescendantsWithFloatsForLayout();

        // Our guess was wrong. Make the child lay itself out again.
        child->layoutIfNeeded();
    }
}

void RenderBlock::clearFloatsIfNeeded(RenderObject* child, MarginInfo& marginInfo, int oldTopPosMargin, int oldTopNegMargin)
{
    int heightIncrease = getClearDelta(child);
    if (heightIncrease) {
        // The child needs to be lowered.  Move the child so that it just clears the float.
        child->setPos(child->xPos(), child->yPos() + heightIncrease);

        // Increase our height by the amount we had to clear.
        if (!child->isSelfCollapsingBlock())
            m_height += heightIncrease;
        else {
            // For self-collapsing blocks that clear, they may end up collapsing
            // into the bottom of the parent block.  We simulate this behavior by
            // setting our positive margin value to compensate for the clear.
            marginInfo.setPosMargin(kMax(0, child->yPos() - m_height));
            marginInfo.setNegMargin(0);
            marginInfo.setSelfCollapsingBlockClearedFloat(true);
        }
        
        if (marginInfo.canCollapseWithTop()) {
            // We can no longer collapse with the top of the block since a clear
            // occurred.  The empty blocks collapse into the cleared block.
            // FIXME: This isn't quite correct.  Need clarification for what to do
            // if the height the cleared block is offset by is smaller than the
            // margins involved.
            m_maxTopPosMargin = oldTopPosMargin;
            m_maxTopNegMargin = oldTopNegMargin;
            marginInfo.setAtTopOfBlock(false);
        }

        // If our value of clear caused us to be repositioned vertically to be
        // underneath a float, we might have to do another layout to take into account
        // the extra space we now have available.
        if (child->style()->width().isPercent() && child->usesLineWidth())
            // The child's width is a percentage of the line width.
            // When the child shifts to clear an item, its width can
            // change (because it has more available line width).
            // So go ahead and mark the item as dirty.
            child->setChildNeedsLayout(true);
        if (!child->avoidsFloats() && child->containsFloats())
            child->markAllDescendantsWithFloatsForLayout();
        child->layoutIfNeeded();
    }
}

int RenderBlock::estimateVerticalPosition(RenderObject* child, const MarginInfo& marginInfo)
{
    // FIXME: We need to eliminate the estimation of vertical position, because when it's wrong we sometimes trigger a pathological
    // relayout if there are intruding floats.
    int yPosEstimate = m_height;
    if (!marginInfo.canCollapseWithTop()) {
        int childMarginTop = child->selfNeedsLayout() ? child->marginTop() : child->collapsedMarginTop();
        yPosEstimate += kMax(marginInfo.margin(), childMarginTop);
    }
    return yPosEstimate;
}

void RenderBlock::determineHorizontalPosition(RenderObject* child)
{
    if (style()->direction() == LTR) {
        int xPos = borderLeft() + paddingLeft();
        
        // Add in our left margin.
        int chPos = xPos + child->marginLeft();
        
        // Some objects (e.g., tables, horizontal rules, overflow:auto blocks) avoid floats.  They need
        // to shift over as necessary to dodge any floats that might get in the way.
        if (child->avoidsFloats()) {
            int leftOff = leftOffset(m_height);
            if (style()->textAlign() != KHTML_CENTER && child->style()->marginLeft().type != Auto) {
                if (child->marginLeft() < 0)
                    leftOff += child->marginLeft();
                chPos = kMax(chPos, leftOff); // Let the float sit in the child's margin if it can fit.
            }
            else if (leftOff != xPos) {
                // The object is shifting right. The object might be centered, so we need to
                // recalculate our horizontal margins. Note that the containing block content
                // width computation will take into account the delta between |leftOff| and |xPos|
                // so that we can just pass the content width in directly to the |calcHorizontalMargins|
                // function.
                static_cast<RenderBox*>(child)->calcHorizontalMargins(child->style()->marginLeft(), child->style()->marginRight(), lineWidth(child->yPos()));
                chPos = leftOff + child->marginLeft();
            }
        }
        child->setPos(chPos, child->yPos());
    } else {
        int xPos = m_width - borderRight() - paddingRight() - (includeScrollbarSize() ? m_layer->verticalScrollbarWidth() : 0);
        int chPos = xPos - (child->width() + child->marginRight());
        if (child->avoidsFloats()) {
            int rightOff = rightOffset(m_height);
            if (style()->textAlign() != KHTML_CENTER && child->style()->marginRight().type != Auto) {
                if (child->marginRight() < 0)
                    rightOff -= child->marginRight();
                chPos = kMin(chPos, rightOff - child->width()); // Let the float sit in the child's margin if it can fit.
            } else if (rightOff != xPos) {
                // The object is shifting left. The object might be centered, so we need to
                // recalculate our horizontal margins. Note that the containing block content
                // width computation will take into account the delta between |rightOff| and |xPos|
                // so that we can just pass the content width in directly to the |calcHorizontalMargins|
                // function.
                static_cast<RenderBox*>(child)->calcHorizontalMargins(child->style()->marginLeft(), child->style()->marginRight(), lineWidth(child->yPos()));
                chPos = rightOff - child->marginRight() - child->width();
            }
        }
        child->setPos(chPos, child->yPos());
    }
}

void RenderBlock::setCollapsedBottomMargin(const MarginInfo& marginInfo)
{
    if (marginInfo.canCollapseWithBottom() && !marginInfo.canCollapseWithTop()) {
        // Update our max pos/neg bottom margins, since we collapsed our bottom margins
        // with our children.
        m_maxBottomPosMargin = kMax(m_maxBottomPosMargin, marginInfo.posMargin());
        m_maxBottomNegMargin = kMax(m_maxBottomNegMargin, marginInfo.negMargin());

        if (!marginInfo.bottomQuirk())
            m_bottomMarginQuirk = false;

        if (marginInfo.bottomQuirk() && marginBottom() == 0)
            // We have no bottom margin and our last child has a quirky margin.
            // We will pick up this quirky margin and pass it through.
            // This deals with the <td><div><p> case.
            m_bottomMarginQuirk = true;
    }
}

void RenderBlock::handleBottomOfBlock(int top, int bottom, MarginInfo& marginInfo)
{
     // If our last flow was a self-collapsing block that cleared a float, then we don't
    // collapse it with the bottom of the block.
    if (!marginInfo.selfCollapsingBlockClearedFloat())
        marginInfo.setAtBottomOfBlock(true);

    // If we can't collapse with children then go ahead and add in the bottom margin.
    if (!marginInfo.canCollapseWithBottom() && !marginInfo.canCollapseWithTop()
        && (!style()->htmlHacks() || !marginInfo.quirkContainer() || !marginInfo.bottomQuirk()))
        m_height += marginInfo.margin();
        
    // Now add in our bottom border/padding.
    m_height += bottom;

    // Negative margins can cause our height to shrink below our minimal height (border/padding).
    // If this happens, ensure that the computed height is increased to the minimal height.
    m_height = kMax(m_height, top + bottom);

    // Always make sure our overflow height is at least our height.
    m_overflowHeight = kMax(m_height, m_overflowHeight);

    // Update our bottom collapsed margin info.
    setCollapsedBottomMargin(marginInfo);
}

void RenderBlock::layoutBlockChildren(bool relayoutChildren)
{
    int top = borderTop() + paddingTop();
    int bottom = borderBottom() + paddingBottom() + (includeScrollbarSize() ? m_layer->horizontalScrollbarHeight() : 0);

    m_height = m_overflowHeight = top;

    // The margin struct caches all our current margin collapsing state.  The compact struct caches state when we encounter compacts,
    MarginInfo marginInfo(this, top, bottom);
    CompactInfo compactInfo;

    // Fieldsets need to find their legend and position it inside the border of the object.
    // The legend then gets skipped during normal layout.
    RenderObject* legend = layoutLegend(relayoutChildren);

    RenderObject* child = firstChild();
    while (child) {
        if (legend == child) {
            child = child->nextSibling();
            continue; // Skip the legend, since it has already been positioned up in the fieldset's border.
        }

        int oldTopPosMargin = m_maxTopPosMargin;
        int oldTopNegMargin = m_maxTopNegMargin;

        // Make sure we layout children if they need it.
        // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
        // an auto value.  Add a method to determine this, so that we can avoid the relayout.
        if (relayoutChildren || child->style()->height().isPercent())
            child->setChildNeedsLayout(true);

        // Handle the four types of special elements first.  These include positioned content, floating content, compacts and
        // run-ins.  When we encounter these four types of objects, we don't actually lay them out as normal flow blocks.
        bool handled = false;
        RenderObject* next = handleSpecialChild(child, marginInfo, compactInfo, handled);
        if (handled) { child = next; continue; }

        // The child is a normal flow object.  Compute its vertical margins now.
        child->calcVerticalMargins();

        // Do not allow a collapse if the margin top collapse style is set to SEPARATE.
        if (child->style()->marginTopCollapse() == MSEPARATE) {
            marginInfo.setAtTopOfBlock(false);
            marginInfo.clearMargin();
        }

        // Try to guess our correct y position.  In most cases this guess will
        // be correct.  Only if we're wrong (when we compute the real y position)
        // will we have to potentially relayout.
        int yPosEstimate = estimateVerticalPosition(child, marginInfo);
        
        // If an element might be affected by the presence of floats, then always mark it for
        // layout.
        if (!child->avoidsFloats() || child->usesLineWidth()) {
            int fb = floatBottom();
            if (fb > m_height || fb > yPosEstimate)
                child->setChildNeedsLayout(true);
        }

        // Cache our old position so that we can dirty the proper repaint rects if the child moves.
        int oldChildX = child->xPos();
        int oldChildY = child->yPos();
        
        // Go ahead and position the child as though it didn't collapse with the top.
        child->setPos(child->xPos(), yPosEstimate);
        child->layoutIfNeeded();

        // Now determine the correct ypos based off examination of collapsing margin
        // values.
        collapseMargins(child, marginInfo, yPosEstimate);
        int postCollapseChildY = child->yPos();

        // Now check for clear.
        clearFloatsIfNeeded(child, marginInfo, oldTopPosMargin, oldTopNegMargin);

        // We are no longer at the top of the block if we encounter a non-empty child.  
        // This has to be done after checking for clear, so that margins can be reset if a clear occurred.
        if (marginInfo.atTopOfBlock() && !child->isSelfCollapsingBlock())
            marginInfo.setAtTopOfBlock(false);

        // Now place the child in the correct horizontal position
        determineHorizontalPosition(child);

        // Update our top overflow in case the child spills out the top of the block.
        m_overflowTop = kMin(m_overflowTop, child->yPos() + child->overflowTop(false));
        
        // Update our height now that the child has been placed in the correct position.
        m_height += child->height();
        if (child->style()->marginBottomCollapse() == MSEPARATE) {
            m_height += child->marginBottom();
            marginInfo.clearMargin();
        }
        int overflowDelta = child->overflowHeight(false) - child->height();
        if (m_height + overflowDelta > m_overflowHeight)
            m_overflowHeight = m_height + overflowDelta;

        // If the child has overhanging floats that intrude into following siblings (or possibly out
        // of this block), then the parent gets notified of the floats now.
        addOverhangingFloats(static_cast<RenderBlock *>(child), -child->xPos(), -child->yPos());

        // See if this child has made our overflow need to grow.
        int rightChildPos = child->xPos() + kMax(child->overflowWidth(false), child->width());
        m_overflowWidth = kMax(rightChildPos, m_overflowWidth);
        m_overflowLeft = kMin(child->xPos() + child->overflowLeft(false), m_overflowLeft);
        
        // Insert our compact into the block margin if we have one.
        insertCompactIfNeeded(child, compactInfo);

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants.  An exception is if we need a layout.  In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout()) {
            int finalChildX = child->xPos();
            int finalChildY = child->yPos();
            if (finalChildX != oldChildX || finalChildY != oldChildY)
                child->repaintDuringLayoutIfMoved(oldChildX, oldChildY);
            else if (finalChildY != yPosEstimate || finalChildY != postCollapseChildY) {
                // The child's repaints during layout were done before it reached its final position,
                // so they were wrong.
                child->repaint();
                child->repaintFloatingDescendants();
            }
        }

        child = child->nextSibling();
    }

    // Now do the handling of the bottom of the block, adding in our bottom border/padding and
    // determining the correct collapsed bottom margin information.
    handleBottomOfBlock(top, bottom, marginInfo);

    // Finished. Clear the dirty layout bits.
    setNeedsLayout(false);
}

void RenderBlock::layoutPositionedObjects(bool relayoutChildren)
{
    if (m_positionedObjects) {
        //kdDebug( 6040 ) << renderName() << " " << this << "::layoutPositionedObjects() start" << endl;
        RenderObject* r;
        QPtrListIterator<RenderObject> it(*m_positionedObjects);
        for ( ; (r = it.current()); ++it ) {
            // When a non-positioned block element moves, it may have positioned children that are implicitly positioned relative to the
            // non-positioned block.  Rather than trying to detect all of these movement cases, we just always lay out positioned
            // objects that are positioned implicitly like this.  Such objects are rare, and so in typical DHTML menu usage (where everything is
            // positioned explicitly) this should not incur a performance penalty.
            if (relayoutChildren || (r->hasStaticY() && r->parent() != this && r->parent()->isBlockFlow()))
                r->setChildNeedsLayout(true);
            r->layoutIfNeeded();
        }
    }
}

void RenderBlock::markPositionedObjectsForLayout()
{
    if (m_positionedObjects) {
        RenderObject* r;
        QPtrListIterator<RenderObject> it(*m_positionedObjects);
        for (; (r = it.current()); ++it)
            r->setChildNeedsLayout(true);
    }
}

void RenderBlock::getAbsoluteRepaintRectIncludingFloats(IntRect& bounds, IntRect& fullBounds)
{
    bounds = fullBounds = getAbsoluteRepaintRect();

    // Include any overhanging floats (if we know we're the one to paint them).
    // We null-check m_floatingObjects here to catch any cases where m_height ends up negative
    // for some reason.  I think I've caught all those cases, but this way we stay robust and don't
    // crash.
    if (hasOverhangingFloats() && m_floatingObjects) {
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it) {
            // Only repaint the object if our noPaint flag isn't set and if it isn't in
            // its own layer.
            if (!r->noPaint && !r->node->layer()) {
                IntRect childRect, childFullRect;
                r->node->getAbsoluteRepaintRectIncludingFloats(childRect, childFullRect);
                fullBounds.unite(childFullRect);
            }
        }
    }
}

void RenderBlock::repaintFloatingDescendants()
{
    // Repaint any overhanging floats (if we know we're the one to paint them).
    if (hasOverhangingFloats()) {
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it) {
            // Only repaint the object if our noPaint flag isn't set and if it isn't in
            // its own layer.
            if (!r->noPaint && !r->node->layer()) {                
                r->node->repaint();
                r->node->repaintFloatingDescendants();
            }
        }
    }
}

void RenderBlock::repaintObjectsBeforeLayout()
{
    RenderFlow::repaintObjectsBeforeLayout();
    if (!needsLayout())
        return;

    // Walk our positioned objects.
    if (m_positionedObjects) {
        RenderObject* r;
        QPtrListIterator<RenderObject> it(*m_positionedObjects);
        for ( ; (r = it.current()); ++it )
            r->repaintObjectsBeforeLayout();
    }
}

void RenderBlock::paint(PaintInfo& i, int _tx, int _ty)
{
    _tx += m_x;
    _ty += m_y;

    // Check if we need to do anything at all.
    if (!isInlineFlow() && !isRoot()) {
        IntRect overflowBox = overflowRect(false);
        overflowBox.inflate(maximalOutlineSize(i.phase));
        overflowBox.move(_tx, _ty);
        bool intersectsOverflowBox = overflowBox.intersects(i.r);
        if (!intersectsOverflowBox) {
            // Check floats next.
            if (i.phase != PaintActionFloat)
                return;
            IntRect floatBox = floatRect();
            floatBox.inflate(maximalOutlineSize(i.phase));
            floatBox.move(_tx, _ty);
            if (!floatBox.intersects(i.r))
                return;
        }
    }

    return paintObject(i, _tx, _ty);
}

void RenderBlock::paintChildren(PaintInfo& i, int _tx, int _ty)
{
    // We don't paint our own background, but we do let the kids paint their backgrounds.
    PaintInfo paintInfo(i.p, i.r, i.phase == PaintActionChildBlockBackgrounds ? PaintActionChildBlockBackground : i.phase,
                        paintingRootForChildren(i));
    bool isPrinting = i.p->printing();

    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {        
        // Check for page-break-before: always, and if it's set, break and bail.
        if (isPrinting && !childrenInline() && child->style()->pageBreakBefore() == PBALWAYS &&
            inRootBlockContext() && (_ty + child->yPos()) > i.r.y() && 
            (_ty + child->yPos()) < i.r.bottom()) {
            canvas()->setBestTruncatedAt(_ty + child->yPos(), this, true);
            return;
        }
        
        if (!child->layer() && !child->isFloating())
            child->paint(paintInfo, _tx, _ty);
        
        // Check for page-break-after: always, and if it's set, break and bail.
        if (isPrinting && !childrenInline() && child->style()->pageBreakAfter() == PBALWAYS && 
            inRootBlockContext() && (_ty + child->yPos() + child->height()) > i.r.y() && 
            (_ty + child->yPos() + child->height()) < i.r.bottom()) {
            canvas()->setBestTruncatedAt(_ty + child->yPos() + child->height() + child->collapsedMarginBottom(), this, true);
            return;
        }
    }
}

void RenderBlock::paintCaret(PaintInfo& i, CaretType type)
{
    const SelectionController &s = type == CursorCaret ? document()->frame()->selection() : document()->frame()->dragCaret();
    NodeImpl *caretNode = s.start().node();
    RenderObject *renderer = caretNode ? caretNode->renderer() : 0;
    if (renderer && (renderer == this || renderer->containingBlock() == this) && caretNode && caretNode->isContentEditable()) {
        if (type == CursorCaret) {
            document()->frame()->paintCaret(i.p, i.r);
        } else {
            document()->frame()->paintDragCaret(i.p, i.r);
        }
    }
}

void RenderBlock::paintObject(PaintInfo& i, int _tx, int _ty)
{
    PaintAction paintAction = i.phase;

    // If we're a repositioned run-in or a compact, don't paint background/borders.
    bool inlineFlow = isInlineFlow();

    // 1. paint background, borders etc
    if (!inlineFlow &&
        (paintAction == PaintActionBlockBackground || paintAction == PaintActionChildBlockBackground) &&
        shouldPaintBackgroundOrBorder() && style()->visibility() == VISIBLE) {
        paintBoxDecorations(i, _tx, _ty);
    }

    // We're done.  We don't bother painting any children.
    if (paintAction == PaintActionBlockBackground)
        return;

    // Adjust our painting position if we're inside a scrolled layer (e.g., an overflow:auto div).s
    int scrolledX = _tx;
    int scrolledY = _ty;
    if (hasOverflowClip())
        m_layer->subtractScrollOffset(scrolledX, scrolledY);

    // 2. paint contents  
    if (childrenInline())
        paintLines(i, scrolledX, scrolledY);
    else
        paintChildren(i, scrolledX, scrolledY);
    
    // 3. paint selection
    bool isPrinting = i.p->printing();
    if (!inlineFlow && !isPrinting)
        paintSelection(i, scrolledX, scrolledY); // Fill in gaps in selection on lines and between blocks.

    // 4. paint floats.
    if (!inlineFlow && (paintAction == PaintActionFloat || paintAction == PaintActionSelection))
        paintFloats(i, scrolledX, scrolledY, paintAction == PaintActionSelection);

    // 5. paint outline.
    if (!inlineFlow && paintAction == PaintActionOutline && 
        style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(i.p, _tx, _ty, width(), height(), style());

    // 6. paint caret.
    // If the caret's node's render object's containing block is this block, and the paint action is PaintActionForeground,
    // then paint the caret.
    if (!inlineFlow && paintAction == PaintActionForeground) {        
        paintCaret(i, CursorCaret);
        paintCaret(i, DragCaret);
    }

#ifdef BOX_DEBUG
    if ( style() && style()->visibility() == VISIBLE ) {
        if(isAnonymous())
            outlineBox(i.p, _tx, _ty, "green");
        if(isFloating())
            outlineBox(i.p, _tx, _ty, "yellow");
        else
            outlineBox(i.p, _tx, _ty);
    }
#endif
}

void RenderBlock::paintFloats(PaintInfo& i, int _tx, int _ty, bool paintSelection)
{
    if (!m_floatingObjects)
        return;

    FloatingObject* r;
    QPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it) {
        // Only paint the object if our noPaint flag isn't set.
        if (!r->noPaint && !r->node->layer()) {
            PaintInfo info(i.p, i.r, paintSelection ? PaintActionSelection : PaintActionBlockBackground, i.paintingRoot);
            int tx = _tx + r->left - r->node->xPos() + r->node->marginLeft();
            int ty = _ty + r->startY - r->node->yPos() + r->node->marginTop();
            r->node->paint(info, tx, ty);
            if (!paintSelection) {
                info.phase = PaintActionChildBlockBackgrounds;
                r->node->paint(info, tx, ty);
                info.phase = PaintActionFloat;
                r->node->paint(info, tx, ty);
                info.phase = PaintActionForeground;
                r->node->paint(info, tx, ty);
                info.phase = PaintActionOutline;
                r->node->paint(info, tx, ty);
            }
        }
    }
}

void RenderBlock::paintEllipsisBoxes(PaintInfo& i, int _tx, int _ty)
{
    if (!shouldPaintWithinRoot(i) || !firstLineBox())
        return;

    if (style()->visibility() == VISIBLE && i.phase == PaintActionForeground) {
        // We can check the first box and last box and avoid painting if we don't
        // intersect.
        int yPos = _ty + firstLineBox()->yPos();;
        int h = lastLineBox()->yPos() + lastLineBox()->height() - firstLineBox()->yPos();
        if (yPos >= i.r.bottom() || yPos + h <= i.r.y())
            return;

        // See if our boxes intersect with the dirty rect.  If so, then we paint
        // them.  Note that boxes can easily overlap, so we can't make any assumptions
        // based off positions of our first line box or our last line box.
        for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
            yPos = _ty + curr->yPos();
            h = curr->height();
            if (curr->ellipsisBox() && yPos < i.r.bottom() && yPos + h > i.r.y())
                curr->paintEllipsisBox(i, _tx, _ty);
        }
    }
}

void RenderBlock::setSelectionState(SelectionState s)
{
    if (m_selectionState == s)
        return;
    
    if (s == SelectionInside && m_selectionState != SelectionNone)
        return;

    if ((s == SelectionStart && m_selectionState == SelectionEnd) ||
        (s == SelectionEnd && m_selectionState == SelectionStart))
        m_selectionState = SelectionBoth;
    else
        m_selectionState = s;
    
    RenderBlock* cb = containingBlock();
    if (cb && !cb->isCanvas())
        cb->setSelectionState(s);
}

bool RenderBlock::shouldPaintSelectionGaps() const
{
    return m_selectionState != SelectionNone && style()->visibility() == VISIBLE && isSelectionRoot();
}

bool RenderBlock::isSelectionRoot() const
{
    // FIXME: Eventually tables should have to learn how to fill gaps between cells, at least in simple non-spanning cases.
    return (isBody() || isRoot() || hasOverflowClip() || isRelPositioned() ||
            isFloatingOrPositioned() || isTableCell() || isInlineBlockOrInlineTable());
}

GapRects RenderBlock::selectionGapRects()
{
    if (!shouldPaintSelectionGaps())
        return GapRects();

    int tx, ty;
    absolutePosition(tx, ty);
    
    int lastTop = -borderTopExtra();
    int lastLeft = leftSelectionOffset(this, lastTop);
    int lastRight = rightSelectionOffset(this, lastTop);
    
    return fillSelectionGaps(this, tx, ty, tx, ty, lastTop, lastLeft, lastRight);
}

void RenderBlock::paintSelection(PaintInfo& i, int tx, int ty)
{
    if (shouldPaintSelectionGaps() && i.phase == PaintActionForeground) {
        int lastTop = -borderTopExtra();
        int lastLeft = leftSelectionOffset(this, lastTop);
        int lastRight = rightSelectionOffset(this, lastTop);
        fillSelectionGaps(this, tx, ty, tx, ty, lastTop, lastLeft, lastRight, &i);
    }
}

GapRects RenderBlock::fillSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, int& lastTop, int& lastLeft, int& lastRight, 
                                        const PaintInfo* i)
{
    // FIXME: overflow: auto/scroll regions need more math here, since painting in the border box is different from painting in the padding box (one is scrolled, the other is
    // fixed).
    GapRects result;
    if (!isBlockFlow())
        return result;

    if (childrenInline())
        result = fillInlineSelectionGaps(rootBlock, blockX, blockY, tx, ty, lastTop, lastLeft, lastRight, i);
    else
        result = fillBlockSelectionGaps(rootBlock, blockX, blockY, tx, ty, lastTop, lastLeft, lastRight, i);
        
    // Go ahead and fill the vertical gap all the way to the bottom of our block if the selection extends past our block.
    if (rootBlock == this && (m_selectionState != SelectionBoth && m_selectionState != SelectionEnd))
        result.uniteCenter(fillVerticalSelectionGap(lastTop, lastLeft, lastRight, ty + height() + borderBottomExtra(),
                                                    rootBlock, blockX, blockY, i));
    return result;
}

GapRects RenderBlock::fillInlineSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, 
                                              int& lastTop, int& lastLeft, int& lastRight, const PaintInfo* i)
{
    GapRects result;
    
    RenderObject* selStart = canvas()->selectionStart();
    // If there is no selection, don't try to get the selection's containing block. 
    // If we do, we'll crash.
    bool containsStart = (selStart && (selStart == this || selStart->containingBlock() == this));

    if (!firstLineBox()) {
        if (containsStart) {
            // Go ahead and update our lastY to be the bottom of the block.  <hr>s or empty blocks with height can trip this
            // case.
            lastTop = (ty - blockY) + height();
            lastLeft = leftSelectionOffset(rootBlock, height());
            lastRight = rightSelectionOffset(rootBlock, height());
        }
        return result;
    }

    RootInlineBox* lastSelectedLine = 0;
    RootInlineBox* curr;
    for (curr = firstRootBox(); curr && !curr->hasSelectedChildren(); curr = curr->nextRootBox());

    // Now paint the gaps for the lines.
    for (; curr && curr->hasSelectedChildren(); curr = curr->nextRootBox()) {
        int selTop =  curr->selectionTop();
        int selHeight = curr->selectionHeight();

        if (!containsStart && !lastSelectedLine && selectionState() != SelectionStart)
            result.uniteCenter(fillVerticalSelectionGap(lastTop, lastLeft, lastRight, ty + selTop,
                                                        rootBlock, blockX, blockY, i));

        if (!i || ty + selTop < i->r.bottom() && ty + selTop + selHeight > i->r.y())
            result.unite(curr->fillLineSelectionGap(selTop, selHeight, rootBlock, blockX, blockY, tx, ty, i));

        lastSelectedLine = curr;
    }

    if (containsStart && !lastSelectedLine)
        // Selection must start just after our last line.
        lastSelectedLine = lastRootBox();

    if (lastSelectedLine && selectionState() != SelectionEnd && selectionState() != SelectionBoth) {
        // Go ahead and update our lastY to be the bottom of the last selected line.
        lastTop = (ty - blockY) + lastSelectedLine->bottomOverflow();
        lastLeft = leftSelectionOffset(rootBlock, lastSelectedLine->bottomOverflow());
        lastRight = rightSelectionOffset(rootBlock, lastSelectedLine->bottomOverflow());
    }
    return result;
}

GapRects RenderBlock::fillBlockSelectionGaps(RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, int& lastTop, int& lastLeft, int& lastRight,
                                          const PaintInfo* i)
{
    GapRects result;
    
    // Go ahead and jump right to the first block child that contains some selected objects.
    RenderObject* curr;
    for (curr = firstChild(); curr && curr->selectionState() == SelectionNone; curr = curr->nextSibling());
    
    for (bool sawSelectionEnd = false; curr && !sawSelectionEnd; curr = curr->nextSibling()) {
        SelectionState childState = curr->selectionState();
        if (childState == SelectionBoth || childState == SelectionEnd)
            sawSelectionEnd = true;

        if (curr->isFloatingOrPositioned())
            continue; // We must be a normal flow object in order to even be considered.
        
        if (curr->isRelPositioned() && curr->layer()) {
            // If the relposition offset is anything other than 0, then treat this just like an absolute positioned element.
            // Just disregard it completely.
            int x = 0;
            int y = 0;
            curr->layer()->relativePositionOffset(x, y);
            if (x || y)
                continue;
        }

        bool paintsOwnSelection = curr->shouldPaintSelectionGaps() || curr->isTable(); // FIXME: Eventually we won't special-case table like this.
        bool fillBlockGaps = paintsOwnSelection || (curr->canBeSelectionLeaf() && childState != SelectionNone);
        if (fillBlockGaps) {
            // We need to fill the vertical gap above this object.
            if (childState == SelectionEnd || childState == SelectionInside)
                // Fill the gap above the object.
                result.uniteCenter(fillVerticalSelectionGap(lastTop, lastLeft, lastRight, 
                                                            ty + curr->yPos(), rootBlock, blockX, blockY, i));

            // Only fill side gaps for objects that paint their own selection if we know for sure the selection is going to extend all the way *past*
            // our object.  We know this if the selection did not end inside our object.
            if (paintsOwnSelection && (childState == SelectionStart || sawSelectionEnd))
                childState = SelectionNone;

            // Fill side gaps on this object based off its state.
            bool leftGap, rightGap;
            getHorizontalSelectionGapInfo(childState, leftGap, rightGap);
            
            if (leftGap)
                result.uniteLeft(fillLeftSelectionGap(this, curr->xPos(), curr->yPos(), curr->height(), rootBlock, blockX, blockY, tx, ty, i));
            if (rightGap)
                result.uniteRight(fillRightSelectionGap(this, curr->xPos() + curr->width(), curr->yPos(), curr->height(), rootBlock, blockX, blockY, tx, ty, i));

            // Update lastTop to be just underneath the object.  lastLeft and lastRight extend as far as
            // they can without bumping into floating or positioned objects.  Ideally they will go right up
            // to the border of the root selection block.
            lastTop = (ty - blockY) + (curr->yPos() + curr->height());
            lastLeft = leftSelectionOffset(rootBlock, curr->yPos() + curr->height());
            lastRight = rightSelectionOffset(rootBlock, curr->yPos() + curr->height());
        }
        else if (childState != SelectionNone)
            // We must be a block that has some selected object inside it.  Go ahead and recur.
            result.unite(static_cast<RenderBlock*>(curr)->fillSelectionGaps(rootBlock, blockX, blockY, tx + curr->xPos(), ty + curr->yPos(), 
                                                                            lastTop, lastLeft, lastRight, i));
    }
    return result;
}

IntRect RenderBlock::fillHorizontalSelectionGap(RenderObject* selObj, int xPos, int yPos, int width, int height,
                                              const PaintInfo* i)
{
    if (width <= 0 || height <= 0)
        return IntRect();

    IntRect gapRect(xPos, yPos, width, height);
    if (i) {
        // Paint the rect.
        Brush selBrush(selObj->selectionColor(i->p));
        i->p->fillRect(gapRect, selBrush);
    }
    return gapRect;
}

IntRect RenderBlock::fillVerticalSelectionGap(int lastTop, int lastLeft, int lastRight,
                                            int bottomY, RenderBlock* rootBlock, int blockX, int blockY,
                                            const PaintInfo* i)
{
    int top = blockY + lastTop;
    int height = bottomY - top;
    if (height <= 0)
        return IntRect();
        
    // Get the selection offsets for the bottom of the gap
    int left = blockX + kMax(lastLeft, leftSelectionOffset(rootBlock, bottomY));
    int right = blockX + kMin(lastRight, rightSelectionOffset(rootBlock, bottomY));
    int width = right - left;
    if (width <= 0)
        return IntRect();

    IntRect gapRect(left, top, width, height);
    if (i) {
        // Paint the rect.
        Brush selBrush(selectionColor(i->p));
        i->p->fillRect(gapRect, selBrush);
    }
    return gapRect;
}

IntRect RenderBlock::fillLeftSelectionGap(RenderObject* selObj, int xPos, int yPos, int height, RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, const PaintInfo* i)
{
    int top = yPos + ty;
    int left = blockX + kMax(leftSelectionOffset(rootBlock, yPos), leftSelectionOffset(rootBlock, yPos + height));
    int width = tx + xPos - left;
    if (width <= 0)
        return IntRect();

    IntRect gapRect(left, top, width, height);
    if (i) {
        // Paint the rect.
        Brush selBrush(selObj->selectionColor(i->p));
        i->p->fillRect(gapRect, selBrush);
    }
    return gapRect;
}

IntRect RenderBlock::fillRightSelectionGap(RenderObject* selObj, int xPos, int yPos, int height, RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, const PaintInfo* i)
{
    int left = xPos + tx;
    int top = yPos + ty;
    int right = blockX + kMin(rightSelectionOffset(rootBlock, yPos), rightSelectionOffset(rootBlock, yPos + height));
    int width = right - left;
    if (width <= 0)
        return IntRect();

    IntRect gapRect(left, top, width, height);
    if (i) {
        // Paint the rect.
        Brush selBrush(selObj->selectionColor(i->p));
        i->p->fillRect(gapRect, selBrush);
    }
    return gapRect;
}

void RenderBlock::getHorizontalSelectionGapInfo(SelectionState state, bool& leftGap, bool& rightGap)
{
    bool ltr = style()->direction() == LTR;
    leftGap = (state == RenderObject::SelectionInside) ||
              (state == RenderObject::SelectionEnd && ltr) ||
              (state == RenderObject::SelectionStart && !ltr);
    rightGap = (state == RenderObject::SelectionInside) ||
               (state == RenderObject::SelectionStart && ltr) ||
               (state == RenderObject::SelectionEnd && !ltr);
}

int RenderBlock::leftSelectionOffset(RenderBlock* rootBlock, int y)
{
    int left = leftOffset(y);
    if (left == borderLeft() + paddingLeft()) {
        if (rootBlock != this)
            // The border can potentially be further extended by our containingBlock().
            return containingBlock()->leftSelectionOffset(rootBlock, y + yPos());
        return 0;
    }
    else {
        RenderBlock* cb = this;
        while (cb != rootBlock) {
            left += cb->xPos();
            cb = cb->containingBlock();
        }
    }
    
    return left;
}

int RenderBlock::rightSelectionOffset(RenderBlock* rootBlock, int y)
{
    int right = rightOffset(y);
    if (right == (contentWidth() + (borderLeft() + paddingLeft()))) {
        if (rootBlock != this)
            // The border can potentially be further extended by our containingBlock().
            return containingBlock()->rightSelectionOffset(rootBlock, y + yPos());
        return width();
    }
    else {
        RenderBlock* cb = this;
        while (cb != rootBlock) {
            right += cb->xPos();
            cb = cb->containingBlock();
        }
    }
    return right;
}

void RenderBlock::insertPositionedObject(RenderObject *o)
{
    // Create the list of special objects if we don't aleady have one
    if (!m_positionedObjects) {
        m_positionedObjects = new QPtrList<RenderObject>;
        m_positionedObjects->setAutoDelete(false);
    }
    else {
        // Don't insert the object again if it's already in the list
        QPtrListIterator<RenderObject> it(*m_positionedObjects);
        RenderObject* f;
        while ( (f = it.current()) ) {
            if (f == o) return;
            ++it;
        }
    }

    m_positionedObjects->append(o);
}

void RenderBlock::removePositionedObject(RenderObject *o)
{
    if (m_positionedObjects) {
        QPtrListIterator<RenderObject> it(*m_positionedObjects);
        while (it.current()) {
            if (it.current() == o)
                m_positionedObjects->removeRef(it.current());
            ++it;
        }
    }
}

void RenderBlock::insertFloatingObject(RenderObject *o)
{
    // Create the list of special objects if we don't aleady have one
    if (!m_floatingObjects) {
        m_floatingObjects = new QPtrList<FloatingObject>;
        m_floatingObjects->setAutoDelete(true);
    }
    else {
        // Don't insert the object again if it's already in the list
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        FloatingObject* f;
        while ( (f = it.current()) ) {
            if (f->node == o) return;
            ++it;
        }
    }

    // Create the special object entry & append it to the list

    FloatingObject *newObj;
    if (o->isFloating()) {
        // floating object
        o->layoutIfNeeded();

        if(o->style()->floating() == FLEFT)
            newObj = new FloatingObject(FloatingObject::FloatLeft);
        else
            newObj = new FloatingObject(FloatingObject::FloatRight);

        newObj->startY = -1;
        newObj->endY = -1;
        newObj->width = o->width() + o->marginLeft() + o->marginRight();
        newObj->noPaint = o->layer(); // If a layer exists, the float will paint itself.  Otherwise someone else will.
    }
    else {
        // We should never get here, as insertFloatingObject() should only ever be called with floating
        // objects.
        KHTMLAssert(false);
        newObj = 0; // keep gcc's uninitialized variable warnings happy
    }

    newObj->node = o;

    m_floatingObjects->append(newObj);
}

void RenderBlock::removeFloatingObject(RenderObject *o)
{
    if (m_floatingObjects) {
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        while (it.current()) {
            if (it.current()->node == o)
                m_floatingObjects->removeRef(it.current());
            ++it;
        }
    }
}

void RenderBlock::positionNewFloats()
{
    if(!m_floatingObjects) return;
    FloatingObject *f = m_floatingObjects->getLast();
    if(!f || f->startY != -1) return;
    FloatingObject *lastFloat;
    while(1)
    {
        lastFloat = m_floatingObjects->prev();
        if (!lastFloat || lastFloat->startY != -1) {
            m_floatingObjects->next();
            break;
        }
        f = lastFloat;
    }


    int y = m_height;


    // the float can not start above the y position of the last positioned float.
    if(lastFloat && lastFloat->startY > y)
        y = lastFloat->startY;

    while(f)
    {
        //skip elements copied from elsewhere and positioned elements
        if (f->node->containingBlock()!=this)
        {
            f = m_floatingObjects->next();
            continue;
        }

        RenderObject *o = f->node;
        int _height = o->height() + o->marginTop() + o->marginBottom();

        int ro = rightOffset(); // Constant part of right offset.
        int lo = leftOffset(); // Constat part of left offset.
        int fwidth = f->width; // The width we look for.
                               //kdDebug( 6040 ) << " Object width: " << fwidth << " available width: " << ro - lo << endl;
        if (ro - lo < fwidth)
            fwidth = ro - lo; // Never look for more than what will be available.
        
        int oldChildX = o->xPos();
        int oldChildY = o->yPos();
        
        if ( o->style()->clear() & CLEFT )
            y = kMax( leftBottom(), y );
        if ( o->style()->clear() & CRIGHT )
            y = kMax( rightBottom(), y );

        if (o->style()->floating() == FLEFT)
        {
            int heightRemainingLeft = 1;
            int heightRemainingRight = 1;
            int fx = leftRelOffset(y,lo, false, &heightRemainingLeft);
            while (rightRelOffset(y,ro, false, &heightRemainingRight)-fx < fwidth)
            {
                y += kMin( heightRemainingLeft, heightRemainingRight );
                fx = leftRelOffset(y,lo, false, &heightRemainingLeft);
            }
            if (fx<0) fx=0;
            f->left = fx;
            //kdDebug( 6040 ) << "positioning left aligned float at (" << fx + o->marginLeft()  << "/" << y + o->marginTop() << ") fx=" << fx << endl;
            o->setPos(fx + o->marginLeft(), y + o->marginTop());
        }
        else
        {
            int heightRemainingLeft = 1;
            int heightRemainingRight = 1;
            int fx = rightRelOffset(y,ro, false, &heightRemainingRight);
            while (fx - leftRelOffset(y,lo, false, &heightRemainingLeft) < fwidth)
            {
                y += kMin(heightRemainingLeft, heightRemainingRight);
                fx = rightRelOffset(y,ro, false, &heightRemainingRight);
            }
            if (fx<f->width) fx=f->width;
            f->left = fx - f->width;
            //kdDebug( 6040 ) << "positioning right aligned float at (" << fx - o->marginRight() - o->width() << "/" << y + o->marginTop() << ")" << endl;
            o->setPos(fx - o->marginRight() - o->width(), y + o->marginTop());
        }
        f->startY = y;
        f->endY = f->startY + _height;

        // If the child moved, we have to repaint it.
        if (o->checkForRepaintDuringLayout())
            o->repaintDuringLayoutIfMoved(oldChildX, oldChildY);

        //kdDebug( 6040 ) << "floatingObject x/y= (" << f->left << "/" << f->startY << "-" << f->width << "/" << f->endY - f->startY << ")" << endl;

        f = m_floatingObjects->next();
    }
}

void RenderBlock::newLine()
{
    positionNewFloats();
    // set y position
    int newY = 0;
    switch(m_clearStatus)
    {
        case CLEFT:
            newY = leftBottom();
            break;
        case CRIGHT:
            newY = rightBottom();
            break;
        case CBOTH:
            newY = floatBottom();
        default:
            break;
    }
    if(m_height < newY)
    {
        //      kdDebug( 6040 ) << "adjusting y position" << endl;
        m_height = newY;
    }
    m_clearStatus = CNONE;
}

int
RenderBlock::leftOffset() const
{
    return borderLeft()+paddingLeft();
}

int
RenderBlock::leftRelOffset(int y, int fixedOffset, bool applyTextIndent,
                           int *heightRemaining ) const
{
    int left = fixedOffset;
    if (m_floatingObjects) {
        if ( heightRemaining ) *heightRemaining = 1;
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it )
        {
            //kdDebug( 6040 ) <<(void *)this << " left: sy, ey, x, w " << r->startY << "," << r->endY << "," << r->left << "," << r->width << " " << endl;
            if (r->startY <= y && r->endY > y &&
                r->type == FloatingObject::FloatLeft &&
                r->left + r->width > left) {
                left = r->left + r->width;
                if ( heightRemaining ) *heightRemaining = r->endY - y;
            }
        }
    }

    if (applyTextIndent && m_firstLine && style()->direction() == LTR) {
        int cw=0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->contentWidth();
        left += style()->textIndent().minWidth(cw);
    }

    //kdDebug( 6040 ) << "leftOffset(" << y << ") = " << left << endl;
    return left;
}

int
RenderBlock::rightOffset() const
{
    int right = m_width - borderRight() - paddingRight();
    if (includeScrollbarSize())
        right -= m_layer->verticalScrollbarWidth();
    return right;
}

int
RenderBlock::rightRelOffset(int y, int fixedOffset, bool applyTextIndent,
                            int *heightRemaining ) const
{
    int right = fixedOffset;

    if (m_floatingObjects) {
        if (heightRemaining) *heightRemaining = 1;
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it )
        {
            //kdDebug( 6040 ) << "right: sy, ey, x, w " << r->startY << "," << r->endY << "," << r->left << "," << r->width << " " << endl;
            if (r->startY <= y && r->endY > y &&
                r->type == FloatingObject::FloatRight &&
                r->left < right) {
                right = r->left;
                if ( heightRemaining ) *heightRemaining = r->endY - y;
            }
        }
    }
    
    if (applyTextIndent && m_firstLine && style()->direction() == RTL) {
        int cw=0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->contentWidth();
        right -= style()->textIndent().minWidth(cw);
    }
    
    //kdDebug( 6040 ) << "rightOffset(" << y << ") = " << right << endl;
    return right;
}

int
RenderBlock::lineWidth(int y) const
{
    //kdDebug( 6040 ) << "lineWidth(" << y << ")=" << rightOffset(y) - leftOffset(y) << endl;
    int result = rightOffset(y) - leftOffset(y);
    return (result < 0) ? 0 : result;
}

int
RenderBlock::nearestFloatBottom(int height) const
{
    if (!m_floatingObjects) return 0;
    int bottom = 0;
    FloatingObject* r;
    QPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>height && (r->endY<bottom || bottom==0))
            bottom=r->endY;
    return kMax(bottom, height);
}

int
RenderBlock::floatBottom() const
{
    if (!m_floatingObjects) return 0;
    int bottom=0;
    FloatingObject* r;
    QPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom)
            bottom=r->endY;
    return bottom;
}

IntRect RenderBlock::floatRect() const
{
    IntRect result(borderBox());
    if (!m_floatingObjects || hasOverflowClip())
        return result;
    FloatingObject* r;
    QPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for (; (r = it.current()); ++it) {
        if (!r->noPaint && !r->node->layer()) {
            // Check this float.
            int bottomDelta = kMax(0, r->startY + r->node->marginTop() + r->node->overflowHeight(false) - result.bottom());
            if (bottomDelta)
                result.setHeight(result.height() + bottomDelta);
            int rightDelta = kMax(0, r->left + r->node->marginLeft() + r->node->overflowWidth(false) - result.right());
            if (rightDelta)
                result.setWidth(result.width() + rightDelta);
            
            // Now check left and top
            int topDelta = kMin(0, r->startY + r->node->marginTop() - result.y());
            if (topDelta < 0)
                result.inflateY(-topDelta);
            int leftDelta = kMin(0, r->left + r->node->marginLeft() - result.x());
            if (leftDelta < 0)
                result.inflateX(-leftDelta);
        }
    }

    return result;
}

int
RenderBlock::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int bottom = RenderFlow::lowestPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return bottom;
    if (includeSelf && m_overflowHeight > bottom)
        bottom = m_overflowHeight;
    
    if (m_floatingObjects) {
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it ) {
            if (!r->noPaint || r->node->layer()) {
                int lp = r->startY + r->node->marginTop() + r->node->lowestPosition(false);
                bottom = kMax(bottom, lp);
            }
        }
    }

    // Fixed positioned objects do not scroll and thus should not constitute
    // part of the lowest position.
    if (m_positionedObjects && !isCanvas()) {
        RenderObject* r;
        QPtrListIterator<RenderObject> it(*m_positionedObjects);
        for ( ; (r = it.current()); ++it ) {
            int lp = r->yPos() + r->lowestPosition(false);
            bottom = kMax(bottom, lp);
        }
    }

    if (!includeSelf && lastLineBox()) {
        int lp = lastLineBox()->yPos() + lastLineBox()->height();
        bottom = kMax(bottom, lp);
    }
    
    return bottom;
}

int RenderBlock::rightmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int right = RenderFlow::rightmostPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return right;
    if (includeSelf && m_overflowWidth > right)
        right = m_overflowWidth;
    
    if (m_floatingObjects) {
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it ) {
            if (!r->noPaint || r->node->layer()) {
                int rp = r->left + r->node->marginLeft() + r->node->rightmostPosition(false);
                right = kMax(right, rp);
            }
        }
    }

    if (m_positionedObjects && !isCanvas()) {
        RenderObject* r;
        QPtrListIterator<RenderObject> it(*m_positionedObjects);
        for ( ; (r = it.current()); ++it ) {
            int rp = r->xPos() + r->rightmostPosition(false);
            right = kMax(right, rp);
        }
    }

    if (!includeSelf && firstLineBox()) {
        for (InlineRunBox* currBox = firstLineBox(); currBox; currBox = currBox->nextLineBox()) {
            int rp = currBox->xPos() + currBox->width();
            right = kMax(right, rp);
        }
    }
    
    return right;
}

int RenderBlock::leftmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int left = RenderFlow::leftmostPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && hasOverflowClip())
        return left;
    if (includeSelf && m_overflowLeft < left)
        left = m_overflowLeft;
    
    if (m_floatingObjects) {
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it ) {
            if (!r->noPaint || r->node->layer()) {
                int lp = r->left + r->node->marginLeft() + r->node->leftmostPosition(false);
                left = kMin(left, lp);
            }
        }
    }
    
    if (m_positionedObjects && !isCanvas()) {
        RenderObject* r;
        QPtrListIterator<RenderObject> it(*m_positionedObjects);
        for ( ; (r = it.current()); ++it ) {
            int lp = r->xPos() + r->leftmostPosition(false);
            left = kMin(left, lp);
        }
    }
    
    if (!includeSelf && firstLineBox()) {
        for (InlineRunBox* currBox = firstLineBox(); currBox; currBox = currBox->nextLineBox())
            left = kMin(left, (int)currBox->xPos());
    }
    
    return left;
}

int
RenderBlock::leftBottom()
{
    if (!m_floatingObjects) return 0;
    int bottom=0;
    FloatingObject* r;
    QPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom && r->type == FloatingObject::FloatLeft)
            bottom=r->endY;

    return bottom;
}

int
RenderBlock::rightBottom()
{
    if (!m_floatingObjects) return 0;
    int bottom=0;
    FloatingObject* r;
    QPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom && r->type == FloatingObject::FloatRight)
            bottom=r->endY;

    return bottom;
}

void
RenderBlock::clearFloats()
{
    if (m_floatingObjects)
        m_floatingObjects->clear();

    // Inline blocks are covered by the isReplaced() check in the avoidFloats method.
    if (avoidsFloats() || isRoot() || isCanvas() || isFloatingOrPositioned() || isTableCell())
        return;
    
    // Attempt to locate a previous sibling with overhanging floats.  We skip any elements that are
    // out of flow (like floating/positioned elements), and we also skip over any objects that may have shifted
    // to avoid floats.
    bool parentHasFloats = false;
    RenderObject *prev = previousSibling();
    while (prev && (!prev->isRenderBlock() || prev->avoidsFloats() || prev->isFloatingOrPositioned())) {
        if (prev->isFloating())
            parentHasFloats = true;
         prev = prev->previousSibling();
    }

    // First add in floats from the parent.
    int offset = m_y;
    if (parentHasFloats)
        addIntrudingFloats(static_cast<RenderBlock *>(parent()),
                           parent()->borderLeft() + parent()->paddingLeft(), offset);

    int xoffset = 0;
    if (prev)
        offset -= prev->yPos();
    else {
        prev = parent();
        xoffset += prev->borderLeft() + prev->paddingLeft();
    }
    //kdDebug() << "RenderBlock::clearFloats found previous "<< (void *)this << " prev=" << (void *)prev<< endl;

    // Add overhanging floats from the previous RenderBlock, but only if it has a float that intrudes into our space.
    if (!prev->isRenderBlock()) return;
    RenderBlock* block = static_cast<RenderBlock *>(prev);
    if (!block->m_floatingObjects) return;
    if (block->floatBottom() > offset)
        addIntrudingFloats(block, xoffset, offset);
}

void RenderBlock::addOverhangingFloats(RenderBlock* child, int xoff, int yoff)
{
    // Prevent floats from being added to the canvas by the root element, e.g., <html>.
    if (child->hasOverflowClip() || !child->hasOverhangingFloats() || !child->m_floatingObjects || child->isRoot())
        return;

    QPtrListIterator<FloatingObject> it(*child->m_floatingObjects);
    for (FloatingObject *r; (r = it.current()); ++it) {
        if (child->yPos() + r->endY > height()) {
            // The object may already be in our list. Check for it up front to avoid
            // creating duplicate entries.
            FloatingObject* f = 0;
            if (m_floatingObjects) {
                QPtrListIterator<FloatingObject> it(*m_floatingObjects);
                while ((f = it.current())) {
                    if (f->node == r->node) break;
                    ++it;
                }
            }

            // If the object is not in the list, we add it now.
            if (!f) {
                FloatingObject *floatingObj = new FloatingObject(r->type);
                floatingObj->startY = r->startY - yoff;
                floatingObj->endY = r->endY - yoff;
                floatingObj->left = r->left - xoff;
                floatingObj->width = r->width;
                floatingObj->node = r->node;

                // The nearest enclosing layer always paints the float (so that zindex and stacking
                // behaves properly).  We always want to propagate the desire to paint the float as
                // far out as we can, to the outermost block that overlaps the float, stopping only
                // if we hit a layer boundary.
                if (r->node->enclosingLayer() == enclosingLayer())
                    r->noPaint = true;
                else
                    floatingObj->noPaint = true;
                
                // We create the floating object list lazily.
                if (!m_floatingObjects) {
                    m_floatingObjects = new QPtrList<FloatingObject>;
                    m_floatingObjects->setAutoDelete(true);
                }
                m_floatingObjects->append(floatingObj);
            }
        }
    }
}

void RenderBlock::addIntrudingFloats(RenderBlock* prev, int xoff, int yoff)
{
    // If the parent or previous sibling doesn't have any floats to add, don't bother.
    if (!prev->m_floatingObjects)
        return;

    QPtrListIterator<FloatingObject> it(*prev->m_floatingObjects);
    for (FloatingObject *r; (r = it.current()); ++it) {
        if (r->endY > yoff) {
            // The object may already be in our list. Check for it up front to avoid
            // creating duplicate entries.
            FloatingObject* f = 0;
            if (m_floatingObjects) {
                QPtrListIterator<FloatingObject> it(*m_floatingObjects);
                while ((f = it.current())) {
                    if (f->node == r->node) break;
                    ++it;
                }
            }
            if (!f) {
                FloatingObject *floatingObj = new FloatingObject(r->type);
                floatingObj->startY = r->startY - yoff;
                floatingObj->endY = r->endY - yoff;
                floatingObj->left = r->left - xoff;
                // Applying the child's margin makes no sense in the case where the child was passed in.
                // since his own margin was added already through the subtraction of the |xoff| variable
                // above.  |xoff| will equal -flow->marginLeft() in this case, so it's already been taken
                // into account.  Only apply this code if |child| is false, since otherwise the left margin
                // will get applied twice.
                if (prev != parent())
                    floatingObj->left += prev->marginLeft();
                floatingObj->left -= marginLeft();
                floatingObj->noPaint = true;  // We are not in the direct inheritance chain for this float. We will never paint it.
                floatingObj->width = r->width;
                floatingObj->node = r->node;
                
                // We create the floating object list lazily.
                if (!m_floatingObjects) {
                    m_floatingObjects = new QPtrList<FloatingObject>;
                    m_floatingObjects->setAutoDelete(true);
                }
                m_floatingObjects->append(floatingObj);
            }
        }
    }
}

bool RenderBlock::containsFloat(RenderObject* o)
{
    if (m_floatingObjects) {
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        while (it.current()) {
            if (it.current()->node == o)
                return true;
            ++it;
        }
    }
    return false;
}

void RenderBlock::markAllDescendantsWithFloatsForLayout(RenderObject* floatToRemove)
{
    setChildNeedsLayout(true);

    if (floatToRemove)
        removeFloatingObject(floatToRemove);

    // Iterate over our children and mark them as needed.
    if (!childrenInline()) {
        for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
            if (isBlockFlow() && !child->isFloatingOrPositioned() &&
                (floatToRemove ? child->containsFloat(floatToRemove) : child->containsFloats()))
                child->markAllDescendantsWithFloatsForLayout(floatToRemove);
        }
    }
}

int RenderBlock::getClearDelta(RenderObject *child)
{
    bool clearSet = child->style()->clear() != CNONE;
    int bottom = 0;
    switch (child->style()->clear()) {
        case CNONE:
            break;
        case CLEFT:
            bottom = leftBottom();
            break;
        case CRIGHT:
            bottom = rightBottom();
            break;
        case CBOTH:
            bottom = floatBottom();
            break;
    }

    // We also clear floats if we are too big to sit on the same line as a float (and wish to avoid floats by default).
    // FIXME: Note that the remaining space checks aren't quite accurate, since you should be able to clear only some floats (the minimum # needed
    // to fit) and not all (we should be using nearestFloatBottom and looping).
    // Do not allow tables to wrap in quirks or even in almost strict mode 
    // (ebay on the PLT, finance.yahoo.com in the real world, versiontracker.com forces even almost strict mode not to work)
    int result = clearSet ? kMax(0, bottom - child->yPos()) : 0;
    if (!result && child->avoidsFloats() && child->style()->width().isFixed() && 
        child->minWidth() > lineWidth(child->yPos()) && child->minWidth() <= contentWidth() && 
        document()->inStrictMode())   
        result = kMax(0, floatBottom() - child->yPos());
    return result;
}

bool RenderBlock::isPointInScrollbar(int _x, int _y, int _tx, int _ty)
{
    if (!scrollsOverflow())
        return false;

    if (m_layer->verticalScrollbarWidth()) {
        IntRect vertRect(_tx + width() - borderRight() - m_layer->verticalScrollbarWidth(),
                       _ty + borderTop() - borderTopExtra(),
                       m_layer->verticalScrollbarWidth(),
                       height() + borderTopExtra() + borderBottomExtra() - borderTop() - borderBottom());
        if (vertRect.contains(_x, _y)) {
            RenderLayer::gScrollBar = m_layer->verticalScrollbar();
            return true;
        }
    }

    if (m_layer->horizontalScrollbarHeight()) {
        IntRect horizRect(_tx + borderLeft(),
                        _ty + height() + borderBottomExtra() - m_layer->horizontalScrollbarHeight() - borderBottom(),
                        width() - borderLeft() - borderRight(),
                        m_layer->horizontalScrollbarHeight());
        if (horizRect.contains(_x, _y)) {
            RenderLayer::gScrollBar = m_layer->horizontalScrollbar();
            return true;
        }
    }

    return false;    
}

bool RenderBlock::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    bool inlineFlow = isInlineFlow();

    int tx = _tx + m_x;
    int ty = _ty + m_y + borderTopExtra();
    
    if (!inlineFlow && !isRoot()) {
        // Check if we need to do anything at all.
        IntRect overflowBox = overflowRect(false);
        overflowBox.move(tx, ty);
        bool insideOverflowBox = overflowBox.contains(_x, _y);
        if (!insideOverflowBox) {
            // Check floats next.
            if (hitTestAction != HitTestFloat)
                return false;
            IntRect floatBox = floatRect();
            floatBox.move(tx, ty);
            if (!floatBox.contains(_x, _y))
                return false;
        }
    }

    if (isPointInScrollbar(_x, _y, tx, ty)) {
        if (hitTestAction == HitTestBlockBackground) {
            setInnerNode(info);
            return true;
        }
        return false;
    }

    // Hit test descendants first.
    int scrolledX = tx;
    int scrolledY = ty;
    if (hasOverflowClip())
        m_layer->subtractScrollOffset(scrolledX, scrolledY);
    if (childrenInline() && !isTable()) {
        // We have to hit-test our line boxes.
        if (hitTestLines(info, _x, _y, scrolledX, scrolledY, hitTestAction)) {
            setInnerNode(info);
            return true;
        }
    }
    else {
        // Hit test our children.
        HitTestAction childHitTest = hitTestAction;
        if (hitTestAction == HitTestChildBlockBackgrounds)
            childHitTest = HitTestChildBlockBackground;
        for (RenderObject* child = lastChild(); child; child = child->previousSibling())
            // FIXME: We have to skip over inline flows, since they can show up inside RenderTables at the moment (a demoted inline <form> for example).  If we ever implement a
            // table-specific hit-test method (which we should do for performance reasons anyway), then we can remove this check.
            if (!child->layer() && !child->isFloating() && !child->isInlineFlow() && child->nodeAtPoint(info, _x, _y, scrolledX, scrolledY, childHitTest)) {
                setInnerNode(info);
                return true;
            }
    }
    
    // Hit test floats.
    if (hitTestAction == HitTestFloat && m_floatingObjects) {
        if (isCanvas()) {
            scrolledX += static_cast<RenderCanvas*>(this)->view()->contentsX();
            scrolledY += static_cast<RenderCanvas*>(this)->view()->contentsY();
        }
        
        FloatingObject* o;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for (it.toLast(); (o = it.current()); --it)
            if (!o->noPaint && !o->node->layer() && o->node->hitTest(info, _x, _y,
                                     scrolledX + o->left + o->node->marginLeft() - o->node->xPos(),
                                     scrolledY + o->startY + o->node->marginTop() - o->node->yPos())) {
                setInnerNode(info);
                return true;
            }
    }

    // Now hit test our background.
    if (!inlineFlow && (hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground)) {
        int topExtra = borderTopExtra();
        IntRect boundsRect(tx, ty - topExtra, m_width, m_height + topExtra + borderBottomExtra());
        if (isRoot() || (style()->visibility() == VISIBLE && boundsRect.contains(_x, _y))) {
            setInnerNode(info);
            return true;
        }
    }

    return false;
}

Position RenderBlock::positionForBox(InlineBox *box, bool start) const
{
    if (!box)
        return Position();

    if (!box->object()->element())
        return Position(element(), start ? caretMinOffset() : caretMaxOffset());

    if (!box->isInlineTextBox())
        return Position(box->object()->element(), start ? box->object()->caretMinOffset() : box->object()->caretMaxOffset());

    InlineTextBox *textBox = static_cast<InlineTextBox *>(box);
    return Position(box->object()->element(), start ? textBox->start() : textBox->start() + textBox->len());
}

Position RenderBlock::positionForRenderer(RenderObject *renderer, bool start) const
{
    if (!renderer)
        return Position(element(), 0);

    NodeImpl *node = renderer->element() ? renderer->element() : element();
    if (!node)
        return Position();

    int offset = start ? node->caretMinOffset() : node->caretMaxOffset();
    return Position(node, offset);
}

VisiblePosition RenderBlock::positionForCoordinates(int _x, int _y)
{
    if (isTable())
        return RenderFlow::positionForCoordinates(_x, _y); 

    int absx, absy;
    absolutePosition(absx, absy);

    int top = absy + borderTop() + paddingTop();
    int bottom = top + contentHeight();

    if (_y < top)
        // y coordinate is above block
        return VisiblePosition(positionForRenderer(firstLeafChild(), true), DOWNSTREAM);

    if (_y >= bottom)
        // y coordinate is below block
        return VisiblePosition(positionForRenderer(lastLeafChild(), false), DOWNSTREAM);

    if (childrenInline()) {
        if (!firstRootBox())
            return VisiblePosition(element(), 0, DOWNSTREAM);
            
        if (_y >= top && _y < absy + firstRootBox()->topOverflow())
            // y coordinate is above first root line box
            return VisiblePosition(positionForBox(firstRootBox()->firstLeafChild(), true), DOWNSTREAM);
        
        // look for the closest line box in the root box which is at the passed-in y coordinate
        for (RootInlineBox *root = firstRootBox(); root; root = root->nextRootBox()) {
            top = absy + root->topOverflow();
            // set the bottom based on whether there is a next root box
            if (root->nextRootBox())
                bottom = absy + root->nextRootBox()->topOverflow();
            else
                bottom = absy + root->bottomOverflow();
            // check if this root line box is located at this y coordinate
            if (_y >= top && _y < bottom && root->firstChild()) {
                InlineBox *closestBox = root->closestLeafChildForXPos(_x, absx);
                if (closestBox) {
                    // pass the box a y position that is inside it
                    return closestBox->object()->positionForCoordinates(_x, absy + closestBox->m_y);
                }
            }
        }

        if (lastRootBox())
            // y coordinate is below last root line box
            return VisiblePosition(positionForBox(lastRootBox()->lastLeafChild(), false), DOWNSTREAM);
        
        return VisiblePosition(element(), 0, DOWNSTREAM);
    }
    
    // see if any child blocks exist at this y coordinate
    RenderObject *lastVisibleChild = 0;
    for (RenderObject *renderer = firstChild(); renderer; renderer = renderer->nextSibling()) {
        if (renderer->height() == 0 || renderer->style()->visibility() != VISIBLE || renderer->isFloatingOrPositioned())
            continue;
        renderer->absolutePosition(absx, top);
        RenderObject *next = renderer->nextSibling();
        while (next && next->isFloatingOrPositioned())
            next = next->nextSibling();
        if (next) 
            next->absolutePosition(absx, bottom);
        else
            bottom = top + contentHeight();
        if (_y >= top && _y < bottom) {
            return renderer->positionForCoordinates(_x, _y);
        }
        lastVisibleChild = renderer;
    }

    // pass along to the last child we saw that had a height and is visible.
    if (lastVisibleChild)
        return lastVisibleChild->positionForCoordinates(_x, _y);
    
    // still no luck...return this render object's element and offset 0
    return VisiblePosition(element(), 0, DOWNSTREAM);
}

void RenderBlock::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderBlock)::calcMinMaxWidth() this=" << this << endl;
#endif

    m_minWidth = 0;
    m_maxWidth = 0;

    if (childrenInline())
        calcInlineMinMaxWidth();
    else
        calcBlockMinMaxWidth();

    if(m_maxWidth < m_minWidth) m_maxWidth = m_minWidth;

    if (!style()->autoWrap() && childrenInline()) {
        m_minWidth = m_maxWidth;
        
        // A horizontal marquee with inline children has no minimum width.
        if (style()->overflow() == OMARQUEE && m_layer && m_layer->marquee() && 
            m_layer->marquee()->isHorizontal() && !m_layer->marquee()->isUnfurlMarquee())
            m_minWidth = 0;
    }

    if (isTableCell()) {
        Length w = static_cast<RenderTableCell*>(this)->styleOrColWidth();
        if (w.isFixed() && w.value > 0)
            m_maxWidth = kMax(m_minWidth, calcContentBoxWidth(w.value));
    } else if (style()->width().isFixed() && style()->width().value > 0)
        m_minWidth = m_maxWidth = calcContentBoxWidth(style()->width().value);
    
    if (style()->minWidth().isFixed() && style()->minWidth().value > 0) {
        m_maxWidth = kMax(m_maxWidth, calcContentBoxWidth(style()->minWidth().value));
        m_minWidth = kMax(m_minWidth, calcContentBoxWidth(style()->minWidth().value));
    }
    
    if (style()->maxWidth().isFixed() && style()->maxWidth().value != UNDEFINED) {
        m_maxWidth = kMin(m_maxWidth, calcContentBoxWidth(style()->maxWidth().value));
        m_minWidth = kMin(m_minWidth, calcContentBoxWidth(style()->maxWidth().value));
    }

    int toAdd = 0;
    toAdd = borderLeft() + borderRight() + paddingLeft() + paddingRight();

    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    setMinMaxKnown();

    //kdDebug( 6040 ) << "Text::calcMinMaxWidth(" << this << "): min = " << m_minWidth << " max = " << m_maxWidth << endl;
}

struct InlineMinMaxIterator
{
/* InlineMinMaxIterator is a class that will iterate over all render objects that contribute to
   inline min/max width calculations.  Note the following about the way it walks:
   (1) Positioned content is skipped (since it does not contribute to min/max width of a block)
   (2) We do not drill into the children of floats or replaced elements, since you can't break
       in the middle of such an element.
   (3) Inline flows (e.g., <a>, <span>, <i>) are walked twice, since each side can have
       distinct borders/margin/padding that contribute to the min/max width.
*/
    RenderObject* parent;
    RenderObject* current;
    bool endOfInline;

    InlineMinMaxIterator(RenderObject* p, RenderObject* o, bool end = false)
        :parent(p), current(o), endOfInline(end) {}

    RenderObject* next();
};

RenderObject* InlineMinMaxIterator::next()
{
    RenderObject* result = 0;
    bool oldEndOfInline = endOfInline;
    endOfInline = false;
    while (current != 0 || (current == parent))
    {
        //kdDebug( 6040 ) << "current = " << current << endl;
        if (!oldEndOfInline &&
            (current == parent ||
             (!current->isFloating() && !current->isReplaced() && !current->isPositioned())))
            result = current->firstChild();
        if (!result) {
            // We hit the end of our inline. (It was empty, e.g., <span></span>.)
            if (!oldEndOfInline && current->isInlineFlow()) {
                result = current;
                endOfInline = true;
                break;
            }

            while (current && current != parent) {
                result = current->nextSibling();
                if (result) break;
                current = current->parent();
                if (current && current != parent && current->isInlineFlow()) {
                    result = current;
                    endOfInline = true;
                    break;
                }
            }
        }

        if (!result) break;

        if (!result->isPositioned() && (result->isText() || result->isBR() ||
            result->isFloating() || result->isReplaced() ||
            result->isInlineFlow()))
            break;
        
        current = result;
        result = 0;
    }

    // Update our position.
    current = result;
    return current;
}

static int getBPMWidth(int childValue, Length cssUnit)
{
    if (cssUnit.type != Auto)
        return (cssUnit.type == Fixed ? cssUnit.value : childValue);
    return 0;
}

static int getBorderPaddingMargin(RenderObject* child, bool endOfInline)
{
    RenderStyle* cstyle = child->style();
    int result = 0;
    bool leftSide = (cstyle->direction() == LTR) ? !endOfInline : endOfInline;
    result += getBPMWidth((leftSide ? child->marginLeft() : child->marginRight()),
                          (leftSide ? cstyle->marginLeft() :
                                      cstyle->marginRight()));
    result += getBPMWidth((leftSide ? child->paddingLeft() : child->paddingRight()),
                          (leftSide ? cstyle->paddingLeft() :
                                      cstyle->paddingRight()));
    result += leftSide ? child->borderLeft() : child->borderRight();
    return result;
}

static inline void stripTrailingSpace(int& inlineMax, int& inlineMin,
                                      RenderObject* trailingSpaceChild)
{
    if (trailingSpaceChild && trailingSpaceChild->isText()) {
        // Collapse away the trailing space at the end of a block.
        RenderText* t = static_cast<RenderText *>(trailingSpaceChild);
        const Font *f = t->htmlFont( false );
        QChar space[1]; space[0] = ' ';
        int spaceWidth = f->width(space, 1, 0, 0);
        inlineMax -= spaceWidth;
        if (inlineMin > inlineMax)
            inlineMin = inlineMax;
    }
}

void RenderBlock::calcInlineMinMaxWidth()
{
    int inlineMax=0;
    int inlineMin=0;

    int cw = containingBlock()->contentWidth();

    // If we are at the start of a line, we want to ignore all white-space.
    // Also strip spaces if we previously had text that ended in a trailing space.
    bool stripFrontSpaces = true;
    RenderObject* trailingSpaceChild = 0;

    bool autoWrap, oldAutoWrap;
    autoWrap = oldAutoWrap = style()->autoWrap();

    InlineMinMaxIterator childIterator(this, this);
    bool addedTextIndent = false; // Only gets added in once.
    RenderObject* prevFloat = 0;
    while (RenderObject* child = childIterator.next())
    {
        autoWrap = child->style()->autoWrap();

        if (!child->isBR()) {
            // Step One: determine whether or not we need to go ahead and
            // terminate our current line.  Each discrete chunk can become
            // the new min-width, if it is the widest chunk seen so far, and
            // it can also become the max-width.

            // Children fall into three categories:
            // (1) An inline flow object.  These objects always have a min/max of 0,
            // and are included in the iteration solely so that their margins can
            // be added in.
            //
            // (2) An inline non-text non-flow object, e.g., an inline replaced element.
            // These objects can always be on a line by themselves, so in this situation
            // we need to go ahead and break the current line, and then add in our own
            // margins and min/max width on its own line, and then terminate the line.
            //
            // (3) A text object.  Text runs can have breakable characters at the start,
            // the middle or the end.  They may also lose whitespace off the front if
            // we're already ignoring whitespace.  In order to compute accurate min-width
            // information, we need three pieces of information.
            // (a) the min-width of the first non-breakable run.  Should be 0 if the text string
            // starts with whitespace.
            // (b) the min-width of the last non-breakable run. Should be 0 if the text string
            // ends with whitespace.
            // (c) the min/max width of the string (trimmed for whitespace).
            //
            // If the text string starts with whitespace, then we need to go ahead and
            // terminate our current line (unless we're already in a whitespace stripping
            // mode.
            //
            // If the text string has a breakable character in the middle, but didn't start
            // with whitespace, then we add the width of the first non-breakable run and
            // then end the current line.  We then need to use the intermediate min/max width
            // values (if any of them are larger than our current min/max).  We then look at
            // the width of the last non-breakable run and use that to start a new line
            // (unless we end in whitespace).
            RenderStyle* cstyle = child->style();
            int childMin = 0;
            int childMax = 0;

            if (!child->isText()) {
                // Case (1) and (2).  Inline replaced and inline flow elements.
                if (child->isInlineFlow()) {
                    // Add in padding/border/margin from the appropriate side of
                    // the element.
                    int bpm = getBorderPaddingMargin(child, childIterator.endOfInline);
                    childMin += bpm;
                    childMax += bpm;

                    inlineMin += childMin;
                    inlineMax += childMax;
                }
                else {
                    // Inline replaced elts add in their margins to their min/max values.
                    int margins = 0;
                    LengthType type = cstyle->marginLeft().type;
                    if ( type != Auto )
                        margins += (type == Fixed ? cstyle->marginLeft().value : child->marginLeft());
                    type = cstyle->marginRight().type;
                    if ( type != Auto )
                        margins += (type == Fixed ? cstyle->marginRight().value : child->marginRight());
                    childMin += margins;
                    childMax += margins;
                }
            }

            if (!child->isRenderInline() && !child->isText()) {
                // Case (2). Inline replaced elements and floats.
                // Go ahead and terminate the current line as far as
                // minwidth is concerned.
                childMin += child->minWidth();
                childMax += child->maxWidth();

                // FIXME: This isn't right.  WinIE, Opera, Mozilla all do this differently and
                // treat replaced elements like characters in a word.
                if (autoWrap || oldAutoWrap) {
                    if(m_minWidth < inlineMin) m_minWidth = inlineMin;
                    inlineMin = 0;
                }

                // Check our "clear" setting.  If we're supposed to clear the previous float, then
                // go ahead and terminate maxwidth as well.
                if (child->isFloating()) {
                    if (prevFloat &&
                        ((prevFloat->style()->floating() == FLEFT && (child->style()->clear() & CLEFT)) ||
                         (prevFloat->style()->floating() == FRIGHT && (child->style()->clear() & CRIGHT)))) {
                        m_maxWidth = kMax(inlineMax, m_maxWidth);
                        inlineMax = 0;
                    }
                    prevFloat = child;
                }
                
                // Add in text-indent.  This is added in only once.
                int ti = 0;
                if (!addedTextIndent) {
                    addedTextIndent = true;
                    ti = style()->textIndent().minWidth(cw);
                    childMin+=ti;
                    childMax+=ti;
                }
                
                // Add our width to the max.
                inlineMax += childMax;

                if (!autoWrap)
                    inlineMin += childMin;
                else {
                    // Now check our line.
                    inlineMin = childMin;
                    if(m_minWidth < inlineMin) m_minWidth = inlineMin;

                    // Now start a new line.
                    inlineMin = 0;
                }

                // We are no longer stripping whitespace at the start of
                // a line.
                if (!child->isFloating()) {
                    stripFrontSpaces = false;
                    trailingSpaceChild = 0;
                }
            }
            else if (child->isText())
            {
                // Case (3). Text.
                RenderText* t = static_cast<RenderText *>(child);

                // Determine if we have a breakable character.  Pass in
                // whether or not we should ignore any spaces at the front
                // of the string.  If those are going to be stripped out,
                // then they shouldn't be considered in the breakable char
                // check.
                bool hasBreakableChar, hasBreak;
                int beginMin, endMin;
                bool beginWS, endWS;
                int beginMax, endMax;
                t->trimmedMinMaxWidth(inlineMax, beginMin, beginWS, endMin, endWS,
                                      hasBreakableChar, hasBreak, beginMax, endMax,
                                      childMin, childMax, stripFrontSpaces);

                // This text object is insignificant and will not be rendered.  Just
                // continue.
                if (!hasBreak && childMax == 0) continue;
                
                if (stripFrontSpaces)
                    trailingSpaceChild = child;
                else
                    trailingSpaceChild = 0;

                // Add in text-indent.  This is added in only once.
                int ti = 0;
                if (!addedTextIndent) {
                    addedTextIndent = true;
                    ti = style()->textIndent().minWidth(cw);
                    childMin+=ti; beginMin += ti;
                    childMax+=ti; beginMax += ti;
                }
                
                // If we have no breakable characters at all,
                // then this is the easy case. We add ourselves to the current
                // min and max and continue.
                if (!hasBreakableChar) {
                    inlineMin += childMin;
                }
                else {
                    // We have a breakable character.  Now we need to know if
                    // we start and end with whitespace.
                    if (beginWS) {
                        // Go ahead and end the current line.
                        if(m_minWidth < inlineMin) m_minWidth = inlineMin;
                    }
                    else {
                        inlineMin += beginMin;
                        if(m_minWidth < inlineMin) m_minWidth = inlineMin;
                        childMin -= ti;
                    }

                    inlineMin = childMin;

                    if (endWS) {
                        // We end in whitespace, which means we can go ahead
                        // and end our current line.
                        if(m_minWidth < inlineMin) m_minWidth = inlineMin;
                        inlineMin = 0;
                    }
                    else {
                        if(m_minWidth < inlineMin) m_minWidth = inlineMin;
                        inlineMin = endMin;
                    }
                }

                if (hasBreak) {
                    inlineMax += beginMax;
                    if (m_maxWidth < inlineMax) m_maxWidth = inlineMax;
                    if (m_maxWidth < childMax) m_maxWidth = childMax;
                    inlineMax = endMax;
                }
                else
                    inlineMax += childMax;
            }
        }
        else
        {
            if(m_minWidth < inlineMin) m_minWidth = inlineMin;
            if(m_maxWidth < inlineMax) m_maxWidth = inlineMax;
            inlineMin = inlineMax = 0;
            stripFrontSpaces = true;
            trailingSpaceChild = 0;
        }

        oldAutoWrap = autoWrap;
    }

    if (style()->collapseWhiteSpace())
        stripTrailingSpace(inlineMax, inlineMin, trailingSpaceChild);
    
    m_minWidth = kMax(inlineMin, m_minWidth);
    m_maxWidth = kMax(inlineMax, m_maxWidth);
}

// Use a very large value (in effect infinite).
#define BLOCK_MAX_WIDTH 15000

void RenderBlock::calcBlockMinMaxWidth()
{
    bool nowrap = style()->whiteSpace() == NOWRAP;

    RenderObject *child = firstChild();
    int floatLeftWidth = 0, floatRightWidth = 0;
    while (child) {
        // Positioned children don't affect the min/max width
        if (child->isPositioned()) {
            child = child->nextSibling();
            continue;
        }

        if (child->isFloating() || child->avoidsFloats()) {
            int floatTotalWidth = floatLeftWidth + floatRightWidth;
            if (child->style()->clear() & CLEFT) {
                m_maxWidth = kMax(floatTotalWidth, m_maxWidth);
                floatLeftWidth = 0;
            }
            if (child->style()->clear() & CRIGHT) {
                m_maxWidth = kMax(floatTotalWidth, m_maxWidth);
                floatRightWidth = 0;
            }
        }

        Length ml = child->style()->marginLeft();
        Length mr = child->style()->marginRight();

        // Call calcWidth on the child to ensure that our margins are
        // up to date.  This method can be called before the child has actually
        // calculated its margins (which are computed inside calcWidth).
        if (ml.type == Percent || mr.type == Percent)
            calcWidth();

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        // Percentage margins are computed as a percentage of the width we calculated in
        // the calcWidth call above.  In this case we use the actual cached margin values on
        // the RenderObject itself.
        int margin = 0, marginLeft = 0, marginRight = 0;
        if (ml.type == Fixed)
            marginLeft += ml.value;
        else if (ml.type == Percent)
            marginLeft += child->marginLeft();
        if (mr.type == Fixed)
            marginRight += mr.value;
        else if (mr.type == Percent)
            marginRight += child->marginRight();
        margin = marginLeft + marginRight;

        int w = child->minWidth() + margin;
        if (m_minWidth < w) m_minWidth = w;
        
        // IE ignores tables for calculation of nowrap. Makes some sense.
        if (nowrap && !child->isTable() && m_maxWidth < w)
            m_maxWidth = w;

        w = child->maxWidth() + margin;

        if (!child->isFloating()) {
            if (child->avoidsFloats()) {
                // Determine a left and right max value based off whether or not the floats can fit in the
                // margins of the object.  For negative margins, we will attempt to overlap the float if the negative margin
                // is smaller than the float width.
                int maxLeft = marginLeft > 0 ? kMax(floatLeftWidth, marginLeft) : floatLeftWidth + marginLeft;
                int maxRight = marginRight > 0 ? kMax(floatRightWidth, marginRight) : floatRightWidth + marginRight;
                w = child->maxWidth() + maxLeft + maxRight;
                w = kMax(w, floatLeftWidth + floatRightWidth);
            }
            else
                m_maxWidth = kMax(floatLeftWidth + floatRightWidth, m_maxWidth);
            floatLeftWidth = floatRightWidth = 0;
        }
        
        if (child->isFloating()) {
            if (style()->floating() == FLEFT)
                floatLeftWidth += w;
            else
                floatRightWidth += w;
        }
        else if (m_maxWidth < w)
            m_maxWidth = w;

        // A very specific WinIE quirk.
        // Example:
        /*
           <div style="position:absolute; width:100px; top:50px;">
              <div style="position:absolute;left:0px;top:50px;height:50px;background-color:green">
                <table style="width:100%"><tr><td></table>
              </div>
           </div>
        */
        // In the above example, the inner absolute positioned block should have a computed width
        // of 100px because of the table.
        // We can achieve this effect by making the maxwidth of blocks that contain tables
        // with percentage widths be infinite (as long as they are not inside a table cell).
        if (style()->htmlHacks() && child->style()->width().type == Percent &&
            !isTableCell() && child->isTable() && m_maxWidth < BLOCK_MAX_WIDTH) {
            RenderBlock* cb = containingBlock();
            while (!cb->isCanvas() && !cb->isTableCell())
                cb = cb->containingBlock();
            if (!cb->isTableCell())
                m_maxWidth = BLOCK_MAX_WIDTH;
        }
        
        child = child->nextSibling();
    }

    // Always make sure these values are non-negative.
    m_minWidth = kMax(0, m_minWidth);
    m_maxWidth = kMax(0, m_maxWidth);

    m_maxWidth = kMax(floatLeftWidth + floatRightWidth, m_maxWidth);
}

short RenderBlock::lineHeight(bool b, bool isRootLineBox) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplaced() && !isRootLineBox)
        return height()+marginTop()+marginBottom();
    return RenderFlow::lineHeight(b, isRootLineBox);
}

short RenderBlock::baselinePosition(bool b, bool isRootLineBox) const
{
    // Inline blocks are replaced elements. Otherwise, just pass off to
    // the base class.  If we're being queried as though we're the root line
    // box, then the fact that we're an inline-block is irrelevant, and we behave
    // just like a block.
    if (isReplaced() && !isRootLineBox) {
        // For "leaf" theme objects, let the theme decide what the baseline position is.
        // FIXME: Might be better to have a custom CSS property instead, so that if the theme
        // is turned off, checkboxes/radios will still have decent baselines.
        if (style()->hasAppearance() && !theme()->isControlContainer(style()->appearance()))
            return theme()->baselinePosition(this);
            
        // CSS2.1 states that the baseline of an inline block is the baseline of the last line box in
        // the normal flow.  We make an exception for marquees, since their baselines are meaningless
        // (the content inside them moves).  This matches WinIE as well, which just bottom-aligns them.
        // We also give up on finding a baseline if we have a vertical scrollbar, or if we are scrolled
        // vertically (e.g., an overflow:hidden block that has had scrollTop moved) or if the baseline is outside
        // of our content box.
        int baselinePos = (m_layer && (m_layer->marquee() || m_layer->verticalScrollbar() || m_layer->scrollYOffset() != 0)) ? -1 : getBaselineOfLastLineBox();
        if (baselinePos != -1 && baselinePos <= borderTop() + paddingTop() + contentHeight())
            return marginTop() + baselinePos;
        return height() + marginTop() + marginBottom();
    }
    return RenderFlow::baselinePosition(b, isRootLineBox);
}

int RenderBlock::getBaselineOfFirstLineBox() const
{
    if (!isBlockFlow())
        return RenderFlow::getBaselineOfFirstLineBox();

    if (childrenInline()) {
        if (m_firstLineBox)
            return m_firstLineBox->yPos() + m_firstLineBox->baseline();
        else
            return -1;
    }
    else {
        for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
            if (!curr->isFloatingOrPositioned()) {
                int result = curr->getBaselineOfFirstLineBox();
                if (result != -1)
                    return curr->yPos() + result; // Translate to our coordinate space.
            }
        }
    }

    return -1;
}

int RenderBlock::getBaselineOfLastLineBox() const
{
    if (!isBlockFlow())
        return RenderFlow::getBaselineOfLastLineBox();

    if (childrenInline()) {
        if (m_lastLineBox)
            return m_lastLineBox->yPos() + m_lastLineBox->baseline();
        else
            return -1;
    }
    else {
        for (RenderObject* curr = lastChild(); curr; curr = curr->previousSibling()) {
            if (!curr->isFloatingOrPositioned()) {
                int result = curr->getBaselineOfLastLineBox();
                if (result != -1)
                    return curr->yPos() + result; // Translate to our coordinate space.
            }
        }
    }

    return -1;
}

RenderBlock* RenderBlock::firstLineBlock() const
{
    const RenderObject* firstLineBlock = this;
    bool hasPseudo = false;
    while (true) {
        hasPseudo = firstLineBlock->style()->hasPseudoStyle(RenderStyle::FIRST_LINE);
        if (hasPseudo)
            break;
        RenderObject* parentBlock = firstLineBlock->parent();
        if (firstLineBlock->isReplaced() || firstLineBlock->isFloating() || 
            !parentBlock || parentBlock->firstChild() != firstLineBlock || !parentBlock->isBlockFlow())
            break;
        firstLineBlock = parentBlock;
    } 
    
    if (!hasPseudo)
        return 0;
    
    return (RenderBlock*)(firstLineBlock);
}

void RenderBlock::updateFirstLetter()
{    
    // FIXME: We need to destroy the first-letter object if it is no longer the first child.  Need to find
    // an efficient way to check for that situation though before implementing anything.
    RenderObject* firstLetterBlock = this;
    bool hasPseudoStyle = false;
    while (true) {
        hasPseudoStyle = firstLetterBlock->style()->hasPseudoStyle(RenderStyle::FIRST_LETTER);
        if (hasPseudoStyle)
            break;
        RenderObject* parentBlock = firstLetterBlock->parent();
        if (firstLetterBlock->isReplaced() || !parentBlock || parentBlock->firstChild() != firstLetterBlock || 
            !parentBlock->isBlockFlow())
            break;
        firstLetterBlock = parentBlock;
    } 

    if (!hasPseudoStyle) 
        return;
    
    // Drill into inlines looking for our first text child.
    RenderObject* currChild = firstLetterBlock->firstChild();
    while (currChild && currChild->needsLayout() && !currChild->isReplaced() && !currChild->isText()) 
        currChild = currChild->firstChild();

    // Get list markers out of the way.
    while (currChild && currChild->isListMarker())
        currChild = currChild->nextSibling();
    
    if (!currChild)
        return;
    
    RenderObject* firstLetterContainer = currChild->parent();

    // If the child already has style, then it has already been created, so we just want
    // to update it.
    if (currChild->style()->styleType() == RenderStyle::FIRST_LETTER) {
        RenderStyle* pseudo = firstLetterBlock->getPseudoStyle(RenderStyle::FIRST_LETTER,
                                                               firstLetterContainer->firstLineStyle());
        currChild->setStyle(pseudo);
        for (RenderObject* genChild = currChild->firstChild(); genChild; genChild = genChild->nextSibling()) {
            if (genChild->isText()) 
                genChild->setStyle(pseudo);
        }
        return;
    }

    // If the child does not already have style, we create it here.
    if (currChild->isText() && !currChild->isBR() && 
        currChild->parent()->style()->styleType() != RenderStyle::FIRST_LETTER) {
        
        RenderText* textObj = static_cast<RenderText*>(currChild);
        
        // Create our pseudo style now that we have our firstLetterContainer determined.
        RenderStyle* pseudoStyle = firstLetterBlock->getPseudoStyle(RenderStyle::FIRST_LETTER,
                                                                    firstLetterContainer->firstLineStyle());
        
        // Force inline display (except for floating first-letters)
        pseudoStyle->setDisplay( pseudoStyle->isFloating() ? BLOCK : INLINE);
        pseudoStyle->setPosition( STATIC ); // CSS2 says first-letter can't be positioned.
        
        RenderObject* firstLetter = RenderFlow::createAnonymousFlow(document(), pseudoStyle); // anonymous box
        // FIXME: This adds in the wrong place if list markers were skipped above.  Should be
        // firstLetterContainer->addChild(firstLetter, currChild);
        firstLetterContainer->addChild(firstLetter, firstLetterContainer->firstChild());
        
        // The original string is going to be either a generated content string or a DOM node's
        // string.  We want the original string before it got transformed in case first-letter has
        // no text-transform or a different text-transform applied to it.
        RefPtr<DOMStringImpl> oldText = textObj->originalString();
        KHTMLAssert(oldText);
        
        if (oldText && oldText->l > 0) {
            unsigned int length = 0;
            
            // account for leading spaces and punctuation
            while ( length < oldText->l && ( (oldText->s+length)->isSpace() || (oldText->s+length)->isPunct() ) )
                length++;
            
            // account for first letter
            length++;
            
            // construct text fragment for the text after the first letter
            // NOTE: this might empty
            RenderTextFragment* remainingText = 
                new (renderArena()) RenderTextFragment(textObj->node(), oldText.get(), length, oldText->l-length);
            remainingText->setStyle(textObj->style());
            if (remainingText->element())
                remainingText->element()->setRenderer(remainingText);
            
            RenderObject* nextObj = textObj->nextSibling();
            firstLetterContainer->removeChild(textObj);
            firstLetterContainer->addChild(remainingText, nextObj);
            
            // construct text fragment for the first letter
            RenderTextFragment* letter = 
                new (renderArena()) RenderTextFragment(remainingText->node(), oldText.get(), 0, length);
            RenderStyle* newStyle = new (renderArena()) RenderStyle();
            newStyle->inheritFrom(pseudoStyle);
            letter->setStyle(newStyle);
            firstLetter->addChild(letter);

            textObj->destroy();
        }
    }
}

bool RenderBlock::inRootBlockContext() const
{
    if (isTableCell() || isFloatingOrPositioned() || hasOverflowClip())
        return false;
    
    if (isRoot() || isCanvas())
        return true;
    
    return containingBlock()->inRootBlockContext();
}

// Helper methods for obtaining the last line, computing line counts and heights for line counts
// (crawling into blocks).
static bool shouldCheckLines(RenderObject* obj)
{
    return !obj->isFloatingOrPositioned() && !obj->isCompact() && !obj->isRunIn() &&
            obj->isBlockFlow() && obj->style()->height().isAuto() &&
            (!obj->isFlexibleBox() || obj->style()->boxOrient() == VERTICAL);
}

static RootInlineBox* getLineAtIndex(RenderBlock* block, int i, int& count)
{
    if (block->style()->visibility() == VISIBLE) {
        if (block->childrenInline()) {
            for (RootInlineBox* box = block->firstRootBox(); box; box = box->nextRootBox()) {
                if (count++ == i)
                    return box;
            }
        }
        else {
            for (RenderObject* obj = block->firstChild(); obj; obj = obj->nextSibling()) {
                if (shouldCheckLines(obj)) {
                    RootInlineBox *box = getLineAtIndex(static_cast<RenderBlock*>(obj), i, count);
                    if (box)
                        return box;
                }
            }
        }
    }
    return 0;
}

int getHeightForLineCount(RenderBlock* block, int l, bool includeBottom, int& count)
{
    if (block->style()->visibility() == VISIBLE) {
        if (block->childrenInline()) {
            for (RootInlineBox* box = block->firstRootBox(); box; box = box->nextRootBox()) {
                if (++count == l)
                    return box->bottomOverflow() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : 0);
            }
        }
        else {
            RenderObject* normalFlowChildWithoutLines = 0;
            for (RenderObject* obj = block->firstChild(); obj; obj = obj->nextSibling()) {
                if (shouldCheckLines(obj)) {
                    int result = getHeightForLineCount(static_cast<RenderBlock*>(obj), l, false, count);
                    if (result != -1)
                        return result + obj->yPos() + (includeBottom ? (block->borderBottom() + block->paddingBottom()) : 0);
                }
                else if (!obj->isFloatingOrPositioned() && !obj->isCompact() && !obj->isRunIn())
                    normalFlowChildWithoutLines = obj;
            }
            if (normalFlowChildWithoutLines && l == 0)
                return normalFlowChildWithoutLines->yPos() + normalFlowChildWithoutLines->height();
        }
    }
    
    return -1;
}

RootInlineBox* RenderBlock::lineAtIndex(int i)
{
    int count = 0;
    return getLineAtIndex(this, i, count);
}

int RenderBlock::lineCount()
{
    int count = 0;
    if (style()->visibility() == VISIBLE) {
        if (childrenInline())
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox())
                count++;
        else
            for (RenderObject* obj = firstChild(); obj; obj = obj->nextSibling())
                if (shouldCheckLines(obj))
                    count += static_cast<RenderBlock*>(obj)->lineCount();
    }
    return count;
}

int RenderBlock::heightForLineCount(int l)
{
    int count = 0;
    return getHeightForLineCount(this, l, true, count);
}

void RenderBlock::clearTruncation()
{
    if (style()->visibility() == VISIBLE) {
        if (childrenInline() && hasMarkupTruncation()) {
            setHasMarkupTruncation(false);
            for (RootInlineBox* box = firstRootBox(); box; box = box->nextRootBox())
                box->clearTruncation();
        }
        else
            for (RenderObject* obj = firstChild(); obj; obj = obj->nextSibling())
                if (shouldCheckLines(obj))
                    static_cast<RenderBlock*>(obj)->clearTruncation();
    }
}

const char *RenderBlock::renderName() const
{
    if (isBody())
        return "RenderBody"; // FIXME: Temporary hack until we know that the regression tests pass.
    
    if (isFloating())
        return "RenderBlock (floating)";
    if (isPositioned())
        return "RenderBlock (positioned)";
    if (isAnonymousBlock())
        return "RenderBlock (anonymous)";
    else if (isAnonymous())
        return "RenderBlock (generated)";
    if (isRelPositioned())
        return "RenderBlock (relative positioned)";
    if (isCompact())
        return "RenderBlock (compact)";
    if (isRunIn())
        return "RenderBlock (run-in)";
    return "RenderBlock";
}

#ifndef NDEBUG

void RenderBlock::dump(QTextStream *stream, QString ind) const
{
    if (m_childrenInline) { *stream << " childrenInline"; }
    if (m_firstLine) { *stream << " firstLine"; }

    if (m_floatingObjects && !m_floatingObjects->isEmpty())
    {
        *stream << " special(";
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        FloatingObject *r;
        bool first = true;
        for ( ; (r = it.current()); ++it )
        {
            if (!first)
                *stream << ",";
            *stream << r->node->renderName();
            first = false;
        }
        *stream << ")";
    }

    // ### EClear m_clearStatus

    RenderFlow::dump(stream,ind);
}

#endif

} // namespace WebCore

