/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2002 Apple Computer, Inc.
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
//#define DEBUG
//#define DEBUG_LAYOUT
//#define BOX_DEBUG
//#define FLOAT_DEBUG

#include <kdebug.h>
#include <assert.h>
#include <qpainter.h>
#include <kglobal.h>

#include "rendering/render_flow.h"
#include "rendering/render_text.h"
#include "rendering/render_table.h"
#include "rendering/render_root.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "html/html_formimpl.h"

#include "khtmlview.h"
#include "htmltags.h"

using namespace DOM;
using namespace khtml;

RenderFlow::RenderFlow(DOM::NodeImpl* node)
    : RenderBox(node)
{
    m_childrenInline = true;
    m_pre = false;
    firstLine = false;
    m_clearStatus = CNONE;

    specialObjects = 0;
    
    m_maxTopPosMargin = m_maxTopNegMargin = m_maxBottomPosMargin = m_maxBottomNegMargin = 0;
    m_topMarginQuirk = m_bottomMarginQuirk = false;
    m_overflowHeight = 0;
    m_overflowWidth = 0;
    
    m_continuation = 0;
}

void RenderFlow::setStyle(RenderStyle *_style)
{

//    kdDebug( 6040 ) << (void*)this<< " renderFlow(" << renderName() << ")::setstyle()" << endl;

    RenderBox::setStyle(_style);

    if(isPositioned())
        setInline(false);
    
    if (!isTableCell() && (isPositioned() || isRelPositioned() || style()->overflow()==OHIDDEN) && !m_layer)
        m_layer = new (renderArena()) RenderLayer(this);
    
    if(isFloating() || !style()->display() == INLINE)
        setInline(false);

    if (isInline() && !m_childrenInline)
        setInline(false);

    m_pre = false;
    if(style()->whiteSpace() == PRE)
        m_pre = true;

    // ### we could save this call when the change only affected
    // non inherited properties
    RenderObject *child = firstChild();
    while(child != 0)
    {
        if(child->isAnonymousBox())
        {
            RenderStyle* newStyle = new RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);
            child->setStyle(newStyle);
            child->setIsAnonymousBox(true);
        }
        child = child->nextSibling();
    }
    
    // Ensure that all of the split inlines pick up the new style. We
    // only do this if we're an inline, since we don't want to propagate
    // a block's style to the other inlines.
    // e.g., <font>foo <h4>goo</h4> moo</font>.  The <font> inlines before
    // and after the block share the same style, but the block doesn't
    // need to pass its style on to anyone else.
    if (isInline()) {
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
}

RenderFlow::~RenderFlow()
{
    delete specialObjects;
}

void RenderFlow::paint(QPainter *p, int _x, int _y, int _w, int _h,
                       int _tx, int _ty, int paintPhase)
{

#ifdef DEBUG_LAYOUT
//    kdDebug( 6040 ) << renderName() << "(RenderFlow) " << this << " ::print() x/y/w/h = ("  << xPos() << "/" << yPos() << "/" << width() << "/" << height()  << ")" << endl;
#endif

    if(!isInline())
    {
        _tx += m_x;
        _ty += m_y;
    }

    // check if we need to do anything at all...
    if(!isInline() && !overhangingContents() && !isRelPositioned() && !isPositioned() )
    {
        int h = m_height;
        if(specialObjects && floatBottom() > h) h = floatBottom();
        if((_ty > _y + _h) || (_ty + h < _y))
        {
            //kdDebug( 6040 ) << "cut!" << endl;
            return;
        }
    }

    paintObject(p, _x, _y, _w, _h, _tx, _ty, paintPhase);
}

void RenderFlow::paintObject(QPainter *p, int _x, int _y,
                             int _w, int _h, int _tx, int _ty, int paintPhase)
{

#ifdef DEBUG_LAYOUT
//    kdDebug( 6040 ) << renderName() << "(RenderFlow) " << this << " ::paintObject() w/h = (" << width() << "/" << height() << ")" << endl;
#endif
    
    // 1. paint background, borders etc
    if (paintPhase == BACKGROUND_PHASE && 
        shouldPaintBackgroundOrBorder() && !isInline() && style()->visibility() == VISIBLE )
        paintBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);

    // 2. paint contents
    RenderObject *child = firstChild();
    while(child != 0)
    {
        if(!child->layer() && !child->isFloating())
            child->paint(p, _x, _y, _w, _h, _tx, _ty, paintPhase);
        child = child->nextSibling();
    }

    // 3. paint floats.
    if (paintPhase == FLOAT_PHASE)
        paintFloats(p, _x, _y, _w, _h, _tx, _ty);
    
    if (paintPhase == BACKGROUND_PHASE &&
        !isInline() && !childrenInline() && style()->outlineWidth())
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

void RenderFlow::paintFloats(QPainter *p, int _x, int _y,
                             int _w, int _h, int _tx, int _ty)
{
    if (!specialObjects)
        return;
        
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*specialObjects);
    for ( ; (r = it.current()); ++it) {
        // Only paint the object if our noPaint flag isn't set.
        if (r->node->isFloating() && !r->noPaint) {
            r->node->paint(p, _x, _y, _w, _h, 
                           _tx + r->left - r->node->xPos() + r->node->marginLeft(), 
                           _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                           BACKGROUND_PHASE);
            r->node->paint(p, _x, _y, _w, _h, 
                           _tx + r->left - r->node->xPos() + r->node->marginLeft(), 
                           _ty + r->startY - r->node->yPos() + r->node->marginTop(),
                           FLOAT_PHASE);
            r->node->paint(p, _x, _y, _w, _h, 
                           _tx + r->left - r->node->xPos() + r->node->marginLeft(), 
                           _ty + r->startY - r->node->yPos() + r->node->marginTop(), 
                           FOREGROUND_PHASE);
        }
    }
}

void RenderFlow::layout()
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
    if ( isTableCell() ) //&& static_cast<RenderTableCell *>(this)->widthChanged() )
        relayoutChildren = true;

//     kdDebug( 6040 ) << specialObjects << "," << oldWidth << ","
//                     << m_width << ","<< layouted() << "," << isAnonymousBox() << ","
//                     << overhangingContents() << "," << isPositioned() << endl;

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderFlow) " << this << " ::layout() width=" << m_width << ", layouted=" << layouted() << endl;
    if(containingBlock() == static_cast<RenderObject *>(this))
        kdDebug( 6040 ) << renderName() << ": containingBlock == this" << endl;
#endif

    // This is an incorrect optimization.  You cannot know at this point whether or not a child will overhang
    // in the horizontal direction without laying out your children.  The following test case illustrates this
    // point, as it will fail with this code commented back in.
    // <html>
    // <body style="width:0px">
    // Hello world!
    // </body>
    // </html>
    //
    // In the real world, this affects (as of 7/24/2002) http://viamichelin.com/. -- dwh
    // 
    /*   if(m_width<=0 && !isPositioned() && !overhangingContents()) {
        if(m_y < 0) m_y = 0;
        setLayouted();
        return;
    }
    */
    
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
        if (m_marginTop >= 0)
            m_maxTopPosMargin = m_marginTop;
        else
            m_maxTopNegMargin = -m_marginTop;
        if (m_marginBottom >= 0)
            m_maxBottomPosMargin = m_marginBottom;
        else
            m_maxBottomNegMargin = -m_marginBottom;
            
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
        if (lastChild() && lastChild()->hasOverhangingFloats() ) {
            m_height = lastChild()->yPos() + static_cast<RenderFlow*>(lastChild())->floatBottom();
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

void RenderFlow::layoutSpecialObjects( bool relayoutChildren )
{
    if(specialObjects) {
	//kdDebug( 6040 ) << renderName() << " " << this << "::layoutSpecialObjects() start" << endl;
        SpecialObject* r;
        QPtrListIterator<SpecialObject> it(*specialObjects);
        for ( ; (r = it.current()); ++it ) {
            //kdDebug(6040) << "   have a positioned object" << endl;
            if (r->type == SpecialObject::Positioned) {
                if ( relayoutChildren )
                    r->node->setLayouted( false );
                if ( !r->node->layouted() )
                    r->node->layout();
            }
        }
        specialObjects->sort();
    }
}

void RenderFlow::layoutBlockChildren( bool relayoutChildren )
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << " layoutBlockChildren( " << this <<" ), relayoutChildren="<< relayoutChildren << endl;
#endif

    int xPos = 0;
    int toAdd = 0;

    m_height = 0;

    if(style()->hasBorder())
    {
        xPos += borderLeft();
        m_height += borderTop();
        toAdd += borderBottom();
    }
    if (hasPadding())
    {
        xPos += paddingLeft();
        m_height += paddingTop();
        toAdd += paddingBottom();
    }

    int minHeight = m_height + toAdd;
    m_overflowHeight = m_height;
    
    if( style()->direction() == RTL ) {
        xPos = marginLeft() + m_width - paddingRight() - borderRight();
    }

    RenderObject *child = firstChild();
    RenderFlow *prevFlow = 0;

    // Whether or not we can collapse our own margins with our children.  We don't do this
    // if we had any border/padding (obviously), if we're the root or HTML elements, or if
    // we're positioned, floating, a table cell.
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
    
    bool strictMode = isAnonymousBox() ? true : (element()->getDocument()->parseMode() == DocumentImpl::Strict);
     
    //kdDebug() << "RenderFlow::layoutBlockChildren " << prevMargin << endl;

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
        if ( child->style()->flowAroundFloats() && !child->isFloating() &&
                style()->width().isFixed() && child->minWidth() > lineWidth( m_height ) ) {
            m_height = QMAX( m_height, floatBottom() );
            shouldCollapseChild = false;
            clearOccurred = true;
        }
	
//         kdDebug( 6040 ) << "   " << child->renderName() << " loop " << child << ", " << child->isInline() << ", " << child->layouted() << endl;
//         kdDebug( 6040 ) << t.elapsed() << endl;
        // ### might be some layouts are done two times... FIX that.

        if (child->isPositioned())
        {
            static_cast<RenderFlow*>(child->containingBlock())->insertSpecialObject(child);
	    //kdDebug() << "RenderFlow::layoutBlockChildren inserting positioned into " << child->containingBlock()->renderName() << endl;
            
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
                
            //kdDebug() << "RenderFlow::layoutBlockChildren inserting float at "<< m_height <<" prevMargin="<<prevMargin << endl;
            child = child->nextSibling();
            continue;
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
                    int cw;
                    RenderObject *cb = child->containingBlock();
                    if ( cb->isFlow() )
                        cw = static_cast<RenderFlow *>(cb)->lineWidth( child->yPos() );
                    else
                        cw = cb->contentWidth();
                    static_cast<RenderBox*>(child)->calcHorizontalMargins															( child->style()->marginLeft(), 				                                          								  child->style()->marginRight(), 
                                  cw);
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
        
        if (child->isFlow())
            prevFlow = static_cast<RenderFlow*>(child);

        if ( child->hasOverhangingFloats() ) {
            // need to add the float to our special objects
            addOverHangingFloats( static_cast<RenderFlow *>(child), -child->xPos(), -child->yPos(), true );
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
    // with it.
    if (canCollapseWithChildren && !autoHeight)
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

bool RenderFlow::checkClear(RenderObject *child)
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

void RenderFlow::insertSpecialObject(RenderObject *o)
{
    // Create the list of special objects if we don't aleady have one
    if (!specialObjects) {
	specialObjects = new QSortedList<SpecialObject>;
	specialObjects->setAutoDelete(true);
    }
    else {
	// Don't insert the object again if it's already in the list
	QPtrListIterator<SpecialObject> it(*specialObjects);
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

    newObj->count = specialObjects->count();
    newObj->node = o;

    specialObjects->append(newObj);
}

void RenderFlow::removeSpecialObject(RenderObject *o)
{
    if (specialObjects) {
        QPtrListIterator<SpecialObject> it(*specialObjects);
	while (it.current()) {
	    if (it.current()->node == o)
		specialObjects->removeRef(it.current());
	    ++it;
	}
    }
}

void RenderFlow::positionNewFloats()
{
    if(!specialObjects) return;
    SpecialObject *f = specialObjects->getLast();
    if(!f || f->startY != -1) return;
    SpecialObject *lastFloat;
    while(1)
    {
        lastFloat = specialObjects->prev();
        if(!lastFloat || (lastFloat->startY != -1 && !(lastFloat->type==SpecialObject::Positioned) )) {
            specialObjects->next();
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
            f = specialObjects->next();
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

        f = specialObjects->next();
    }
}

void RenderFlow::newLine()
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
RenderFlow::leftOffset() const
{
    int left = 0;

    if (style()->hasBorder())
        left += borderLeft();
    if (hasPadding())
        left += paddingLeft();

    if ( firstLine && style()->direction() == LTR ) {
        int cw=0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->contentWidth();
        left += style()->textIndent().minWidth(cw);
    }

    return left;
}

int
RenderFlow::leftRelOffset(int y, int fixedOffset, int *heightRemaining ) const
{
    int left = fixedOffset;
    if(!specialObjects)
	return left;

    if ( heightRemaining ) *heightRemaining = 1;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*specialObjects);
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
RenderFlow::rightOffset() const
{
    int right = m_width;

    if (style()->hasBorder())
        right -= borderRight();
    if (hasPadding())
        right -= paddingRight();

    if ( firstLine && style()->direction() == RTL ) {
        int cw=0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->contentWidth();
        right += style()->textIndent().minWidth(cw);
    }

    return right;
}

int
RenderFlow::rightRelOffset(int y, int fixedOffset, int *heightRemaining ) const
{
    int right = fixedOffset;

    if (!specialObjects) return right;

    if (heightRemaining) *heightRemaining = 1;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*specialObjects);
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
RenderFlow::lineWidth(int y) const
{
    //kdDebug( 6040 ) << "lineWidth(" << y << ")=" << rightOffset(y) - leftOffset(y) << endl;
    return rightOffset(y) - leftOffset(y);
}

int
RenderFlow::floatBottom() const
{
    if (!specialObjects) return 0;
    int bottom=0;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*specialObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom && (int)r->type <= (int)SpecialObject::FloatRight)
            bottom=r->endY;
    return bottom;
}

int
RenderFlow::lowestPosition() const
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

    if (specialObjects) {
        SpecialObject* r;
        QPtrListIterator<SpecialObject> it(*specialObjects);
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

int RenderFlow::rightmostPosition() const
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

    if (specialObjects) {
        SpecialObject* r;
        QPtrListIterator<SpecialObject> it(*specialObjects);
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
RenderFlow::leftBottom()
{
    if (!specialObjects) return 0;
    int bottom=0;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*specialObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom && r->type == SpecialObject::FloatLeft)
            bottom=r->endY;

    return bottom;
}

int
RenderFlow::rightBottom()
{
    if (!specialObjects) return 0;
    int bottom=0;
    SpecialObject* r;
    QPtrListIterator<SpecialObject> it(*specialObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom && r->type == SpecialObject::FloatRight)
            bottom=r->endY;

    return bottom;
}

void
RenderFlow::clearFloats()
{
    //kdDebug( 6040 ) << this <<" clearFloats" << endl;

    if (specialObjects) {
	if( overhangingContents() ) {
            specialObjects->first();
            while ( specialObjects->current()) {
		if ( !(specialObjects->current()->type == SpecialObject::Positioned) )
		    specialObjects->remove();
                else
		    specialObjects->next();
	    }
	} else
	    specialObjects->clear();
    }

    if (isFloating() || isPositioned()) return;

    RenderObject *prev = previousSibling();

    // find the element to copy the floats from
    // pass non-flows
    // pass fAF's unless they contain overhanging stuff
    bool parentHasFloats = false;
    while (prev) {
	if (!prev->isFlow() || prev->isFloating() ||
	    (prev->style()->flowAroundFloats() &&
        // A <table> or <ul> can have a height of 0, so its ypos may be the same
        // as m_y.  That's why we have a <= and not a < here. -dwh
	     (static_cast<RenderFlow *>(prev)->floatBottom()+prev->yPos() <= m_y ))) {
	    if ( prev->isFloating() && parent()->isFlow() ) {
		parentHasFloats = true;
	    }
	    prev = prev->previousSibling();
	} else
	    break;
    }

    int offset = m_y;

    if ( parentHasFloats ) {
	addOverHangingFloats( static_cast<RenderFlow *>( parent() ), parent()->borderLeft() + parent()->paddingLeft() , offset, false );
    }

    if(prev ) {
        if(prev->isTableCell()) return;

        offset -= prev->yPos();
    } else {
        prev = parent();
	if(!prev) return;
    }
    //kdDebug() << "RenderFlow::clearFloats found previous "<< (void *)this << " prev=" << (void *)prev<< endl;

    // add overhanging special objects from the previous RenderFlow
    if(!prev->isFlow()) return;
    RenderFlow * flow = static_cast<RenderFlow *>(prev);
    if(!flow->specialObjects) return;
    if( ( style()->htmlHacks() || isTable() ) && style()->flowAroundFloats())
        return; //html tables and lists flow as blocks

    if(flow->floatBottom() > offset)
	addOverHangingFloats( flow, 0, offset );
}

void RenderFlow::addOverHangingFloats( RenderFlow *flow, int xoff, int offset, bool child )
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << (void *)this << ": adding overhanging floats xoff=" << xoff << "  offset=" << offset << " child=" << child << endl;
#endif
    if ( !flow->specialObjects )
        return;

    // we have overhanging floats
    if(!specialObjects) {
        specialObjects = new QSortedList<SpecialObject>;
        specialObjects->setAutoDelete(true);
    }

    QPtrListIterator<SpecialObject> it(*flow->specialObjects);
    SpecialObject *r;
    for ( ; (r = it.current()); ++it ) {
        if ( (int)r->type <= (int)SpecialObject::FloatRight &&
            ( ( !child && r->endY > offset ) ||
            ( child && flow->yPos() + r->endY > height() ) ) ) {
            
            if ( child )
                r->noPaint = true;
                
            SpecialObject* f = 0;
            // don't insert it twice!
            QPtrListIterator<SpecialObject> it(*specialObjects);
            while ( (f = it.current()) ) {
            if (f->node == r->node) break;
            ++it;
            }
            if ( !f ) {
                SpecialObject *special = new SpecialObject(r->type);
                special->count = specialObjects->count();
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
                specialObjects->append(special);
#ifdef DEBUG_LAYOUT
                kdDebug( 6040 ) << "addOverHangingFloats x/y= (" << special->left << "/" << special->startY << "-" << special->width << "/" << special->endY - special->startY << ")" << endl;
#endif
            }
        }
    }
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

void RenderFlow::calcInlineMinMaxWidth()
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
                
                if (child->isInline() && child->isFlow()) {
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
            
            if (!(child->isInline() && child->isFlow()) && !child->isText()) {
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

void RenderFlow::calcBlockMinMaxWidth()
{
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
        w = child->maxWidth() + margin;
        if(m_maxWidth < w) m_maxWidth = w;
        child = child->nextSibling();
    }
}

void RenderFlow::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderBox)::calcMinMaxWidth() this=" << this << endl;
#endif

    m_minWidth = 0;
    m_maxWidth = 0;

    if (isInline()) {
        // Irrelevant, since some enclosing block will actually flow our children.
        setMinMaxKnown();
        return;
    }

    if (childrenInline())
        calcInlineMinMaxWidth();
    else
        calcBlockMinMaxWidth();
        
    if(m_maxWidth < m_minWidth) m_maxWidth = m_minWidth;

    if (style()->width().isFixed())
        m_maxWidth = KMAX(m_minWidth,short(style()->width().value));

    if ( style()->whiteSpace() != NORMAL )
        m_minWidth = m_maxWidth;

    int toAdd = 0;
    if(style()->hasBorder())
        toAdd = borderLeft() + borderRight();
    if (hasPadding())
        toAdd += paddingLeft() + paddingRight();

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

void RenderFlow::close()
{
    if(lastChild() && lastChild()->isAnonymousBox()) {
        lastChild()->close();
    }

    RenderBox::close();
}

short RenderFlow::offsetWidth() const
{
    if (isInline() && !isText()) {
        short w = 0;
        RenderObject* object = firstChild();
        while (object) {
            w += object->offsetWidth();
            object = object->nextSibling();
        }
        return w;
    }    
    return width();
}

int RenderFlow::offsetHeight() const
{
    if (isInline() && !isText() && firstChild())
        return firstChild()->offsetHeight();
    return height();
}

int RenderFlow::offsetLeft() const
{
    int x = RenderBox::offsetLeft();
    RenderObject* textChild = (RenderObject*)this;
    while (textChild && textChild->isInline() && !textChild->isText())
        textChild = textChild->firstChild();
    if (textChild && textChild != this)
        x += textChild->xPos() - textChild->borderLeft() - textChild->paddingLeft();
    return x;
}

int RenderFlow::offsetTop() const
{
    RenderObject* textChild = (RenderObject*)this;
    while (textChild && textChild->isInline() && !textChild->isText())
        textChild = textChild->firstChild();
    int y = RenderBox::offsetTop();
    if (textChild && textChild != this)
        y += textChild->yPos() - textChild->borderTop() - textChild->paddingTop();
    return y;
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

static RenderFlow* cloneInline(RenderFlow* src)
{
    RenderFlow *o = new (src->renderArena()) RenderFlow(src->element());
    o->setStyle(src->style());
    return o;
}

void RenderFlow::splitInlines(RenderFlow* fromBlock, RenderFlow* toBlock,
                              RenderFlow* middleBlock,
                              RenderObject* beforeChild, RenderFlow* oldCont)
{
    // Create a clone of this inline.
    RenderFlow* clone = cloneInline(this);
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
        RenderFlow* cloneChild = clone;
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

void RenderFlow::splitFlow(RenderObject* beforeChild, RenderFlow* newBlockBox, 
                           RenderObject* newChild, RenderFlow* oldCont)
{
    RenderObject* block = containingBlock();
    RenderFlow* pre = 0;
    RenderFlow* post = 0;
    
    RenderStyle* newStyle = new RenderStyle();
    newStyle->inheritFrom(block->style());
    newStyle->setDisplay(BLOCK);
    pre = new (renderArena()) RenderFlow(0 /* anonymous box */);
    pre->setStyle(newStyle);
    pre->setIsAnonymousBox(true);
    pre->setChildrenInline(true);
    
    newStyle = new RenderStyle();
    newStyle->inheritFrom(block->style());
    newStyle->setDisplay(BLOCK);
    post = new (renderArena()) RenderFlow(0 /* anonymous box */);
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

void RenderFlow::addChildWithContinuation(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderFlow* flow = continuationBefore(beforeChild);
    RenderFlow* beforeChildParent = beforeChild ? static_cast<RenderFlow*>(beforeChild->parent()) : 
                                    (flow->continuation() ? flow->continuation() : flow);
    
    if (newChild->isSpecial())
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

void RenderFlow::addChildToFlow(RenderObject* newChild, RenderObject* beforeChild)
{
    setLayouted( false );
    
    bool madeBoxesNonInline = FALSE;

    RenderStyle* pseudoStyle=0;
    if (!isInline() && (!firstChild() || firstChild() == beforeChild) && newChild->isText())
    {
        RenderText* newTextChild = static_cast<RenderText*>(newChild);
        //kdDebug( 6040 ) << "first letter" << endl;

        if ( (pseudoStyle=style()->getPseudoStyle(RenderStyle::FIRST_LETTER)) ) {
            RenderFlow* firstLetter = new (renderArena()) RenderFlow(0 /* anonymous box */);
            pseudoStyle->setDisplay( INLINE );
            firstLetter->setStyle(pseudoStyle);
    
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
    
                RenderText* letter = new (renderArena()) RenderText(0 /* anonymous object */, oldText->substring(0,length));
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

    if (!newChild->isText())
    {
        if (newChild->style()->position() != STATIC)
            setOverhangingContents();
    }

    // RenderFlow has to either have all of its children inline, or all of its children as blocks.
    // So, if our children are currently inline and a block child has to be inserted, we move all our
    // inline children into anonymous block boxes
    if ( m_childrenInline && !newChild->isInline() && !newChild->isSpecial() )
    {
        if (isInline()) {
            // We are placing a block inside an inline. We have to perform a split of this
            // inline into continuations.  This involves creating an anonymous block box to hold 
            // |newChild|.  We then make that block box a continuation of this inline.  We take all of
            // the children after |beforeChild| and put them in a clone of this object.
            RenderStyle *newStyle = new RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);

            RenderFlow *newBox = new (renderArena()) RenderFlow(0 /* anonymous box */);
            newBox->setStyle(newStyle);
            newBox->setIsAnonymousBox(true);
            RenderFlow* oldContinuation = continuation();
            setContinuation(newBox);
            splitFlow(beforeChild, newBox, newChild, oldContinuation);
            return;
        }
        else {
            // This is a block with inline content. Wrap the inline content in anonymous blocks.
            makeChildrenNonInline(beforeChild);
            madeBoxesNonInline = true;
        }
        
        if (beforeChild) {
            if ( beforeChild->parent() != this ) {
                beforeChild = beforeChild->parent();
                KHTMLAssert(beforeChild->isAnonymousBox());
                KHTMLAssert(beforeChild->parent() == this);
            }
        }
    }
    else if (!m_childrenInline)
    {
        // If we're inserting an inline child but all of our children are blocks, then we have to make sure
        // it is put into an anomyous block box. We try to use an existing anonymous box if possible, otherwise
        // a new one is created and inserted into our list of children in the appropriate position.
        if(newChild->isInline()) {
            if (beforeChild) {
                if (beforeChild->previousSibling() && beforeChild->previousSibling()->isAnonymousBox()) {
                    beforeChild->previousSibling()->addChild(newChild);
                    newChild->setLayouted( false );
                    newChild->setMinMaxKnown( false );
                    return;
                }
            }
            else{
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

            RenderFlow *newBox = new (renderArena()) RenderFlow(0 /* anonymous box */);
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

void RenderFlow::makeChildrenNonInline(RenderObject *box2Start)
{
    KHTMLAssert(!isInline());
    KHTMLAssert(!box2Start || box2Start->parent() == this);

    m_childrenInline = false;

    RenderObject *child = firstChild();
    RenderObject *next;
    RenderObject *boxFirst = 0;
    RenderObject *boxLast = 0;
    while (child) {
        next = child->nextSibling();

        if (child->isInline()) {
            if ( !boxFirst )
                boxFirst = child;
            boxLast = child;
        }

        if (boxFirst &&
            (!child->isInline() || !next || child == box2Start)) {
            // Create a new anonymous box containing all children starting from boxFirst
            // and up to boxLast, and put it in place of the children
            RenderStyle *newStyle = new RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);

            RenderFlow *box = new (renderArena()) RenderFlow(0 /* anonymous box */);
            box->setStyle(newStyle);
            box->setIsAnonymousBox(true);
            // ### the children have a wrong style!!!
            // They get exactly the style of this element, not of the anonymous box
            // might be important for bg colors!

            insertChildNode(box, boxFirst);
            RenderObject* o = boxFirst;
            while(o && o != boxLast)
            {
                RenderObject* no = o;
                o = no->nextSibling();
                box->appendChildNode(removeChildNode(no));
            }
            if (child && box2Start == child) {
                boxFirst = boxLast = (child->isInline() ? box2Start : 0);
                box2Start = 0;
                continue;
            }
            else {
                box->appendChildNode(removeChildNode(boxLast));
                boxFirst = boxLast = 0;
            }
            box->close();
            box->setPos(box->xPos(), -500000);
            box->setLayouted(false);
        }

        child = next;
    }

    setLayouted(false);
}

void RenderFlow::removeChild(RenderObject *oldChild)
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
    
    RenderBox::removeChild(oldChild);
    
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

bool RenderFlow::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, bool inBox)
{
    if (specialObjects) {
        int stx = _tx + xPos();
        int sty = _ty + yPos();
        if (isRoot()) {
            stx += static_cast<RenderRoot*>(this)->view()->contentsX();
            sty += static_cast<RenderRoot*>(this)->view()->contentsY();
        }
        SpecialObject* o;
        QPtrListIterator<SpecialObject> it(*specialObjects);
        for (it.toLast(); (o = it.current()); --it)
            if (o->node->isFloating() && !o->noPaint)
                inBox |= o->node->nodeAtPoint(info, _x, _y, 
                    stx+o->left + o->node->marginLeft() - o->node->xPos(), 
                    sty+o->startY + o->node->marginTop() - o->node->yPos());
    }

    inBox |= RenderBox::nodeAtPoint(info, _x, _y, _tx, _ty, inBox);
    return inBox;
}


#ifndef NDEBUG
void RenderFlow::printTree(int indent) const
{
    RenderBox::printTree(indent);

    if(specialObjects)
    {
        QPtrListIterator<SpecialObject> it(*specialObjects);
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

void RenderFlow::dump(QTextStream *stream, QString ind) const
{
    if (m_childrenInline) { *stream << " childrenInline"; }
    if (m_pre) { *stream << " pre"; }
    if (firstLine) { *stream << " firstLine"; }

    if(specialObjects && !specialObjects->isEmpty())
    {
	*stream << " special(";
        QPtrListIterator<SpecialObject> it(*specialObjects);
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

    RenderBox::dump(stream,ind);
}
#endif

#undef DEBUG
#undef DEBUG_LAYOUT
#undef BOX_DEBUG
