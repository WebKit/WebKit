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

//#define DEBUG
//#define DEBUG_LAYOUT
//#define BOX_DEBUG
//#define FLOAT_DEBUG

#include <kdebug.h>
#include "rendering/render_text.h"
#include "rendering/render_table.h"
#include "rendering/render_root.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "html/html_formimpl.h"
#include "render_block.h"

#include "khtmlview.h"
#include "htmltags.h"

using namespace khtml;
using namespace DOM;

RenderBlock::RenderBlock(DOM::NodeImpl* node)
:RenderFlow(node)
{
    m_childrenInline = true;
    m_specialObjects = 0;
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
    delete m_specialObjects;
}

void RenderBlock::setStyle(RenderStyle* _style)
{
    setInline(false);
    RenderFlow::setStyle(_style);
    m_pre = false;
    if (_style->whiteSpace() == PRE)
        m_pre = true;

    // ### we could save this call when the change only affected
    // non inherited properties
    RenderObject *child = firstChild();
    while (child != 0)
    {
        if (child->isAnonymousBox())
        {
            RenderStyle* newStyle = new RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);
            child->setStyle(newStyle);
            child->setIsAnonymousBox(true);
        }
        child = child->nextSibling();
    }    
}

void RenderBlock::addChildToFlow(RenderObject* newChild, RenderObject* beforeChild)
{
    setLayouted( false );

    bool madeBoxesNonInline = FALSE;

    RenderStyle* pseudoStyle=0;
    if ((!firstChild() || firstChild() == beforeChild) && newChild->isText())
    {
        RenderText* newTextChild = static_cast<RenderText*>(newChild);
        //kdDebug( 6040 ) << "first letter" << endl;

        if ( (pseudoStyle=style()->getPseudoStyle(RenderStyle::FIRST_LETTER)) ) {
            pseudoStyle->setDisplay( INLINE );
            pseudoStyle->setPosition( STATIC ); // CSS2 says first-letter can't be positioned.

            RenderObject* firstLetter = RenderFlow::createFlow(0, pseudoStyle, renderArena()); // anonymous box
            addChild(firstLetter);

            DOMStringImpl* oldText = newTextChild->string();

            if(oldText->l >= 1) {
                unsigned int length = 0;
                while ( length < oldText->l &&
                        ( (oldText->s+length)->isSpace() || (oldText->s+length)->isPunct() ) )
                    length++;
                length++;
                //kdDebug( 6040 ) << "letter= '" << DOMString(oldText->substring(0,length)).string() << "'" << endl;
                newTextChild->setText(oldText->substring(length,oldText->l-length));

                RenderText* letter = new (renderArena()) RenderText(newTextChild->element(), oldText->substring(0,length));
                RenderStyle* newStyle = new RenderStyle();
                newStyle->inheritFrom(pseudoStyle);
                letter->setStyle(newStyle);
                firstLetter->addChild(letter);
            }
            firstLetter->close();
        }
    }

    insertPseudoChild(RenderStyle::BEFORE, newChild, beforeChild);

    // If the requested beforeChild is not one of our children, then this is most likely because
    // there is an anonymous block box within this object that contains the beforeChild. So
    // just insert the child into the anonymous block box instead of here.
    if (beforeChild && beforeChild->parent() != this) {

        KHTMLAssert(beforeChild->parent());
        KHTMLAssert(beforeChild->parent()->isAnonymousBox());

        if (newChild->isInline()) {
            beforeChild->parent()->addChild(newChild,beforeChild);
            newChild->setLayouted( false );
            newChild->setMinMaxKnown( false );
            return;
        }
        else if (beforeChild->parent()->firstChild() != beforeChild)
            return beforeChild->parent()->addChild(newChild, beforeChild);
        else
            return addChildToFlow(newChild, beforeChild->parent());
    }

    // prevent non-layouted elements from getting painted by pushing them far above the top of the
    // page
    if (!newChild->isInline())
        newChild->setPos(newChild->xPos(), -500000);

    if (!newChild->isText() && newChild->style()->position() != STATIC)
        setOverhangingContents();

    // A block has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes
    if ( m_childrenInline && !newChild->isInline() && !newChild->isSpecial() )
    {
        // This is a block with inline content. Wrap the inline content in anonymous blocks.
        makeChildrenNonInline(beforeChild);
        madeBoxesNonInline = true;
        
        if (beforeChild && beforeChild->parent() != this) {
            beforeChild = beforeChild->parent();
            KHTMLAssert(beforeChild->isAnonymousBox());
            KHTMLAssert(beforeChild->parent() == this);
        }
    }
    else if (!m_childrenInline && !newChild->isSpecial())
    {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        if (newChild->isInline()) {
            if (beforeChild) {
                if (beforeChild->previousSibling() && beforeChild->previousSibling()->isAnonymousBox()) {
                    beforeChild->previousSibling()->addChild(newChild);
                    newChild->setLayouted( false );
                    newChild->setMinMaxKnown( false );
                    return;
                }
            }
            else {
                if (m_last && m_last->isAnonymousBox()) {
                    m_last->addChild(newChild);
                    newChild->setLayouted( false );
                    newChild->setMinMaxKnown( false );
                    return;
                }
            }

            // no suitable existing anonymous box - create a new one
            RenderStyle *newStyle = new RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);

            RenderBlock *newBox = new (renderArena()) RenderBlock(0 /* anonymous box */);
            newBox->setStyle(newStyle);
            newBox->setIsAnonymousBox(true);

            RenderBox::addChild(newBox,beforeChild);
            newBox->addChild(newChild);
            newBox->setPos(newBox->xPos(), -500000);

            newChild->setLayouted( false );
            newChild->setMinMaxKnown( false );
            return;
        }
        else {
            // We are adding another block child... if the current last child is an anonymous box
            // then it needs to be closed.
            // ### get rid of the closing thing altogether this will only work during initial parsing
            if (lastChild() && lastChild()->isAnonymousBox()) {
                lastChild()->close();
            }
        }
    }

    RenderBox::addChild(newChild,beforeChild);
    // ### care about aligned stuff

    newChild->setLayouted( false );
    newChild->setMinMaxKnown( false );
    insertPseudoChild(RenderStyle::AFTER, newChild, beforeChild);

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
    KHTMLAssert(!isInline());
    KHTMLAssert(!insertionPoint || insertionPoint->parent() == this);

    m_childrenInline = false;

    RenderObject *child = firstChild();

    while (child) {
        RenderObject *inlineRunStart, *inlineRunEnd;
        getInlineRun(child, insertionPoint, inlineRunStart, inlineRunEnd);

        if (!inlineRunStart)
            break;

        child = inlineRunEnd->nextSibling();

        RenderStyle *newStyle = new RenderStyle();
        newStyle->inheritFrom(style());
        newStyle->setDisplay(BLOCK);

        RenderBlock *box = new (renderArena()) RenderBlock(0 /* anonymous box */);
        box->setStyle(newStyle);
        box->setIsAnonymousBox(true);

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
        box->setLayouted(false);
    }

    setLayouted(false);
}

void RenderBlock::removeChild(RenderObject *oldChild)
{
    // If this child is a block, and if our previous and next siblings are
    // both anonymous blocks with inline content, then we can go ahead and
    // fold the inline content back together.
    RenderObject* prev = oldChild->previousSibling();
    RenderObject* next = oldChild->nextSibling();
    bool mergedBlocks = false;
    if (!isInline() && !oldChild->isInline() && !oldChild->continuation() &&
        prev && prev->isAnonymousBox() && prev->childrenInline() &&
        next && next->isAnonymousBox() && next->childrenInline()) {
        // Take all the children out of the |next| block and put them in
        // the |prev| block.
        RenderObject* o = next->firstChild();
        while (o) {
            RenderObject* no = o;
            o = no->nextSibling();
            prev->appendChildNode(next->removeChildNode(no));
            no->setLayouted(false);
            no->setMinMaxKnown(false);
        }
        prev->setLayouted(false);
        prev->setMinMaxKnown(false);

        // Nuke the now-empty block.
        next->detach(renderArena());

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
            no->setLayouted(false);
            no->setMinMaxKnown(false);
        }
        setLayouted(false);
        setMinMaxKnown(false);
    }
}

void RenderBlock::layout()
{
    //    kdDebug( 6040 ) << renderName() << " " << this << "::layout() start" << endl;
    //     QTime t;
    //     t.start();

    KHTMLAssert( !layouted() );
    KHTMLAssert( minMaxKnown() );
    if (isInline()) // Inline <form>s inside various table elements can cause us to
        return;		// come in here.  Just bail. -dwh

    int oldWidth = m_width;

    calcWidth();
    m_overflowWidth = m_width;

    bool relayoutChildren = false;
    if ( oldWidth != m_width )
        relayoutChildren = true;

    // need a small hack here, as tables are done a bit differently
    if ( isTableCell() )
        relayoutChildren = true;

    //     kdDebug( 6040 ) << specialObjects << "," << oldWidth << ","
    //                     << m_width << ","<< layouted() << "," << isAnonymousBox() << ","
    //                     << overhangingContents() << "," << isPositioned() << endl;

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderBlock) " << this << " ::layout() width=" << m_width << ", layouted=" << layouted() << endl;
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

    // A quirk that has become an unfortunate standard.  Positioned elements, floating elements
    // and table cells don't ever collapse their margins with either themselves or their
    // children.
    bool canCollapseOwnMargins = !isPositioned() && !isFloating() && !isTableCell();

    //    kdDebug( 6040 ) << "childrenInline()=" << childrenInline() << endl;
    if(childrenInline())
        layoutInlineChildren( relayoutChildren );
    else
        layoutBlockChildren( relayoutChildren );

    int oldHeight = m_height;
    calcHeight();
    if (oldHeight != m_height) {
        relayoutChildren = true;

        // If the block got expanded in size, then increase our overflowheight to match.
        if (m_overflowHeight > m_height)
            m_overflowHeight -= (borderBottom()+paddingBottom());
        if (m_overflowHeight < m_height)
            m_overflowHeight = m_height;
    }

    if (isTableCell()) {
        // Table cells need to grow to accommodate both overhanging floats and
        // blocks that have overflowed content.
        // Check for an overhanging float first.
        // FIXME: This needs to look at the last flow, not the last child.
        if (lastChild() && lastChild()->hasOverhangingFloats() ) {
            KHTMLAssert(lastChild()->isRenderBlock());
            m_height = lastChild()->yPos() + static_cast<RenderBlock*>(lastChild())->floatBottom();
            m_height += borderBottom() + paddingBottom();
        }

        if (m_overflowHeight > m_height)
            m_height = m_overflowHeight + borderBottom() + paddingBottom();
    }

    if( hasOverhangingFloats() && (isFloating() || isTableCell())) {
        m_height = floatBottom();
        m_height += borderBottom() + paddingBottom();
    }

    layoutSpecialObjects( relayoutChildren );

    //kdDebug() << renderName() << " layout width=" << m_width << " height=" << m_height << endl;

    if (canCollapseOwnMargins && m_height == 0) {
        // We are a block with no border and padding and a computed height
        // of 0.  The CSS spec states that zero-height blocks collapse their margins
        // together.
        // When blocks are self-collapsing, we just use the top margin values and set the
        // bottom margin max values to 0.  This way we don't factor in the values
        // twice when we collapse with our previous vertically adjacent and
        // following vertically adjacent blocks.
        if (m_maxBottomPosMargin > m_maxTopPosMargin)
            m_maxTopPosMargin = m_maxBottomPosMargin;
        if (m_maxBottomNegMargin > m_maxTopNegMargin)
            m_maxTopNegMargin = m_maxBottomNegMargin;
        m_maxBottomNegMargin = m_maxBottomPosMargin = 0;
    }

    // Always ensure our overflow width is at least as large as our width.
    if (m_overflowWidth < m_width)
        m_overflowWidth = m_width;

    // overflow:hidden will just clip, so we don't have overflow.
    if (style()->overflow()==OHIDDEN) {
        m_overflowHeight = m_height;
        m_overflowWidth = m_width;
    }

    setLayouted();
}

void RenderBlock::layoutBlockChildren( bool relayoutChildren )
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << " layoutBlockChildren( " << this <<" ), relayoutChildren="<< relayoutChildren << endl;
#endif

    int xPos = 0;
    int toAdd = 0;

    m_height = 0;

    xPos += borderLeft() + paddingLeft();
    m_height += borderTop() + paddingTop();
    toAdd += borderBottom() + paddingBottom();

    int minHeight = m_height + toAdd;
    m_overflowHeight = m_height;

    if( style()->direction() == RTL ) {
        xPos = marginLeft() + m_width - paddingRight() - borderRight();
    }

    RenderObject *child = firstChild();
    RenderBlock *prevFlow = 0;

    // Whether or not we can collapse our own margins with our children.  We don't do this
    // if we had any border/padding (obviously), if we're the root or HTML elements, or if
    // we're positioned, floating, a table cell.
    // For now we only worry about the top border/padding.  We will update the variable's
    // value when it comes time to check against the bottom border/padding.
    bool canCollapseWithChildren = !isRoot() && !isHtml() && !isPositioned() &&
        !isFloating() && !isTableCell() && (m_height == 0);

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
    int prevPosMargin = canCollapseWithChildren ? maxTopMargin(true) : 0;
    int prevNegMargin = canCollapseWithChildren ? maxTopMargin(false) : 0;

    // Whether or not we encountered an element with clear set that actually had to
    // be pushed down below a float.
    int clearOccurred = false;

    int oldPosMargin = prevPosMargin;
    int oldNegMargin = prevNegMargin;

    bool topChildQuirk = false;
    bool bottomChildQuirk = false;
    bool determinedTopQuirk = false;

    bool strictMode = isAnonymousBox() ? true : (!element()->getDocument()->inQuirksMode());

    //kdDebug() << "RenderBlock::layoutBlockChildren " << prevMargin << endl;

    // take care in case we inherited floats
    if (child && floatBottom() > m_height)
        child->setLayouted(false);

    //     QTime t;
    //     t.start();

    while( child != 0 )
    {
        // make sure we relayout children if we need it.
        if ( relayoutChildren || floatBottom() > m_y ||
             (child->isReplaced() && (child->style()->width().isPercent() || child->style()->height().isPercent())))
            child->setLayouted(false);

        //         kdDebug( 6040 ) << "   " << child->renderName() << " loop " << child << ", " << child->isInline() << ", " << child->layouted() << endl;
        //         kdDebug( 6040 ) << t.elapsed() << endl;
        // ### might be some layouts are done two times... FIX that.

        if (child->isPositioned())
        {
            static_cast<RenderBlock*>(child->containingBlock())->insertSpecialObject(child);
            //kdDebug() << "RenderBlock::layoutBlockChildren inserting positioned into " << child->containingBlock()->renderName() << endl;

            child = child->nextSibling();
            continue;
        } else if ( child->isReplaced() ) {
            if ( !child->layouted() )
                child->layout();
        }

        if ( child->isFloating() ) {
            insertSpecialObject( child );

            // The float should be positioned taking into account the bottom margin
            // of the previous flow.  We add that margin into the height, get the
            // float positioned properly, and then subtract the margin out of the
            // height again. -dwh
            if (prevFlow)
                m_height += prevFlow->collapsedMarginBottom();
            positionNewFloats();
            if (prevFlow)
                m_height -= prevFlow->collapsedMarginBottom();

            //kdDebug() << "RenderBlock::layoutBlockChildren inserting float at "<< m_height <<" prevMargin="<<prevMargin << endl;
            child = child->nextSibling();
            continue;
        }

        // Note this occurs after the test for positioning and floating above, since
        // we want to ensure that we don't artificially increase our height because of
        // a positioned or floating child.
        if ( child->style()->flowAroundFloats() && !child->isFloating() &&
             style()->width().isFixed() && child->minWidth() > lineWidth( m_height ) ) {
            m_height = QMAX( m_height, floatBottom() );
            shouldCollapseChild = false;
            clearOccurred = true;
        }

        child->calcVerticalMargins();

        //kdDebug(0) << "margin = " << margin << " yPos = " << m_height << endl;

        // Try to guess our correct y position.  In most cases this guess will
        // be correct.  Only if we're wrong (when we compute the real y position)
        // will we have to relayout.
        int yPosEstimate = m_height;
        if (prevFlow)
        {
            yPosEstimate += QMAX(prevFlow->collapsedMarginBottom(), child->marginTop());
            if (prevFlow->yPos()+prevFlow->floatBottom() > yPosEstimate)
                child->setLayouted(false);
            else
                prevFlow=0;
        }
        else if (!canCollapseWithChildren || !topMarginContributor)
            yPosEstimate += child->marginTop();

        // Go ahead and position the child as though it didn't collapse with the top.
        child->setPos(child->xPos(), yPosEstimate);
        if ( !child->layouted() )
            child->layout();

        // Now determine the correct ypos based off examination of collapsing margin
        // values.
        if (shouldCollapseChild) {
            // Get our max pos and neg top margins.
            int posTop = child->maxTopMargin(true);
            int negTop = child->maxTopMargin(false);

            // XXX A hack we have to put in to deal with the fact
            // that KHTML morphs inlines with blocks
            // inside them into blocks themselves. -dwh
            if (!strictMode && child->style()->display() == INLINE && child->marginTop())
                posTop = negTop = 0;

            // See if the top margin is quirky. We only care if this child has
            // margins that will collapse with us.
            bool topQuirk = child->isTopMarginQuirk();

            if (canCollapseWithChildren && topMarginContributor && !clearOccurred) {
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
                // This child has no height.  Update our previous pos and neg
                // values and just keep going.
                if (posTop > prevPosMargin)
                    prevPosMargin = posTop;
                if (negTop > prevNegMargin)
                    prevNegMargin = negTop;

                if (!topMarginContributor)
                    // We need to make sure that the position of the self-collapsing block
                    // is correct, since it could have overflowing content
                    // that needs to be positioned correctly (e.g., a block that
                    // had a specified height of 0 but that actually had subcontent).
                    ypos = m_height + prevPosMargin - prevNegMargin;
            }
            else {
                if (!topMarginContributor ||
                    (!canCollapseWithChildren
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

                // XXX A hack we have to put in to deal with the fact
                // that KHTML morphs inlines with blocks
                // inside them into blocks themselves.
                if (!strictMode && child->style()->display() == INLINE && child->marginBottom())
                    prevPosMargin = prevNegMargin = 0;

                if (prevPosMargin-prevNegMargin) {
                    bottomChildQuirk = child->isBottomMarginQuirk();
                }
            }

            child->setPos(child->xPos(), ypos);
            if (ypos != yPosEstimate && (child->containsSpecial() || containsSpecial())) {
                // Our guess was wrong. Make the child lay itself out again.
                // XXXdwh some debugging code for this.
                // printf("WE WERE WRONG for object %d (%d, %d)!\n", (int)child, yPosEstimate, ypos);
                child->setLayouted(false);
                child->layout();
            }
        }

        // Now check for clear.
        if (checkClear(child)) {
            // The child needs to be lowered.  Move the child so that it just clears the float.
            child->setPos(child->xPos(), m_height);
            clearOccurred = true;

            if (topMarginContributor) {
                // We can no longer collapse with the top of the block since a clear
                // occurred.  The empty blocks collapse into the cleared block.
                // XXX This isn't quite correct.  Need clarification for what to do
                // if the height the cleared block is offset by is smaller than the
                // margins involved. -dwh
                m_maxTopPosMargin = oldPosMargin;
                m_maxTopNegMargin = oldNegMargin;
            }

            // If our value of clear caused us to be repositioned vertically to be
            // underneath a float, we have to do another layout to take into account
            // the extra space we now have available.
            child->setLayouted(false);
            child->layout();
        }

        // Reset the top margin contributor to false if we encountered
        // a non-empty child.  This has to be done after checking for clear,
        // so that margins can be reset if a clear occurred.
        if (topMarginContributor && !child->isSelfCollapsingBlock())
            topMarginContributor = false;

        int chPos = xPos + child->marginLeft();

        if(style()->direction() == LTR) {
            // html blocks flow around floats
            if ( ( style()->htmlHacks() || child->isTable() ) && child->style()->flowAroundFloats() ) 			{
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
            chPos -= child->width() + child->marginLeft() + child->marginRight();
            if ( ( style()->htmlHacks() || child->isTable() ) && child->style()->flowAroundFloats() )
                chPos -= leftOffset(m_height);
        }

        child->setPos(chPos, child->yPos());

        m_height += child->height();
        if (m_overflowHeight < m_height) {
            int overflowDelta = child->overflowHeight() - child->height();
            if (m_height + overflowDelta > m_overflowHeight)
                m_overflowHeight = m_height + overflowDelta;
        }

        if (child->isRenderBlock())
            prevFlow = static_cast<RenderBlock*>(child); 

        if ( child->hasOverhangingFloats() ) {
            // need to add the float to our special objects
            addOverHangingFloats( static_cast<RenderBlock *>(child), -child->xPos(), -child->yPos(), true );
        }

        // See if this child has made our overflow need to grow.
        // XXXdwh Work with left overflow as well as right overflow.
        int rightChildPos = child->overflowWidth() + child->xPos();
        if (rightChildPos > m_overflowWidth)
            m_overflowWidth = rightChildPos;

        child = child->nextSibling();
    }

    bool autoHeight = style()->height().isVariable() && style()->height().value == 0;

    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins.  To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it.  We also don't collapse if we had any bottom border/padding (represented by
    // |toAdd|).
    if (canCollapseWithChildren && (toAdd || !autoHeight))
        canCollapseWithChildren = false;

    // If we can't collapse with children then go ahead and add in the bottom margins.
    if (!canCollapseWithChildren
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

    if (canCollapseWithChildren && !topMarginContributor) {
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

    setLayouted();

    // kdDebug( 6040 ) << "layouted = " << layouted_ << endl;
}

void RenderBlock::layoutSpecialObjects( bool relayoutChildren )
{
    if (m_specialObjects) {
        //kdDebug( 6040 ) << renderName() << " " << this << "::layoutSpecialObjects() start" << endl;
        SpecialObject* r;
        QPtrListIterator<SpecialObject> it(*m_specialObjects);
        for ( ; (r = it.current()); ++it ) {
            //kdDebug(6040) << "   have a positioned object" << endl;
            if (r->type == SpecialObject::Positioned) {
                if ( relayoutChildren )
                    r->node->setLayouted( false );
                if ( !r->node->layouted() )
                    r->node->layout();
            }
        }
        m_specialObjects->sort();
    }
}

void RenderBlock::paint(QPainter* p, int _x, int _y, int _w, int _h, int _tx, int _ty, PaintAction paintAction)
{
    _tx += m_x;
    _ty += m_y;

    // check if we need to do anything at all...
    if (!overhangingContents() && !isRelPositioned() && !isPositioned() )
    {
        int h = m_height;
        if(m_specialObjects && floatBottom() > h) h = floatBottom();
        if((_ty > _y + _h) || (_ty + h < _y))
        {
            //kdDebug( 6040 ) << "cut!" << endl;
            return;
        }
    }

    paintObject(p, _x, _y, _w, _h, _tx, _ty, paintAction);
}

void RenderBlock::paintObject(QPainter *p, int _x, int _y,
                              int _w, int _h, int _tx, int _ty, PaintAction paintAction)
{

#ifdef DEBUG_LAYOUT
    //    kdDebug( 6040 ) << renderName() << "(RenderBlock) " << this << " ::paintObject() w/h = (" << width() << "/" << height() << ")" << endl;
#endif

    // 1. paint background, borders etc
    if (paintAction == PaintActionBackground &&
        shouldPaintBackgroundOrBorder() && style()->visibility() == VISIBLE )
        paintBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);

    // 2. paint contents
    RenderObject *child = firstChild();
    while(child != 0)
    {
        if(!child->layer() && !child->isFloating())
            child->paint(p, _x, _y, _w, _h, _tx, _ty, paintAction);
        child = child->nextSibling();
    }

    // 3. paint floats.
    if (paintAction == PaintActionFloat || paintAction == PaintActionSelection)
        paintFloats(p, _x, _y, _w, _h, _tx, _ty, paintAction == PaintActionSelection);

    if (paintAction == PaintActionBackground &&
        !childrenInline() && style()->outlineWidth())
        paintOutline(p, _tx, _ty, width(), height(), style());

#ifdef BOX_DEBUG
    if ( style() && style()->visibility() == VISIBLE ) {
        if(isAnonymousBox())
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
    if (!m_specialObjects)
        return;

    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*m_specialObjects);
    for ( ; (r = it.current()); ++it) {
        // Only paint the object if our noPaint flag isn't set.
        if (r->node->isFloating() && !r->noPaint) {
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
                               PaintActionBackground);
                r->node->paint(p, _x, _y, _w, _h,
                               _tx + r->left - r->node->xPos() + r->node->marginLeft(),
                               _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                               PaintActionFloat);
                r->node->paint(p, _x, _y, _w, _h,
                               _tx + r->left - r->node->xPos() + r->node->marginLeft(),
                               _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                               PaintActionForeground);
            }
        }
    }
}

void RenderBlock::insertSpecialObject(RenderObject *o)
{
    // Create the list of special objects if we don't aleady have one
    if (!m_specialObjects) {
        m_specialObjects = new QSortedList<SpecialObject>;
        m_specialObjects->setAutoDelete(true);
    }
    else {
        // Don't insert the object again if it's already in the list
        QPtrListIterator<SpecialObject> it(*m_specialObjects);
        SpecialObject* f;
        while ( (f = it.current()) ) {
            if (f->node == o) return;
            ++it;
        }
    }

    // Create the special object entry & append it to the list

    SpecialObject *newObj;
    if (o->isPositioned()) {
        // positioned object
        newObj = new SpecialObject(SpecialObject::Positioned);
        setOverhangingContents();
    }
    else if (o->isFloating()) {
        // floating object
        if ( !o->layouted() )
            o->layout();

        if(o->style()->floating() == FLEFT)
            newObj = new SpecialObject(SpecialObject::FloatLeft);
        else
            newObj = new SpecialObject(SpecialObject::FloatRight);

        newObj->startY = -1;
        newObj->endY = -1;
        newObj->width = o->width() + o->marginLeft() + o->marginRight();
    }
    else {
        // We should never get here, as insertSpecialObject() should only ever be called with positioned or floating
        // objects.
        KHTMLAssert(false);
        newObj = 0; // keep gcc's uninitialized variable warnings happy
    }

    newObj->count = m_specialObjects->count();
    newObj->node = o;

    m_specialObjects->append(newObj);
}

void RenderBlock::removeSpecialObject(RenderObject *o)
{
    if (m_specialObjects) {
        QPtrListIterator<SpecialObject> it(*m_specialObjects);
        while (it.current()) {
            if (it.current()->node == o)
                m_specialObjects->removeRef(it.current());
            ++it;
        }
    }
}

void RenderBlock::positionNewFloats()
{
    if(!m_specialObjects) return;
    SpecialObject *f = m_specialObjects->getLast();
    if(!f || f->startY != -1) return;
    SpecialObject *lastFloat;
    while(1)
    {
        lastFloat = m_specialObjects->prev();
        if(!lastFloat || (lastFloat->startY != -1 && !(lastFloat->type==SpecialObject::Positioned) )) {
            m_specialObjects->next();
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
        if (f->node->containingBlock()!=this || f->type==SpecialObject::Positioned)
        {
            f = m_specialObjects->next();
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
        if (o->style()->floating() == FLEFT)
        {
            if ( o->style()->clear() & CLEFT )
                y = QMAX( leftBottom(), y );
            int heightRemainingLeft = 1;
            int heightRemainingRight = 1;
            int fx = leftRelOffset(y,lo, &heightRemainingLeft);
            while (rightRelOffset(y,ro, &heightRemainingRight)-fx < fwidth)
            {
                y += QMIN( heightRemainingLeft, heightRemainingRight );
                fx = leftRelOffset(y,lo, &heightRemainingLeft);
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
            int fx = rightRelOffset(y,ro, &heightRemainingRight);
            while (fx - leftRelOffset(y,lo, &heightRemainingLeft) < fwidth)
            {
                y += QMIN(heightRemainingLeft, heightRemainingRight);
                fx = rightRelOffset(y,ro, &heightRemainingRight);
            }
            if (fx<f->width) fx=f->width;
            f->left = fx - f->width;
            //kdDebug( 6040 ) << "positioning right aligned float at (" << fx - o->marginRight() - o->width() << "/" << y + o->marginTop() << ")" << endl;
            o->setPos(fx - o->marginRight() - o->width(), y + o->marginTop());
        }
        f->startY = y;
        f->endY = f->startY + _height;


        //kdDebug( 6040 ) << "specialObject x/y= (" << f->left << "/" << f->startY << "-" << f->width << "/" << f->endY - f->startY << ")" << endl;

        f = m_specialObjects->next();
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
    int left = 0;

    left += borderLeft() + paddingLeft();

    if ( m_firstLine && style()->direction() == LTR ) {
        int cw=0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->contentWidth();
        left += style()->textIndent().minWidth(cw);
    }

    return left;
}

int
RenderBlock::leftRelOffset(int y, int fixedOffset, int *heightRemaining ) const
{
    int left = fixedOffset;
    if(!m_specialObjects)
        return left;

    if ( heightRemaining ) *heightRemaining = 1;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*m_specialObjects);
    for ( ; (r = it.current()); ++it )
    {
        //kdDebug( 6040 ) <<(void *)this << " left: sy, ey, x, w " << r->startY << "," << r->endY << "," << r->left << "," << r->width << " " << endl;
        if (r->startY <= y && r->endY > y &&
            r->type == SpecialObject::FloatLeft &&
            r->left + r->width > left) {
            left = r->left + r->width;
            if ( heightRemaining ) *heightRemaining = r->endY - y;
        }
    }
    //kdDebug( 6040 ) << "leftOffset(" << y << ") = " << left << endl;
    return left;
}

int
RenderBlock::rightOffset() const
{
    int right = m_width;

    right -= borderRight() + paddingRight();

    if ( m_firstLine && style()->direction() == RTL ) {
        int cw=0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->contentWidth();
        right += style()->textIndent().minWidth(cw);
    }

    return right;
}

int
RenderBlock::rightRelOffset(int y, int fixedOffset, int *heightRemaining ) const
{
    int right = fixedOffset;

    if (!m_specialObjects) return right;

    if (heightRemaining) *heightRemaining = 1;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*m_specialObjects);
    for ( ; (r = it.current()); ++it )
    {
        //kdDebug( 6040 ) << "right: sy, ey, x, w " << r->startY << "," << r->endY << "," << r->left << "," << r->width << " " << endl;
        if (r->startY <= y && r->endY > y &&
            r->type == SpecialObject::FloatRight &&
            r->left < right) {
            right = r->left;
            if ( heightRemaining ) *heightRemaining = r->endY - y;
        }
    }
    //kdDebug( 6040 ) << "rightOffset(" << y << ") = " << right << endl;
    return right;
}

unsigned short
RenderBlock::lineWidth(int y) const
{
    //kdDebug( 6040 ) << "lineWidth(" << y << ")=" << rightOffset(y) - leftOffset(y) << endl;
    return rightOffset(y) - leftOffset(y);
}

int
RenderBlock::nearestFloatBottom(int height) const
{
    if (!m_specialObjects) return 0;
    int bottom=0;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*m_specialObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>height && (r->endY<bottom || bottom==0) && (int)r->type <= (int)SpecialObject::FloatRight)
            bottom=r->endY;
    return bottom;
}

int
RenderBlock::floatBottom() const
{
    if (!m_specialObjects) return 0;
    int bottom=0;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*m_specialObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom && (int)r->type <= (int)SpecialObject::FloatRight)
            bottom=r->endY;
    return bottom;
}

int
RenderBlock::lowestPosition() const
{
    int bottom = RenderBox::lowestPosition();
    //kdDebug(0) << renderName() << "("<<this<<") lowest = " << bottom << endl;
    int lp = 0;
    RenderObject *last = 0;
    if ( !m_childrenInline ) {
        last = lastChild();
        while( last && (last->isPositioned() || last->isFloating()) )
            last = last->previousSibling();
        if( last )
            lp = last->yPos() + last->lowestPosition();
    }

    if(  lp > bottom )
        bottom = lp;

    //kdDebug(0) << "     bottom = " << bottom << endl;

    if (m_specialObjects) {
        SpecialObject* r;
        QPtrListIterator<SpecialObject> it(*m_specialObjects);
        for ( ; (r = it.current()); ++it ) {
            lp = 0;
            if ( r->type == SpecialObject::FloatLeft || r->type == SpecialObject::FloatRight ){
                lp = r->startY + r->node->lowestPosition();
                //kdDebug(0) << r->node->renderName() << " lp = " << lp << "startY=" << r->startY << endl;
            } else if ( r->type == SpecialObject::Positioned ) {
                lp = r->node->yPos() + r->node->lowestPosition();
            }
            if( lp > bottom)
                bottom = lp;
        }
    }

    if ( overhangingContents() ) {
        RenderObject *child = firstChild();
        while( child ) {
            if ( child != last && child->overhangingContents() ) {
                int lp = child->yPos() + child->lowestPosition();
                if ( lp > bottom ) bottom = lp;
            }
            child = child->nextSibling();
        }
    }

    //kdDebug(0) << renderName() << "      bottom final = " << bottom << endl;
    return bottom;
}

int RenderBlock::rightmostPosition() const
{
    int right = RenderBox::rightmostPosition();

    RenderObject *c;
    for (c = firstChild(); c; c = c->nextSibling()) {
        if (!c->isPositioned() && !c->isFloating()) {
            int childRight = c->xPos() + c->rightmostPosition();
            if (childRight > right)
                right = childRight;
        }
    }

    if (m_specialObjects) {
        SpecialObject* r;
        QPtrListIterator<SpecialObject> it(*m_specialObjects);
        for ( ; (r = it.current()); ++it ) {
            int specialRight=0;
            if ( r->type == SpecialObject::FloatLeft || r->type == SpecialObject::FloatRight ){
                specialRight = r->left + r->node->rightmostPosition();
            } else if ( r->type == SpecialObject::Positioned ) {
                specialRight = r->node->xPos() + r->node->rightmostPosition();
            }
            if (specialRight > right)
                right = specialRight;
        }
    }

    if ( overhangingContents() ) {
        RenderObject *child = firstChild();
        while( child ) {
            if ( (child->isPositioned() || child->isFloating()) && child->overhangingContents() ) {
                int r = child->xPos() + child->rightmostPosition();
                if ( r > right ) right = r;
            }
            child = child->nextSibling();
        }
    }

    return right;
}


int
RenderBlock::leftBottom()
{
    if (!m_specialObjects) return 0;
    int bottom=0;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*m_specialObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom && r->type == SpecialObject::FloatLeft)
            bottom=r->endY;

    return bottom;
}

int
RenderBlock::rightBottom()
{
    if (!m_specialObjects) return 0;
    int bottom=0;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*m_specialObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom && r->type == SpecialObject::FloatRight)
            bottom=r->endY;

    return bottom;
}

void
RenderBlock::clearFloats()
{
    //kdDebug( 6040 ) << this <<" clearFloats" << endl;
    if (m_specialObjects) {
        if( overhangingContents() ) {
            m_specialObjects->first();
            while ( m_specialObjects->current()) {
                if ( !(m_specialObjects->current()->type == SpecialObject::Positioned) )
                    m_specialObjects->remove();
                else
                    m_specialObjects->next();
            }
        } else
            m_specialObjects->clear();
    }

    if (isFloating() || isPositioned()) return;

    RenderObject *prev = previousSibling();

    // find the element to copy the floats from
    // pass non-flows
    // pass fAF's unless they contain overhanging stuff
    bool parentHasFloats = false;
    while (prev) {
        if (!(prev->isRenderBlock() && prev->isRenderInline()) || prev->isFloating() ||
            (prev->style()->flowAroundFloats() &&
             // A <table> or <ul> can have a height of 0, so its ypos may be the same
             // as m_y.  That's why we have a <= and not a < here. -dwh
             (static_cast<RenderBlock *>(prev)->floatBottom()+prev->yPos() <= m_y ))) {
            if ( prev->isFloating() && parent()->isRenderBlock() ) {
                parentHasFloats = true;
            }
            prev = prev->previousSibling();
        } else
            break;
    }

    int offset = m_y;

    if ( parentHasFloats ) {
        addOverHangingFloats( static_cast<RenderBlock *>( parent() ), parent()->borderLeft() + parent()->paddingLeft() , offset, false );
    }

    if(prev ) {
        if(prev->isTableCell()) return;

        offset -= prev->yPos();
    } else {
        prev = parent();
        if(!prev) return;
    }
    //kdDebug() << "RenderBlock::clearFloats found previous "<< (void *)this << " prev=" << (void *)prev<< endl;

    // add overhanging special objects from the previous RenderBlock
    if(!prev->isRenderBlock()) return;
    RenderBlock * flow = static_cast<RenderBlock *>(prev);
    if(!flow->m_specialObjects) return;
    if( ( style()->htmlHacks() || isTable() ) && style()->flowAroundFloats())
        return; //html tables and lists flow as blocks

    if(flow->floatBottom() > offset)
        addOverHangingFloats( flow, 0, offset );
}

void RenderBlock::addOverHangingFloats( RenderBlock *flow, int xoff, int offset, bool child )
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << (void *)this << ": adding overhanging floats xoff=" << xoff << "  offset=" << offset << " child=" << child << endl;
#endif
    if ( !flow->m_specialObjects || (child && flow->layer()) )
        return;

    // we have overhanging floats
    if (!m_specialObjects) {
        m_specialObjects = new QSortedList<SpecialObject>;
        m_specialObjects->setAutoDelete(true);
    }

    QPtrListIterator<SpecialObject> it(*flow->m_specialObjects);
    SpecialObject *r;
    for ( ; (r = it.current()); ++it ) {
        if ( (int)r->type <= (int)SpecialObject::FloatRight &&
             ( ( !child && r->endY > offset ) ||
               ( child && flow->yPos() + r->endY > height() ) ) ) {

            if (child)
                r->noPaint = true;

            SpecialObject* f = 0;
            // don't insert it twice!
            QPtrListIterator<SpecialObject> it(*m_specialObjects);
            while ( (f = it.current()) ) {
                if (f->node == r->node) break;
                ++it;
            }
            if ( !f ) {
                SpecialObject *special = new SpecialObject(r->type);
                special->count = m_specialObjects->count();
                special->startY = r->startY - offset;
                special->endY = r->endY - offset;
                special->left = r->left - xoff;
                // Applying the child's margin makes no sense in the case where the child was passed in.
                // since his own margin was added already through the subtraction of the |xoff| variable
                // above.  |xoff| will equal -flow->marginLeft() in this case, so it's already been taken
                // into account.  Only apply this code if |child| is false, since otherwise the left margin
                // will get applied twice. -dwh
                if (!child && flow != parent())
                    special->left += flow->marginLeft();
                if ( !child ) {
                    special->left -= marginLeft();
                    special->noPaint = true;
                }
                special->width = r->width;
                special->node = r->node;
                m_specialObjects->append(special);
#ifdef DEBUG_LAYOUT
                kdDebug( 6040 ) << "addOverHangingFloats x/y= (" << special->left << "/" << special->startY << "-" << special->width << "/" << special->endY - special->startY << ")" << endl;
#endif
            }
        }
    }
}

bool RenderBlock::checkClear(RenderObject *child)
{
    //kdDebug( 6040 ) << "checkClear oldheight=" << m_height << endl;
    int bottom = 0;
    switch(child->style()->clear())
    {
        case CNONE:
            return false;
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

    if (m_height < bottom) {
        m_height = bottom;
        return true;
    }
    return false;
}

bool RenderBlock::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, bool inBox)
{
    if (m_specialObjects) {
        int stx = _tx + xPos();
        int sty = _ty + yPos();
        if (isRoot()) {
            stx += static_cast<RenderRoot*>(this)->view()->contentsX();
            sty += static_cast<RenderRoot*>(this)->view()->contentsY();
        }
        SpecialObject* o;
        QPtrListIterator<SpecialObject> it(*m_specialObjects);
        for (it.toLast(); (o = it.current()); --it)
            if (o->node->isFloating() && !o->noPaint)
                inBox |= o->node->nodeAtPoint(info, _x, _y,
                                              stx+o->left + o->node->marginLeft() - o->node->xPos(),
                                              sty+o->startY + o->node->marginTop() - o->node->yPos());
    }

    inBox |= RenderFlow::nodeAtPoint(info, _x, _y, _tx, _ty, inBox);
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

    if (preOrNowrap && childrenInline())
        m_minWidth = m_maxWidth;

    if (style()->width().isFixed() && style()->width().value > 0)
        m_maxWidth = KMAX(m_minWidth,short(style()->width().value));

    int toAdd = 0;
    toAdd = borderLeft() + borderRight() + paddingLeft() + paddingRight();

    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    // Scrolling marquees like to use this trick:
    // <td><div style="overflow:hidden; width:300px"><nobr>.....[lots of text].....</nobr></div></td>
    // We need to sanity-check our m_minWidth, and not let it exceed our clipped boundary. -dwh
    if (style()->overflow() == OHIDDEN && m_minWidth > m_width)
        m_minWidth = m_width;

    setMinMaxKnown();

    //kdDebug( 6040 ) << "Text::calcMinMaxWidth(" << this << "): min = " << m_minWidth << " max = " << m_maxWidth << endl;
    // ### compare with min/max width set in style sheet...
}

static inline RenderObject *next(RenderObject *par, RenderObject *current)
{
    RenderObject *next = 0;
    while(current != 0)
    {
        //kdDebug( 6040 ) << "current = " << current << endl;
        if(!current->isFloating() && !current->isReplaced() && !current->isPositioned())
            next = current->firstChild();
        if(!next) {
            while(current && current != par) {
                next = current->nextSibling();
                if(next) break;
                current = current->parent();
            }
        }

        if(!next) break;

        if(next->isText() || next->isBR() || next->isFloating() || next->isReplaced() || next->isPositioned())
            break;
        current = next;
    }
    return next;
}

void RenderBlock::calcInlineMinMaxWidth()
{
    int inlineMax=0;
    int inlineMin=0;

    int cw = containingBlock()->contentWidth();

    RenderObject *child = firstChild();

    // If we are at the start of a line, we want to ignore all white-space.
    // Also strip spaces if we previously had text that ended in a trailing space.
    bool stripFrontSpaces = true;
    RenderObject* trailingSpaceChild = 0;

    bool nowrap, oldnowrap;
    nowrap = oldnowrap = style()->whiteSpace() == NOWRAP;

    while(child != 0)
    {
        // positioned children don't affect the minmaxwidth
        if (child->isPositioned())
        {
            child = next(this, child);
            continue;
        }

        nowrap = child->style()->whiteSpace() == NOWRAP;

        if( !child->isBR() )
        {
            // Step One: determine whether or not we need to go ahead and
            // terminate our current line.  Each discrete chunk can become
            // the new min-width, if it is the widest chunk seen so far, and
            // it can also become the max-width.

            // Children fall into three categories:
            // (1) An inline flow object.  These objects always have a min/max of 0,
            // and are included in the iteration solely so that their margins can
            // be added in.  XXXdwh Just adding in the margins is totally bogus, since
            // a <span> could wrap to multiple lines.  Technically the left margin should
            // be considered part of the first descendant's start, and the right margin
            // should be considered part of the last descendant's end.  Leave this alone
            // for now, but fix later.
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
                // Case (1) and (2).  Inline replaced and inline flow elements.  Both
                // add in their margins to their min/max values.
                int margins = 0;
                LengthType type = cstyle->marginLeft().type;
                if ( type != Variable )
                    margins += (type == Fixed ? cstyle->marginLeft().value : child->marginLeft());
                type = cstyle->marginRight().type;
                if ( type != Variable )
                    margins += (type == Fixed ? cstyle->marginRight().value : child->marginRight());
                childMin += margins;
                childMax += margins;

                if (child->isRenderInline()) {
                    // Add in padding for inline flow elements.  This is wrong in the
                    // same way the margin addition is wrong. XXXdwh fixme.
                    int padding = 0;
                    type = cstyle->paddingLeft().type;
                    if ( type != Variable )
                        padding += (type == Fixed ? cstyle->paddingLeft().value : child->paddingLeft());
                    type = cstyle->paddingRight().type;
                    if ( type != Variable )
                        padding += (type == Fixed ? cstyle->paddingRight().value : child->paddingRight());
                    childMin += padding;
                    childMax += padding;

                    inlineMin += childMin;
                    inlineMax += childMax;
                }
            }

            if (!child->isRenderInline() && !child->isText()) {
                // Case (2). Inline replaced elements.
                // Go ahead and terminate the current line as far as
                // minwidth is concerned.
                childMin += child->minWidth();
                childMax += child->maxWidth();

                if (!nowrap || !oldnowrap) {
                    if(m_minWidth < inlineMin) m_minWidth = inlineMin;
                    inlineMin = 0;
                }

                // Add our width to the max.
                inlineMax += childMax;

                if (nowrap)
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
                if (!child->isFloating())
                    stripFrontSpaces = false;
                trailingSpaceChild = 0;
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
                if (stripFrontSpaces)
                    trailingSpaceChild = child;
                else
                    trailingSpaceChild = 0;

                // Add in text-indent.
                int ti = cstyle->textIndent().minWidth(cw);
                childMin+=ti; beginMin += ti;
                childMax+=ti; beginMax += ti;

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

        oldnowrap = nowrap;

        child = next(this, child);
    }

    if (trailingSpaceChild && trailingSpaceChild->isText() && !m_pre) {
        // Collapse away the trailing space at the end of a block.
        RenderText* t = static_cast<RenderText *>(trailingSpaceChild);
        const Font *f = t->htmlFont( false );
        QChar space[1]; space[0] = ' ';
        int spaceWidth = f->width(space, 1, 0);
        inlineMax -= spaceWidth;
        if (inlineMin > inlineMax)
            inlineMin = inlineMax;
    }

    if(m_minWidth < inlineMin) m_minWidth = inlineMin;
    if(m_maxWidth < inlineMax) m_maxWidth = inlineMax;
    //         kdDebug( 6040 ) << "m_minWidth=" << m_minWidth
    // 			<< " m_maxWidth=" << m_maxWidth << endl;
}

void RenderBlock::calcBlockMinMaxWidth()
{
    bool nowrap = style()->whiteSpace() == NOWRAP;

    RenderObject *child = firstChild();
    while(child != 0)
    {
        // positioned children don't affect the minmaxwidth
        if (child->isPositioned())
        {
            child = child->nextSibling();
            continue;
        }

        int margin=0;
        //  auto margins don't affect minwidth

        Length ml = child->style()->marginLeft();
        Length mr = child->style()->marginRight();

        if (style()->textAlign() == KONQ_CENTER)
        {
            if (ml.type==Fixed) margin+=ml.value;
            if (mr.type==Fixed) margin+=mr.value;
        }
        else
        {
            // Call calcWidth on the child to ensure that our margins are
            // up to date.  This method can be called before the child has actually
            // calculated its margins (which are computed inside calcWidth).
            child->calcWidth();

            if (!(ml.type==Variable) && !(mr.type==Variable))
            {
                if (!(child->style()->width().type==Variable))
                {
                    if (child->style()->direction()==LTR)
                        margin = child->marginLeft();
                    else
                        margin = child->marginRight();
                }
                else
                    margin = child->marginLeft()+child->marginRight();

            }
            else if (!(ml.type == Variable))
                margin = child->marginLeft();
            else if (!(mr.type == Variable))
                margin = child->marginRight();
        }

        if (margin<0) margin=0;

        int w = child->minWidth() + margin;
        if(m_minWidth < w) m_minWidth = w;
        // IE ignores tables for calculation of nowrap. Makes some sense.
        if ( nowrap && !child->isTable() && m_maxWidth < w )
            m_maxWidth = w;

        w = child->maxWidth() + margin;
        if(m_maxWidth < w) m_maxWidth = w;
        child = child->nextSibling();
    }
}

void RenderBlock::close()
{
    if (lastChild() && lastChild()->isAnonymousBox())
        lastChild()->close();

    RenderFlow::close();
}

const char *RenderBlock::renderName() const
{
    if (isFloating())
        return "RenderBlock (floating)";
    if (isPositioned())
        return "RenderBlock (positioned)";
    if (isAnonymousBox())
        return "RenderBlock (anonymous)";
    if (isRelPositioned())
        return "RenderBlock (relative positioned)";
    return "RenderBlock";
}

#ifndef NDEBUG
void RenderBlock::printTree(int indent) const
{
    RenderFlow::printTree(indent);

    if (m_specialObjects)
    {
        QPtrListIterator<SpecialObject> it(*m_specialObjects);
        SpecialObject *r;
        for ( ; (r = it.current()); ++it )
        {
            QString s;
            s.fill(' ', indent);
            kdDebug() << s << renderName() << ":  " <<
                (r->type == SpecialObject::FloatLeft ? "FloatLeft" : (r->type == SpecialObject::FloatRight ? "FloatRight" : "Positioned"))  <<
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

    if (m_specialObjects && !m_specialObjects->isEmpty())
    {
        *stream << " special(";
        QPtrListIterator<SpecialObject> it(*m_specialObjects);
        SpecialObject *r;
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


