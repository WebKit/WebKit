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
 *
 * $Id$
 */
// -------------------------------------------------------------------------
//#define DEBUG
//#define DEBUG_LAYOUT
//#define BOX_DEBUG


#include "dom_string.h"

#include <qfontmetrics.h>
#include <qsortedlist.h>
#include <qpainter.h>

#include <kglobal.h>

#include "render_flow.h"
#include "render_text.h"
#include "misc/helper.h"

#include "htmlhashes.h"

#include "render_style.h"
#include "render_root.h"

#include <kdebug.h>
#include <assert.h>

using namespace DOM;
using namespace khtml;

#define TABLECELLMARGIN -0x4000

static inline int collapseMargins(int a, int b)
{
    if ( a == TABLECELLMARGIN || b == TABLECELLMARGIN ) return TABLECELLMARGIN;
    if(a >= 0 && b >= 0) return (a > b ? a : b );
    if(a > 0 && b < 0) return a + b;
    if(a < 0 && b > 0) return b + a;
    return ( a > b ? b : a);
}


RenderFlow::RenderFlow()
    : RenderBox()
{
    m_childrenInline = true;
    m_haveAnonymous = false;

    specialObjects = 0;
}

void RenderFlow::setStyle(RenderStyle *_style)
{

//    kdDebug( 6040 ) << (void*)this<< " renderFlow(" << renderName() << ")::setstyle()" << endl;

    RenderBox::setStyle(_style);

    if(isPositioned())
        setInline(false);

    if(isFloating() || !style()->display() == INLINE)
        setInline(false);

    if (isInline() && !m_childrenInline)
        setInline(false);

    m_pre = false;
    if(style()->whiteSpace() == PRE)
        m_pre = true;

    if (haveAnonymousBox())
    {
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
    if(!isInline() && !containsPositioned() && !isRelPositioned() && !isPositioned() )
    {
        int h = m_height;
        if(specialObjects && floatBottom() > h) h = floatBottom();
        if((_ty > _y + _h) || (_ty + h < _y))
        {
            //kdDebug( 6040 ) << "cut!" << endl;
#ifndef APPLE_CHANGES
            return;
#endif /* APPLE_CHANGES not defined */
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
    // add offset for relative positioning
    if(isRelPositioned())
        relativePositionOffset(_tx, _ty);
   
    
    // 1. print background, borders etc
    if(hasSpecialObjects() && !isInline() && isVisible())
        printBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);


    // overflow: hidden    
    // save old clip region, set a new one
    QRegion oldClip;
    if (style()->overflow()==OHIDDEN)
    {
        if (p->hasClipping())
            oldClip = p->clipRegion();        
        calcClip(p, _tx, _ty, oldClip);   
    }
    
    

    // 2. print contents
    RenderObject *child = firstChild();
    while(child != 0)
    {
        if(!child->isFloating() && !child->isPositioned())
            child->print(p, _x, _y, _w, _h, _tx, _ty);
        child = child->nextSibling();
    }

    // 3. print floats and other non-flow objects
    if(specialObjects)
	printSpecialObjects( p,  _x, _y, _w, _h, _tx , _ty);

    // overflow: hidden
    // restore clip region
    if (style()->overflow()==OHIDDEN)
    {
        if (oldClip.isNull())
            p->setClipping(false);
        else
            p->setClipRegion(oldClip);        
    }    
    
    if(!isInline() && style()->outlineWidth())
        printOutline(p, _tx, _ty, width(), height(), style());

#ifdef BOX_DEBUG
    if(isAnonymousBox())
	outlineBox(p, _tx, _ty, "green");
    if(isFloating())
	outlineBox(p, _tx, _ty, "yellow");
    else
	outlineBox(p, _tx, _ty);
#endif

}

void RenderFlow::printSpecialObjects( QPainter *p, int x, int y, int w, int h, int tx, int ty)
{
    SpecialObject* r;
    QListIterator<SpecialObject> it(*specialObjects);
    for ( ; (r = it.current()); ++it ) {
	if (r->node->containingBlock()==this) {
	    RenderObject *o = r->node;
	    //kdDebug(0) << renderName() << "printing positioned at " << tx + o->xPos() << "/" << ty + o->yPos()<< endl;
	    o->print(p, x, y, w, h, tx , ty);
	}
    }
}


void RenderFlow::layout()
{
    //kdDebug( 6040 ) << renderName() << " " << this << "::layout() start" << endl;
    //QTime t;
    //t.start();

     assert(!isInline());

    int oldWidth = m_width;

    calcWidth();

//    kdDebug( 6040 ) << specialObjects << "," << oldWidth << ","
//            << m_width << ","<< layouted() << "," << isAnonymousBox() << endl;

    if ( !containsSpecial() && oldWidth == m_width && layouted() && !isAnonymousBox()
            && !containsPositioned() && !isPositioned()) return;

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderFlow) " << this << " ::layout() width=" << m_width << ", layouted=" << layouted() << endl;
    if(containingBlock() == static_cast<RenderObject *>(this))
        kdDebug( 6040 ) << renderName() << ": containingBlock == this" << endl;
#endif

    if(m_width<=0 && !isPositioned()) {
        if(m_y < 0) m_y = 0;
        setLayouted();
        return;
    }

    clearFloats();

    m_height = 0;
    m_clearStatus = CNONE;

//    kdDebug( 6040 ) << "childrenInline()=" << childrenInline() << endl;
    if(childrenInline()) {
        // ### make bidi resumeable so that we can get rid of this ugly hack
        if(!parsing())
            layoutInlineChildren();
    }
    else
        layoutBlockChildren();

    calcHeight();

    if ( lastChild() && lastChild()->hasOverhangingFloats() && isTableCell() ) {
	m_height = lastChild()->yPos() + static_cast<RenderFlow*>(lastChild())->floatBottom();
	m_height += borderBottom() + paddingBottom();
    }
    if( hasOverhangingFloats() && (isFloating() || isTableCell()) ) {
	m_height = floatBottom();
	m_height += borderBottom() + paddingBottom();
    }

    layoutSpecialObjects();

    //kdDebug() << renderName() << " layout width=" << m_width << " height=" << m_height << endl;

    // ### REMOVE ME! (see above)
     if(childrenInline() && parsing())
         setLayouted(false);
     else
        setLayouted();
}

void RenderFlow::layoutSpecialObjects()
{
    if(specialObjects) {
	//kdDebug( 6040 ) << renderName() << " " << this << "::layoutSpecialObjects() start" << endl;

        SpecialObject* r;
        QListIterator<SpecialObject> it(*specialObjects);
        for ( ; (r = it.current()); ++it ) {
            //kdDebug(6040) << "have a positioned object" << endl;
            if (r->type == SpecialObject::Positioned)
                r->node->layout();
        }
        specialObjects->sort();
    }
}

void RenderFlow::layoutBlockChildren()
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << " layoutBlockChildren( " << this <<" )" << endl;
#endif

    bool _layouted = true;
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

    if( style()->direction() == RTL ) {
        xPos = marginLeft() + m_width - paddingRight() - borderRight();
    }

    RenderObject *child = firstChild();
    RenderFlow *prevFlow = 0;

    int prevMargin = 0;
    if(isTableCell() ) {
	prevMargin = TABLECELLMARGIN;
    } else if ( m_height == 0 ) {
	// the elements and childs margin collapse if there is no border and padding.
	prevMargin = marginTop();
	if ( parent() )
	    prevMargin = collapseMargins( prevMargin, parent()->marginTop() );
	if ( prevMargin != TABLECELLMARGIN )
	    m_height = -prevMargin;
    }
    //kdDebug() << "RenderFlow::layoutBlockChildren " << prevMargin << endl; 

    // take care in case we inherited floats
    if (child && floatBottom() > m_height)
	child->setLayouted(false);

    
    //QTime t;
    //t.start();

    while( child != 0 )
    {
        //kdDebug( 6040 ) << "   " << child->renderName() << " loop " << child << ", " << child->isInline() << ", " << child->layouted() << endl;
        //kdDebug( 6040 ) << t.elapsed() << endl;
        // ### might be some layouts are done two times... FIX that.

        if (child->isPositioned())
        {
            child->layout();
            static_cast<RenderFlow*>(child->containingBlock())->insertPositioned(child);
	    //kdDebug() << "RenderFlow::layoutBlockChildren inserting positioned into " << child->containingBlock()->renderName() << endl;
            child = child->nextSibling();
            continue;
        } else if ( child->isReplaced() ) {
            child->layout();
	} else if ( child->isFloating() ) {
	    // margins of floats and other objects do not collapse. The hack below assures this.
	    m_height += prevMargin;
	    insertFloat( child );
	    positionNewFloats();
	    //kdDebug() << "RenderFlow::layoutBlockChildren inserting float at "<< m_height <<" prevMargin="<<prevMargin << endl; 
	    m_height -= prevMargin;
	    child = child->nextSibling();
	    continue;
	}

        child->calcVerticalMargins();

        if(checkClear(child)) prevMargin = 0; // ### should only be 0
        // if oldHeight+prevMargin < newHeight
        int margin = child->marginTop();
        //kdDebug(0) << "margin = " << margin << " prevMargin = " << prevMargin << endl;
        margin = collapseMargins(margin, prevMargin);

	if ( margin != TABLECELLMARGIN )
	    m_height += margin;

        //kdDebug(0) << "margin = " << margin << " yPos = " << m_height << endl;

        if(prevFlow)
        {
            if (prevFlow->yPos()+prevFlow->floatBottom() > m_height)
                child->setLayouted(false);
            else
                prevFlow=0;
        }

        child->setPos(child->xPos(), m_height);
        child->layout();

        int chPos = xPos + child->marginLeft();


        if(style()->direction() == LTR) {
            // html blocks flow around floats
            if ( ( style()->htmlHacks() || child->isTable() ) && child->style()->flowAroundFloats() )
              chPos = leftOffset(m_height) + child->marginLeft();
        } else {
            chPos -= child->width() + child->marginLeft() + child->marginRight();
            if ( ( style()->htmlHacks() || child->isTable() ) && child->style()->flowAroundFloats() )
                chPos -= leftOffset(m_height);
        }
        child->setPos(chPos, child->yPos());

        m_height += child->height();

        prevMargin = child->marginBottom();

        if (child->isFlow())
            prevFlow = static_cast<RenderFlow*>(child);

	if ( child->hasOverhangingFloats() ) {
	    // need to add the float to our special objects
	    addOverHangingFloats( static_cast<RenderFlow *>(child), -child->xPos(), -child->yPos(), true );
	}

        child = child->nextSibling();
    }

    if( !isTableCell() && toAdd != 0 )
	m_height += prevMargin;
    m_height += toAdd;

    setLayouted(_layouted);

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
    if(m_height < bottom)
	m_height = bottom;
    return true;
}

void
RenderFlow::insertPositioned(RenderObject *o)
{
    //kdDebug() << renderName() << "::insertPositioned " << this<< isAnonymousBox() << " " << o << endl;
    if(!specialObjects) {
        specialObjects = new QSortedList<SpecialObject>;
        specialObjects->setAutoDelete(true);
    }

    // don't insert it twice!
    QListIterator<SpecialObject> it(*specialObjects);
    SpecialObject* f;
    while ( (f = it.current()) ) {
        if (f->node == o) return;
        ++it;
    }

    if (!f) f = new SpecialObject;

    f->type = SpecialObject::Positioned;
    f->node = o;
    f->count = specialObjects->count();

    specialObjects->append(f);

    setContainsPositioned(true);
}

void
RenderFlow::insertFloat(RenderObject *o)
{
    //kdDebug( 6040 ) << renderName() << " " << (void *)this << "::insertFloat(" << o <<")" << endl;

    // a floating element
    if(!specialObjects) {
        specialObjects = new QSortedList<SpecialObject>;
        specialObjects->setAutoDelete(true);
    }

    // don't insert it twice!
    QListIterator<SpecialObject> it(*specialObjects);
    SpecialObject* f;
    while ( (f = it.current()) ) {
        if (f->node == o) return;
        ++it;
    }

    o->layout();

    if(!f) f = new SpecialObject;

    f->count = specialObjects->count();
    f->startY = -1;
    f->endY = -1;
    f->width = o->width() + o->marginLeft() + o->marginRight();

    if(o->style()->floating() == FLEFT)
        f->type = SpecialObject::FloatLeft;
    else
        f->type = SpecialObject::FloatRight;
    f->node = o;

    specialObjects->append(f);
//    kdDebug( 6040 ) << "inserting node " << o << " number of specialobject = " << //     specialObjects->count() << endl;

}

void RenderFlow::removeSpecialObject(RenderObject *o)
{
    if (specialObjects) {
	QListIterator<SpecialObject> it(*specialObjects);
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
        if(!lastFloat || (lastFloat->startY != -1 && lastFloat->type!=SpecialObject::Positioned)) {
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


//      kdDebug( 6040 ) << "specialObject y= (" << f->startY << "-" << f->endY << ")" << endl;

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

    if ( firstLine && style()->direction() == LTR ) {
        int cw=0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->contentWidth();
        left += style()->textIndent().minWidth(cw);
    }

    if(style()->hasBorder())
        left = borderLeft();
    if(style()->hasPadding())
        left += paddingLeft();

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
    QListIterator<SpecialObject> it(*specialObjects);
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

    if ( firstLine && style()->direction() == RTL ) {
        int cw=0;
        if (style()->textIndent().isPercent())
            cw = containingBlock()->contentWidth();
        right += style()->textIndent().minWidth(cw);
    }

    if(style()->hasBorder())
        right -= borderRight();
    if(style()->hasPadding())
        right -= paddingRight();
    return right;
}

int
RenderFlow::rightRelOffset(int y, int fixedOffset, int *heightRemaining ) const
{
    int right = fixedOffset;

    if (!specialObjects) return right;

    if (heightRemaining) *heightRemaining = 1;
    SpecialObject* r;
    QListIterator<SpecialObject> it(*specialObjects);
    for ( ; (r = it.current()); ++it )
    {
//      kdDebug( 6040 ) << "right: sy, ey, x, w " << //     r->startY << "," << r->endY << "," << r->left << "," << r->width << " " << endl;
        if (r->startY <= y && r->endY > y &&
            r->type == SpecialObject::FloatRight &&
            r->left < right) {
	    right = r->left;
	    if ( heightRemaining ) *heightRemaining = r->endY - y;
	}
    }
//    kdDebug( 6040 ) << "rightOffset(" << y << ") = " << right << endl;
    return right;
}

unsigned short
RenderFlow::lineWidth(int y) const
{
//    kdDebug( 6040 ) << "lineWidth(" << y << ")=" << rightOffset(y) - leftOffset(y) << endl;
    return rightOffset(y) - leftOffset(y);
}

int
RenderFlow::floatBottom() const
{
    if (!specialObjects) return 0;
    int bottom=0;
    SpecialObject* r;
    QListIterator<SpecialObject> it(*specialObjects);
    for ( ; (r = it.current()); ++it )
        if (r->endY>bottom && r->type <= SpecialObject::FloatRight)
            bottom=r->endY;
    return bottom;
}

int
RenderFlow::lowestPosition() const
{
    int bottom = RenderBox::lowestPosition();
    int lp = 0;
    if ( !m_childrenInline ) {
        RenderObject *last = lastChild();
        while( last && (last->isPositioned() || last->isFloating()) )
            last = last->previousSibling();
        if( last )
            lp = yPos() + last->lowestPosition();
    }

    if(  lp > bottom )
        bottom = lp;

    //kdDebug(0) << renderName() << " bottom = " << bottom << endl;

    if (specialObjects) {
        SpecialObject* r;
        QListIterator<SpecialObject> it(*specialObjects);
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
    //kdDebug(0) << renderName() << " bottom = " << bottom << endl;
    return bottom;
}

int RenderFlow::rightmostPosition() const
{
    int right = RenderBox::rightmostPosition();

    RenderObject *c;
    for (c = firstChild(); c; c = c->nextSibling()) {
	if (!c->isPositioned() && !c->isFloating()) {
	    int childRight = xPos() + c->rightmostPosition();
	    if (childRight > right)
		right = childRight;
	}
    }

    if (specialObjects) {
        SpecialObject* r;
        QListIterator<SpecialObject> it(*specialObjects);
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

    return right;
}


int
RenderFlow::leftBottom()
{
    if (!specialObjects) return 0;
    int bottom=0;
    SpecialObject* r;
    QListIterator<SpecialObject> it(*specialObjects);
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
    QListIterator<SpecialObject> it(*specialObjects);
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
	if( containsPositioned() ) {
            specialObjects->first();
            while ( specialObjects->current()) {
		if ( specialObjects->current()->type != SpecialObject::Positioned )
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
	addOverHangingFloats( static_cast<RenderFlow *>( parent() ), 0, offset, false );
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
    kdDebug( 6040 ) << (void *)this << ": adding overhanging floats offset=" << offset << " child=" << child << endl;
#endif
    if ( !flow->specialObjects )
        return;

    // we have overhanging floats
    if(!specialObjects) {
	specialObjects = new QSortedList<SpecialObject>;
	specialObjects->setAutoDelete(true);
    }

    QListIterator<SpecialObject> it(*flow->specialObjects);
    SpecialObject *r;
    for ( ; (r = it.current()); ++it ) {
	if ( r->type <= SpecialObject::FloatRight &&
	     ( ( !child && r->endY > offset ) ||
	       ( child && flow->yPos() + r->endY > height() ) ) ) {

	    SpecialObject* f = 0;
	    // don't insert it twice!
	    QListIterator<SpecialObject> it(*specialObjects);
	    while ( (f = it.current()) ) {
		if (f->node == r->node) break;
		++it;
	    }
	    if ( !f ) {
		SpecialObject *special = new SpecialObject;
		special->count = specialObjects->count();
		special->startY = r->startY - offset;
		special->endY = r->endY - offset;
		special->left = r->left - xoff;
		if (flow != parent())
		    special->left += flow->marginLeft();
		if ( !child )
		    special->left -= marginLeft();
		special->width = r->width;
		special->node = r->node;
		special->type = r->type;
		specialObjects->append(special);
#ifdef DEBUG_LAYOUT
		kdDebug( 6040 ) << "    y: " << special->startY << "-" << special->endY << " left: " << special->left << " width: " << special->width << endl;
#endif
	    }
	}
    }
}


void RenderFlow::calcMinMaxWidth()
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderBox)::calcMinMaxWidth() known=" << minMaxKnown() << endl;
#endif

    m_minWidth = 0;
    m_maxWidth = 0;   

    if (isInline())
        return;    

//    if(minMaxKnown())
//        return;

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
            if( !child->isBR() )
            {
                RenderStyle* cstyle = child->style();
                int margins = 0;
                if (!cstyle->marginLeft().isVariable())
                    margins += child->marginLeft();
                if (!cstyle->marginRight().isVariable())
                    margins += child->marginRight();
                int childMin = child->minWidth() + margins;
                int childMax = child->maxWidth() + margins;
                if (child->isText() && static_cast<RenderText *>(child)->length() > 0)
                {

                    int ti = cstyle->textIndent().minWidth(cw);
                    childMin+=ti;
                    childMax+=ti;

                    if(!child->minMaxKnown())
                        child->calcMinMaxWidth();
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
                        child = next(child);
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
            child = next(child);
        }
        if(m_minWidth < inlineMin) m_minWidth = inlineMin;
        if(m_maxWidth < inlineMax) m_maxWidth = inlineMax;
//        kdDebug( 6040 ) << "m_maxWidth=" << m_maxWidth << endl;
    }
    else
    {
        while(child != 0)
        {
//            if(!child->minMaxKnown())
//                child->calcMinMaxWidth();

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
                if (ml.type!=Variable && mr.type!=Variable)
                {
                    if (child->style()->width().type!=Variable)
                    {
                        if (child->style()->direction()==LTR)
                            margin = child->marginLeft();
                        else
                            margin = child->marginRight();
                    }
                    else
                        margin = child->marginLeft()+child->marginRight();

                }
                else if (ml.type != Variable)
                    margin = child->marginLeft();
                else if (mr.type != Variable)
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

    int toAdd = 0;
    if(style()->hasBorder())
        toAdd = borderLeft() + borderRight();
    if(style()->hasPadding())
        toAdd += paddingLeft() + paddingRight();

    m_minWidth += toAdd;
    m_maxWidth += toAdd;

//    seems to work but I'm not sure so I better leave it out
//    maybe checking minMaxKnown() for each child and only set it if
//    all childs have minMaxKnown() set ? this should be save? (Dirk)
    if(childrenInline())
        setMinMaxKnown();

    // ### compare with min/max width set in style sheet...
}

void RenderFlow::close()
{
//    kdDebug( 6040 ) << (void*)this<< " renderFlow::close()" << endl;
    if(haveAnonymousBox())
    {
        lastChild()->close();
        //kdDebug( 6040 ) << "RenderFlow::close(): closing anonymous box" << endl;
        setHaveAnonymousBox(false);
    }

    calcWidth();
    calcHeight();

    calcMinMaxWidth();

    setParsing(false);

    if ( isInline() )
	return;

    if(containingBlockWidth() < m_minWidth && parent())
        containingBlock()->updateSize();
    else
        containingBlock()->updateHeight();


#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderFlow)::close() total height =" << m_height << endl;
#endif
}

void RenderFlow::addChild(RenderObject *newChild, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderFlow)::addChild( " << newChild->renderName() <<
                       ", " << (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
    kdDebug( 6040 ) << "current height = " << m_height << endl;
#endif

    RenderStyle* pseudoStyle=0;
    if ( ( !firstChild() || firstChild() == beforeChild )
	&& ( pseudoStyle=style()->getPseudoStyle(RenderStyle::FIRST_LETTER) ) )
    {

        if (newChild->isText()) {
	    RenderText* newTextChild = static_cast<RenderText*>(newChild);

	    //kdDebug( 6040 ) << "first letter" << endl;

	    RenderFlow* firstLetter = new RenderFlow();
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

		RenderText* letter = new RenderText(oldText->substring(0,length));
		RenderStyle* newStyle = new RenderStyle();
		newStyle->inheritFrom(pseudoStyle);
		letter->setStyle(newStyle);
		firstLetter->addChild(letter);
	    }
	    firstLetter->close();

	}
    }
    bool nonInlineInChild = false;

    if (beforeChild && beforeChild->parent() != this) {
        // perhaps beforeChild is inside an anonymous box that is our child
        if (!newChild->isInline() && !newChild->isFloating() && beforeChild->parent() &&
            beforeChild->parent()->isAnonymousBox() && beforeChild->parent()->parent() == this)
            nonInlineInChild = true;
        else {
            beforeChild->parent()->addChild(newChild,beforeChild);
            return;
        }
    }

    //to prevents non-layouted elements from getting printed
    if (!newChild->isInline() && !newChild->isFloating())
        newChild->setPos(newChild->xPos(), -500000);

    if (!newChild->isText())
    {
        if (newChild->style()->position() != STATIC)
                setContainsPositioned(true);
    }


    if((m_childrenInline && !newChild->isInline() && !newChild->isFloating()) ||
       nonInlineInChild)
    {
        RenderObject *boxSource;
        if (nonInlineInChild)
            boxSource = beforeChild->parent();
        else
            boxSource = this;
        // put all inline children from boxSource in two anonymous block boxes -
        // one containing those before beforeChild, and one containing beforeChild and after
        if(boxSource->lastChild())
        {
//          kdDebug( 6040 ) << "no inline child, moving previous inline children!" << endl;
            RenderFlow *beforeBox = 0;

            if(beforeChild != boxSource->firstChild()) {
                RenderStyle *newStyle = new RenderStyle();
                newStyle->inheritFrom(boxSource->style());
                newStyle->setDisplay(BLOCK);

                beforeBox = new RenderFlow();
                beforeBox->setStyle(newStyle);
                beforeBox->setIsAnonymousBox(true);
                // ### the children have a wrong style!!!
                // They get exactly the style of this element, not of the anonymous box
                // might be important for bg colors!

                RenderObject* node = boxSource->firstChild();
                while(node && node != beforeChild) {
                    RenderObject* cnode = node;
                    node = node->nextSibling();
                    beforeBox->appendChildNode(boxSource->removeChildNode(cnode));
                }
                boxSource->insertChildNode(beforeBox,boxSource->firstChild());
                beforeBox->close();
                beforeBox->setPos(beforeBox->xPos(), -100000);
                beforeBox->setLayouted(false);
            }
            if (beforeChild) {
                RenderFlow *afterBox = new RenderFlow();
                RenderStyle *newStyle = new RenderStyle();
                newStyle->inheritFrom(boxSource->style());
                newStyle->setDisplay(BLOCK);
                afterBox->setStyle(newStyle);
                afterBox->setIsAnonymousBox(true);
                // ### the children have a wrong style!!!
                // They get exactly the style of this element, not of the anonymous box
                // might be important for bg colors!
                RenderObject* node = beforeChild;
                while(node) {
                    RenderObject* cnode = node;
                    node = node->nextSibling();
                    afterBox->appendChildNode(boxSource->removeChildNode(cnode));
                }
                boxSource->appendChildNode(afterBox);
                afterBox->close();
                afterBox->setPos(afterBox->xPos(), -100000);
                afterBox->setLayouted(false);
                beforeChild = afterBox;
            }
            if (nonInlineInChild) {
                boxSource->setLayouted(false);
                // boxSource will now contain up to two anonymous boxes - move
                // them into this in place of boxSource
                if(boxSource->firstChild())
                    insertChildNode(boxSource->removeChildNode(boxSource->firstChild()), boxSource);
                if(boxSource->lastChild())
                    insertChildNode(boxSource->removeChildNode(boxSource->lastChild()), boxSource);

                // make sure boxSource doesn't muck other objects up when deleted
                removeChildNode(boxSource);
                boxSource->detach();
                // ### what happens with boxSource's bg image if it had one?
            }
        }
        m_childrenInline = false;
    }
    else if(!m_childrenInline)
    {
        if( newChild->isInline() )
        {
            // #### this won't work with beforeChild != 0 !!!!
            if (beforeChild && beforeChild->previousSibling() && beforeChild->previousSibling()->isAnonymousBox()) {
                beforeChild->previousSibling()->addChild(newChild);
                setLayouted(false);
                return;
            }

//          kdDebug( 6040 ) << "adding inline child to anonymous box" << endl;
            if(!haveAnonymousBox())
            {
                //kdDebug( 6040 ) << "creating anonymous box" << endl;
                RenderStyle *newStyle = new RenderStyle();
                newStyle->inheritFrom(style());

                newStyle->setDisplay(BLOCK);
                RenderFlow *newBox = new RenderFlow();
                newBox->setStyle(newStyle);
                newBox->setIsAnonymousBox(true);
                RenderBox::addChild(newBox,beforeChild);
                newBox->addChild(newChild);
                newBox->setPos(newBox->xPos(), -100000);
                if (beforeChild)
                {
                    newBox->close();
                    newBox->layout();
                } else
                    setHaveAnonymousBox();
                return;
            }
            else
            {
                //kdDebug( 6040 ) << "adding to last box" << endl;
                lastChild()->addChild(newChild); // ,beforeChild ???
                return;
            }
        }
        else if(haveAnonymousBox())
        {
            lastChild()->close();
            lastChild()->layout();
            setHaveAnonymousBox(false);
//          kdDebug( 6040 ) << "closing anonymous box" << endl;
        }
    }
    else if(!newChild->isInline() && !newChild->isFloating())
    {
        m_childrenInline = false;
    }

    if(!newChild->isInline() && !newChild->isFloating())
    {
        if (style()->display() == INLINE)
        {
            setInline(false); // inline can't contain blocks
            if (parent() && parent()->isFlow())
                static_cast<RenderFlow*>(parent())->makeChildrenNonInline();
        }
    }

    setLayouted(false);
    RenderBox::addChild(newChild,beforeChild);
    // ### care about aligned stuff
}

void RenderFlow::makeChildrenNonInline()
{
// Put all inline children into anonymous block boxes
// ### should we call this all the way up to the top of the tree?

    m_childrenInline = false;

    RenderObject *child = firstChild();
    RenderObject *next;
    RenderObject *boxFirst = firstChild();
    RenderObject *boxLast = firstChild();
    while (child) {
        next = child->nextSibling();

        if (child->isInline() || child->isFloating()) {
            boxLast = child;
        }

        if ((!child->isInline() && !child->isFloating() && boxFirst != child) ||
            (!next && (boxFirst->isInline() || boxFirst->isFloating()))) {

            // make anon box of those before child
            RenderStyle *newStyle = new RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(BLOCK);

            RenderFlow *box = new RenderFlow();
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
            box->setPos(box->xPos(), -100000);
            box->setLayouted(false);
        }

        if (!child->isInline() && !child->isFloating())
            boxFirst = boxLast = next;

        child = next;
    }
    if ( isInline() ) {
        setInline(false);
        if ( parent()->isFlow() )
            static_cast<RenderFlow *>(parent())->makeChildrenNonInline();
    }
    setLayouted(false);
}

void RenderFlow::specialHandler(RenderObject *o)
{
//    kdDebug( 6040 ) << "specialHandler" << endl;
    if(o->isFloating())
        insertFloat(o);
    else if(o->isPositioned())
        static_cast<RenderFlow*>(o->containingBlock())->insertPositioned(o);

}

bool RenderFlow::containsPoint(int _x, int _y, int _tx, int _ty)
{
    if (!RenderBox::containsPoint(_x,_y,_tx,_ty))
	return false;

    return ((_x >= _tx+leftOffset(_y-_ty)) && (_x <= _tx+rightOffset(_y-_ty)));
}

void RenderFlow::printTree(int indent) const
{
    RenderBox::printTree(indent);

    if(specialObjects)
    {
        QListIterator<SpecialObject> it(*specialObjects);
        SpecialObject *r;
        for ( ; (r = it.current()); ++it )
        {
            QString s;
            s.fill(' ', indent);
            kdDebug() << s << renderName() << ":     special -> (" << r->startY << " - " << r->endY << ")" << endl;
        }
    }
}

#undef DEBUG
#undef DEBUG_LAYOUT
#undef BOX_DEBUG
