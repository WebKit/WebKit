/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
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

#include "rendering/render_object.h"
#include "rendering/render_table.h"
#include "rendering/render_list.h"
#include "rendering/render_canvas.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_docimpl.h"
#include "misc/htmlhashes.h"
#include <kdebug.h>
#include <qpainter.h>
#include "khtmlview.h"
#include "render_arena.h"
#include "render_inline.h"
#include "render_block.h"

#include <assert.h>
using namespace DOM;
using namespace khtml;

#ifndef NDEBUG
static void *baseOfRenderObjectBeingDeleted;
#endif

void* RenderObject::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RenderObject::operator delete(void* ptr, size_t sz)
{
    assert(baseOfRenderObjectBeingDeleted == ptr);
    
    // Stash size where detach can find it.
    *(size_t *)ptr = sz;
}

RenderObject *RenderObject::createObject(DOM::NodeImpl* node,  RenderStyle* style)
{
    RenderObject *o = 0;
    RenderArena* arena = node->getDocument()->renderArena();
    switch(style->display())
    {
    case NONE:
        break;
    case INLINE:
    case BLOCK:
        // In compat mode, if <td> has a display of block, build a table cell instead.
        // This corrects erroneous HTML.  A better fix would be to implement full-blown
        // CSS2 anonymous table render object construction, but until then, this will have
        // to suffice. -dwh
        if (style->display() == BLOCK && node->id() == ID_TD && style->htmlHacks())
            o = new (arena) RenderTableCell(node);
        // In quirks mode if <table> has a display of block, make a table. If it has 
        // a display of inline, make an inline-table.
        else if (node->id() == ID_TABLE && style->htmlHacks())
            o = new (arena) RenderTable(node);
        else if (style->display() == INLINE)
            o = new (arena) RenderInline(node);
        else
            o = new (arena) RenderBlock(node);
        break;
    case LIST_ITEM:
        o = new (arena) RenderListItem(node);
        break;
    case RUN_IN:
    case COMPACT:
        o = new (arena) RenderBlock(node);
        break;
    case MARKER:
        break;
    case TABLE:
    case INLINE_TABLE:
        //kdDebug( 6040 ) << "creating RenderTable" << endl;
        o = new (arena) RenderTable(node);
        break;
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
        o = new (arena) RenderTableSection(node);
        break;
    case TABLE_ROW:
        o = new (arena) RenderTableRow(node);
        break;
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
        o = new (arena) RenderTableCol(node);
        break;
    case TABLE_CELL:
        o = new (arena) RenderTableCell(node);
        break;
    case TABLE_CAPTION:
        o = new (arena) RenderBlock(node);
        break;
    }
    if(o) o->setStyle(style);
    return o;
}

RenderObject::RenderObject(DOM::NodeImpl* node)
    : CachedObjectClient(),
m_style( 0 ),
m_node( node ),
m_parent( 0 ),
m_previous( 0 ),
m_next( 0 ),
m_verticalPosition( PositionUndefined ),
m_needsLayout( true ),
m_unused( false ),
m_minMaxKnown( false ),
m_floating( false ),

m_positioned( false ),
m_overhangingContents( false ),
m_relPositioned( false ),
m_paintBackground( false ),

m_isAnonymous( false ),
m_recalcMinMax( false ),
m_isText( false ),
m_inline( true ),

m_replaced( false ),
m_mouseInside( false ),
m_hasFirstLine( false ),
m_isSelectionBorder( false )
{
}

RenderObject::~RenderObject()
{
    if(m_style->backgroundImage())
        m_style->backgroundImage()->deref(this);

    if (m_style)
        m_style->deref();
}

bool RenderObject::isRoot() const {
    return element() && element()->renderer() == this &&
           element()->getDocument()->documentElement() == element();
}

bool RenderObject::canHaveChildren() const
{
    return false;
}

void RenderObject::addChild(RenderObject* , RenderObject *)
{
    KHTMLAssert(0);
}

RenderObject* RenderObject::removeChildNode(RenderObject* )
{
    KHTMLAssert(0);
    return 0;
}

void RenderObject::removeChild(RenderObject* )
{
    KHTMLAssert(0);
}

void RenderObject::appendChildNode(RenderObject*)
{
    KHTMLAssert(0);
}

void RenderObject::insertChildNode(RenderObject*, RenderObject*)
{
    KHTMLAssert(0);
}

static void addLayers(RenderObject* obj, RenderLayer* parentLayer, RenderObject*& newObject,
                      RenderLayer*& beforeChild)
{
    if (obj->layer()) {
        if (!beforeChild && newObject) {
            // We need to figure out the layer that follows newObject.  We only do
            // this the first time we find a child layer, and then we update the
            // pointer values for newObject and beforeChild used by everyone else.
            beforeChild = newObject->parent()->findNextLayer(parentLayer, newObject);
            newObject = 0;
        }
        parentLayer->addChild(obj->layer(), beforeChild);
        return;
    }

    for (RenderObject* curr = obj->firstChild(); curr; curr = curr->nextSibling())
        addLayers(curr, parentLayer, newObject, beforeChild);
}

void RenderObject::addLayers(RenderLayer* parentLayer, RenderObject* newObject)
{
    if (!parentLayer)
        return;
    
    RenderObject* object = newObject;
    RenderLayer* beforeChild = 0;
    ::addLayers(this, parentLayer, object, beforeChild);
}

void RenderObject::removeLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;
    
    if (layer()) {
        parentLayer->removeChild(layer());
        return;
    }

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->removeLayers(parentLayer);
}

void RenderObject::moveLayers(RenderLayer* oldParent, RenderLayer* newParent)
{
    if (!newParent)
        return;
    
    if (layer()) {
        if (oldParent)
            oldParent->removeChild(layer());
        newParent->addChild(layer());
        return;
    }

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->moveLayers(oldParent, newParent);
}

RenderLayer* RenderObject::findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint,
                                         bool checkParent)
{
    // Error check the parent layer passed in.  If it's null, we can't find anything.
    if (!parentLayer)
        return 0;
        
    // Step 1: Descend into our siblings trying to find the next layer.  If we do find
    // a layer, and if its parent layer matches our desired parent layer, then we have
    // a match.
    for (RenderObject* curr = startPoint ? startPoint->nextSibling() : firstChild();
         curr; curr = curr->nextSibling()) {
        RenderLayer* nextLayer = curr->findNextLayer(parentLayer, 0, false);
        if (nextLayer) {
            if (nextLayer->parent() == parentLayer)
                return nextLayer;
            return 0;
        }
    }
    
    // Step 2: If our layer is the desired parent layer, then we're finished.  We didn't
    // find anything.
    RenderLayer* ourLayer = layer();
    if (parentLayer == ourLayer)
        return 0;
    
    // Step 3: If we have a layer, then return that layer.  It will be checked against
    // the desired parent layer in the for loop above.
    if (ourLayer)
        return ourLayer;
    
    // Step 4: If |checkParent| is set, climb up to our parent and check its siblings that
    // follow us to see if we can locate a layer.
    if (checkParent && parent())
        return parent()->findNextLayer(parentLayer, this, true);
    
    return 0;
}
    
RenderLayer* RenderObject::enclosingLayer()
{
    RenderObject* curr = this;
    while (curr) {
        RenderLayer *layer = curr->layer();
        if (layer)
            return layer;
        curr = curr->parent();
    }
    return 0;
}

int RenderObject::offsetLeft() const
{
    int x = xPos();
    if (!isPositioned()) {
        if (isRelPositioned()) {
            int y = 0;
            ((RenderBox*)this)->relativePositionOffset(x, y);
        }
        
        RenderObject* offsetPar = offsetParent();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            x += curr->xPos();
            curr = curr->parent();
        }
    }
    return x;
}

int RenderObject::offsetTop() const
{
    int y = yPos();
    if (!isPositioned()) {
        if (isRelPositioned()) {
            int x = 0;
            ((RenderBox*)this)->relativePositionOffset(x, y);
        }
        RenderObject* offsetPar = offsetParent();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            y += curr->yPos();
            curr = curr->parent();
        }
    }
    return y;
}
    
RenderObject* RenderObject::offsetParent() const
{
    bool skipTables = isPositioned() || isRelPositioned();
    RenderObject* curr = parent();
    while (curr && !curr->isPositioned() && !curr->isRelPositioned() &&
           !curr->isBody()) {
        if (!skipTables && (curr->isTableCell() || curr->isTable()))
            break;
        curr = curr->parent();
    }
    return curr;
}

// More IE extensions.  clientWidth and clientHeight represent the interior of an object
// excluding border and scrollbar.
short
RenderObject::clientWidth() const
{
    return width() - borderLeft() - borderRight() -
        (layer() ? layer()->verticalScrollbarWidth() : 0);
}

short
RenderObject::clientHeight() const
{
    return height() - borderTop() - borderBottom() -
      (layer() ? layer()->horizontalScrollbarHeight() : 0);
}

// scrollWidth/scrollHeight will be the same as clientWidth/clientHeight unless the
// object has overflow:hidden/scroll/auto specified and also has overflow.
short
RenderObject::scrollWidth() const
{
    return (style()->hidesOverflow() && layer()) ? layer()->scrollWidth() : clientWidth();
}

short
RenderObject::scrollHeight() const
{
    return (style()->hidesOverflow() && layer()) ? layer()->scrollHeight() : clientHeight();
}

bool
RenderObject::hasStaticX() const
{
    return (style()->left().isVariable() && style()->right().isVariable()) ||
            style()->left().isStatic() ||
            style()->right().isStatic();
}

bool
RenderObject::hasStaticY() const
{
    return (style()->top().isVariable() && style()->bottom().isVariable()) || style()->top().isStatic();
}

void RenderObject::markAllDescendantsWithFloatsForLayout(RenderObject*)
{
}

void RenderObject::setNeedsLayout(bool b) 
{
    m_needsLayout = b;
    if (b) {
        RenderObject *o = container();
        RenderObject *root = this;

        // If an attempt is made to
        // setNeedsLayout(true) an object inside a clipped (overflow:hidden) object, we 
        // have to make sure to repaint only the clipped rectangle.
        // We do this by passing an argument to scheduleRelayout.  This hint really
        // shouldn't be needed, and it's unfortunate that it is necessary.  -dwh

        RenderObject* clippedObj = 
            (style()->hidesOverflow() && !isText()) ? this : 0;
        
        while( o ) {
            root = o;
            o->m_needsLayout = true;
            if (o->style()->hidesOverflow() && !clippedObj)
                clippedObj = o;
            o = o->container();
        }
        
        root->scheduleRelayout(clippedObj);
    }
}
    
RenderBlock* RenderObject::containingBlock() const
{
    if(isTableCell())
        return static_cast<const RenderTableCell *>(this)->table();
    if (isCanvas())
        return (RenderBlock*)this;
    
    RenderObject *o = parent();
    if (m_style->position() == FIXED) {
        while ( o && !o->isCanvas() )
            o = o->parent();
    }
    else if (m_style->position() == ABSOLUTE) {
        while (o && (o->style()->position() == STATIC || (o->isInline() && !o->isReplaced()))
               && !o->isRoot() && !o->isCanvas())
            o = o->parent();
    } else {
        while (o && ((o->isInline() && !o->isReplaced()) || o->isTableRow() || o->isTableSection()
                     || o->isTableCol()))
            o = o->parent();
    }

    if (!o || !o->isRenderBlock())
        // This can happen because of setOverhangingContents in RenderImage's setStyle method.
        return 0;
    
    return static_cast<RenderBlock*>(o);
}

short RenderObject::containingBlockWidth() const
{
    // ###
    return containingBlock()->contentWidth();
}

int RenderObject::containingBlockHeight() const
{
    // ###
    return containingBlock()->contentHeight();
}

bool RenderObject::sizesToMaxWidth() const
{
    return isFloating() || isCompact() ||
      (element() && (element()->id() == ID_BUTTON || element()->id() == ID_LEGEND));
}

void RenderObject::drawBorder(QPainter *p, int x1, int y1, int x2, int y2,
                              BorderSide s, QColor c, const QColor& textcolor, EBorderStyle style,
                              int adjbw1, int adjbw2, bool invalidisInvert)
{
    int width = (s==BSTop||s==BSBottom?y2-y1:x2-x1);

    if(style == DOUBLE && width < 3)
        style = SOLID;

    if(!c.isValid()) {
        if(invalidisInvert)
        {
            p->setRasterOp(Qt::XorROP);
            c = Qt::white;
        }
        else {
            if(style == INSET || style == OUTSET || style == RIDGE || style ==
	    GROOVE)
                c.setRgb(238, 238, 238);
            else
                c = textcolor;
        }
    }

    switch(style)
    {
    case BNONE:
    case BHIDDEN:
        // should not happen
        if(invalidisInvert && p->rasterOp() == Qt::XorROP)
            p->setRasterOp(Qt::CopyROP);

        return;
    case DOTTED:
        p->setPen(QPen(c, width == 1 ? 0 : width, Qt::DotLine));
        /* nobreak; */
    case DASHED:
        if(style == DASHED)
            p->setPen(QPen(c, width == 1 ? 0 : width, Qt::DashLine));

        if (width > 0)
            switch(s)
            {
            case BSBottom:
            case BSTop:
                p->drawLine(x1, (y1+y2)/2, x2, (y1+y2)/2);
                break;
            case BSRight:
            case BSLeft:
                p->drawLine((x1+x2)/2, y1, (x1+x2)/2, y2);
                break;
            }
                
        break;

    case DOUBLE:
    {
        int third = (width+1)/3;

	if (adjbw1 == 0 && adjbw2 == 0)
	{
	    p->setPen(Qt::NoPen);
	    p->setBrush(c);
	    switch(s)
	    {
	    case BSTop:
	    case BSBottom:
	        p->drawRect(x1, y1      , x2-x1, third);
		p->drawRect(x1, y2-third, x2-x1, third);
		break;
	    case BSLeft:
	        p->drawRect(x1      , y1+1, third, y2-y1-1);
		p->drawRect(x2-third, y1+1, third, y2-y1-1);
		break;
	    case BSRight:
	        p->drawRect(x1      , y1+1, third, y2-y1-1);
		p->drawRect(x2-third, y1+1, third, y2-y1-1);
		break;
	    }
	}
	else
	{
	    int adjbw1bigthird;
	    if (adjbw1>0) adjbw1bigthird = adjbw1+1;
	    else adjbw1bigthird = adjbw1 - 1;
	    adjbw1bigthird /= 3;

	    int adjbw2bigthird;
	    if (adjbw2>0) adjbw2bigthird = adjbw2 + 1;
	    else adjbw2bigthird = adjbw2 - 1;
	    adjbw2bigthird /= 3;

	  switch(s)
	    {
	    case BSTop:
	      drawBorder(p, x1+QMAX((-adjbw1*2+1)/3,0), y1        , x2-QMAX((-adjbw2*2+1)/3,0), y1 + third, s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
	      drawBorder(p, x1+QMAX(( adjbw1*2+1)/3,0), y2 - third, x2-QMAX(( adjbw2*2+1)/3,0), y2        , s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
	      break;
	    case BSLeft:
	      drawBorder(p, x1        , y1+QMAX((-adjbw1*2+1)/3,0), x1+third, y2-QMAX((-adjbw2*2+1)/3,0), s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
	      drawBorder(p, x2 - third, y1+QMAX(( adjbw1*2+1)/3,0), x2      , y2-QMAX(( adjbw2*2+1)/3,0), s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
	      break;
	    case BSBottom:
	      drawBorder(p, x1+QMAX(( adjbw1*2+1)/3,0), y1      , x2-QMAX(( adjbw2*2+1)/3,0), y1+third, s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
	      drawBorder(p, x1+QMAX((-adjbw1*2+1)/3,0), y2-third, x2-QMAX((-adjbw2*2+1)/3,0), y2      , s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
	      break;
	    case BSRight:
            drawBorder(p, x1      , y1+QMAX(( adjbw1*2+1)/3,0), x1+third, y2-QMAX(( adjbw2*2+1)/3,0), s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
	    drawBorder(p, x2-third, y1+QMAX((-adjbw1*2+1)/3,0), x2      , y2-QMAX((-adjbw2*2+1)/3,0), s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
	      break;
	    default:
	      break;
	    }
	}
        break;
    }
    case RIDGE:
    case GROOVE:
    {
        EBorderStyle s1;
        EBorderStyle s2;
        if (style==GROOVE)
        {
            s1 = INSET;
            s2 = OUTSET;
        }
        else
        {
            s1 = OUTSET;
            s2 = INSET;
        }

	int adjbw1bighalf;
	int adjbw2bighalf;
	if (adjbw1>0) adjbw1bighalf=adjbw1+1;
	else adjbw1bighalf=adjbw1-1;
	adjbw1bighalf/=2;

	if (adjbw2>0) adjbw2bighalf=adjbw2+1;
	else adjbw2bighalf=adjbw2-1;
	adjbw2bighalf/=2;

        switch (s)
	{
	case BSTop:
	    drawBorder(p, x1+QMAX(-adjbw1  ,0)/2,  y1        , x2-QMAX(-adjbw2,0)/2, (y1+y2+1)/2, s, c, textcolor, s1, adjbw1bighalf, adjbw2bighalf);
	    drawBorder(p, x1+QMAX( adjbw1+1,0)/2, (y1+y2+1)/2, x2-QMAX( adjbw2+1,0)/2,  y2        , s, c, textcolor, s2, adjbw1/2, adjbw2/2);
	    break;
	case BSLeft:
            drawBorder(p,  x1        , y1+QMAX(-adjbw1  ,0)/2, (x1+x2+1)/2, y2-QMAX(-adjbw2,0)/2, s, c, textcolor, s1, adjbw1bighalf, adjbw2bighalf);
	    drawBorder(p, (x1+x2+1)/2, y1+QMAX( adjbw1+1,0)/2,  x2        , y2-QMAX( adjbw2+1,0)/2, s, c, textcolor, s2, adjbw1/2, adjbw2/2);
	    break;
	case BSBottom:
	    drawBorder(p, x1+QMAX( adjbw1  ,0)/2,  y1        , x2-QMAX( adjbw2,0)/2, (y1+y2+1)/2, s, c, textcolor, s2,  adjbw1bighalf, adjbw2bighalf);
	    drawBorder(p, x1+QMAX(-adjbw1+1,0)/2, (y1+y2+1)/2, x2-QMAX(-adjbw2+1,0)/2,  y2        , s, c, textcolor, s1, adjbw1/2, adjbw2/2);
	    break;
	case BSRight:
            drawBorder(p,  x1        , y1+QMAX( adjbw1  ,0)/2, (x1+x2+1)/2, y2-QMAX( adjbw2,0)/2, s, c, textcolor, s2, adjbw1bighalf, adjbw2bighalf);
	    drawBorder(p, (x1+x2+1)/2, y1+QMAX(-adjbw1+1,0)/2,  x2        , y2-QMAX(-adjbw2+1,0)/2, s, c, textcolor, s1, adjbw1/2, adjbw2/2);
	    break;
	}
        break;
    }
    case INSET:
        if(s == BSTop || s == BSLeft)
            c = c.dark();

        /* nobreak; */
    case OUTSET:
        if(style == OUTSET && (s == BSBottom || s == BSRight))
            c = c.dark();
        /* nobreak; */
    case SOLID:
        QPointArray quad(4);
        p->setPen(Qt::NoPen);
        p->setBrush(c);
	Q_ASSERT(x2>=x1);
	Q_ASSERT(y2>=y1);
	if (adjbw1==0 && adjbw2 == 0)
	  {
            p->drawRect(x1,y1,x2-x1,y2-y1);
	    return;
	  }
	switch(s) {
        case BSTop:
            quad.setPoints(4,
			   x1+QMAX(-adjbw1,0), y1,
                           x1+QMAX( adjbw1,0), y2,
                           x2-QMAX( adjbw2,0), y2,
                           x2-QMAX(-adjbw2,0), y1);
            break;
        case BSBottom:
            quad.setPoints(4,
			   x1+QMAX( adjbw1,0), y1,
                           x1+QMAX(-adjbw1,0), y2,
                           x2-QMAX(-adjbw2,0), y2,
                           x2-QMAX( adjbw2,0), y1);
            break;
        case BSLeft:
	  quad.setPoints(4,
			 x1, y1+QMAX(-adjbw1,0),
               		 x1, y2-QMAX(-adjbw2,0),
			 x2, y2-QMAX( adjbw2,0),
			 x2, y1+QMAX( adjbw1,0));
            break;
        case BSRight:
	  quad.setPoints(4,
			 x1, y1+QMAX( adjbw1,0),
               		 x1, y2-QMAX( adjbw2,0),
			 x2, y2-QMAX(-adjbw2,0),
			 x2, y1+QMAX(-adjbw1,0));
            break;
        }
	p->drawConvexPolygon(quad);
        break;
    }

    if(invalidisInvert && p->rasterOp() == Qt::XorROP)
        p->setRasterOp(Qt::CopyROP);
}

void RenderObject::paintBorder(QPainter *p, int _tx, int _ty, int w, int h, const RenderStyle* style, bool begin, bool end)
{
    const QColor& tc = style->borderTopColor();
    const QColor& bc = style->borderBottomColor();
    const QColor& lc = style->borderLeftColor();
    const QColor& rc = style->borderRightColor();

    bool tt = style->borderTopIsTransparent();
    bool bt = style->borderBottomIsTransparent();
    bool rt = style->borderRightIsTransparent();
    bool lt = style->borderLeftIsTransparent();
    
    EBorderStyle ts = style->borderTopStyle();
    EBorderStyle bs = style->borderBottomStyle();
    EBorderStyle ls = style->borderLeftStyle();
    EBorderStyle rs = style->borderRightStyle();

    bool render_t = ts > BHIDDEN && !tt;
    bool render_l = ls > BHIDDEN && begin && !lt;
    bool render_r = rs > BHIDDEN && end && !rt;
    bool render_b = bs > BHIDDEN && !bt;

    if(render_t) {
        bool ignore_left =
            (tc == lc) && (tt == lt) &&
            (ts <= OUTSET) &&
            (ls == DOTTED || ls == DASHED || ls == SOLID || ls == OUTSET);

        bool ignore_right =
            (tc == rc) && (tt == rt) &&
            (ts <= OUTSET) &&
            (rs == DOTTED || rs == DASHED || rs == SOLID || rs == INSET);
        
        drawBorder(p, _tx, _ty, _tx + w, _ty +  style->borderTopWidth(), BSTop, tc, style->color(), ts,
                   ignore_left?0:style->borderLeftWidth(),
                   ignore_right?0:style->borderRightWidth());
    }

    if(render_b) {
        bool ignore_left =
        (bc == lc) && (bt == lt) &&
        (bs <= OUTSET) &&
        (ls == DOTTED || ls == DASHED || ls == SOLID || ls == OUTSET);

        bool ignore_right =
            (bc == rc) && (bt == rt) &&
            (bs <= OUTSET) &&
            (rs == DOTTED || rs == DASHED || rs == SOLID || rs == INSET);
        
        drawBorder(p, _tx, _ty + h - style->borderBottomWidth(), _tx + w, _ty + h, BSBottom, bc, style->color(), bs,
                   ignore_left?0:style->borderLeftWidth(),
                   ignore_right?0:style->borderRightWidth());
    }
    
    if(render_l)
    {
	bool ignore_top =
	  (tc == lc) && (tt == lt) &&
	  (ls <= OUTSET) &&
	  (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

	bool ignore_bottom =
	  (bc == lc) && (bt == lt) &&
	  (ls <= OUTSET) &&
	  (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        drawBorder(p, _tx, _ty, _tx + style->borderLeftWidth(), _ty + h, BSLeft, lc, style->color(), ls,
		   ignore_top?0:style->borderTopWidth(),
		   ignore_bottom?0:style->borderBottomWidth());
    }

    if(render_r)
    {
	bool ignore_top =
	  (tc == rc) && (tt == rt) &&
	  (rs <= SOLID || rs == INSET) &&
	  (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

	bool ignore_bottom =
	  (bc == rc) && (bt == rt) &&
	  (rs <= SOLID || rs == INSET) &&
	  (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        drawBorder(p, _tx + w - style->borderRightWidth(), _ty, _tx + w, _ty + h, BSRight, rc, style->color(), rs,
		   ignore_top?0:style->borderTopWidth(),
		   ignore_bottom?0:style->borderBottomWidth());
    }
}

void RenderObject::paintOutline(QPainter *p, int _tx, int _ty, int w, int h, const RenderStyle* style)
{
    int ow = style->outlineWidth();
    if(!ow) return;

    const QColor& oc = style->outlineColor();
    EBorderStyle os = style->outlineStyle();

    drawBorder(p, _tx-ow, _ty-ow, _tx, _ty+h+ow, BSLeft,
	       QColor(oc), style->color(),
               os, ow, ow, true);

    drawBorder(p, _tx-ow, _ty-ow, _tx+w+ow, _ty, BSTop,
	       QColor(oc), style->color(),
	       os, ow, ow, true);

    drawBorder(p, _tx+w, _ty-ow, _tx+w+ow, _ty+h+ow, BSRight,
	       QColor(oc), style->color(),
	       os, ow, ow, true);

    drawBorder(p, _tx-ow, _ty+h, _tx+w+ow, _ty+h+ow, BSBottom,
	       QColor(oc), style->color(),
	       os, ow, ow, true);

}

void RenderObject::paint(QPainter *p, int x, int y, int w, int h, int tx, int ty,
                         PaintAction paintAction)
{
    paintObject(p, x, y, w, h, tx, ty, paintAction);
}

void RenderObject::repaintRectangle(int x, int y, int w, int h, bool immediate, bool f)
{
    if(parent()) parent()->repaintRectangle(x, y, w, h, immediate, f);
}

#ifndef NDEBUG

QString RenderObject::information() const
{
    QString str;
    QTextStream ts( &str, IO_WriteOnly );
    ts << renderName()
	<< "(" << (style() ? style()->refCount() : 0) << ")"
       << ": " << (void*)this << "  ";
    if (isInline()) ts << "il ";
    if (childrenInline()) ts << "ci ";
    if (isFloating()) ts << "fl ";
    if (isAnonymousBox()) ts << "an ";
    if (isRelPositioned()) ts << "rp ";
    if (isPositioned()) ts << "ps ";
    if (overhangingContents()) ts << "oc ";
    if (needsLayout()) ts << "nl ";
    if (m_recalcMinMax) ts << "rmm ";
    if (mouseInside()) ts << "mi ";
    if (style() && style()->zIndex()) ts << "zI: " << style()->zIndex();
    if (element() && element()->active()) ts << "act ";
    if (element() && element()->hasAnchor()) ts << "anchor ";
    if (element() && element()->focused()) ts << "focus ";
    if (element()) ts << " <" <<  getTagName(element()->id()).string() << ">";
    ts << " (" << xPos() << "," << yPos() << "," << width() << "," << height() << ")"
	<< (isTableCell() ?
	    ( QString::fromLatin1(" [r=") +
	      QString::number( static_cast<const RenderTableCell *>(this)->row() ) +
	      QString::fromLatin1(" c=") +
	      QString::number( static_cast<const RenderTableCell *>(this)->col() ) +
	      QString::fromLatin1(" rs=") +
	      QString::number( static_cast<const RenderTableCell *>(this)->rowSpan() ) +
	      QString::fromLatin1(" cs=") +
	      QString::number( static_cast<const RenderTableCell *>(this)->colSpan() ) +
	      QString::fromLatin1("]") ) : QString::null );
	return str;
}

void RenderObject::printTree(int indent) const
{
    QString ind;
    ind.fill(' ', indent);

    kdDebug() << ind << information() << endl;

    RenderObject *child = firstChild();
    while( child != 0 )
    {
        child->printTree(indent+2);
        child = child->nextSibling();
    }
}

void RenderObject::dump(QTextStream *stream, QString ind) const
{
    if (isAnonymousBox()) { *stream << " anonymousBox"; }
    if (isFloating()) { *stream << " floating"; }
    if (isPositioned()) { *stream << " positioned"; }
    if (isRelPositioned()) { *stream << " relPositioned"; }
    if (isText()) { *stream << " text"; }
    if (isInline()) { *stream << " inline"; }
    if (isReplaced()) { *stream << " replaced"; }
    if (shouldPaintBackgroundOrBorder()) { *stream << " paintBackground"; }
    if (needsLayout()) { *stream << " needsLayout"; }
    if (minMaxKnown()) { *stream << " minMaxKnown"; }
    if (overhangingContents()) { *stream << " overhangingContents"; }
    if (hasFirstLine()) { *stream << " hasFirstLine"; }
    *stream << endl;

    RenderObject *child = firstChild();
    while( child != 0 )
    {
	*stream << ind << child->renderName() << ": ";
	child->dump(stream,ind+"  ");
	child = child->nextSibling();
    }
}
#endif

void RenderObject::selectionStartEnd(int& spos, int& epos)
{
    if (parent())
        parent()->selectionStartEnd(spos, epos);
}

RenderBlock* RenderObject::createAnonymousBlock()
{
    RenderStyle *newStyle = new RenderStyle();
    newStyle->inheritFrom(m_style);
    newStyle->setDisplay(BLOCK);

    RenderBlock *newBox = new (renderArena()) RenderBlock(0 /* anonymous box */);
    newBox->setStyle(newStyle);
    newBox->setIsAnonymousBox(true);
    return newBox;
}

void RenderObject::handleDynamicFloatPositionChange()
{
    // We have gone from not affecting the inline status of the parent flow to suddenly
    // having an impact.  See if there is a mismatch between the parent flow's
    // childrenInline() state and our state.
    setInline(m_style->display() == INLINE || // m_style->display() == INLINE_BLOCK ||
              m_style->display() == INLINE_TABLE);
    if (isInline() != parent()->childrenInline()) {
        if (!isInline()) {
            if (parent()->isRenderInline()) {
                // We have to split the parent flow.
                RenderInline* parentInline = static_cast<RenderInline*>(parent());
                RenderBlock* newBox = parentInline->createAnonymousBlock();
                
                RenderFlow* oldContinuation = parent()->continuation();
                parentInline->setContinuation(newBox);

                RenderObject* beforeChild = nextSibling();
                parent()->removeChildNode(this);
                parentInline->splitFlow(beforeChild, newBox, this, oldContinuation);
            }
            else if (parent()->isRenderBlock())
                static_cast<RenderBlock*>(parent())->makeChildrenNonInline();
        }
        else {
            // An anonymous block must be made to wrap this inline.
            RenderBlock* box = createAnonymousBlock();
            parent()->insertChildNode(box, this);
            box->appendChildNode(parent()->removeChildNode(this));
        }
    }
}

void RenderObject::setStyle(RenderStyle *style)
{
    if (m_style == style)
        return;

    RenderStyle::Diff d = m_style ? m_style->diff( style ) : RenderStyle::Layout;

    if (m_style && m_parent && d == RenderStyle::Visible && !isText())
        // Do a repaint with the old style first, e.g., for example if we go from
        // having an outline to not having an outline.
        repaint();

    if (m_style && isFloating() && (m_style->floating() != style->floating()))
        // For changes in float styles, we need to conceivably remove ourselves
        // from the floating objects list.
        removeFromObjectLists();
    else if (isPositioned() && (style->position() != ABSOLUTE && style->position() != FIXED))
        // For changes in positioning styles, we need to conceivably remove ourselves
        // from the positioned objects list.
        removeFromObjectLists();

    bool affectsParentBlock = m_style && isFloatingOrPositioned() &&
        (!style->isFloating() && style->position() != ABSOLUTE && style->position() != FIXED)
        && parent() && (parent()->isBlockFlow() || parent()->isInlineFlow());
    
    //qDebug("new style, diff=%d", d);
    // reset style flags
    m_floating = false;
    m_positioned = false;
    m_relPositioned = false;
    m_paintBackground = false;
    // no support for changing the display type dynamically... object must be
    // detached and re-attached as a different type
    //m_inline = true;

    RenderStyle *oldStyle = m_style;
    m_style = style;

    CachedImage* ob = 0;
    CachedImage* nb = 0;

    if (m_style)
    {
        m_style->ref();
        nb = m_style->backgroundImage();
    }
    if (oldStyle)
    {
        ob = oldStyle->backgroundImage();
        oldStyle->deref();
    }

    if( ob != nb ) {
        if(ob) ob->deref(this);
        if(nb) nb->ref(this);
    }

    setShouldPaintBackgroundOrBorder(m_style->backgroundColor().isValid() || 
                                     m_style->hasBorder() || nb );
    m_hasFirstLine = (style->getPseudoStyle(RenderStyle::FIRST_LINE) != 0);

    if (affectsParentBlock)
        handleDynamicFloatPositionChange();
            
    if ( d >= RenderStyle::Position && m_parent ) {
        //qDebug("triggering relayout");
        setNeedsLayoutAndMinMaxRecalc();
    } else if ( m_parent && !isText()) {
        //qDebug("triggering repaint");
    	repaint();
    }
}

void RenderObject::setOverhangingContents(bool p)
{
    if (m_overhangingContents == p)
	return;

    RenderObject *cb = containingBlock();
    if (p)
    {
        m_overhangingContents = true;
        if (cb && cb != this)
            cb->setOverhangingContents();
    }
    else
    {
        RenderObject *n;
        bool c=false;

        for( n = firstChild(); n != 0; n = n->nextSibling() )
        {
            if (n->isPositioned() || n->overhangingContents())
                c=true;
        }

        if (c)
            return;
        else
        {
            m_overhangingContents = false;
            if (cb && cb != this)
                cb->setOverhangingContents(false);
        }
    }
}

QRect RenderObject::viewRect() const
{
    return containingBlock()->viewRect();
}

bool RenderObject::absolutePosition(int &xPos, int &yPos, bool f)
{
    if(parent())
        return parent()->absolutePosition(xPos, yPos, f);
    else
    {
        xPos = yPos = 0;
        return false;
    }
}

void RenderObject::cursorPos(int /*offset*/, int &_x, int &_y, int &height)
{
    _x = _y = height = -1;
}

int RenderObject::paddingTop() const
{
    int w = 0;
    Length padding = m_style->paddingTop();
    if (padding.isPercent())
        w = containingBlock()->contentWidth();
    w = padding.minWidth(w);
    if ( isTableCell() && padding.isVariable() )
	w = static_cast<const RenderTableCell *>(this)->table()->cellPadding();
    return w;
}

int RenderObject::paddingBottom() const
{
    int w = 0;
    Length padding = style()->paddingBottom();
    if (padding.isPercent())
        w = containingBlock()->contentWidth();
    w = padding.minWidth(w);
    if ( isTableCell() && padding.isVariable() )
	w = static_cast<const RenderTableCell *>(this)->table()->cellPadding();
    return w;
}

int RenderObject::paddingLeft() const
{
    int w = 0;
    Length padding = style()->paddingLeft();
    if (padding.isPercent())
        w = containingBlock()->contentWidth();
    w = padding.minWidth(w);
    if ( isTableCell() && padding.isVariable() )
	w = static_cast<const RenderTableCell *>(this)->table()->cellPadding();
    return w;
}

int RenderObject::paddingRight() const
{
    int w = 0;
    Length padding = style()->paddingRight();
    if (padding.isPercent())
        w = containingBlock()->contentWidth();
    w = padding.minWidth(w);
    if ( isTableCell() && padding.isVariable() )
	w = static_cast<const RenderTableCell *>(this)->table()->cellPadding();
    return w;
}

RenderCanvas* RenderObject::canvas() const
{
    RenderObject* o = const_cast<RenderObject*>( this );
    while ( o->parent() ) o = o->parent();

    KHTMLAssert( o->isCanvas() );
    return static_cast<RenderCanvas*>( o );
}

RenderObject *RenderObject::container() const
{
    EPosition pos = m_style->position();
    RenderObject *o = 0;
    if( pos == FIXED ) {
        // container() can be called on an object that is not in the
        // tree yet.  We don't call canvas() since it will assert if it
        // can't get back to the canvas.  Instead we just walk as high up
        // as we can.  If we're in the tree, we'll get the root.  If we
        // aren't we'll get the root of our little subtree (most likely
        // we'll just return 0).
        o = parent();
        while ( o && o->parent() ) o = o->parent();
    }
    else if ( pos == ABSOLUTE ) {
        // Same goes here.  We technically just want our containing block, but
        // we may not have one if we're part of an uninstalled subtree.  We'll
        // climb as high as we can though.
        o = parent();
        while (o && o->style()->position() == STATIC && !o->isRoot() && !o->isCanvas())
            o = o->parent();
    }
    else
	o = parent();
    return o;
}

#if 0  /// this method is unused
void RenderObject::invalidateLayout()
{
    kdDebug() << "RenderObject::invalidateLayout " << renderName() << endl;
    setNeedsLayout(true);
    if (m_parent && !m_parent->needsLayout())
        m_parent->invalidateLayout();
}
#endif

void RenderObject::removeFromObjectLists()
{
    if (isFloating()) {
        RenderBlock* outermostBlock = containingBlock();
        for (RenderBlock* p = outermostBlock;
             p && !p->isCanvas() && p->containsFloat(this) && !p->isFloatingOrPositioned();
             outermostBlock = p, p = p->containingBlock());
        if (outermostBlock)
            outermostBlock->markAllDescendantsWithFloatsForLayout(this);
    }

    if (isPositioned()) {
        RenderObject *p;
        for (p = parent(); p; p = p->parent()) {
            if (p->isRenderBlock())
                static_cast<RenderBlock*>(p)->removePositionedObject(this);
        }
    }
}

RenderArena* RenderObject::renderArena() const
{
    DOM::NodeImpl* elt = element();
    RenderObject* current = parent();
    while (!elt && current) {
        elt = current->element();
        current = current->parent();
    }
    if (!elt)
        return 0;
    return elt->getDocument()->renderArena();
}


void RenderObject::detach(RenderArena* renderArena)
{
    // If we're an overflow:hidden object that currently needs layout, we need
    // to make sure the view isn't holding on to us.
    if (needsLayout() && style()->hidesOverflow()) {
        RenderCanvas* r = canvas();
        if (r && r->view()->layoutObject() == this)
            r->view()->unscheduleRelayout();
    }
    
    remove();
    
    m_next = m_previous = 0;
    
    // by default no refcounting
    arenaDelete(renderArena, this);
}

void RenderObject::arenaDelete(RenderArena *arena, void *base)
{
#ifndef NDEBUG
    void *savedBase = baseOfRenderObjectBeingDeleted;
    baseOfRenderObjectBeingDeleted = base;
#endif
    delete this;
#ifndef NDEBUG
    baseOfRenderObjectBeingDeleted = savedBase;
#endif
    
    // Recover the size left there for us by operator delete and free the memory.
    arena->free(*(size_t *)base, base);
}

void RenderObject::arenaDelete(RenderArena *arena)
{
    arenaDelete(arena, dynamic_cast<void *>(this));
}

FindSelectionResult RenderObject::checkSelectionPoint(int _x, int _y, int _tx, int _ty, DOM::NodeImpl*& node, int & offset )
{
    FindSelectionResult result = checkSelectionPointIgnoringContinuations(_x, _y, _tx, _ty, node, offset);
    
    if (isInline())
        for (RenderObject *c = continuation(); result == SelectionPointAfter && c; c = c->continuation())
            if (c->isInline()) {
                int ncx, ncy;
                c->absolutePosition(ncx, ncy);
                result = c->checkSelectionPointIgnoringContinuations(_x, _y, ncx - c->xPos(), ncy - c->yPos(), node, offset);
            }
    
    return result;
}

FindSelectionResult RenderObject::checkSelectionPointIgnoringContinuations(int _x, int _y, int _tx, int _ty, DOM::NodeImpl*& node, int & offset )
{
    int lastOffset=0;
    int off = offset;
    DOM::NodeImpl* nod = node;
    DOM::NodeImpl* lastNode = 0;
    
    for (RenderObject *child = firstChild(); child; child=child->nextSibling()) {
        FindSelectionResult pos = child->checkSelectionPointIgnoringContinuations(_x, _y, _tx+xPos(), _ty+yPos(), nod, off);
        //kdDebug(6030) << this << " child->findSelectionNode returned " << pos << endl;
        switch(pos) {
        case SelectionPointBeforeInLine:
        case SelectionPointAfterInLine:
        case SelectionPointInside:
            node = nod;
            offset = off;
            return SelectionPointInside;
        case SelectionPointBefore:
            //x,y is before this element -> stop here
            if ( lastNode) {
                node = lastNode;
                offset = lastOffset;
//                 kdDebug(6030) << "ElementImpl::findSelectionNode " << this << " before this child "
//                               << node << "-> returning offset=" << offset << endl;
                return SelectionPointInside;
            } else {
//                 kdDebug(6030) << "ElementImpl::findSelectionNode " << this << " before us -> returning -2" << endl;
                return SelectionPointBefore;
            }
            break;
        case SelectionPointAfter:
//             kdDebug(6030) << "ElementImpl::findSelectionNode: selection after: " << nod << " offset: " << off << endl;
            lastNode = nod;
            lastOffset = off;
        }
    }
    
    node = lastNode;
    offset = lastOffset;
    return SelectionPointAfter;
}

bool RenderObject::mouseInside() const
{ 
    if (!m_mouseInside && continuation()) 
        return continuation()->mouseInside();
    return m_mouseInside; 
}

void RenderObject::setHoverAndActive(NodeInfo& info, bool oldinside, bool inside)
{
    DOM::NodeImpl* elt = element();
    if (elt) {
        bool oldactive = elt->active();
        if (oldactive != (inside && info.active()))
            elt->setActive(inside && info.active());
        if ((oldinside != mouseInside() && style()->affectedByHoverRules()) ||
            (oldactive != elt->active() && style()->affectedByActiveRules()))
            elt->setChanged();
    }
}

bool RenderObject::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, bool inside)
{
    int tx = _tx + xPos();
    int ty = _ty + yPos();

    QRect boundsRect(tx, ty, width(), height());
    inside |= (style()->visibility() != HIDDEN && boundsRect.contains(_x, _y)) || isBody() || isRoot();
    bool inOverflowRect = inside;
    if (!inOverflowRect) {
        QRect overflowRect(tx, ty, overflowWidth(false), overflowHeight(false));
        inOverflowRect = overflowRect.contains(_x, _y);
    }
    
    // ### table should have its own, more performant method
    if ((!isRenderBlock() ||
         !static_cast<RenderBlock*>(this)->isPointInScrollbar(_x, _y, _tx, _ty)) &&
        (overhangingContents() || inOverflowRect || isInline() || isCanvas() ||
         isTableRow() || isTableSection() || inside || mouseInside() ||
         (childrenInline() && firstChild() && firstChild()->isCompact()))) {
        int stx = _tx + xPos();
        int sty = _ty + yPos();
        if (style()->hidesOverflow() && layer())
            layer()->subtractScrollOffset(stx, sty);
        for (RenderObject* child = lastChild(); child; child = child->previousSibling())
            if (!child->layer() && !child->isFloating() &&
                child->nodeAtPoint(info, _x, _y, stx, sty))
                inside = true;
    }

    if (inside) {
        if (info.innerNode() && info.innerNode()->renderer() && 
            !info.innerNode()->renderer()->isInline() && element() && isInline()) {
            // Within the same layer, inlines are ALWAYS fully above blocks.  Change inner node.
            info.setInnerNode(element());
            
            // Clear everything else.
            info.setInnerNonSharedNode(0);
            info.setURLElement(0);
        }
        
        if (!info.innerNode() && element())
            info.setInnerNode(element());

        if(!info.innerNonSharedNode() && element())
            info.setInnerNonSharedNode(element());
        
        if (!info.URLElement()) {
            RenderObject* p = (!isInline() && continuation()) ? continuation() : this;
            while (p) {
                if (p->element() && p->element()->hasAnchor()) {
                    info.setURLElement(p->element());
                    break;
                }
                if (!isFloatingOrPositioned()) break;
                p = p->parent();
            }
        }
    }

    if (!info.readonly()) {
        // lets see if we need a new style
        bool oldinside = mouseInside();
        setMouseInside(inside);
        
        setHoverAndActive(info, oldinside, inside);
        if (!isInline() && continuation())
            continuation()->setHoverAndActive(info, oldinside, inside);
    }

    return inside;
}

short RenderObject::verticalPositionHint( bool firstLine ) const
{
    short vpos = m_verticalPosition;
    if ( m_verticalPosition == PositionUndefined || firstLine ) {
	vpos = getVerticalPosition( firstLine );
	if ( !firstLine )
	    const_cast<RenderObject *>(this)->m_verticalPosition = vpos;
    }
    return vpos;

}

short RenderObject::getVerticalPosition( bool firstLine ) const
{
    if (!isInline())
        return 0;

    // This method determines the vertical position for inline elements.
    int vpos = 0;
    EVerticalAlign va = style()->verticalAlign();
    if ( va == TOP ) {
        vpos = PositionTop;
    } else if ( va == BOTTOM ) {
        vpos = PositionBottom;
    } else if ( va == LENGTH ) {
        vpos = -style()->verticalAlignLength().width( lineHeight( firstLine ) );
    } else  {
        vpos = parent()->verticalPositionHint( firstLine );
        // don't allow elements nested inside text-top to have a different valignment.
        if ( va == BASELINE )
            return vpos;

    //     if ( vpos == PositionTop )
//                 vpos = 0;

        const QFont &f = parent()->font( firstLine );
        int fontsize = f.pixelSize();
    
        if ( va == SUB )
            vpos += fontsize/5 + 1;
        else if ( va == SUPER )
            vpos -= fontsize/3 + 1;
        else if ( va == TEXT_TOP ) {
//                 qDebug( "got TEXT_TOP vertical pos hint" );
//                 qDebug( "parent:" );
//                 qDebug( "CSSLH: %d, CSS_FS: %d, basepos: %d", fontheight, fontsize, parent()->baselinePosition( firstLine ) );
//                 qDebug( "this:" );
//                 qDebug( "CSSLH: %d, CSS_FS: %d, basepos: %d", lineHeight( firstLine ), style()->font().pixelSize(), baselinePosition( firstLine ) );
            vpos += ( baselinePosition( firstLine ) - parent()->baselinePosition( firstLine ) );
        } else if ( va == MIDDLE ) {
#if APPLE_CHANGES
            vpos += - (int)(QFontMetrics(f).xHeight()/2) - lineHeight( firstLine )/2 + baselinePosition( firstLine );
#else
            QRect b = QFontMetrics(f).boundingRect('x');
            vpos += -b.height()/2 - lineHeight( firstLine )/2 + baselinePosition( firstLine );
#endif
        } else if ( va == TEXT_BOTTOM ) {
            vpos += QFontMetrics(f).descent();
            if ( !isReplaced() )
                vpos -= fontMetrics(firstLine).descent();
        } else if ( va == BASELINE_MIDDLE )
            vpos += - lineHeight( firstLine )/2 + baselinePosition( firstLine );
    }
    
    return vpos;
}

short RenderObject::lineHeight( bool firstLine ) const
{
    Length lh = style(firstLine)->lineHeight();

    // its "unset", choose nice default
    if ( lh.value < 0 )
        return style(firstLine)->fontMetrics().lineSpacing();

    if ( lh.isPercent() )
        return lh.minWidth( style(firstLine)->font().pixelSize() );

    // its fixed
    return lh.value;
}

short RenderObject::baselinePosition( bool firstLine ) const
{
    const QFontMetrics &fm = fontMetrics( firstLine );
    return fm.ascent() + ( lineHeight( firstLine ) - fm.height() ) / 2;
}

void RenderObject::invalidateVerticalPositions()
{
    m_verticalPosition = PositionUndefined;
    RenderObject *child = firstChild();
    while( child ) {
	child->invalidateVerticalPositions();
	child = child->nextSibling();
    }
}

void RenderObject::recalcMinMaxWidths()
{
    KHTMLAssert( m_recalcMinMax );

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << " recalcMinMaxWidths() this=" << this <<endl;
#endif

    RenderObject *child = firstChild();
    while( child ) {
        // gcc sucks. if anybody knows a trick to get rid of the
        // warning without adding an extra (unneeded) initialisation,
        // go ahead
	int cmin = 0;
	int cmax = 0;
	bool test = false;
	if ( ( m_minMaxKnown && child->m_recalcMinMax ) || !child->m_minMaxKnown ) {
	    cmin = child->minWidth();
	    cmax = child->maxWidth();
	    test = true;
	}
	if ( child->m_recalcMinMax )
	    child->recalcMinMaxWidths();
	if ( !child->m_minMaxKnown )
	    child->calcMinMaxWidth();
	if ( m_minMaxKnown && test && (cmin != child->minWidth() || cmax != child->maxWidth()) )
	    m_minMaxKnown = false;
	child = child->nextSibling();
    }

    // we need to recalculate, if the contains inline children, as the change could have
    // happened somewhere deep inside the child tree. Also do this for blocks or tables that
    // are inline (i.e., inline-block and inline-table).
    if ((!isInline() || isInlineBlockOrInlineTable()) && childrenInline())
	m_minMaxKnown = false;

    if ( !m_minMaxKnown )
	calcMinMaxWidth();
    m_recalcMinMax = false;
}

void RenderObject::scheduleRelayout(RenderObject* clippedObj)
{
    if (!isCanvas()) return;
    KHTMLView *view = static_cast<RenderCanvas *>(this)->view();
    if ( view )
        view->scheduleRelayout(clippedObj);
}


void RenderObject::removeLeftoverAnonymousBoxes()
{
}

InlineBox* RenderObject::createInlineBox(bool makePlaceHolderBox)
{
    return new (renderArena()) InlineBox(this);
}

void RenderObject::getTextDecorationColors(int decorations, QColor& underline, QColor& overline,
                                           QColor& linethrough, bool quirksMode)
{
    RenderObject* curr = this;
    do {
        int currDecs = curr->style()->textDecoration();
        if (currDecs) {
            if (currDecs & UNDERLINE) {
                decorations &= ~UNDERLINE;
                underline = curr->style()->color();
            }
            if (currDecs & OVERLINE) {
                decorations &= ~OVERLINE;
                overline = curr->style()->color();
            }
            if (currDecs & LINE_THROUGH) {
                decorations &= ~LINE_THROUGH;
                linethrough = curr->style()->color();
            }
        }
        curr = curr->parent();
        if (curr && curr->isRenderBlock() && curr->continuation())
            curr = curr->continuation();
    } while (curr && decorations && (!quirksMode || !curr->element() ||
                                     (curr->element()->id() != ID_A && curr->element()->id() != ID_FONT)));

    // If we bailed out, use the element we bailed out at (typically a <font> or <a> element).
    if (decorations && curr) {
        if (decorations & UNDERLINE)
            underline = curr->style()->color();
        if (decorations & OVERLINE)
            overline = curr->style()->color();
        if (decorations & LINE_THROUGH)
            linethrough = curr->style()->color();
    }        
}

QChar RenderObject::backslashAsCurrencySymbol() const
{
#if !APPLE_CHANGES
    return '\\';
#else
    NodeImpl *node = element();
    if (!node)
        return '\\';
    DocumentImpl *document = node->getDocument();
    if (!document)
        return '\\';
    Decoder *decoder = document->decoder();
    if (!decoder)
        return '\\';
    const QTextCodec *codec = decoder->codec();
    if (!codec)
        return '\\';
    return codec->backslashAsCurrencySymbol();
#endif
}
