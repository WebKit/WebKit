/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
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
#include "rendering/render_root.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_docimpl.h"
#include "misc/htmlhashes.h"
#include <kdebug.h>
#include <qpainter.h>
#include "khtmlview.h"

#include <assert.h>
using namespace DOM;
using namespace khtml;

RenderObject *RenderObject::createObject(DOM::NodeImpl* node,  RenderStyle* style)
{
    RenderObject *o = 0;
    switch(style->display())
    {
    case NONE:
        break;
    case INLINE:
    case BLOCK:
        // In quirks mode, if <td> has a display of block, build a table cell instead.
        // This corrects erroneous HTML.  A better fix would be to implement full-blown
        // CSS2 anonymous table render object construction, but until then, this will have
        // to suffice. -dwh
        if (style->display() == BLOCK && node->id() == ID_TD &&
            node->getDocument()->parseMode() != DocumentImpl::Strict)
            o = new RenderTableCell(node);
        else
            o = new RenderFlow(node);
        break;
    case LIST_ITEM:
        o = new RenderListItem(node);
        break;
    case RUN_IN:
    case COMPACT:
    case MARKER:
        break;
    case TABLE:
    case INLINE_TABLE:
        // ### set inline/block right
        //kdDebug( 6040 ) << "creating RenderTable" << endl;
        o = new RenderTable(node);
        break;
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
        o = new RenderTableSection(node);
        break;
    case TABLE_ROW:
        o = new RenderTableRow(node);
        break;
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
        o = new RenderTableCol(node);
        break;
    case TABLE_CELL:
        o = new RenderTableCell(node);
        break;
    case TABLE_CAPTION:
        o = new RenderTableCaption(node);
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
m_layouted( false ),
m_unused( false ),
m_minMaxKnown( false ),
m_floating( false ),

m_positioned( false ),
m_overhangingContents( false ),
m_relPositioned( false ),
m_printSpecial( false ),

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

void RenderObject::setLayouted(bool b) 
{
    m_layouted = b;
    if (b) {
        RenderLayer* l = layer();
        if (l) {
            l->setWidth(width());
            l->setHeight(height());
            l->updateLayerPosition();
        }
    }
    else {
        RenderObject *o = m_parent;
        RenderObject *root = this;
        while( o ) {
            o->m_layouted = false;
            root = o;
            o = o->m_parent;
        }
        root->scheduleRelayout();
    }
}
    
RenderObject *RenderObject::containingBlock() const
{
    if(isTableCell()) {
        return static_cast<const RenderTableCell *>(this)->table();
    }

    RenderObject *o = parent();
    if(m_style->position() == FIXED) {
        while ( o && !o->isRoot() )
            o = o->parent();
    }
    else if(m_style->position() == ABSOLUTE) {
        while (o && o->style()->position() == STATIC && !o->isHtml() && !o->isRoot())
            o = o->parent();
    } else {
        while(o && o->isInline())
            o = o->parent();
    }
    // this is just to make sure we return a valid element.
    // the case below should never happen...
    if(!o) {
        if(!isRoot()) {
#ifndef NDEBUG
            kdDebug( 6040 ) << this << ": " << renderName() << "(RenderObject): No containingBlock!" << endl;
            const RenderObject* p = this;
            while (p->parent()) p = p->parent();
            p->printTree();
#endif
        }
        return const_cast<RenderObject *>(this);
    } else
        return o;
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
        {
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

void RenderObject::printBorder(QPainter *p, int _tx, int _ty, int w, int h, const RenderStyle* style, bool begin, bool end)
{
    const QColor& tc = style->borderTopColor();
    const QColor& bc = style->borderBottomColor();

    EBorderStyle ts = style->borderTopStyle();
    EBorderStyle bs = style->borderBottomStyle();
    EBorderStyle ls = style->borderLeftStyle();
    EBorderStyle rs = style->borderRightStyle();

    bool render_t = ts > BHIDDEN;
    bool render_l = ls > BHIDDEN && begin;
    bool render_r = rs > BHIDDEN && end;
    bool render_b = bs > BHIDDEN;

    if(render_t)
        drawBorder(p, _tx, _ty, _tx + w, _ty +  style->borderTopWidth(), BSTop, tc, style->color(), ts,
                   (render_l && ls<=DOUBLE?style->borderLeftWidth():0),
		   (render_r && rs<=DOUBLE?style->borderRightWidth():0));

    if(render_b)
        drawBorder(p, _tx, _ty + h - style->borderBottomWidth(), _tx + w, _ty + h, BSBottom, bc, style->color(), bs,
                   (render_l && ls<=DOUBLE?style->borderLeftWidth():0),
		   (render_r && rs<=DOUBLE?style->borderRightWidth():0));

    if(render_l)
    {
	const QColor& lc = style->borderLeftColor();

	bool ignore_top =
	  (tc == lc) &&
	  (ls <= OUTSET) &&
	  (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

	bool ignore_bottom =
	  (bc == lc) &&
	  (ls <= OUTSET) &&
	  (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        drawBorder(p, _tx, _ty, _tx + style->borderLeftWidth(), _ty + h, BSLeft, lc, style->color(), ls,
		   ignore_top?0:style->borderTopWidth(),
		   ignore_bottom?0:style->borderBottomWidth());
    }

    if(render_r)
    {
	const QColor& rc = style->borderRightColor();

	bool ignore_top =
	  (tc == rc) &&
	  (rs <= SOLID || rs == INSET) &&
	  (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

	bool ignore_bottom =
	  (bc == rc) &&
	  (rs <= SOLID || rs == INSET) &&
	  (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        drawBorder(p, _tx + w - style->borderRightWidth(), _ty, _tx + w, _ty + h, BSRight, rc, style->color(), rs,
		   ignore_top?0:style->borderTopWidth(),
		   ignore_bottom?0:style->borderBottomWidth());
    }
}

void RenderObject::printOutline(QPainter *p, int _tx, int _ty, int w, int h, const RenderStyle* style)
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

void RenderObject::print( QPainter *p, int x, int y, int w, int h, int tx, int ty)
{
    printObject(p, x, y, w, h, tx, ty);
}

void RenderObject::repaintRectangle(int x, int y, int w, int h, bool f)
{
    if(parent()) parent()->repaintRectangle(x, y, w, h, f);
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
    if (layouted()) ts << "lt ";
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
    if (hasSpecialObjects()) { *stream << " specialObjects"; }
    if (layouted()) { *stream << " layouted"; }
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

void RenderObject::setStyle(RenderStyle *style)
{
    if (m_style == style)
        return;

    RenderStyle::Diff d = m_style ? m_style->diff( style ) : RenderStyle::Layout;
    //qDebug("new style, diff=%d", d);
    // reset style flags
    m_floating = false;
    m_positioned = false;
    m_relPositioned = false;
    m_printSpecial = false;
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

    setSpecialObjects( m_style->backgroundColor().isValid() || m_style->hasBorder() || nb );
    m_hasFirstLine = (style->getPseudoStyle(RenderStyle::FIRST_LINE) != 0);

    if ( d >= RenderStyle::Position && m_parent ) {
	//qDebug("triggering relayout");
	setMinMaxKnown(false);
	setLayouted(false);
    } else if ( m_parent ) {
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
        if (cb!=this)
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
            if (cb!=this)
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
    int cw=0;
    if (style()->paddingTop().isPercent())
        cw = containingBlock()->contentWidth();
    return m_style->paddingTop().minWidth(cw);
}

int RenderObject::paddingBottom() const
{
    int cw=0;
    if (style()->paddingBottom().isPercent())
        cw = containingBlock()->contentWidth();
    return m_style->paddingBottom().minWidth(cw);
}

int RenderObject::paddingLeft() const
{
    int cw=0;
    if (style()->paddingLeft().isPercent())
        cw = containingBlock()->contentWidth();
    return m_style->paddingLeft().minWidth(cw);
}

int RenderObject::paddingRight() const
{
    int cw=0;
    if (style()->paddingRight().isPercent())
        cw = containingBlock()->contentWidth();
    return m_style->paddingRight().minWidth(cw);
}

RenderRoot* RenderObject::root() const
{
    RenderObject* o = const_cast<RenderObject*>( this );
    while ( o->parent() ) o = o->parent();

    KHTMLAssert( o->isRoot() );
    return static_cast<RenderRoot*>( o );
}

RenderObject *RenderObject::container() const
{
    EPosition pos = m_style->position();
    RenderObject *o = 0;
    if( pos == FIXED )
	o = root();
    else if ( pos == ABSOLUTE )
	o = containingBlock();
    else
	o = parent();
    return o;
}

#if 0  /// this method is unused
void RenderObject::invalidateLayout()
{
    kdDebug() << "RenderObject::invalidateLayout " << renderName() << endl;
    setLayouted(false);
    if (m_parent && m_parent->layouted())
        m_parent->invalidateLayout();
}
#endif

void RenderObject::removeFromSpecialObjects()
{
    if (isPositioned() || isFloating()) {
	RenderObject *p;
	for (p = parent(); p; p = p->parent()) {
	    if (p->isFlow())
		static_cast<RenderFlow*>(p)->removeSpecialObject(this);
	}
    }
}

void RenderObject::detach()
{
    remove();
    // by default no refcounting
    delete this;
}

FindSelectionResult RenderObject::checkSelectionPoint( int _x, int _y, int _tx, int _ty, DOM::NodeImpl*& node, int & offset )
{
    int lastOffset=0;
    int off = offset;
    DOM::NodeImpl* nod = node;
    DOM::NodeImpl* lastNode = 0;
    for (RenderObject *child = firstChild(); child; child=child->nextSibling()) {
        khtml::FindSelectionResult pos = child->checkSelectionPoint(_x, _y, _tx+xPos(), _ty+yPos(), nod, off);
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

bool RenderObject::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty)
{
    int tx = _tx + xPos();
    int ty = _ty + yPos();
    
    bool inside = (style()->visibility() != HIDDEN && ((_y >= ty) && (_y < ty + height()) &&
                  (_x >= tx) && (_x < tx + width()))) || isBody() || isHtml();
    bool inner = !info.innerNode();

    // ### table should have its own, more performant method
    if (overhangingContents() || isInline() || isRoot() || isTableRow() || isTableSection() || inside || mouseInside() ) {
        for (RenderObject* child = lastChild(); child; child = child->previousSibling())
            if (!child->layer() && child->nodeAtPoint(info, _x, _y, _tx+xPos(), _ty+yPos()))
                inside = true;
    }

    if (inside && element()) {
        if (!info.innerNode())
            info.setInnerNode(element());

        if(!info.innerNonSharedNode())
            info.setInnerNonSharedNode(element());
        
        if (!info.URLElement()) {
            RenderObject* p = this;
            while (p) {
                if (p->element() && p->element()->hasAnchor()) {
                    info.setURLElement(p->element());
                    break;
                }
                if (!isSpecial()) break;
                p = p->parent();
            }
        }
    }

    if (!info.readonly()) {
        // lets see if we need a new style
        bool oldinside = mouseInside();
        setMouseInside(inside && inner);
        if (element()) {
            bool oldactive = element()->active();
            if (oldactive != (inside && info.active() && element() == info.innerNode()))
                element()->setActive(inside && info.active() && element() == info.innerNode());
            if ( ((oldinside != mouseInside()) && style()->hasHover()) ||
                 ((oldactive != element()->active()) && style()->hasActive()))
                element()->setChanged();
        }
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
    // vertical align for table cells has a different meaning
    int vpos = 0;
    if ( !isTableCell() ) {
      if (parent() && parent()->childrenInline()) { // Vertical-align only has meaning for inline elements and table cells. -dwh
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
            int fontheight = parent()->lineHeight( firstLine );
            int fontsize = f.pixelSize();
            int halfleading = ( fontheight - fontsize ) / 2;

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
                vpos += ( baselinePosition( firstLine ) - parent()->baselinePosition( firstLine ) +
                        halfleading );
	    } else if ( va == MIDDLE ) {
#ifdef APPLE_CHANGES
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
      }
    }
    return vpos;
}

short RenderObject::lineHeight( bool firstLine ) const
{
    Length lh;
    if( firstLine && hasFirstLine() ) {
	RenderStyle *pseudoStyle  = style()->getPseudoStyle(RenderStyle::FIRST_LINE);
	if ( pseudoStyle )
            lh = pseudoStyle->lineHeight();
    }
    else
        lh = style()->lineHeight();

    // its "unset", choose nice default
    if ( lh.value < 0 )
        return style()->fontMetrics().lineSpacing();

    if ( lh.isPercent() )
        return lh.minWidth( style()->font().pixelSize() );

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
    // happened somewhere deep inside the child tree
    if ( !isInline() && childrenInline() )
	m_minMaxKnown = false;

    if ( !m_minMaxKnown )
	calcMinMaxWidth();
    m_recalcMinMax = false;
}

void RenderObject::scheduleRelayout()
{
    if (!isRoot()) return;
    KHTMLView *view = static_cast<RenderRoot *>(this)->view();
    if ( view )
	view->scheduleRelayout();
}


void RenderObject::removeLeftoverAnonymousBoxes()
{
}
