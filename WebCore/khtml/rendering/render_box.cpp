/**
 * This file is part of the DOM implementation for KDE.
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
 */
// -------------------------------------------------------------------------
//#define DEBUG_LAYOUT


#include <qpainter.h>

#include "rendering/render_box.h"
#include "rendering/render_replaced.h"
#include "rendering/render_root.h"
#include "rendering/render_table.h"

#include "misc/htmlhashes.h"
#include "xml/dom_nodeimpl.h"

#include <khtmlview.h>
#include <kdebug.h>
#include <assert.h>


using namespace DOM;
using namespace khtml;

#define TABLECELLMARGIN -0x4000

RenderBox::RenderBox(DOM::NodeImpl* node)
    : RenderContainer(node)
{
    m_minWidth = -1;
    m_maxWidth = -1;
    m_width = m_height = 0;
    m_x = 0;
    m_y = 0;
    m_marginTop = 0;
    m_marginBottom = 0;
    m_marginLeft = 0;
    m_marginRight = 0;
}

void RenderBox::setStyle(RenderStyle *_style)
{
    RenderObject::setStyle(_style);

    // ### move this into the parser. --> should work. Lars
    // if only horizontal position was defined, vertical should be 50%
    //if(!_style->backgroundXPosition().isVariable() && _style->backgroundYPosition().isVariable())
    //style()->setBackgroundYPosition(Length(50, Percent));

    switch(_style->position())
    {
    case ABSOLUTE:
    case FIXED:
        setPositioned(true);
        break;
    default:
        setPositioned(false);
        if(_style->isFloating()) {
            setFloating(true);
        } else {
            if(_style->position() == RELATIVE)
                setRelPositioned(true);
        }
    }
}

RenderBox::~RenderBox()
{
    //kdDebug( 6040 ) << "Element destructor: this=" << nodeName().string() << endl;
}

short RenderBox::contentWidth() const
{
    short w = m_width - style()->borderLeftWidth() - style()->borderRightWidth();
    if(style()->hasPadding())
        w -= paddingLeft() + paddingRight();

    //kdDebug( 6040 ) << "RenderBox::contentWidth(2) = " << w << endl;
    return w;
}

int RenderBox::contentHeight() const
{
    int h = m_height - style()->borderTopWidth() - style()->borderBottomWidth();
    if(style()->hasPadding())
        h -= paddingTop() + paddingBottom();

    return h;
}

void RenderBox::setPos( int xPos, int yPos )
{
    m_x = xPos; m_y = yPos;
}

short RenderBox::width() const
{
    return m_width;
}

int RenderBox::height() const
{
    return m_height;
}


// --------------------- printing stuff -------------------------------

void RenderBox::print(QPainter *p, int _x, int _y, int _w, int _h,
                                  int _tx, int _ty)
{
    _tx += m_x;
    _ty += m_y;

    // default implementation. Just pass things through to the children
    RenderObject *child = firstChild();
    while(child != 0)
    {
        child->print(p, _x, _y, _w, _h, _tx, _ty);
        child = child->nextSibling();
    }
}

void RenderBox::setPixmap(const QPixmap &, const QRect&, CachedImage *image)
{
    if(image && image->pixmap_size() == image->valid_rect().size() && parent())
        repaint();      //repaint bg when it finished loading
}


void RenderBox::printBoxDecorations(QPainter *p,int, int _y,
                                       int, int _h, int _tx, int _ty)
{
    //kdDebug( 6040 ) << renderName() << "::printDecorations()" << endl;

    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
    _ty -= borderTopExtra();

    int my = QMAX(_ty,_y);
    int mh;
    if (_ty<_y)
        mh= QMAX(0,h-(_y-_ty));
    else
        mh = QMIN(_h,h);

    printBackground(p, style()->backgroundColor(), style()->backgroundImage(), my, mh, _tx, _ty, w, h);

    if(style()->hasBorder())
        printBorder(p, _tx, _ty, w, h, style());
}

void RenderBox::printBackground(QPainter *p, const QColor &c, CachedImage *bg, int clipy, int cliph, int _tx, int _ty, int w, int h)
{
    if(c.isValid())
        p->fillRect(_tx, clipy, w, cliph, c);
    // no progressive loading of the background image
    if(bg && bg->pixmap_size() == bg->valid_rect().size() && !bg->isTransparent() && !bg->isErrorImage()) {
        //kdDebug( 6040 ) << "printing bgimage at " << _tx << "/" << _ty << endl;
        // ### might need to add some correct offsets
        // ### use paddingX/Y

        //hacky stuff
        RenderStyle* sptr = style();
        if ( isHtml() && firstChild() && !style()->backgroundImage() )
            sptr = firstChild()->style();

        int sx = 0;
        int sy = 0;
	    int cw,ch;
        int cx,cy;
        int vpab = borderRight() + borderLeft();
        int hpab = borderTop() + borderBottom();

        // CSS2 chapter 14.2.1

        if (sptr->backgroundAttachment())
        {
            //scroll
            int pw = m_width - vpab;
            int h = m_height;
            if (isTableCell()) {
                // Table cells' m_height variable is wrong.  You have to take into
                // account this hack extra stuff to get the right height. 
                // Otherwise using background-position: bottom won't work in
                // a table cell that has the extra height. -dwh
                RenderTableCell* tableCell = static_cast<RenderTableCell*>(this);
                h += tableCell->borderTopExtra() + tableCell->borderBottomExtra();
            }
            int ph = h - hpab;
            
            int pixw = bg->pixmap_size().width();
            int pixh = bg->pixmap_size().height();
            EBackgroundRepeat bgr = sptr->backgroundRepeat();
            if( (bgr == NO_REPEAT || bgr == REPEAT_Y) && w > pixw ) {
                cw = pixw;
                cx = _tx + sptr->backgroundXPosition().minWidth(pw-pixw);
            } else {
                cw = w-vpab;
                cx = _tx;
                sx =  pixw - ((sptr->backgroundXPosition().minWidth(pw-pixw)) % pixw );
            }

            cx += borderLeft();

            if( (bgr == NO_REPEAT || bgr == REPEAT_X) && h > pixh ) {
                ch = pixh;
                cy = _ty + sptr->backgroundYPosition().minWidth(ph-pixh);
            } else {
                ch = h-hpab;
                cy = _ty;
                sy = pixh - ((sptr->backgroundYPosition().minWidth(ph-pixh)) % pixh );
            }

            cy += borderTop();
        }
        else
        {
            //fixed
            QRect vr = viewRect();
            int pw = vr.width();
            int ph = vr.height();

            int pixw = bg->pixmap_size().width();
            int pixh = bg->pixmap_size().height();
            EBackgroundRepeat bgr = sptr->backgroundRepeat();
            if( (bgr == NO_REPEAT || bgr == REPEAT_Y) && w > pixw ) {
                cw = pixw;
                cx = vr.x() + sptr->backgroundXPosition().minWidth(pw-pixw);
            } else {
                cw = pw;
                cx = vr.x();
                sx =  pixw - ((sptr->backgroundXPosition().minWidth(pw-pixw)) % pixw );
            }

            if( (bgr == NO_REPEAT || bgr == REPEAT_X) && h > pixh ) {
                ch = pixh;
                cy = vr.y() + sptr->backgroundYPosition().minWidth(ph-pixh);
            } else {
                ch = ph;
                cy = vr.y();
                sy = pixh - ((sptr->backgroundYPosition().minWidth(ph-pixh)) % pixh );
            }

            QRect fix(cx,cy,cw,ch);
            QRect ele(_tx+borderLeft(),_ty+borderTop(),w-vpab,h-hpab);
            QRect b = fix.intersect(ele);
            sx+=b.x()-cx;
            sy+=b.y()-cy;
            cx=b.x();cy=b.y();cw=b.width();ch=b.height();
        }


//        kdDebug() << "cx="<<cx << " cy="<<cy<< " cw="<<cw << " ch="<<ch << " sx="<<sx << " sy="<<sy << endl;

        if (cw>0 && ch>0)
            p->drawTiledPixmap(cx, cy, cw, ch, bg->tiled_pixmap(c), sx, sy);
    }
}

void RenderBox::outlineBox(QPainter *p, int _tx, int _ty, const char *color)
{
    p->setPen(QPen(QColor(color), 1, Qt::DotLine));
    p->setBrush( Qt::NoBrush );
    p->drawRect(_tx, _ty, m_width, m_height);
}


void RenderBox::calcClip(QPainter* p, int tx, int ty)
{
    int bl=borderLeft(),bt=borderTop(),bb=borderBottom(),br=borderRight();
    int clipx = tx+bl;
    int clipy = ty+bt;
    int clipw = m_width-bl-br;
    int cliph = m_height-bt-bb;

    if (!style()->clipLeft().isVariable())
    {
        int c=style()->clipLeft().width(m_width-bl-br);
        clipx+=c;
        clipw-=c;
    }
    if (!style()->clipRight().isVariable())
    {
	int w = style()->clipRight().width(m_width-bl-br);
	if ( style()->jsClipMode() )
	    clipw = w + tx + bl;
	else
	    clipw -= m_width - bl - br - w;
    }
    if (!style()->clipTop().isVariable())
    {
        int c=style()->clipTop().width(m_height-bt-bb);
        clipy+=c;
        cliph-=c;
    }
    if (!style()->clipBottom().isVariable())
    {
	int h = style()->clipBottom().width(m_height-bt-bb);
	if ( style()->jsClipMode() )
	    cliph = h + ty + bt;
	else
            cliph -= m_height - bt - bb - h;
    }
    //kdDebug( 6040 ) << "setting clip("<<clipx<<","<<clipy<<","<<clipw<<","<<cliph<<")"<<endl;

    QRect cr(clipx,clipy,clipw,cliph);
    cr = p->xForm(cr);
#if APPLE_CHANGES
    p->save();
    p->addClip(cr);
#else
    QRegion creg(cr);
    QRegion old = p->clipRegion();
    if (!old.isNull())
        creg = old.intersect(creg);

    p->save();
    p->setClipRegion(creg);
#endif
}

void RenderBox::close()
{
    setMinMaxKnown(false);
    setLayouted( false );
}

short RenderBox::containingBlockWidth() const
{
    if ( ( style()->htmlHacks() || isTable() ) && style()->flowAroundFloats() && containingBlock()->isFlow()
            && style()->width().isVariable())
        return static_cast<RenderFlow*>(containingBlock())->lineWidth(m_y);
    else
        return containingBlock()->contentWidth();
}

bool RenderBox::absolutePosition(int &xPos, int &yPos, bool f)
{
    if ( style()->position() == FIXED )
	f = true;
    RenderObject *o = container();
    if( o && o->absolutePosition(xPos, yPos, f))
    {
        if(!isInline() || isReplaced())
            xPos += m_x, yPos += m_y;

        if(isRelPositioned())
            relativePositionOffset(xPos, yPos);
        return true;
    }
    else
    {
        xPos = yPos = 0;
        return false;
    }
}

void RenderBox::position(int x, int y, int, int, int, bool, bool, int)
{
    m_x = x + marginLeft();
    m_y = y;
    // ### paddings
    //m_width = width;
}

void RenderBox::repaint()
{
    //kdDebug( 6040 ) << "repaint!" << endl;
    int ow = style() ? style()->outlineWidth() : 0;
    repaintRectangle(-ow, -ow, m_width+ow*2, m_height+ow*2);
}

void RenderBox::repaintRectangle(int x, int y, int w, int h, bool f)
{
    x += m_x;
    y += m_y;

    if (style()->position()==FIXED) f=true;

    // kdDebug( 6040 ) << "RenderBox(" << renderName() << ")::repaintRectangle (" << x << "/" << y << ") (" << w << "/" << h << ")" << endl;
    RenderObject *o = container();
    if( o ) o->repaintRectangle(x, y, w, h, f);
}

void RenderBox::relativePositionOffset(int &tx, int &ty)
{
    if(!style()->left().isVariable())
        tx += style()->left().width(containingBlockWidth());
    else if(!style()->right().isVariable())
        tx -= style()->right().width(containingBlockWidth());
    if(!style()->top().isVariable())
    {
        if (!style()->top().isPercent()
                || containingBlock()->style()->height().isFixed())
            ty += style()->top().width(containingBlockHeight());
    }
    else if(!style()->bottom().isVariable())
    {
        if (!style()->bottom().isPercent()
                || containingBlock()->style()->height().isFixed())
            ty -= style()->bottom().width(containingBlockHeight());
    }
}

void RenderBox::calcWidth()
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << "RenderBox("<<renderName()<<")::calcWidth()" << endl;
#endif
    if (isPositioned())
    {
        calcAbsoluteHorizontal();
    }
    else
    {
        Length w;
        if ( isReplaced () )
            w = Length( calcReplacedWidth(), Fixed );
        else
            w = style()->width();

        Length ml = style()->marginLeft();
        Length mr = style()->marginRight();

        int cw;
	RenderObject *cb = containingBlock();
	if ( style()->flowAroundFloats() && cb->isFlow() )
	    cw = static_cast<RenderFlow *>(cb)->lineWidth( m_y );
	else
	    cw = cb->contentWidth();

        if (cw<0) cw = 0;

        m_marginLeft = 0;
        m_marginRight = 0;

        if (isInline())
        {
            // just calculate margins
            m_marginLeft = ml.minWidth(cw);
            m_marginRight = mr.minWidth(cw);
            if (isReplaced())
            {
                m_width = w.width(cw);
                m_width += paddingLeft() + paddingRight() + style()->borderLeftWidth() + style()->borderRightWidth();

                if(m_width < m_minWidth) m_width = m_minWidth;
            }

            return;
        }
        else if (w.type == Variable)
        {
//          kdDebug( 6040 ) << "variable" << endl;
            m_marginLeft = ml.minWidth(cw);
            m_marginRight = mr.minWidth(cw);
            if (cw) m_width = cw - m_marginLeft - m_marginRight;

//          kdDebug( 6040 ) <<  m_width <<"," << cw <<"," <<
//              m_marginLeft <<"," <<  m_marginRight << endl;

            if (isFloating()) {
                if(m_width < m_minWidth) m_width = m_minWidth;
                if(m_width > m_maxWidth) m_width = m_maxWidth;
            }
        }
        else
        {
//          kdDebug( 6040 ) << "non-variable " << w.type << ","<< w.value << endl;
            m_width = w.width(cw);
            m_width += paddingLeft() + paddingRight() + style()->borderLeftWidth() + style()->borderRightWidth();

            calcHorizontalMargins(ml,mr,cw);
        }

        if (cw && cw != m_width + m_marginLeft + m_marginRight && !isFloating() && !isInline())
        {
            if (style()->direction()==LTR)
                m_marginRight = cw - m_width - m_marginLeft;
            else
                m_marginLeft = cw - m_width - m_marginRight;
        }
    }

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << "RenderBox::calcWidth(): m_width=" << m_width << " containingBlockWidth()=" << containingBlockWidth() << endl;
    kdDebug( 6040 ) << "m_marginLeft=" << m_marginLeft << " m_marginRight=" << m_marginRight << endl;
#endif
}

void RenderBox::calcHorizontalMargins(const Length& ml, const Length& mr, int cw)
{
    if (isFloating())
    {
        m_marginLeft = ml.minWidth(cw);
        m_marginRight = mr.minWidth(cw);
    }
    else
    {
        if ( (ml.type == Variable && mr.type == Variable) ||
             (!(ml.type == Variable) &&
                containingBlock()->style()->textAlign() == KONQ_CENTER) )
        {
            m_marginLeft = (cw - m_width)/2;
            if (m_marginLeft<0) m_marginLeft=0;
            m_marginRight = cw - m_width - m_marginLeft;
        }
        else if (mr.type == Variable)
        {
            m_marginLeft = ml.width(cw);
            m_marginRight = cw - m_width - m_marginLeft;
        }
        else if (ml.type == Variable)
        {
            m_marginRight = mr.width(cw);
            m_marginLeft = cw - m_width - m_marginRight;
        }
        else
        {
            m_marginLeft = ml.minWidth(cw);
            m_marginRight = mr.minWidth(cw);
        }
    }
}

void RenderBox::calcHeight()
{

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << "RenderBox::calcHeight()" << endl;
#endif

    //cell height is managed by table, inline elements do not have a height property.
    if ( isTableCell() || (isInline() && !isReplaced()) )
        return;

    if (isPositioned())
        calcAbsoluteVertical();
    else
    {
        Length h;
        if ( isReplaced() )
            h = Length( calcReplacedHeight(), Fixed );
        else
            h = style()->height();

        calcVerticalMargins();

        // for tables, calculate margins only
        if (isTable())
            return;

        if (!h.isVariable())
        {
            int fh=-1;
            if (h.isFixed())
                fh = h.value + borderTop() + paddingTop() + borderBottom() + paddingBottom();
            else if (h.isPercent()) {
                Length ch = containingBlock()->style()->height();
                if (ch.isFixed())
                    fh = h.width(ch.value) + borderTop() + paddingTop() + borderBottom() + paddingBottom();
            }
            if (fh!=-1)
            {
                if (fh<m_height && !overhangingContents() && style()->overflow()==OVISIBLE)
                    setOverhangingContents();

                m_height = fh;
            }
        }
    }
}

short RenderBox::calcReplacedWidth(bool* ieHack) const
{
    Length w = style()->width();
    short width;
    if ( ieHack )
        *ieHack = style()->height().isPercent() || w.isPercent();

    switch( w.type ) {
    case Variable:
    {
        Length h = style()->height();
        int ih = intrinsicHeight();
        if ( ih > 0 && ( h.isPercent() || h.isFixed() ) )
            width = ( ( h.isPercent() ? calcReplacedHeight() : h.value )*intrinsicWidth() ) / ih;
        else
            width = intrinsicWidth();
        break;
    }
    case Percent:
    {
        //RenderObject* p = parent();
        int cw = containingBlockWidth();
        if ( cw )
            width = w.minWidth( cw );
        else
            width = intrinsicWidth();
        break;
    }
    case Fixed:
        width = w.value;
        break;
    default:
        width = intrinsicWidth();
        break;
    };

    return width;
}

int RenderBox::calcReplacedHeight() const
{
    Length h = style()->height();
    short height;
    switch( h.type ) {
    case Variable:
    {
        Length w = style()->width();
        int iw = intrinsicWidth();
        if( iw > 0 && ( w.isFixed() || w.isPercent() ))
            height = (( w.isPercent() ? calcReplacedWidth() : w.value ) * intrinsicHeight()) / iw;
        else
            height = intrinsicHeight();
    }
    break;
    case Percent:
    {
        RenderObject* p = parent();
        while (p && !p->isTableCell()) p = p->parent();
        bool doIEHack = !p;
        RenderObject* cb = containingBlock();
        if ( !cb->isTableCell() && doIEHack)
            height = h.minWidth( cb->root()->view()->visibleHeight() );
        else {
	    if (cb->isTableCell()) {
	        RenderTableCell* tableCell = static_cast<RenderTableCell*>(cb);
	        if (tableCell->style()->height().isPercent() && tableCell->getCellPercentageHeight()) {
                    height = h.minWidth(tableCell->getCellPercentageHeight());
                    break;
                }
            }
            
            if (!doIEHack)
                cb = cb->containingBlock();
            
            if ( cb->style()->height().isFixed() )
                height = h.minWidth( cb->style()->height().value );
            else
                height = intrinsicHeight();
        }
    }
    break;
    case Fixed:
        height = h.value;
        break;
    default:
        height = intrinsicHeight();
    };

    return height;
}

void RenderBox::calcVerticalMargins()
{
    if( isTableCell() ) {
	// table margins are basically infinite
	m_marginTop = TABLECELLMARGIN;
	m_marginBottom = TABLECELLMARGIN;
	return;
    }

    Length tm = style()->marginTop();
    Length bm = style()->marginBottom();

    // margins are calculated with respect to the _width_ of
    // the containing block (8.3)
    int cw = containingBlock()->contentWidth();

    m_marginTop = tm.minWidth(cw);
    m_marginBottom = bm.minWidth(cw);
}

void RenderBox::calcAbsoluteHorizontal()
{
    const int AUTO = -666666;
    int l,r,w,ml,mr,cw;

    int pab = borderLeft()+ borderRight()+ paddingLeft()+ paddingRight();

    l=r=ml=mr=w=AUTO;
    cw = containingBlockWidth()
        +containingBlock()->paddingLeft() +containingBlock()->paddingRight();

    if(!style()->left().isVariable())
        l = style()->left().width(cw);
    if(!style()->right().isVariable())
        r = style()->right().width(cw);
    if(!style()->width().isVariable())
        w = style()->width().width(cw);
    else if (isReplaced())
        w = intrinsicWidth();
    if(!style()->marginLeft().isVariable())
        ml = style()->marginLeft().width(cw);
    if(!style()->marginRight().isVariable())
        mr = style()->marginRight().width(cw);


//    printf("h1: w=%d, l=%d, r=%d, ml=%d, mr=%d\n",w,l,r,ml,mr);

    int static_distance=0;
    if ((style()->direction()==LTR && (l==AUTO && r==AUTO ))
            || style()->left().isStatic())
    {
        // calc hypothetical location in the normal flow
        // used for 1) left=static-position
        //          2) left, right, width are all auto -> calc top -> 3.
        //          3) precalc for case 2 below

        // all positioned elements are blocks, so that
        // would be at the left edge
        RenderObject* po = parent();
        while (po && po!=containingBlock()) {
            static_distance+=po->xPos();
            po=po->parent();
        }

        if (l==AUTO || style()->left().isStatic())
            l = static_distance;
    }

    else if ((style()->direction()==RTL && (l==AUTO && r==AUTO ))
            || style()->right().isStatic())
    {
            RenderObject* po = parent();

            static_distance = cw - po->width();

            while (po && po!=containingBlock()) {
                static_distance-=po->xPos();
                po=po->parent();
            }

        if (r==AUTO || style()->right().isStatic())
            r = static_distance;
    }


    if (l!=AUTO && w!=AUTO && r!=AUTO)
    {
        // left, width, right all given, play with margins
        int ot = l + w + r + pab;

        if (ml==AUTO && mr==AUTO)
        {
            // both margins auto, solve for equality
            ml = (cw - ot)/2;
            mr = cw - ot - ml;
        }
        else if (ml==AUTO)
            // solve for left margin
            ml = cw - ot - mr;
        else if (mr==AUTO)
            // solve for right margin
            mr = cw - ot - ml;
        else
        {
            // overconstrained, solve according to dir
            if (style()->direction()==LTR)
                r = cw - ( l + w + ml + mr + pab);
            else
                l = cw - ( r + w + ml + mr + pab);
        }
    }
    else
    {
        // one or two of (left, width, right) missing, solve

        // auto margins are ignored
        if (ml==AUTO) ml = 0;
        if (mr==AUTO) mr = 0;

        //1. solve left & width.
        if (l==AUTO && w==AUTO && r!=AUTO)
        {
            w = QMIN(m_maxWidth, cw - ( r + ml + mr + pab));
            l = cw - ( r + w + ml + mr + pab);
        }
        else

        //2. solve left & right. use static positioning.
        if (l==AUTO && w!=AUTO && r==AUTO)
        {
            if (style()->direction()==RTL)
            {
                r = static_distance;
                l = cw - ( r + w + ml + mr + pab);
            }
            else
            {
                l = static_distance;
                r = cw - ( l + w + ml + mr + pab);
            }

        }
        else

        //3. solve width & right.
        if (l!=AUTO && w==AUTO && r==AUTO)
        {
            w = QMIN(m_maxWidth, cw - ( l + ml + mr + pab));
            r = cw - ( l + w + ml + mr + pab);
        }
        else

        //4. solve left
        if (l==AUTO && w!=AUTO && r!=AUTO)
            l = cw - ( r + w + ml + mr + pab);
        else

        //5. solve width
        if (l!=AUTO && w==AUTO && r!=AUTO)
            w = cw - ( r + l + ml + mr + pab);
        else

        //6. solve right
        if (l!=AUTO && w!=AUTO && r==AUTO)
            r = cw - ( l + w + ml + mr + pab);
    }

    m_width = w + pab;
    m_marginLeft = ml;
    m_marginRight = mr;
    m_x = l + ml + containingBlock()->borderLeft();

//    printf("h: w=%d, l=%d, r=%d, ml=%d, mr=%d\n",w,l,r,ml,mr);
}


void RenderBox::calcAbsoluteVertical()
{
    // css2 spec 10.6.4 & 10.6.5

    // based on
    // http://www.w3.org/Style/css2-updates/REC-CSS2-19980512-errata
    // (actually updated 2000-10-24)
    // that introduces static-position value for top, left & right

    const int AUTO = -666666;
    int t,b,h,mt,mb,ch;

    t=b=h=mt=mb=AUTO;

    int pab = borderTop()+borderBottom()+paddingTop()+paddingBottom();

    Length hl = containingBlock()->style()->height();
    if (hl.isFixed())
        ch = hl.value + containingBlock()->paddingTop()
             + containingBlock()->paddingBottom();
    else
        ch = containingBlock()->height();

    if(!style()->top().isVariable())
        t = style()->top().width(ch);
    if(!style()->bottom().isVariable())
        b = style()->bottom().width(ch);
    if(!style()->height().isVariable())
    {
        h = style()->height().width(ch);
        if (m_height-pab>h)
            h=m_height-pab;
    }
    else if (isReplaced())
        h = intrinsicHeight();

    if(!style()->marginTop().isVariable())
        mt = style()->marginTop().width(ch);
    if(!style()->marginBottom().isVariable())
        mb = style()->marginBottom().width(ch);

    int static_top=0;
    if ((t==AUTO && b==AUTO ) || style()->top().isStatic())
    {
        // calc hypothetical location in the normal flow
        // used for 1) top=static-position
        //          2) top, bottom, height are all auto -> calc top -> 3.
        //          3) precalc for case 2 below

        RenderObject* ro = previousSibling();
        while ( ro && ro->isPositioned())
            ro = ro->previousSibling();

        if (ro) static_top = ro->yPos()+ro->marginBottom()+ro->height();

        RenderObject* po = parent();
        while (po && po!=containingBlock()) {
            static_top+=po->yPos();
            po=po->parent();
        }

        if (h==AUTO || style()->top().isStatic())
            t = static_top;
    }



    if (t!=AUTO && h!=AUTO && b!=AUTO)
    {
        // top, height, bottom all given, play with margins
        int ot = h + t + b + pab;

        if (mt==AUTO && mb==AUTO)
        {
            // both margins auto, solve for equality
            mt = (ch - ot)/2;
            mb = ch - ot - mt;
        }
        else if (mt==AUTO)
            // solve for top margin
            mt = ch - ot - mb;
        else if (mb==AUTO)
            // solve for bottom margin
            mb = ch - ot - mt;
        else
            // overconstrained, solve for bottom
            b = ch - ( h+t+mt+mb+pab);
    }
    else
    {
        // one or two of (top, height, bottom) missing, solve

        // auto margins are ignored
        if (mt==AUTO) mt = 0;
        if (mb==AUTO) mb = 0;

        //1. solve top & height. use content height.
        if (t==AUTO && h==AUTO && b!=AUTO)
        {
            h = m_height-pab;
            t = ch - ( h+b+mt+mb+pab);
        }
        else

        //2. solve top & bottom. use static positioning.
        if (t==AUTO && h!=AUTO && b==AUTO)
        {
            t = static_top;
            b = ch - ( h+t+mt+mb+pab);
        }
        else

        //3. solve height & bottom. use content height.
        if (t!=AUTO && h==AUTO && b==AUTO)
        {
            h = m_height-pab;
            b = ch - ( h+t+mt+mb+pab);
        }
        else

        //4. solve top
        if (t==AUTO && h!=AUTO && b!=AUTO)
            t = ch - ( h+b+mt+mb+pab);
        else

        //5. solve height
        if (t!=AUTO && h==AUTO && b!=AUTO)
            h = ch - ( t+b+mt+mb+pab);
        else

        //6. solve bottom
        if (t!=AUTO && h!=AUTO && b==AUTO)
            b = ch - ( h+t+mt+mb+pab);
    }


    if (m_height<h+pab) //content must still fit
        m_height = h+pab;

    m_marginTop = mt;
    m_marginBottom = mb;
    m_y = t + mt + containingBlock()->borderTop();

//    printf("v: h=%d, t=%d, b=%d, mt=%d, mb=%d, m_y=%d\n",h,t,b,mt,mb,m_y);

}


int RenderBox::lowestPosition() const
{
    return m_height + marginBottom();
}

int RenderBox::rightmostPosition() const
{
    return m_width;
}

#undef DEBUG_LAYOUT
