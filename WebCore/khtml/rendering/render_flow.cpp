/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
    m_blockBidi = false;
    m_clearStatus = CNONE;

    specialObjects = 0;
    
    m_maxTopPosMargin = m_maxTopNegMargin = m_maxBottomPosMargin = m_maxBottomNegMargin = 0;
    m_topMarginQuirk = m_bottomMarginQuirk = false;
}

void RenderFlow::setStyle(RenderStyle *_style)
{

//    kdDebug( 6040 ) << (void*)this<< " renderFlow(" << renderName() << ")::setstyle()" << endl;

    RenderBox::setStyle(_style);

    if(isPositioned())
        setInline(false);
    
    if ((isPositioned() || isRelPositioned() || style()->overflow()==OHIDDEN) && !m_layer)
        m_layer = new RenderLayer(this);
    
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
}

RenderFlow::~RenderFlow()
{
    delete specialObjects;
}

void RenderFlow::print(QPainter *p, int _x, int _y, int _w, int _h,
                                 int _tx, int _ty)
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

    printObject(p, _x, _y, _w, _h, _tx, _ty);
}

void RenderFlow::printObject(QPainter *p, int _x, int _y,
                                       int _w, int _h, int _tx, int _ty)
{

#ifdef DEBUG_LAYOUT
//    kdDebug( 6040 ) << renderName() << "(RenderFlow) " << this << " ::printObject() w/h = (" << width() << "/" << height() << ")" << endl;
#endif
    
    // 1. print background, borders etc
    if(hasSpecialObjects() && !isInline() && style()->visibility() == VISIBLE )
        printBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);

    // 2. print contents
    RenderObject *child = firstChild();
    while(child != 0)
    {
        if(!child->layer() && !child->isFloating())
            child->print(p, _x, _y, _w, _h, _tx, _ty);
        child = child->nextSibling();
    }

    if(!isInline() && !childrenInline() && style()->outlineWidth())
        printOutline(p, _tx, _ty, width(), height(), style());

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

void RenderFlow::layout()
{
//    kdDebug( 6040 ) << renderName() << " " << this << "::layout() start" << endl;
//     QTime t;
//     t.start();

    KHTMLAssert( !layouted() );
    KHTMLAssert( minMaxKnown() );
    KHTMLAssert(!isInline());

    int oldWidth = m_width;

    calcWidth();

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
    }
    
    // A quirk that has become an unfortunate standard.  Positioned elements, floating elements
    // and table cells don't ever collapse their margins with either themselves or their
    // children.
    bool canCollapseOwnMargins = !isPositioned() && !isFloating() && !isTableCell();
                                       
//    kdDebug( 6040 ) << "childrenInline()=" << childrenInline() << endl;
    if(childrenInline()) {
        // ### make bidi resumeable so that we can get rid of this ugly hack
        if (!m_blockBidi)
            layoutInlineChildren();
    }
    else
        layoutBlockChildren( relayoutChildren );

    int oldHeight = m_height;
    calcHeight();
    if (oldHeight != m_height)
        relayoutChildren = true;

    if ( isTableCell() && lastChild() && lastChild()->hasOverhangingFloats() ) {
        m_height = lastChild()->yPos() + static_cast<RenderFlow*>(lastChild())->floatBottom();
        m_height += borderBottom() + paddingBottom();
    }
    if( hasOverhangingFloats() && (isFloating() || isTableCell()) ) {
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
    if(style()->hasPadding())
    {
        xPos += paddingLeft();
        m_height += paddingTop();
        toAdd += paddingBottom();
    }

    int minHeight = m_height + toAdd;
    
    if( style()->direction() == RTL ) {
        xPos = marginLeft() + m_width - paddingRight() - borderRight();
    }

    RenderObject *child = firstChild();
    RenderFlow *prevFlow = 0;

    // Whether or not we can collapse our own margins with our children.  We don't do this
    // if we had any border/padding (obviously), if we're the root or HTML elements, or if
    // we're positioned, floating, or a table cell.
    bool canCollapseWithChildren = !isRoot() && !isHtml() && !isPositioned() && 
      !isFloating() && !isTableCell() && (m_height == 0);
      
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
    
    bool strictMode = (element()->getDocument()->parseMode() == DocumentImpl::Strict);
    
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
        } else if ( child->isFloating() ) {
            insertSpecialObject( child );
            positionNewFloats();
            //kdDebug() << "RenderFlow::layoutBlockChildren inserting float at "<< m_height <<" prevMargin="<<prevMargin << endl;
            child = child->nextSibling();
            continue;
        }

        child->calcVerticalMargins();

        //kdDebug(0) << "margin = " << margin << " yPos = " << m_height << endl;

        if(prevFlow)
        {
            if (prevFlow->yPos()+prevFlow->floatBottom() > m_height)
                child->setLayouted(false);
            else
                prevFlow=0;
        }

        child->setPos(child->xPos(), m_height);
        if ( !child->layouted() )
            child->layout();

        // Now determine the correct ypos based off examination of collapsing margin
        // values.
        if (shouldCollapseChild) {
            // Get our max pos and neg top margins.
            int posTop = child->maxTopMargin(true);
            int negTop = child->maxTopMargin(false);
            
            // XXX A hack we have to put in to deal with the fact
            // that KHTML incorrectly morphs inlines with blocks
            // inside them into blocks themselves.
            if (child->style()->display() == INLINE && child->marginTop())
                posTop = negTop = 0;
                
            // See if the top margin is quirky. We only care if this child has
            // margins that will collapse with us.
            bool topQuirk = child->isTopMarginQuirk();
            
            if (canCollapseWithChildren && topMarginContributor && !clearOccurred) {
                // This child is collapsing with the top of the
                // block.  If it has larger margin values, then we need to update
                // our own maximal values.
                if (posTop > m_maxTopPosMargin)
                    m_maxTopPosMargin = posTop;
                
                if (negTop > m_maxTopNegMargin)
                    m_maxTopNegMargin = negTop;
                 
                // The minute any of the margins involved isn't a quirk, don't
                // collapse it away, even if the margin is smaller (www.webreference.com
                // has an example of this, a <dt> with 0.8em author-specified inside
                // a <dl> inside a <td>.
                if (!topQuirk && (posTop-negTop))
                    m_topMarginQuirk = false;
            }
            
            if (isTableCell() && topMarginContributor && (posTop-negTop))
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
                       && (strictMode || !isTableCell() || !topChildQuirk)
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
                // that KHTML incorrectly morphs inlines with blocks
                // inside them into blocks themselves.
                if (child->style()->display() == INLINE && child->marginBottom())
                    prevPosMargin = prevNegMargin = 0;
                
                if (prevPosMargin-prevNegMargin)
                    bottomChildQuirk = child->isBottomMarginQuirk();
            }
            child->setPos(child->xPos(), ypos);
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

        if (child->isFlow())
            prevFlow = static_cast<RenderFlow*>(child);

        if ( child->hasOverhangingFloats() ) {
            // need to add the float to our special objects
            addOverHangingFloats( static_cast<RenderFlow *>(child), -child->xPos(), -child->yPos(), true );
        }

        child = child->nextSibling();
    }

    // XXX This check is unconvincing. Needs to be a reliable way to test for auto height. -dwh
    bool autoHeight = style()->height().isVariable() && style()->height().value == 0;
    
    // If any height other than auto is specified in CSS, then we don't collapse our bottom
    // margins with our children's margins.  To do otherwise would be to risk odd visual
    // effects when the children overflow out of the parent block and yet still collapse
    // with it.
    if (canCollapseWithChildren && !autoHeight)
        canCollapseWithChildren = false;
    
    // If we can't collapse with children then go ahead and add in the bottom margins.
    if (!topMarginContributor && 
        (!canCollapseWithChildren 
            && (strictMode || !isTableCell() || !bottomChildQuirk)
        ))
        m_height += prevPosMargin - prevNegMargin;
        
    m_height += toAdd;
    
    // Negative margins can cause our height to shrink below our minimal height (border/padding).
    // If this happens, ensure that the computed height is increased to the minimal height.
    if (m_height < minHeight)
        m_height = minHeight;
    
    if (canCollapseWithChildren && !topMarginContributor) {
        // Update our max pos/neg bottom margins, since we collapsed our bottom margins
        // with our children.
        if (prevPosMargin > m_maxBottomPosMargin)
            m_maxBottomPosMargin = prevPosMargin;
        
        if (prevNegMargin > m_maxBottomNegMargin)
            m_maxBottomNegMargin = prevNegMargin;
           
        if (!bottomChildQuirk)
            m_bottomMarginQuirk = false;
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

    if(style()->hasBorder())
        left += borderLeft();
    if(style()->hasPadding())
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

    if(style()->hasBorder())
        right -= borderRight();
    if(style()->hasPadding())
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
	     (static_cast<RenderFlow *>(prev)->floatBottom()+prev->yPos() < m_y ))) {
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


void RenderFlow::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderBox)::calcMinMaxWidth() this=" << this << endl;
#endif

    m_minWidth = 0;
    m_maxWidth = 0;

    if (isInline()) {
	setMinMaxKnown();
        return;
    }

    int cw = containingBlock()->contentWidth();

    // non breaking space
    const QChar nbsp = 0xa0;

    RenderObject *child = firstChild();
    RenderObject *prevchild = 0;
    if(childrenInline())
    {
        int inlineMax=0;
        int currentMin=0;
        int inlineMin=0;
        bool noBreak=false;
	bool prevWasText = false;

        while(child != 0)
        {
            // positioned children don't affect the minmaxwidth
            if (child->isPositioned())
            {
                child = next(this, child);
                continue;
            }

            if( !child->isBR() )
            {
                RenderStyle* cstyle = child->style();
                int margins = 0;
		LengthType type = cstyle->marginLeft().type;
                if ( type != Variable )
                    margins += (type == Fixed ? cstyle->marginLeft().value : child->marginLeft());
		type = cstyle->marginRight().type;
                if ( type != Variable )
                    margins += (type == Fixed ? cstyle->marginRight().value : child->marginRight());
                int childMin = child->minWidth() + margins;
                int childMax = child->maxWidth() + margins;
                if (child->isText() && static_cast<RenderText *>(child)->length() > 0)
                {

                    int ti = cstyle->textIndent().minWidth(cw);
                    childMin+=ti;
                    childMax+=ti;

                    bool hasNbsp=false;
                    RenderText* t = static_cast<RenderText *>(child);
                    if (t->data()[0] == nbsp) //inline starts with nbsp
                    {
                        currentMin += childMin;
                        inlineMax += childMax;
                        hasNbsp = true;
                    }
                    if (hasNbsp && t->data()[t->length()-1]==nbsp )
                    {                           //inline starts and ends with nbsp
                        noBreak=true;
                    }
                    else if (t->data()[t->length()-1] == nbsp && t->data()[0] != ' ')
                    {                           //inline only ends with nbsp
                        if(currentMin < childMin) currentMin = childMin;
                        inlineMax += childMax;
                        noBreak = true;
                        hasNbsp = true;
                    }
                    if ( t->hasBreakableChar() )
                        noBreak = false;
                    prevWasText = true;
                    if (hasNbsp)
                    {
                        if(inlineMin < currentMin) inlineMin = currentMin;
                        prevchild = child;
                        child = next(this, child);
                        hasNbsp = false;
                        continue;
                    }
                }
		prevWasText = false;
                if (noBreak ||
                        (prevchild && prevchild->isFloating() && child->isFloating()))
                {
                    currentMin += childMin;
                    if(inlineMin < currentMin) inlineMin = currentMin;
                    inlineMax += childMax;
                    noBreak = false;
                }
                else
                {
                    currentMin = childMin;
                    if(inlineMin < currentMin) inlineMin = currentMin;
                    inlineMax += childMax;
                }

            }
            else
            {
                if(m_minWidth < inlineMin) m_minWidth = inlineMin;
                if(m_maxWidth < inlineMax) m_maxWidth = inlineMax;
                inlineMin = currentMin = inlineMax = 0;
            }
	    prevWasText = false;
            prevchild = child;
            child = next(this, child);
        }
        if(m_minWidth < inlineMin) m_minWidth = inlineMin;
        if(m_maxWidth < inlineMax) m_maxWidth = inlineMax;
//         kdDebug( 6040 ) << "m_minWidth=" << m_minWidth
// 			<< " m_maxWidth=" << m_maxWidth << endl;
    }
    else
    {
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
    if(m_maxWidth < m_minWidth) m_maxWidth = m_minWidth;

    if (style()->width().isFixed())
        m_maxWidth = KMAX(m_minWidth,short(style()->width().value));

    if ( style()->whiteSpace() != NORMAL )
        m_minWidth = m_maxWidth;

    int toAdd = 0;
    if(style()->hasBorder())
        toAdd = borderLeft() + borderRight();
    if(style()->hasPadding())
        toAdd += paddingLeft() + paddingRight();

    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    setMinMaxKnown();

    //kdDebug( 6040 ) << "Text::calcMinMaxWidth(" << this << "): min = " << m_minWidth << " max = " << m_maxWidth << endl;
    // ### compare with min/max width set in style sheet...
}

void RenderFlow::close()
{
    // ### get rid of me
    m_blockBidi = false;

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

void RenderFlow::addChild(RenderObject *newChild, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderFlow)::addChild( " << newChild->renderName() <<
                       ", " << (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
    kdDebug( 6040 ) << "current height = " << m_height << endl;
#endif
    setLayouted( false );
    
    bool madeBoxesNonInline = FALSE;

    if ( newChild->isPositioned() ) {
        m_blockBidi = false;
    }
    if (m_blockBidi)
	    newChild->setBlockBidi();

    RenderStyle* pseudoStyle=0;
    if ( !isInline() && ( !firstChild() || firstChild() == beforeChild )
        && ( pseudoStyle=style()->getPseudoStyle(RenderStyle::FIRST_LETTER) ) )
    {
        if (newChild->isText()) {
            RenderText* newTextChild = static_cast<RenderText*>(newChild);

            //kdDebug( 6040 ) << "first letter" << endl;
    
            RenderFlow* firstLetter = new RenderFlow(0 /* anonymous box */);
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
    
                RenderText* letter = new RenderText(0 /* anonymous object */, oldText->substring(0,length));
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
        KHTMLAssert(beforeChild->parent()->parent() == this);

        if (newChild->isInline()) {
            beforeChild->parent()->addChild(newChild,beforeChild);
            newChild->setLayouted( false );
            newChild->setMinMaxKnown( false );
            return;
        }
        else {
            // Trying to insert a block child into an anonymous block box which contains only
            // inline elements... move all of the anonymous box's inline children into other
            // anonymous boxes which become children of this

            RenderObject *anonBox = beforeChild->parent();
            KHTMLAssert (anonBox->isFlow()); // ### RenderTableSection the only exception - should never happen here

            if ( anonBox->childrenInline() ) {
                static_cast<RenderFlow*>(anonBox)->makeChildrenNonInline(beforeChild);
                madeBoxesNonInline = true;
            }
            
            beforeChild = beforeChild->parent();

            RenderObject *child;
            while ((child = anonBox->firstChild()) != 0) {
                anonBox->removeChild(child);
                addChild(child,anonBox);
            }

            removeChildNode(anonBox);
            anonBox->detach(); // does necessary cleanup & deletes anonBox

            KHTMLAssert(beforeChild->parent() == this);
        }
    }

    // prevent non-layouted elements from getting printed by pushing them far above the top of the
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
        if ( m_childrenInline ) {
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

            RenderFlow *newBox = new RenderFlow(0 /* anonymous box */);
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

    if(!newChild->isInline()) // block child
    {
        // XXX This is all just completely wrong and is going to have to be
        // rewritten. -dwh
        
        // If we are inline ourselves and have become block, we have to make sure our parent
        // makes the necessary adjustments so that all of its other children are moved into
        // anonymous block boxes where necessary
        if (style()->display() == INLINE)
        {
            setInline(false); // inline can't contain blocks
            RenderObject *p = parent();
            if (p && p->isFlow() && p->childrenInline() ) {
                static_cast<RenderFlow*>(p)->makeChildrenNonInline();
                madeBoxesNonInline = true;
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

        if ( boxFirst &&
	     ( !child->isInline() || !next || child == box2Start ) ) {
            // Create a new anonymous box containing all children starting from boxFirst
            // and up to (but not including) boxLast, and put it in place of the children
            RenderStyle *newStyle = new RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);

            RenderFlow *box = new RenderFlow(0 /* anonymous box */);
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
            box->appendChildNode(removeChildNode(boxLast));
            box->close();
            box->setPos(box->xPos(), -500000);
            box->setLayouted(false);
            boxFirst = boxLast = next;
        }

        child = next;
    }

    if (isInline()) {
        setInline(false);
        if (parent()->isFlow()) {
            KHTMLAssert(parent()->childrenInline());
	    static_cast<RenderFlow *>(parent())->makeChildrenNonInline();
        }
    } 
    
    setLayouted(false);
}

bool RenderFlow::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty)
{
    bool inBox = false;
    if (specialObjects) {
        int stx = _tx;
        int sty = _ty;
        SpecialObject* o;
        QPtrListIterator<SpecialObject> it(*specialObjects);
        for (it.toLast(); (o = it.current()); --it)
            if (!o->node->layer() && o->node->containingBlock() == this)
                inBox |= o->node->nodeAtPoint(info, _x, _y, stx+xPos(), sty+yPos());
    }

    inBox |= RenderBox::nodeAtPoint(info, _x, _y, _tx, _ty);
    return inBox;
}


#ifndef NDEBUG
void RenderFlow::printTree(int indent) const
{
    RenderBox::printTree(indent);

//     KHTMLAssert(!m_blockBidi);

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
