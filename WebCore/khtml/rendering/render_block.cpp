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

//#define DEBUG
//#define DEBUG_LAYOUT
//#define BOX_DEBUG
//#define FLOAT_DEBUG

#include <kdebug.h>
#include "rendering/render_text.h"
#include "rendering/render_table.h"
#include "rendering/render_canvas.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "html/html_formimpl.h"
#include "render_block.h"

#include "khtmlview.h"
#include "htmltags.h"

using namespace DOM;

namespace khtml {

RenderBlock::RenderBlock(DOM::NodeImpl* node)
:RenderFlow(node)
{
    m_childrenInline = true;
    m_floatingObjects = 0;
    m_positionedObjects = 0;
    m_pre = false;
    m_firstLine = false;
    m_clearStatus = CNONE;
    m_maxTopPosMargin = m_maxTopNegMargin = m_maxBottomPosMargin = m_maxBottomNegMargin = 0;
    m_topMarginQuirk = m_bottomMarginQuirk = false;
    m_overflowHeight = 0;
    m_overflowWidth = 0;
}

RenderBlock::~RenderBlock()
{
    delete m_floatingObjects;
    delete m_positionedObjects;
}

void RenderBlock::setStyle(RenderStyle* _style)
{
    RenderFlow::setStyle(_style);
    setReplaced(style()->display()==INLINE_BLOCK);
    
    m_pre = false;
    if (_style->whiteSpace() == PRE)
        m_pre = true;

    // ### we could save this call when the change only affected
    // non inherited properties
    RenderObject *child = firstChild();
    while (child != 0)
    {
        if (child->isAnonymous() && child->style()->styleType() == RenderStyle::NOPSEUDO && !child->isListMarker())
        {
            RenderStyle* newStyle = new RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);
            child->setStyle(newStyle);
        }
        child = child->nextSibling();
    }

    // Update pseudos for :before and :after now.
    updatePseudoChild(RenderStyle::BEFORE, firstChild());
    updatePseudoChild(RenderStyle::AFTER, lastChild());
}

void RenderBlock::addChildToFlow(RenderObject* newChild, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild && lastChild() && lastChild()->style()->styleType() == RenderStyle::AFTER)
        beforeChild = lastChild();
    
    bool madeBoxesNonInline = FALSE;

    // If the requested beforeChild is not one of our children, then this is most likely because
    // there is an anonymous block box within this object that contains the beforeChild. So
    // just insert the child into the anonymous block box instead of here.
    if (beforeChild && beforeChild->parent() != this) {

        KHTMLAssert(beforeChild->parent());
        KHTMLAssert(beforeChild->parent()->isAnonymous());

        if (newChild->isInline()) {
            beforeChild->parent()->addChild(newChild,beforeChild);
            return;
        }
        else if (beforeChild->parent()->firstChild() != beforeChild)
            return beforeChild->parent()->addChild(newChild, beforeChild);
        else
            return addChildToFlow(newChild, beforeChild->parent());
    }

    // prevent elements that haven't received a layout yet from getting painted by pushing
    // them far above the top of the page
    if (!newChild->isInline())
        newChild->setPos(newChild->xPos(), -500000);

    if (!newChild->isText() && newChild->style()->position() != STATIC)
        setOverhangingContents();

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
            KHTMLAssert(beforeChild->isAnonymous());
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
                if (beforeChild->previousSibling() && beforeChild->previousSibling()->isAnonymous()) {
                    beforeChild->previousSibling()->addChild(newChild);
                    return;
                }
            }
            else {
                if (m_last && m_last->isAnonymous()) {
                    m_last->addChild(newChild);
                    return;
                }
            }

            // no suitable existing anonymous box - create a new one
            RenderBlock* newBox = createAnonymousBlock();
            RenderBox::addChild(newBox,beforeChild);
            newBox->addChild(newChild);
            newBox->setPos(newBox->xPos(), -500000);
            return;
        }
    }

    RenderBox::addChild(newChild,beforeChild);
    // ### care about aligned stuff

    if ( madeBoxesNonInline )
        removeLeftoverAnonymousBoxes();
}

static void getInlineRun(RenderObject* start, RenderObject* stop,
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
    // |stop| indicates a non-inclusive stop point.  Regardless of whether |stop|
    // is inline or not, we will not include it.  It's as though we encountered
    // a non-inline.
    inlineRunStart = inlineRunEnd = 0;

    // Start by skipping as many non-inlines as we can.
    RenderObject * curr = start;
    while (curr && !curr->isInline())
        curr = curr->nextSibling();

    if (!curr)
        return; // No more inline children to be found.

    inlineRunStart = inlineRunEnd = curr;

    curr = curr->nextSibling();
    while (curr && curr->isInline() && (curr != stop)) {
        inlineRunEnd = curr;
        curr = curr->nextSibling();
    }
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
        box->close();
        box->setPos(box->xPos(), -500000);
    }
}

void RenderBlock::removeChild(RenderObject *oldChild)
{
    // If this child is a block, and if our previous and next siblings are
    // both anonymous blocks with inline content, then we can go ahead and
    // fold the inline content back together.
    RenderObject* prev = oldChild->previousSibling();
    RenderObject* next = oldChild->nextSibling();
    bool mergedBlocks = false;
    if (document()->renderer() && !isInline() && !oldChild->isInline() && !oldChild->continuation() &&
        prev && prev->isAnonymous() && prev->childrenInline() &&
        next && next->isAnonymous() && next->childrenInline()) {
        // Take all the children out of the |next| block and put them in
        // the |prev| block.
        RenderObject* o = next->firstChild();
        while (o) {
            RenderObject* no = o;
            o = no->nextSibling();
            prev->appendChildNode(next->removeChildNode(no));
            no->setNeedsLayoutAndMinMaxRecalc();
        }
        prev->setNeedsLayoutAndMinMaxRecalc();

        // Nuke the now-empty block.
        next->detach();

        mergedBlocks = true;
    }

    RenderFlow::removeChild(oldChild);

    if (mergedBlocks && prev && !prev->previousSibling() && !prev->nextSibling()) {
        // The remerge has knocked us down to containing only a single anonymous
        // box.  We can go ahead and pull the content right back up into our
        // box.
        RenderObject* anonBlock = removeChildNode(prev);
        m_childrenInline = true;
        RenderObject* o = anonBlock->firstChild();
        while (o) {
            RenderObject* no = o;
            o = no->nextSibling();
            appendChildNode(anonBlock->removeChildNode(no));
            no->setNeedsLayoutAndMinMaxRecalc();
        }
        
        // Nuke the now-empty block.
        anonBlock->detach();
    }
}

bool RenderBlock::requiresLayer() {
    // FIXME: The bogus table cell check is only here until we figure out how to position
    // table cells properly when they have layers.
    // Note that we also restrict overflow to blocks for now.
    return !isTableCell() && (RenderObject::requiresLayer() || (style()->hidesOverflow() && !isBody()));
}

bool RenderBlock::isSelfCollapsingBlock() const
{
    // We are not self-collapsing if we
    // (a) have a non-zero height according to layout (an optimization to avoid wasting time)
    // (b) are a table,
    // (c) have border/padding,
    // (d) have a min-height
    if (m_height > 0 ||
        isTable() || (borderBottom() + paddingBottom() + borderTop() + paddingTop()) != 0 ||
        style()->minHeight().value > 0)
        return false;

    // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
    // on whether we have content that is all self-collapsing or not.
    if (style()->height().isVariable() ||
        (style()->height().isFixed() && style()->height().value == 0)) {
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
    //    kdDebug( 6040 ) << renderName() << " " << this << "::layoutBlock() start" << endl;
    //     QTime t;
    //     t.start();
    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

    if (isInline() && !isInlineBlockOrInlineTable()) // Inline <form>s inside various table elements can
        return;	    		                     // cause us to come in here.  Just bail. -dwh

    if (!relayoutChildren && posChildNeedsLayout() && !normalChildNeedsLayout() && !selfNeedsLayout()) {
        // All we have to is lay out our positioned objects.
        layoutPositionedObjects(relayoutChildren);
        setNeedsLayout(false);
        return;
    }
    
#ifdef INCREMENTAL_REPAINTING
    QRect oldBounds, oldFullBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        getAbsoluteRepaintRectIncludingFloats(oldBounds, oldFullBounds);
#endif

    int oldWidth = m_width;
    
    calcWidth();
    m_overflowWidth = m_width;

    if ( oldWidth != m_width )
        relayoutChildren = true;

    //     kdDebug( 6040 ) << floatingObjects << "," << oldWidth << ","
    //                     << m_width << ","<< needsLayout() << "," << isAnonymous() << ","
    //                     << overhangingContents() << "," << isPositioned() << endl;

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderBlock) " << this << " ::layout() width=" << m_width << ", needsLayout=" << needsLayout() << endl;
    if(containingBlock() == static_cast<RenderObject *>(this))
        kdDebug( 6040 ) << renderName() << ": containingBlock == this" << endl;
#endif

    clearFloats();

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

        if (element() && element()->id() == ID_FORM && element()->isMalformed())
            // See if this form is malformed (i.e., unclosed). If so, don't give the form
            // a bottom margin.
            m_maxBottomPosMargin = m_maxBottomNegMargin = 0;
    }

    if (style()->scrollsOverflow() && m_layer) {
        // For overflow:scroll blocks, ensure we have both scrollbars in place always.
        if (style()->overflow() == OSCROLL) {
            m_layer->setHasHorizontalScrollbar(true);
            m_layer->setHasVerticalScrollbar(true);
        }
        
        // Move the scrollbars aside during layout.  The layer will move them back when it
        // does painting or event handling.
        m_layer->moveScrollbarsAside();
    }
    
    //    kdDebug( 6040 ) << "childrenInline()=" << childrenInline() << endl;
    if (childrenInline())
        layoutInlineChildren( relayoutChildren );
    else
        layoutBlockChildren( relayoutChildren );

    // Expand our intrinsic height to encompass floats.
    int toAdd = borderBottom() + paddingBottom();
    if (style()->hidesOverflow() && m_layer)
        toAdd += m_layer->horizontalScrollbarHeight();
    if ( hasOverhangingFloats() && (isInlineBlockOrInlineTable() || isFloatingOrPositioned() || style()->hidesOverflow() ||
                                    (parent() && parent()->isFlexibleBox())) )
        m_height = floatBottom() + toAdd;
           
    int oldHeight = m_height;
    calcHeight();
    if (oldHeight != m_height) {
        relayoutChildren = true;

        // If the block got expanded in size, then increase our overflowheight to match.
        if (m_overflowHeight > m_height)
            m_overflowHeight -= paddingBottom() + borderBottom();
        if (m_overflowHeight < m_height)
            m_overflowHeight = m_height;
    }

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

        if (m_overflowHeight > m_height && !style()->hidesOverflow())
            m_height = m_overflowHeight + borderBottom() + paddingBottom();
    }

    if( hasOverhangingFloats() && (isFloating() || isTableCell())) {
        m_height = floatBottom();
        m_height += borderBottom() + paddingBottom();
    }

    layoutPositionedObjects( relayoutChildren );

    //kdDebug() << renderName() << " layout width=" << m_width << " height=" << m_height << endl;

    // Always ensure our overflow width/height are at least as large as our width/height.
    if (m_overflowWidth < m_width)
        m_overflowWidth = m_width;
    if (m_overflowHeight < m_height)
        m_overflowHeight = m_height;
    
    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (style()->hidesOverflow() && m_layer)
        m_layer->updateScrollInfoAfterLayout();

#ifdef INCREMENTAL_REPAINTING
    // Repaint with our new bounds if they are different from our old bounds.
    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldFullBounds);
#endif

    setNeedsLayout(false);
}

void RenderBlock::layoutBlockChildren( bool relayoutChildren )
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << " layoutBlockChildren( " << this <<" ), relayoutChildren="<< relayoutChildren << endl;
#endif

    int xPos = borderLeft() + paddingLeft();
    if( style()->direction() == RTL )
        xPos = m_width - paddingRight() - borderRight();

    int toAdd = borderBottom() + paddingBottom();
    if (style()->hidesOverflow() && m_layer)
        toAdd += m_layer->horizontalScrollbarHeight();
    
    m_height = borderTop() + paddingTop();

    // Fieldsets need to find their legend and position it inside the border of the object.
    // The legend then gets skipped during normal layout.
    RenderObject* legend = layoutLegend(relayoutChildren);
    
    int minHeight = m_height + toAdd;
    m_overflowHeight = m_height;

    RenderObject *child = firstChild();
    RenderBlock *prevFlow = 0;

    // A compact child that needs to be collapsed into the margin of the following block.
    RenderObject* compactChild = 0;
    // The block with the open margin that the compact child is going to place itself within.
    RenderObject* blockForCompactChild = 0;
    // For compact children that don't fit, we lay them out as though they are blocks.  This
    // boolean allows us to temporarily treat a compact like a block and lets us know we need
    // to turn the block back into a compact when we're done laying out.
    bool treatCompactAsBlock = false;
    
    // Whether or not we can collapse our own margins with our children.  We don't do this
    // if we had any border/padding (obviously), if we're the root or HTML elements, or if
    // we're positioned, floating, a table cell.
    // For now we only worry about the top border/padding.  We will update the variable's
    // value when it comes time to check against the bottom border/padding.
    bool canCollapseWithChildren = !isCanvas() && !isRoot() && !isPositioned() &&
        !isFloating() && !isTableCell() && !style()->hidesOverflow() && !isInlineBlockOrInlineTable();
    bool canCollapseTopWithChildren = canCollapseWithChildren && (m_height == 0);

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins.  To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it.  We also don't collapse if we had any bottom border/padding (represented by
    // |toAdd|).
    bool canCollapseBottomWithChildren = canCollapseWithChildren && (toAdd == 0) &&
        (style()->height().isVariable() && style()->height().value == 0);
    
    // Whether or not we are a quirky container, i.e., do we collapse away top and bottom
    // margins in our container.
    bool quirkContainer = isTableCell() || isBody();

    // Sometimes an element will be shoved down away from a previous sibling, e.g., when
    // clearing to pass beyond a float.  In this case, you don't need to collapse.  This
    // boolean is updated with each iteration through our child list to reflect whether
    // that particular child should be collapsed with its previous sibling (or with the top
    // of the block).
    bool shouldCollapseChild = true;

    // This flag tracks whether the child should collapse with the top margins of the block.
    // It can remain set through multiple iterations as long as we keep encountering
    // self-collapsing blocks.
    bool topMarginContributor = true;

    // These flags track the previous maximal positive and negative margins.
    int prevPosMargin = canCollapseTopWithChildren ? maxTopMargin(true) : 0;
    int prevNegMargin = canCollapseTopWithChildren ? maxTopMargin(false) : 0;

    // Whether or not we encountered an element with clear set that actually had to
    // be pushed down below a float.
    bool clearOccurred = false;

    // If our last normal flow child was a self-collapsing block that cleared a float,
    // we track it in this variable.
    bool selfCollapsingBlockClearedFloat = false;
    
    bool topChildQuirk = false;
    bool bottomChildQuirk = false;
    bool determinedTopQuirk = false;

    bool strictMode = !style()->htmlHacks();

    //kdDebug() << "RenderBlock::layoutBlockChildren " << prevMargin << endl;

    //     QTime t;
    //     t.start();

    while( child != 0 )
    {
        int oldTopPosMargin = m_maxTopPosMargin;
        int oldTopNegMargin = m_maxTopNegMargin;

        if (legend == child) {
            child = child->nextSibling();
            continue; // Skip the legend, since it has already been positioned up in the fieldset's border.
        }
        
        // make sure we relayout children if we need it.
        if (relayoutChildren || floatBottom() > m_y ||
            (child->isReplaced() && (child->style()->width().isPercent() || child->style()->height().isPercent())) ||
            (child->isRenderBlock() && child->style()->height().isPercent()))
            child->setChildNeedsLayout(true);

        //         kdDebug( 6040 ) << "   " << child->renderName() << " loop " << child << ", " << child->isInline() << ", " << child->needsLayout() << endl;
        //         kdDebug( 6040 ) << t.elapsed() << endl;
        // ### might be some layouts are done two times... FIX that.

        if (child->isPositioned())
        {
            child->containingBlock()->insertPositionedObject(child);
            if (child->hasStaticX()) {
                if (style()->direction() == LTR)
                    child->setStaticX(xPos);
                else
                    child->setStaticX(borderRight()+paddingRight());
            }
            if (child->hasStaticY()) {
                int yPosEstimate = m_height + (!topMarginContributor ? (prevPosMargin - prevNegMargin) : 0);
                child->setStaticY(yPosEstimate);
            }
            child = child->nextSibling();
            continue;
        } else if (child->isReplaced())
            child->layoutIfNeeded();
        
        if ( child->isFloating() ) {
            insertFloatingObject( child );

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
            // for by simply checking the boolean |topMarginContributor| variable.  See
            // http://www.hixie.ch/tests/adhoc/css/box/block/margin-collapse/046.html for
            // an example of this scenario.
            int marginOffset = !topMarginContributor ? (prevPosMargin - prevNegMargin) : 0;
            
            m_height += marginOffset;
            positionNewFloats();
            m_height -= marginOffset;

            //kdDebug() << "RenderBlock::layoutBlockChildren inserting float at "<< m_height <<" prevMargin="<<prevMargin << endl;
            child = child->nextSibling();
            continue;
        }

        // See if we have a compact element.  If we do, then try to tuck the compact
        // element into the margin space of the next block.
        // FIXME: We only deal with one compact at a time.  It is unclear what should be
        // done if multiple contiguous compacts are encountered.  For now we assume that
        // compact A followed by another compact B should simply be treated as block A.
        if (child->isCompact() && !compactChild && (child->childrenInline() || child->isReplaced())) {
            // Get the next non-positioned/non-floating RenderBlock.
            RenderObject* next = child->nextSibling();
            RenderObject* curr = next;
            while (curr && curr->isFloatingOrPositioned())
                curr = curr->nextSibling();
            if (curr && curr->isRenderBlock() && !curr->isCompact() && !curr->isRunIn()) {
                curr->calcWidth(); // So that horizontal margins are correct.
                // Need to compute margins for the child as though it is a block.
                child->style()->setDisplay(BLOCK);
                child->calcWidth();
                child->style()->setDisplay(COMPACT);
                int childMargins = child->marginLeft() + child->marginRight();
                int margin = style()->direction() == LTR ? curr->marginLeft() : curr->marginRight();
                if (margin < (childMargins + child->maxWidth())) {
                    // It won't fit. Kill the "compact" boolean and just treat
                    // the child like a normal block. This is only temporary.
                    child->style()->setDisplay(BLOCK);
                    treatCompactAsBlock = true;
                }
                else {
                    blockForCompactChild = curr;
                    compactChild = child;
                    child->setInline(true);
                    child->setPos(0,0); // This position will be updated to reflect the compact's
                                        // desired position and the line box for the compact will
                                        // pick that position up.

                    // Remove the child.
                    RenderObject* next = child->nextSibling();
                    removeChildNode(child);

                    // Now insert the child under |curr|.
                    curr->insertChildNode(child, curr->firstChild());
                    child = next;
                    continue;
                }
            }
        }

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
                child->setInline(true);
                child->setPos(0,0);

                // Remove the child.
                RenderObject* next = child->nextSibling();
                removeChildNode(child);

                // Now insert the child under |curr|.
                curr->insertChildNode(child, curr->firstChild());
                child = next;
                continue;
            }
        }
        
        // Note this occurs after the test for positioning and floating above, since
        // we want to ensure that we don't artificially increase our height because of
        // a positioned or floating child.
        if (child->avoidsFloats() && style()->width().isFixed() && child->minWidth() > lineWidth(m_height)) {
            m_height = kMax(m_height, floatBottom());
            shouldCollapseChild = false;
            clearOccurred = true;
        }

        // take care in case we inherited floats
        if (child && floatBottom() > m_height)
            child->setChildNeedsLayout(true);
        
        child->calcVerticalMargins();

        //kdDebug(0) << "margin = " << margin << " yPos = " << m_height << endl;

#ifdef INCREMENTAL_REPAINTING
        int oldChildX = child->xPos();
        int oldChildY = child->yPos();
#endif
        
        // Try to guess our correct y position.  In most cases this guess will
        // be correct.  Only if we're wrong (when we compute the real y position)
        // will we have to relayout.
        int yPosEstimate = m_height;
        if (prevFlow)
        {
            yPosEstimate += QMAX(prevFlow->collapsedMarginBottom(), child->marginTop());
            if (prevFlow->yPos()+prevFlow->floatBottom() > yPosEstimate)
                child->setChildNeedsLayout(true);
            else
                prevFlow=0;
        }
        else if (!canCollapseTopWithChildren || !topMarginContributor)
            yPosEstimate += child->marginTop();

        // Go ahead and position the child as though it didn't collapse with the top.
        child->setPos(child->xPos(), yPosEstimate);
        child->layoutIfNeeded();

        // Now determine the correct ypos based off examination of collapsing margin
        // values.
        if (shouldCollapseChild) {
            // Get our max pos and neg top margins.
            int posTop = child->maxTopMargin(true);
            int negTop = child->maxTopMargin(false);

            // For self-collapsing blocks, collapse our bottom margins into our
            // top to get new posTop and negTop values.
            if (child->isSelfCollapsingBlock()) {
                if (child->maxBottomMargin(true) > posTop)
                    posTop = child->maxBottomMargin(true);
                if (child->maxBottomMargin(false) > negTop)
                    negTop = child->maxBottomMargin(false);
            }
            
            // See if the top margin is quirky. We only care if this child has
            // margins that will collapse with us.
            bool topQuirk = child->isTopMarginQuirk();

            if (canCollapseTopWithChildren && topMarginContributor && !clearOccurred) {
                // This child is collapsing with the top of the
                // block.  If it has larger margin values, then we need to update
                // our own maximal values.
                if (strictMode || !quirkContainer || !topQuirk) {
                    if (posTop > m_maxTopPosMargin)
                        m_maxTopPosMargin = posTop;

                    if (negTop > m_maxTopNegMargin)
                        m_maxTopNegMargin = negTop;
                }

                // The minute any of the margins involved isn't a quirk, don't
                // collapse it away, even if the margin is smaller (www.webreference.com
                // has an example of this, a <dt> with 0.8em author-specified inside
                // a <dl> inside a <td>.
                if (!determinedTopQuirk && !topQuirk && (posTop-negTop)) {
                    m_topMarginQuirk = false;
                    determinedTopQuirk = true;
                }

                if (!determinedTopQuirk && topQuirk && marginTop() == 0)
                    // We have no top margin and our top child has a quirky margin.
                    // We will pick up this quirky margin and pass it through.
                    // This deals with the <td><div><p> case.
                    // Don't do this for a block that split two inlines though.  You do
                    // still apply margins in this case.
                    m_topMarginQuirk = true;
            }

            if (quirkContainer && topMarginContributor && (posTop-negTop))
                topChildQuirk = topQuirk;

            int ypos = m_height;
            if (child->isSelfCollapsingBlock()) {
                // This child has no height.  We need to compute our
                // position before we collapse the child's margins together,
                // so that we can get an accurate position for the zero-height block.
                int collapsedTopPos = prevPosMargin;
                int collapsedTopNeg = prevNegMargin;
                if (child->maxTopMargin(true) > prevPosMargin)
                    collapsedTopPos = prevPosMargin = child->maxTopMargin(true);
                if (child->maxTopMargin(false) > prevNegMargin)
                    collapsedTopNeg = prevNegMargin = child->maxTopMargin(false);

                // Now collapse the child's margins together, which means examining our
                // bottom margin values as well. 
                if (child->maxBottomMargin(true) > prevPosMargin)
                    prevPosMargin = child->maxBottomMargin(true);
                if (child->maxBottomMargin(false) > prevNegMargin)
                    prevNegMargin = child->maxBottomMargin(false);

                if (!canCollapseTopWithChildren || !topMarginContributor)
                    // We need to make sure that the position of the self-collapsing block
                    // is correct, since it could have overflowing content
                    // that needs to be positioned correctly (e.g., a block that
                    // had a specified height of 0 but that actually had subcontent).
                    ypos = m_height + collapsedTopPos - collapsedTopNeg;
            }
            else {
                if (!topMarginContributor ||
                    (!canCollapseTopWithChildren
                     && (strictMode || !quirkContainer || !topChildQuirk)
                     )) {
                    // We're collapsing with a previous sibling's margins and not
                    // with the top of the block.
                    int absPos = prevPosMargin > posTop ? prevPosMargin : posTop;
                    int absNeg = prevNegMargin > negTop ? prevNegMargin : negTop;
                    int collapsedMargin = absPos - absNeg;
                    m_height += collapsedMargin;
                    ypos = m_height;
                }
                prevPosMargin = child->maxBottomMargin(true);
                prevNegMargin = child->maxBottomMargin(false);

                if (prevPosMargin-prevNegMargin) {
                    bottomChildQuirk = child->isBottomMarginQuirk();
                }

                selfCollapsingBlockClearedFloat = false;
            }

            child->setPos(child->xPos(), ypos);
            if (ypos != yPosEstimate) {
                if (child->style()->width().isPercent() && child->usesLineWidth())
                    // The child's width is a percentage of the line width.
                    // When the child shifts to clear an item, its width can
                    // change (because it has more available line width).
                    // So go ahead and mark the item as dirty.
                    child->setChildNeedsLayout(true);

                if (child->containsFloats() || containsFloats())
                    child->markAllDescendantsWithFloatsForLayout();

                // Our guess was wrong. Make the child lay itself out again.
                child->layoutIfNeeded();
            }
        }
        else
            selfCollapsingBlockClearedFloat = false;

        // Now check for clear.
        int heightIncrease = getClearDelta(child);
        if (heightIncrease) {
            // The child needs to be lowered.  Move the child so that it just clears the float.
            child->setPos(child->xPos(), child->yPos()+heightIncrease);
            clearOccurred = true;

            // Increase our height by the amount we had to clear.
            if (!child->isSelfCollapsingBlock())
                m_height += heightIncrease;
            else {
                // For self-collapsing blocks that clear, they may end up collapsing
                // into the bottom of the parent block.  We simulate this behavior by
                // setting our positive margin value to compensate for the clear.
                prevPosMargin = QMAX(0, child->yPos() - m_height);
                prevNegMargin = 0;
                selfCollapsingBlockClearedFloat = true;
            }
            
            if (topMarginContributor && canCollapseTopWithChildren) {
                // We can no longer collapse with the top of the block since a clear
                // occurred.  The empty blocks collapse into the cleared block.
                // XXX This isn't quite correct.  Need clarification for what to do
                // if the height the cleared block is offset by is smaller than the
                // margins involved. -dwh
                m_maxTopPosMargin = oldTopPosMargin;
                m_maxTopNegMargin = oldTopNegMargin;
                topMarginContributor = false;
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
            if (child->containsFloats())
                child->markAllDescendantsWithFloatsForLayout();
            child->layoutIfNeeded();
        }

        // Reset the top margin contributor to false if we encountered
        // a non-empty child.  This has to be done after checking for clear,
        // so that margins can be reset if a clear occurred.
        if (topMarginContributor && !child->isSelfCollapsingBlock())
            topMarginContributor = false;

        int chPos = xPos; 

        if (style()->direction() == LTR) {
            // Add in our left margin.
            chPos += child->marginLeft();
            
            // Some objects (e.g., tables, horizontal rules, overflow:auto blocks) avoid floats.  They need
            // to shift over as necessary to dodge any floats that might get in the way.
            if (child->avoidsFloats()) {
                int leftOff = leftOffset(m_height);
                if (leftOff != xPos) {
                    // The object is shifting right. The object might be centered, so we need to
                    // recalculate our horizontal margins. Note that the containing block content
                    // width computation will take into account the delta between |leftOff| and |xPos|
                    // so that we can just pass the content width in directly to the |calcHorizontalMargins|
                    // function.
                    // -dwh
                    int cw = lineWidth( child->yPos() );
                    static_cast<RenderBox*>(child)->calcHorizontalMargins
                        ( child->style()->marginLeft(), child->style()->marginRight(), cw);
                    chPos = leftOff + child->marginLeft();
                }
            }
        } else {
            chPos -= child->width() + child->marginRight();
            if (child->avoidsFloats())
                chPos -= (xPos - rightOffset(m_height));
        }

        child->setPos(chPos, child->yPos());

        m_height += child->height();
        int overflowDelta = child->overflowHeight(false) - child->height();
        if (m_height + overflowDelta > m_overflowHeight)
            m_overflowHeight = m_height + overflowDelta;

        if (child->isRenderBlock())
            prevFlow = static_cast<RenderBlock*>(child); 

        if (child->hasOverhangingFloats() && !child->style()->hidesOverflow()) {
            // need to add the child's floats to our floating objects list, but not in the case where
            // overflow is auto/scroll
            addOverHangingFloats( static_cast<RenderBlock *>(child), -child->xPos(), -child->yPos(), true );
        }

        // See if this child has made our overflow need to grow.
        // XXXdwh Work with left overflow as well as right overflow.
        int rightChildPos = child->xPos() + QMAX(child->overflowWidth(false), child->width());
        if (rightChildPos > m_overflowWidth)
            m_overflowWidth = rightChildPos;

        if (child == blockForCompactChild) {
            blockForCompactChild = 0;
            if (compactChild) {
                // We have a compact child to squeeze in.
                int compactXPos = xPos+compactChild->marginLeft();
                if (style()->direction() == RTL) {
                    compactChild->calcWidth(); // have to do this because of the capped maxwidth
                    compactXPos = width() - borderRight() - paddingRight() - marginRight() -
                        compactChild->width() - compactChild->marginRight();
                }
                compactXPos -= child->xPos(); // Put compactXPos into the child's coordinate space.
                compactChild->setPos(compactXPos, compactChild->yPos()); // Set the x position.
                compactChild = 0;
            }
        }

        // We did a layout as though the compact child was a block.  Set it back to compact now.
        if (treatCompactAsBlock) {
            child->style()->setDisplay(COMPACT);
            treatCompactAsBlock = false;
        }

#ifdef INCREMENTAL_REPAINTING
        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants.  An exception is if we need a layout.  In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldChildX, oldChildY);
#endif
        
        child = child->nextSibling();
    }

    // If our last flow was a self-collapsing block that cleared a float, then we don't
    // collapse it with the bottom of the block.
    if (selfCollapsingBlockClearedFloat)
        canCollapseBottomWithChildren = false;
    
    // If we can't collapse with children then go ahead and add in the bottom margins.
    if (!canCollapseBottomWithChildren
        && (strictMode || !quirkContainer || !bottomChildQuirk))
        m_height += prevPosMargin - prevNegMargin;

    m_height += toAdd;

    // Negative margins can cause our height to shrink below our minimal height (border/padding).
    // If this happens, ensure that the computed height is increased to the minimal height.
    if (m_height < minHeight)
        m_height = minHeight;

    // Always make sure our overflowheight is at least our height.
    if (m_overflowHeight < m_height)
        m_overflowHeight = m_height;

    if (canCollapseBottomWithChildren && !topMarginContributor) {
        // Update our max pos/neg bottom margins, since we collapsed our bottom margins
        // with our children.
        if (prevPosMargin > m_maxBottomPosMargin)
            m_maxBottomPosMargin = prevPosMargin;

        if (prevNegMargin > m_maxBottomNegMargin)
            m_maxBottomNegMargin = prevNegMargin;

        if (!bottomChildQuirk)
            m_bottomMarginQuirk = false;

        if (bottomChildQuirk && marginBottom() == 0)
            // We have no bottom margin and our last child has a quirky margin.
            // We will pick up this quirky margin and pass it through.
            // This deals with the <td><div><p> case.
            m_bottomMarginQuirk = true;
    }

    setNeedsLayout(false);

    // kdDebug( 6040 ) << "needsLayout = " << needsLayout_ << endl;
}

void RenderBlock::layoutPositionedObjects(bool relayoutChildren)
{
    if (m_positionedObjects) {
        //kdDebug( 6040 ) << renderName() << " " << this << "::layoutPositionedObjects() start" << endl;
        RenderObject* r;
        QPtrListIterator<RenderObject> it(*m_positionedObjects);
        for ( ; (r = it.current()); ++it ) {
            //kdDebug(6040) << "   have a positioned object" << endl;
            if ( relayoutChildren )
                r->setChildNeedsLayout(true);
            r->layoutIfNeeded();
        }
    }
}

#ifdef INCREMENTAL_REPAINTING
void RenderBlock::getAbsoluteRepaintRectIncludingFloats(QRect& bounds, QRect& fullBounds)
{
    bounds = fullBounds = getAbsoluteRepaintRect();

    // Include any overhanging floats (if we know we're the one to paint them).
    // We null-check m_floatingObjects here to catch any cases where m_height ends up negative
    // for some reason.  I think I've caught all those cases, but this way we stay robust and don't
    // crash. -dwh
    if (hasOverhangingFloats() && m_floatingObjects) {
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it) {
            // Only repaint the object if our noPaint flag isn't set and if it isn't in
            // its own layer.
            if (!r->noPaint && !r->node->layer()) {
                QRect childRect, childFullRect;
                r->node->getAbsoluteRepaintRectIncludingFloats(childRect, childFullRect);
                fullBounds = fullBounds.unite(childFullRect);
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
#endif

void RenderBlock::paint(QPainter* p, int _x, int _y, int _w, int _h, int _tx, int _ty, PaintAction paintAction)
{
    _tx += m_x;
    _ty += m_y;

    // check if we need to do anything at all...
    if (!isRoot() && !isInlineFlow() && !overhangingContents() && !isRelPositioned() && !isPositioned() )
    {
        int h = m_overflowHeight;
        int yPos = _ty;
        if (m_floatingObjects && floatBottom() > h)
            h = floatBottom();

        // Sanity check the first line
        // to see if it extended a little above our box. Overflow out the bottom is already handled via
        // overflowHeight(), so we don't need to check that.
        if (m_firstLineBox && m_firstLineBox->topOverflow() < 0)
            yPos += m_firstLineBox->topOverflow();
        
        int os = 2*maximalOutlineSize(paintAction);
        if( (yPos >= _y + _h + os) || (_ty + h <= _y - os))
            return;
    }

    paintObject(p, _x, _y, _w, _h, _tx, _ty, paintAction);
}

void RenderBlock::paintObject(QPainter *p, int _x, int _y,
                              int _w, int _h, int _tx, int _ty, PaintAction paintAction)
{

#ifdef DEBUG_LAYOUT
    //    kdDebug( 6040 ) << renderName() << "(RenderBlock) " << this << " ::paintObject() w/h = (" << width() << "/" << height() << ")" << endl;
#endif

    // If we're a repositioned run-in, don't paint background/borders.
    bool inlineFlow = isInlineFlow();
    bool isPrinting = (p->device()->devType() == QInternal::Printer);

    // 1. paint background, borders etc
    if (!inlineFlow &&
        (paintAction == PaintActionElementBackground || paintAction == PaintActionChildBackground) &&
        shouldPaintBackgroundOrBorder() && style()->visibility() == VISIBLE) {
        paintBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);
    }

    // We're done.  We don't bother painting any children.
    if (paintAction == PaintActionElementBackground)
        return;
    // We don't paint our own background, but we do let the kids paint their backgrounds.
    if (paintAction == PaintActionChildBackgrounds)
        paintAction = PaintActionChildBackground;
    
    paintLineBoxBackgroundBorder(p, _x, _y, _w, _h, _tx, _ty, paintAction);

    // 2. paint contents
    int scrolledX = _tx;
    int scrolledY = _ty;
    if (style()->hidesOverflow() && m_layer)
        m_layer->subtractScrollOffset(scrolledX, scrolledY);
    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {        
        // Check for page-break-before: always, and if it's set, break and bail.
        if (isPrinting && !childrenInline() && child->style()->pageBreakBefore() == PBALWAYS &&
            inRootBlockContext() && (_ty + child->yPos()) > _y && (_ty + child->yPos()) < _y + _h) {
            canvas()->setBestTruncatedAt(_ty + child->yPos(), this, true);
            return;
        }
        
        if (!child->layer() && !child->isFloating())
            child->paint(p, _x, _y, _w, _h, scrolledX, scrolledY, paintAction);
        
        // Check for page-break-after: always, and if it's set, break and bail.
        if (isPrinting && !childrenInline() && child->style()->pageBreakAfter() == PBALWAYS && 
            inRootBlockContext() && (_ty + child->yPos() + child->height()) > _y && 
            (_ty + child->yPos() + child->height()) < _y + _h) {
            canvas()->setBestTruncatedAt(_ty + child->yPos() + child->height() + child->collapsedMarginBottom(), this, true);
            return;
        }
    }
    paintLineBoxDecorations(p, _x, _y, _w, _h, scrolledX, scrolledY, paintAction);
    
    // 3. paint floats.
    if (!inlineFlow && (paintAction == PaintActionFloat || paintAction == PaintActionSelection))
        paintFloats(p, _x, _y, _w, _h, scrolledX, scrolledY, paintAction == PaintActionSelection);

    // 4. paint outline.
    if (!inlineFlow && paintAction == PaintActionOutline && 
        style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(p, _tx, _ty, width(), height(), style());

#ifdef BOX_DEBUG
    if ( style() && style()->visibility() == VISIBLE ) {
        if(isAnonymous())
            outlineBox(p, _tx, _ty, "green");
        if(isFloating())
            outlineBox(p, _tx, _ty, "yellow");
        else
            outlineBox(p, _tx, _ty);
    }
#endif
}

void RenderBlock::paintFloats(QPainter *p, int _x, int _y,
                              int _w, int _h, int _tx, int _ty, bool paintSelection)
{
    if (!m_floatingObjects)
        return;

    FloatingObject* r;
    QPtrListIterator<FloatingObject> it(*m_floatingObjects);
    for ( ; (r = it.current()); ++it) {
        // Only paint the object if our noPaint flag isn't set.
        if (!r->noPaint && !r->node->layer()) {
            if (paintSelection) {
                r->node->paint(p, _x, _y, _w, _h,
                               _tx + r->left - r->node->xPos() + r->node->marginLeft(),
                               _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                               PaintActionSelection);
            }
            else {
                r->node->paint(p, _x, _y, _w, _h,
                               _tx + r->left - r->node->xPos() + r->node->marginLeft(),
                               _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                               PaintActionElementBackground);
                r->node->paint(p, _x, _y, _w, _h,
                               _tx + r->left - r->node->xPos() + r->node->marginLeft(),
                               _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                               PaintActionChildBackgrounds);
                r->node->paint(p, _x, _y, _w, _h,
                               _tx + r->left - r->node->xPos() + r->node->marginLeft(),
                               _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                               PaintActionFloat);
                r->node->paint(p, _x, _y, _w, _h,
                               _tx + r->left - r->node->xPos() + r->node->marginLeft(),
                               _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                               PaintActionForeground);
                r->node->paint(p, _x, _y, _w, _h,
                               _tx + r->left - r->node->xPos() + r->node->marginLeft(),
                               _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                               PaintActionOutline);
            }
        }
    }
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

    // Create the special object entry & append it to the list
    setOverhangingContents();
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
        
#ifdef INCREMENTAL_REPAINTING
        int oldChildX = o->xPos();
        int oldChildY = o->yPos();
#endif
        
        if (o->style()->floating() == FLEFT)
        {
            if ( o->style()->clear() & CLEFT )
                y = QMAX( leftBottom(), y );
            int heightRemainingLeft = 1;
            int heightRemainingRight = 1;
            int fx = leftRelOffset(y,lo, false, &heightRemainingLeft);
            while (rightRelOffset(y,ro, false, &heightRemainingRight)-fx < fwidth)
            {
                y += QMIN( heightRemainingLeft, heightRemainingRight );
                fx = leftRelOffset(y,lo, false, &heightRemainingLeft);
            }
            if (fx<0) fx=0;
            f->left = fx;
            //kdDebug( 6040 ) << "positioning left aligned float at (" << fx + o->marginLeft()  << "/" << y + o->marginTop() << ") fx=" << fx << endl;
            o->setPos(fx + o->marginLeft(), y + o->marginTop());
        }
        else
        {
            if ( o->style()->clear() & CRIGHT )
                y = QMAX( rightBottom(), y );
            int heightRemainingLeft = 1;
            int heightRemainingRight = 1;
            int fx = rightRelOffset(y,ro, false, &heightRemainingRight);
            while (fx - leftRelOffset(y,lo, false, &heightRemainingLeft) < fwidth)
            {
                y += QMIN(heightRemainingLeft, heightRemainingRight);
                fx = rightRelOffset(y,ro, false, &heightRemainingRight);
            }
            if (fx<f->width) fx=f->width;
            f->left = fx - f->width;
            //kdDebug( 6040 ) << "positioning right aligned float at (" << fx - o->marginRight() - o->width() << "/" << y + o->marginTop() << ")" << endl;
            o->setPos(fx - o->marginRight() - o->width(), y + o->marginTop());
        }
        f->startY = y;
        f->endY = f->startY + _height;

#ifdef INCREMENTAL_REPAINTING
        // If the child moved, we have to repaint it.
        if (checkForRepaintDuringLayout())
            o->repaintDuringLayoutIfMoved(oldChildX, oldChildY);
#endif    

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
    if (style()->scrollsOverflow() && m_layer)
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
        right += style()->textIndent().minWidth(cw);
    }
    
    //kdDebug( 6040 ) << "rightOffset(" << y << ") = " << right << endl;
    return right;
}

unsigned short
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
    return QMAX(bottom, height);
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

int
RenderBlock::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    int bottom = RenderFlow::lowestPosition(includeOverflowInterior, includeSelf);
    if (!includeOverflowInterior && style()->hidesOverflow())
        return bottom;
    if (includeSelf && m_overflowHeight > bottom)
        bottom = m_overflowHeight;
    
    if (m_floatingObjects) {
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it ) {
            if (!r->noPaint) {
                int lp = r->startY + r->node->lowestPosition(false);
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
    if (!includeOverflowInterior && style()->hidesOverflow())
        return right;
    if (includeSelf && m_overflowWidth > right)
        right = m_overflowWidth;
    
    if (m_floatingObjects) {
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it ) {
            if (!r->noPaint) {
                int rp = r->left + r->node->rightmostPosition(false);
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
    if (!includeOverflowInterior && style()->hidesOverflow())
        return left;
    
    // FIXME: Check left overflow when we eventually support it.
    
    if (m_floatingObjects) {
        FloatingObject* r;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for ( ; (r = it.current()); ++it ) {
            if (!r->noPaint) {
                int lp = r->left + r->node->leftmostPosition(false);
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
        addOverHangingFloats( static_cast<RenderBlock *>( parent() ),
                              parent()->borderLeft() + parent()->paddingLeft(), offset, false );

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
        addOverHangingFloats(block, xoffset, offset);
}

void RenderBlock::addOverHangingFloats( RenderBlock *flow, int xoff, int offset, bool child )
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << (void *)this << ": adding overhanging floats xoff=" << xoff << "  offset=" << offset << " child=" << child << endl;
#endif

    // Prevent floats from being added to the canvas by the root element, e.g., <html>.
    if ( !flow->m_floatingObjects || (child && flow->isRoot()) )
        return;

    // we have overhanging floats
    if (!m_floatingObjects) {
        m_floatingObjects = new QPtrList<FloatingObject>;
        m_floatingObjects->setAutoDelete(true);
    }

    QPtrListIterator<FloatingObject> it(*flow->m_floatingObjects);
    FloatingObject *r;
    for ( ; (r = it.current()); ++it ) {
        if ( ( !child && r->endY > offset ) ||
             ( child && flow->yPos() + r->endY > height() ) ) {

            if (child && (flow->enclosingLayer() == enclosingLayer()))
                // Set noPaint to true only if we didn't cross layers.
                r->noPaint = true;

            FloatingObject* f = 0;
            // don't insert it twice!
            QPtrListIterator<FloatingObject> it(*m_floatingObjects);
            while ( (f = it.current()) ) {
                if (f->node == r->node) break;
                ++it;
            }
            if ( !f ) {
                FloatingObject *floatingObj = new FloatingObject(r->type);
                floatingObj->startY = r->startY - offset;
                floatingObj->endY = r->endY - offset;
                floatingObj->left = r->left - xoff;
                // Applying the child's margin makes no sense in the case where the child was passed in.
                // since his own margin was added already through the subtraction of the |xoff| variable
                // above.  |xoff| will equal -flow->marginLeft() in this case, so it's already been taken
                // into account.  Only apply this code if |child| is false, since otherwise the left margin
                // will get applied twice. -dwh
                if (!child && flow != parent())
                    floatingObj->left += flow->marginLeft();
                if ( !child ) {
                    floatingObj->left -= marginLeft();
                    floatingObj->noPaint = true;
                }
                else
                    // Only paint if |flow| isn't.
                    floatingObj->noPaint = !r->noPaint;
                
                floatingObj->width = r->width;
                floatingObj->node = r->node;
                m_floatingObjects->append(floatingObj);
#ifdef DEBUG_LAYOUT
                kdDebug( 6040 ) << "addOverHangingFloats x/y= (" << floatingObj->left << "/" << floatingObj->startY << "-" << floatingObj->width << "/" << floatingObj->endY - floatingObj->startY << ")" << endl;
#endif
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
    setNeedsLayout(true);

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
    //kdDebug( 6040 ) << "checkClear oldheight=" << m_height << endl;
    int bottom = 0;
    switch(child->style()->clear())
    {
        case CNONE:
            return 0;
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

    return QMAX(0, bottom-(child->yPos()));
}

bool RenderBlock::isPointInScrollbar(int _x, int _y, int _tx, int _ty)
{
    if (!style()->scrollsOverflow() || !m_layer)
        return false;

    if (m_layer->verticalScrollbarWidth()) {
        QRect vertRect(_tx + width() - borderRight() - m_layer->verticalScrollbarWidth(),
                       _ty + borderTop(),
                       m_layer->verticalScrollbarWidth(),
                       height()-borderTop()-borderBottom());
        if (vertRect.contains(_x, _y)) {
            RenderLayer::gScrollBar = m_layer->verticalScrollbar();
            return true;
        }
    }

    if (m_layer->horizontalScrollbarHeight()) {
        QRect horizRect(_tx + borderLeft(),
                        _ty + height() - borderBottom() - m_layer->horizontalScrollbarHeight(),
                        width()-borderLeft()-borderRight(),
                        m_layer->horizontalScrollbarHeight());
        if (horizRect.contains(_x, _y)) {
            RenderLayer::gScrollBar = m_layer->horizontalScrollbar();
            return true;
        }
    }

    return false;    
}

bool RenderBlock::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty,
                              HitTestAction hitTestAction, bool inBox)
{
    bool inScrollbar = isPointInScrollbar(_x, _y, _tx+xPos(), _ty+yPos());
    if (inScrollbar && hitTestAction != HitTestChildrenOnly)
        inBox = true;
    
    if (hitTestAction != HitTestSelfOnly && m_floatingObjects && !inScrollbar) {
        int stx = _tx + xPos();
        int sty = _ty + yPos();
        if (style()->hidesOverflow() && m_layer)
            m_layer->subtractScrollOffset(stx, sty);
        if (isCanvas()) {
            stx += static_cast<RenderCanvas*>(this)->view()->contentsX();
            sty += static_cast<RenderCanvas*>(this)->view()->contentsY();
        }
        FloatingObject* o;
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        for (it.toLast(); (o = it.current()); --it)
            if (!o->noPaint && !o->node->layer())
                inBox |= o->node->nodeAtPoint(info, _x, _y,
                                              stx+o->left + o->node->marginLeft() - o->node->xPos(),
                                              sty+o->startY + o->node->marginTop() - o->node->yPos());
    }

    inBox |= RenderFlow::nodeAtPoint(info, _x, _y, _tx, _ty, hitTestAction, inBox);
    return inBox;
}

void RenderBlock::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderBlock)::calcMinMaxWidth() this=" << this << endl;
#endif

    m_minWidth = 0;
    m_maxWidth = 0;

    bool preOrNowrap = style()->whiteSpace() != NORMAL;
    if (childrenInline())
        calcInlineMinMaxWidth();
    else
        calcBlockMinMaxWidth();

    if(m_maxWidth < m_minWidth) m_maxWidth = m_minWidth;

    if (preOrNowrap && childrenInline()) {
        m_minWidth = m_maxWidth;
        
        // A horizontal marquee with inline children has no minimum width.
        if (style()->overflow() == OMARQUEE && m_layer && m_layer->marquee() && 
            m_layer->marquee()->isHorizontal() && !m_layer->marquee()->isUnfurlMarquee())
            m_minWidth = 0;
    }

    if (style()->width().isFixed() && style()->width().value > 0) {
        if (isTableCell())
            m_maxWidth = KMAX(m_minWidth,short(style()->width().value));
        else
            m_minWidth = m_maxWidth = short(style()->width().value);
    }
    
    if (style()->minWidth().isFixed() && style()->minWidth().value > 0) {
        m_maxWidth = KMAX(m_maxWidth, short(style()->minWidth().value));
        m_minWidth = KMAX(m_minWidth, short(style()->minWidth().value));
    }
    
    if (style()->maxWidth().isFixed() && style()->maxWidth().value != UNDEFINED) {
        m_maxWidth = KMIN(m_maxWidth, short(style()->maxWidth().value));
        m_minWidth = KMIN(m_minWidth, short(style()->maxWidth().value));
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

        if (result->isText() || result->isBR() ||
            result->isFloating() || result->isReplaced() ||
            result->isInlineFlow())
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
    if (cssUnit.type != Variable)
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

static void stripTrailingSpace(bool pre,
                               int& inlineMax, int& inlineMin,
                               RenderObject* trailingSpaceChild)
{
    if (!pre && trailingSpaceChild && trailingSpaceChild->isText()) {
        // Collapse away the trailing space at the end of a block.
        RenderText* t = static_cast<RenderText *>(trailingSpaceChild);
        const Font *f = t->htmlFont( false );
        QChar space[1]; space[0] = ' ';
        int spaceWidth = f->width(space, 1, 0);
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

    bool normal, oldnormal;
    normal = oldnormal = style()->whiteSpace() == NORMAL;

    InlineMinMaxIterator childIterator(this, this);
    bool addedTextIndent = false; // Only gets added in once.
    while (RenderObject* child = childIterator.next())
    {
        normal = child->style()->whiteSpace() == NORMAL;

        if( !child->isBR() )
        {
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
            short childMin = 0;
            short childMax = 0;

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
                    if ( type != Variable )
                        margins += (type == Fixed ? cstyle->marginLeft().value : child->marginLeft());
                    type = cstyle->marginRight().type;
                    if ( type != Variable )
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

                if (normal || oldnormal) {
                    if(m_minWidth < inlineMin) m_minWidth = inlineMin;
                    inlineMin = 0;
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

                if (!normal)
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
                short beginMin, endMin;
                bool beginWS, endWS;
                short beginMax, endMax;
                t->trimmedMinMaxWidth(beginMin, beginWS, endMin, endWS, hasBreakableChar,
                                      hasBreak, beginMax, endMax,
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

        oldnormal = normal;
    }

    stripTrailingSpace(m_pre, inlineMax, inlineMin, trailingSpaceChild);
    
    if(m_minWidth < inlineMin) m_minWidth = inlineMin;
    if(m_maxWidth < inlineMax) m_maxWidth = inlineMax;

    //         kdDebug( 6040 ) << "m_minWidth=" << m_minWidth
    // 			<< " m_maxWidth=" << m_maxWidth << endl;
}

// Use a very large value (in effect infinite).
#define BLOCK_MAX_WIDTH 15000

void RenderBlock::calcBlockMinMaxWidth()
{
    bool nowrap = style()->whiteSpace() == NOWRAP;

    RenderObject *child = firstChild();
    while(child != 0)
    {
        // Positioned children don't affect the min/max width
        if (child->isPositioned()) {
            child = child->nextSibling();
            continue;
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
        int margin = 0;
        if (ml.type == Fixed)
            margin += ml.value;
        else if (ml.type == Percent)
            margin += child->marginLeft();

        if (mr.type == Fixed)
            margin += mr.value;
        else if (mr.type == Percent)
            margin += child->marginRight();
        
        if (margin < 0) margin = 0;

        int w = child->minWidth() + margin;
        if(m_minWidth < w) m_minWidth = w;
        // IE ignores tables for calculation of nowrap. Makes some sense.
        if ( nowrap && !child->isTable() && m_maxWidth < w )
            m_maxWidth = w;

        w = child->maxWidth() + margin;

        if(m_maxWidth < w) m_maxWidth = w;

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
    if (isReplaced() && !isRootLineBox)
        return height()+marginTop()+marginBottom();
    return RenderFlow::baselinePosition(b, isRootLineBox);
}

int RenderBlock::getBaselineOfFirstLineBox()
{
    if (m_firstLineBox)
        return m_firstLineBox->yPos() + m_firstLineBox->baseline();

    if (isInline())
        return -1; // We're inline and had no line box, so we have no baseline we can return.

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
        int result = curr->getBaselineOfFirstLineBox();
        if (result != -1)
            return curr->yPos() + result; // Translate to our coordinate space.
    }

    return -1;
}

InlineFlowBox* RenderBlock::getFirstLineBox()
{
    if (m_firstLineBox)
        return m_firstLineBox;

    if (isInline())
        return 0; // We're inline and had no line box, so we have no baseline we can return.

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
        InlineFlowBox* result = curr->getFirstLineBox();
        if (result)
            return result;
    }

    return 0;    
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
        if (firstLineBlock->isReplaced() || !parentBlock || parentBlock->firstChild() != firstLineBlock || 
            !parentBlock->isBlockFlow())
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
    
    if (currChild && currChild->isText() && !currChild->isBR() && 
        currChild->parent()->style()->styleType() != RenderStyle::FIRST_LETTER) {
        RenderObject* firstLetterContainer = currChild->parent();
        if (!firstLetterContainer)
            firstLetterContainer = this;
        
        RenderText* textObj = static_cast<RenderText*>(currChild);
        
        // Create our pseudo style now that we have our firstLetterContainer determined.
        RenderStyle* pseudoStyle = firstLetterBlock->getPseudoStyle(RenderStyle::FIRST_LETTER,
                                                                    firstLetterContainer->style(true));
        
        // Force inline display (except for floating first-letters)
        pseudoStyle->setDisplay( pseudoStyle->isFloating() ? BLOCK : INLINE);
        pseudoStyle->setPosition( STATIC ); // CSS2 says first-letter can't be positioned.
        
        RenderObject* firstLetter = RenderFlow::createAnonymousFlow(document(), pseudoStyle); // anonymous box
        firstLetterContainer->addChild(firstLetter, firstLetterContainer->firstChild());
        
        // The original string is going to be either a generated content string or a DOM node's
        // string.  We want the original string before it got transformed in case first-letter has
        // no text-transform or a different text-transform applied to it.
        DOMStringImpl* oldText = textObj->originalString();
        
        if (oldText->l >= 1) {
            unsigned int length = 0;
            while ( length < oldText->l &&
                    ( (oldText->s+length)->isSpace() || (oldText->s+length)->isPunct() ) )
                length++;
            length++;
            //kdDebug( 6040 ) << "letter= '" << DOMString(oldText->substring(0,length)).string() << "'" << endl;
            
            RenderTextFragment* remainingText = 
                new (renderArena()) RenderTextFragment(textObj->node(), oldText, length, oldText->l-length);
            remainingText->setStyle(textObj->style());
            if (remainingText->element())
                remainingText->element()->setRenderer(remainingText);
            
            RenderObject* nextObj = textObj->nextSibling();
            firstLetterContainer->removeChild(textObj);
            firstLetterContainer->addChild(remainingText, nextObj);
            
            RenderTextFragment* letter = 
                new (renderArena()) RenderTextFragment(remainingText->node(), oldText, 0, length);
            RenderStyle* newStyle = new RenderStyle();
            newStyle->inheritFrom(pseudoStyle);
            letter->setStyle(newStyle);
            firstLetter->addChild(letter);
        }
    }
}

bool RenderBlock::inRootBlockContext() const
{
    if (isTableCell() || isFloatingOrPositioned() || style()->hidesOverflow())
        return false;
    
    if (isRoot() || isCanvas())
        return true;
    
    return containingBlock()->inRootBlockContext();
}

const char *RenderBlock::renderName() const
{
    if (isBody())
        return "RenderBody"; // FIXME: Temporary hack until we know that the regression tests pass.
    
    if (isFloating())
        return "RenderBlock (floating)";
    if (isPositioned())
        return "RenderBlock (positioned)";
    if (isAnonymous())
        return "RenderBlock (anonymous)";
    if (isRelPositioned())
        return "RenderBlock (relative positioned)";
    if (isCompact())
        return "RenderBlock (compact)";
    if (isRunIn())
        return "RenderBlock (run-in)";
    return "RenderBlock";
}

#ifndef NDEBUG
void RenderBlock::printTree(int indent) const
{
    RenderFlow::printTree(indent);

    if (m_floatingObjects)
    {
        QPtrListIterator<FloatingObject> it(*m_floatingObjects);
        FloatingObject *r;
        for ( ; (r = it.current()); ++it )
        {
            QString s;
            s.fill(' ', indent);
            kdDebug() << s << renderName() << ":  " <<
                (r->type == FloatingObject::FloatLeft ? "FloatLeft" : "FloatRight" )  <<
                "[" << r->node->renderName() << ": " << (void*)r->node << "] (" << r->startY << " - " << r->endY << ")" << "width: " << r->width <<
                endl;
        }
    }
}

void RenderBlock::dump(QTextStream *stream, QString ind) const
{
    if (m_childrenInline) { *stream << " childrenInline"; }
    if (m_pre) { *stream << " pre"; }
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

#undef DEBUG
#undef DEBUG_LAYOUT
#undef BOX_DEBUG

} // namespace khtml

