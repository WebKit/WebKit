/**
 * This file is part of the DOM implementation for KDE.
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
// -------------------------------------------------------------------------
//#define DEBUG_LAYOUT


#include <qpainter.h>

#include "rendering/render_box.h"
#include "rendering/render_replaced.h"
#include "rendering/render_canvas.h"
#include "rendering/render_table.h"
#include "render_arena.h"

#include "misc/htmlhashes.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"

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
    m_staticX = 0;
    m_staticY = 0;
    m_layer = 0;
}

void RenderBox::setStyle(RenderStyle *_style)
{
    RenderObject::setStyle(_style);

    // The root always paints its background/border.
    if (isRoot())
        setShouldPaintBackgroundOrBorder(true);

    setInline(_style->isDisplayInlineType());
    
    switch(_style->position())
    {
    case ABSOLUTE:
    case FIXED:
        setPositioned(true);
        break;
    default:
        setPositioned(false);

        if (_style->isFloating())
            setFloating(true);

        if (_style->position() == RELATIVE)
            setRelPositioned(true);
    }

    if (requiresLayer()) {
        if (!m_layer) {
            m_layer = new (renderArena()) RenderLayer(this);
            m_layer->insertOnlyThisLayer();
        }
    }
    else if (m_layer && !isRoot() && !isCanvas()) {
        m_layer->removeOnlyThisLayer();
        m_layer = 0;
    }

    if (m_layer)
        m_layer->styleChanged();
    
    // Set the text color if we're the body.
    if (isBody())
        element()->getDocument()->setTextColor(_style->color());
    
    if (style()->outlineWidth() > 0 && style()->outlineSize() > maximalOutlineSize(PaintActionOutline))
        static_cast<RenderCanvas*>(document()->renderer())->setMaximalOutlineSize(style()->outlineSize());
}

RenderBox::~RenderBox()
{
    //kdDebug( 6040 ) << "Element destructor: this=" << nodeName().string() << endl;
}

void RenderBox::detach()
{
    RenderLayer* layer = m_layer;

    RenderArena* arena = renderArena();
    
    RenderContainer::detach();
    
    if (layer)
        layer->detach(arena);
}

short RenderBox::contentWidth() const
{
    short w = m_width - borderLeft() - borderRight();
    w -= paddingLeft() + paddingRight();

    if (style()->scrollsOverflow() && m_layer)
        w -= m_layer->verticalScrollbarWidth();
    
    //kdDebug( 6040 ) << "RenderBox::contentWidth(2) = " << w << endl;
    return w;
}

int RenderBox::contentHeight() const
{
    int h = m_height - borderTop() - borderBottom();
    h -= paddingTop() + paddingBottom();

    if (style()->scrollsOverflow() && m_layer)
        h -= m_layer->horizontalScrollbarHeight();

    return h;
}

void RenderBox::setPos( int xPos, int yPos )
{
    if (xPos == m_x && yPos == m_y)
        return; // Optimize for the case where we don't move at all.
    
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


// --------------------- painting stuff -------------------------------

void RenderBox::paint(QPainter *p, int _x, int _y, int _w, int _h,
                      int _tx, int _ty, PaintAction paintAction)
{
    _tx += m_x;
    _ty += m_y;

    // default implementation. Just pass things through to the children
    RenderObject *child = firstChild();
    while(child != 0)
    {
        child->paint(p, _x, _y, _w, _h, _tx, _ty, paintAction);
        child = child->nextSibling();
    }
}

void RenderBox::paintRootBoxDecorations(QPainter *p,int, int _y,
                                        int, int _h, int _tx, int _ty)
{
    //kdDebug( 6040 ) << renderName() << "::paintBoxDecorations()" << _tx << "/" << _ty << endl;
    QColor c = style()->backgroundColor();
    CachedImage *bg = style()->backgroundImage();

    bool canBeTransparent = true;
    if (!c.isValid() && !bg) {
        // Locate the <body> element using the DOM.  This is easier than trying
        // to crawl around a render tree with potential :before/:after content and
        // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
        // render object very easily via the DOM.
        RenderObject* bodyObject = 0;
        for (DOM::NodeImpl* elt = element()->firstChild(); elt; elt = elt->nextSibling()) {
            if (elt->id() == ID_BODY) {
                bodyObject = elt->renderer();
                break;
            }
            else if (elt->id() == ID_FRAMESET) {
                canBeTransparent = false; // Can't scroll a frameset document anyway.
                break;
            }
        }

        if (bodyObject) {
            c = bodyObject->style()->backgroundColor();
            bg = bodyObject->style()->backgroundImage();
        }
    }

    // Only fill with a base color (e.g., white) if we're the root document, since iframes/frames with
    // no background in the child document should show the parent's background.
    if ((!c.isValid() || qAlpha(c.rgb()) == 0) && canvas()->view()) {
        DOM::NodeImpl* elt = element()->getDocument()->ownerElement();
        if (canBeTransparent && elt && elt->id() != ID_FRAME) // Frames are never transparent.
            canvas()->view()->useSlowRepaints(); // The parent must show behind the child.
        else
            c = canvas()->view()->palette().active().color(QColorGroup::Base);
    }
    
    int w = width();
    int h = height();

    //    kdDebug(0) << "width = " << w <<endl;

    int rw, rh;
    if (canvas()->view()) {
        rw = canvas()->view()->contentsWidth();
        rh = canvas()->view()->contentsHeight();
    }
    else {
        rw = canvas()->width();
        rh = canvas()->height();
    }
    
    //    kdDebug(0) << "rw = " << rw <<endl;

    int bx = _tx - marginLeft();
    int by = _ty - marginTop();
    int bw = QMAX(w + marginLeft() + marginRight() + borderLeft() + borderRight(), rw);
    int bh = QMAX(h + marginTop() + marginBottom() + borderTop() + borderBottom(), rh);

    // CSS2 14.2:
    // " The background of the box generated by the root element covers the entire canvas."
    // hence, paint the background even in the margin areas (unlike for every other element!)
    // I just love these little inconsistencies .. :-( (Dirk)
    int my = QMAX(by,_y);

    paintBackground(p, c, bg, my, _h, bx, by, bw, bh);

    if (style()->hasBorder() && style()->display() != INLINE)
        paintBorder( p, _tx, _ty, w, h, style() );
}

void RenderBox::paintBoxDecorations(QPainter *p,int _x, int _y,
                                    int _w, int _h, int _tx, int _ty)
{
    //kdDebug( 6040 ) << renderName() << "::paintDecorations()" << endl;
    if (isRoot())
        return paintRootBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);
    
    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
    _ty -= borderTopExtra();

    int my = QMAX(_ty,_y);
    int mh;
    if (_ty<_y)
        mh= QMAX(0,h-(_y-_ty));
    else
        mh = QMIN(_h,h);

    // The <body> only paints its background if the root element has defined a background
    // independent of the body.  Go through the DOM to get to the root element's render object,
    // since the root could be inline and wrapped in an anonymous block.
    if (!isBody()
        || element()->getDocument()->documentElement()->renderer()->style()->backgroundColor().isValid()
        || element()->getDocument()->documentElement()->renderer()->style()->backgroundImage())
        paintBackground(p, style()->backgroundColor(), style()->backgroundImage(), my, mh, _tx, _ty, w, h);
   
    if (style()->hasBorder())
        paintBorder(p, _tx, _ty, w, h, style());
}

void RenderBox::paintBackground(QPainter *p, const QColor &c, CachedImage *bg, int clipy, int cliph, int _tx, int _ty, int w, int height)
{
    paintBackgroundExtended(p, c, bg, clipy, cliph, _tx, _ty, w, height,
                            borderLeft(), borderRight());
}

void RenderBox::paintBackgroundExtended(QPainter *p, const QColor &c, CachedImage *bg, int clipy, int cliph,
                                        int _tx, int _ty, int w, int h,
                                        int bleft, int bright)
{
    if (c.isValid() && qAlpha(c.rgb()) > 0) {
        // If we have an alpha and we are painting the root element, go ahead and blend with our default
        // background color (typically white).
        if (qAlpha(c.rgb()) < 0xFF && isRoot())
            p->fillRect(_tx, clipy, w, cliph, canvas()->view()->palette().active().color(QColorGroup::Base));
        p->fillRect(_tx, clipy, w, cliph, c);
    }
    
    // no progressive loading of the background image
    if(bg && bg->pixmap_size() == bg->valid_rect().size() && !bg->isTransparent() && !bg->isErrorImage()) {
        //kdDebug( 6040 ) << "painting bgimage at " << _tx << "/" << _ty << endl;
        // ### might need to add some correct offsets
        // ### use paddingX/Y

        // for propagation of <body> up to <html>
        RenderStyle* sptr = style();
        if ((isRoot() && element() && element()->id() == ID_HTML) && firstChild() && !style()->backgroundImage())
            sptr = firstChild()->style();

        int sx = 0;
        int sy = 0;
	    int cw,ch;
        int cx,cy;
        int vpab = bleft + bright;
        int hpab = borderTop() + borderBottom();
        
        // CSS2 chapter 14.2.1

        if (sptr->backgroundAttachment())
        {
            //scroll
            int pw = w - vpab;
            int ph = h - hpab;
            
            int pixw = bg->pixmap_size().width();
            int pixh = bg->pixmap_size().height();
            EBackgroundRepeat bgr = sptr->backgroundRepeat();
            if( (bgr == NO_REPEAT || bgr == REPEAT_Y) && w > pixw ) {
                cw = pixw;
                int xPosition = sptr->backgroundXPosition().minWidth(pw-pixw);
                if (xPosition >= 0)
                    cx = _tx + xPosition;
                else {
                    cx = _tx;
                    if (pixw == 0)
                        sx = 0;
                    else {
                        sx = -xPosition;
                        cw += xPosition;
                    }
                }
                cx += bleft;
            } else {
                cw = w;
                cx = _tx;
                if (pixw == 0)
                    sx = 0;
                else
                    sx =  pixw - ((sptr->backgroundXPosition().minWidth(pw-pixw)) % pixw );
                sx -= bleft % pixw;
            }

            if( (bgr == NO_REPEAT || bgr == REPEAT_X) && h > pixh ) {
                ch = pixh;
                int yPosition = sptr->backgroundYPosition().minWidth(ph-pixh);
                if (yPosition >= 0)
                    cy = _ty + yPosition;
                else {
                    cy = _ty;
                    if (pixh == 0)
                        sy = 0;
                    else {
                        sy = -yPosition;
                        ch += yPosition;
                    }
                }
                
                cy += borderTop();
            } else {
                ch = h;
                cy = _ty;
                if(pixh == 0){
                    sy = 0;
                }else{
                    sy = pixh - ((sptr->backgroundYPosition().minWidth(ph-pixh)) % pixh );
                }
                sy -= borderTop() % pixh;
            }
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
            if( (bgr == NO_REPEAT || bgr == REPEAT_Y) && pw > pixw ) {
                cw = pixw;
                cx = vr.x() + sptr->backgroundXPosition().minWidth(pw-pixw);
            } else {
                cw = pw;
                cx = vr.x();
                if(pixw == 0){
                    sx = 0;
                }else{
                    sx =  pixw - ((sptr->backgroundXPosition().minWidth(pw-pixw)) % pixw );
                }
            }

            if( (bgr == NO_REPEAT || bgr == REPEAT_X) && ph > pixh ) {
                ch = pixh;
                cy = vr.y() + sptr->backgroundYPosition().minWidth(ph-pixh);
            } else {
                ch = ph;
                cy = vr.y();
                if(pixh == 0){
                    sy = 0;
                }else{
                    sy = pixh - ((sptr->backgroundYPosition().minWidth(ph-pixh)) % pixh );
                }
            }

            QRect fix(cx,cy,cw,ch);
            QRect ele(_tx,_ty,w,h);
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

QRect RenderBox::getOverflowClipRect(int tx, int ty)
{
    // XXX When overflow-clip (CSS3) is implemented, we'll obtain the property
    // here.
    int bl=borderLeft(),bt=borderTop(),bb=borderBottom(),br=borderRight();
    int clipx = tx+bl;
    int clipy = ty+bt;
    int clipw = m_width-bl-br;
    int cliph = m_height-bt-bb;

    // Subtract out scrollbars if we have them.
    if (m_layer) {
        clipw -= m_layer->verticalScrollbarWidth();
        cliph -= m_layer->horizontalScrollbarHeight();
    }
    return QRect(clipx,clipy,clipw,cliph);
}

QRect RenderBox::getClipRect(int tx, int ty)
{
    int clipx = tx;
    int clipy = ty;
    int clipw = m_width;
    int cliph = m_height;

    if (!style()->clipLeft().isVariable())
    {
        int c=style()->clipLeft().width(m_width);
        clipx+=c;
        clipw-=c;
    }
        
    if (!style()->clipRight().isVariable())
    {
        int w = style()->clipRight().width(m_width);
        clipw -= m_width - w;
    }
    
    if (!style()->clipTop().isVariable())
    {
        int c=style()->clipTop().width(m_height);
        clipy+=c;
        cliph-=c;
    }
    if (!style()->clipBottom().isVariable())
    {
        int h = style()->clipBottom().width(m_height);
        cliph -= m_height - h;
    }
    //kdDebug( 6040 ) << "setting clip("<<clipx<<","<<clipy<<","<<clipw<<","<<cliph<<")"<<endl;

    QRect cr(clipx,clipy,clipw,cliph);
    return cr;
}

short RenderBox::containingBlockWidth() const
{
    RenderBlock* cb = containingBlock();
    if (!cb)
        return 0;
    if (usesLineWidth())
        return cb->lineWidth(m_y);
    else
        return cb->contentWidth();
}

bool RenderBox::absolutePosition(int &xPos, int &yPos, bool f)
{
    if ( style()->position() == FIXED )
	f = true;
    RenderObject *o = container();
    if( o && o->absolutePosition(xPos, yPos, f))
    {
        if (o->style()->hidesOverflow() && o->layer())
            o->layer()->subtractScrollOffset(xPos, yPos); 
            
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

void RenderBox::position(InlineBox* box, int from, int len, bool reverse)
{
    if (isPositioned()) {
        // Cache the x position only if we were an INLINE type originally.
        bool wasInline = style()->originalDisplay() == INLINE ||
                         style()->originalDisplay() == INLINE_TABLE;
        if (wasInline && hasStaticX()) {
            // The value is cached in the xPos of the box.  We only need this value if
            // our object was inline originally, since otherwise it would have ended up underneath
            // the inlines.
            m_staticX = box->xPos();
        }
        else if (!wasInline && hasStaticY())
            // Our object was a block originally, so we make our normal flow position be
            // just below the line box (as though all the inlines that came before us got
            // wrapped in an anonymous block, which is what would have happened had we been
            // in flow).  This value was cached in the yPos() of the box.
            m_staticY = box->yPos();

        // Nuke the box.
        box->detach(renderArena());
    }
    else if (isReplaced()) {
        m_x = box->xPos();
        m_y = box->yPos();

        // Nuke the box.  We don't need it for replaced elements.
        box->detach(renderArena());
    }
}

QRect RenderBox::getAbsoluteRepaintRect()
{
    int ow = style() ? style()->outlineSize() : 0;
    QRect r(-ow, -ow, overflowWidth(false)+ow*2, overflowHeight(false)+ow*2);
    computeAbsoluteRepaintRect(r);
    return r;
}

void RenderBox::computeAbsoluteRepaintRect(QRect& r, bool f)
{
    int x = r.x() + m_x;
    int y = r.y() + m_y;
     
    // Apply the relative position offset when invalidating a rectangle.  The layer
    // is translated, but the render box isn't, so we need to do this to get the
    // right dirty rect.  Since this is called from RenderObject::setStyle, the relative position
    // flag on the RenderObject has been cleared, so use the one on the style().
#ifdef INCREMENTAL_REPAINTING
    if (style()->position() == RELATIVE && m_layer)
        m_layer->relativePositionOffset(x,y);
#else
    if (style()->position() == RELATIVE)
        relativePositionOffset(x,y);
#endif
    
    if (style()->position()==FIXED)
        f = true;

    RenderObject* o = container();
    if (o) {
        // <body> may not have a layer, since it might be applying its overflow value to the
        // scrollbars.
        if (o->style()->hidesOverflow() && o->layer()) {
            // o->height() is inaccurate if we're in the middle of a layout of |o|, so use the
            // layer's size instead.  Even if the layer's size is wrong, the layer itself will repaint
            // anyway if its size does change.
            QRect boxRect(0, 0, o->layer()->width(), o->layer()->height());
            o->layer()->subtractScrollOffset(x,y); // For overflow:auto/scroll/hidden.
            QRect repaintRect(x, y, r.width(), r.height());
            r = repaintRect.intersect(boxRect);
            if (r.isEmpty())
                return;
        }
        else {
            r.setX(x);
            r.setY(y);
        }
        o->computeAbsoluteRepaintRect(r, f);
    }
}

#ifdef INCREMENTAL_REPAINTING
void RenderBox::repaintDuringLayoutIfMoved(int oldX, int oldY)
{
    int newX = m_x;
    int newY = m_y;
    if (oldX != newX || oldY != newY) {
        // The child moved.  Invalidate the object's old and new positions.  We have to do this
        // since the object may not have gotten a layout.
        m_x = oldX; m_y = oldY;
        repaint();
        repaintFloatingDescendants();
        m_x = newX; m_y = newY;
        repaint();
        repaintFloatingDescendants();
    }
}
#endif

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
        // The parent box is flexing us, so it has increased or decreased our width.  We just bail
        // and leave our width unchanged in this case.
        if (parent()->isFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL
            && parent()->isFlexingChildren())
            return;

        bool inVerticalBox = parent()->isFlexibleBox() && parent()->style()->boxOrient() == VERTICAL;
        bool stretching = parent()->style()->boxAlign() == BSTRETCH;
        bool treatAsReplaced = isReplaced() && !isInlineBlockOrInlineTable() &&
            (!inVerticalBox || !stretching);
        Length w;
        if (treatAsReplaced)
            w = Length( calcReplacedWidth(), Fixed );
        else
            w = style()->width();

        Length ml = style()->marginLeft();
        Length mr = style()->marginRight();

        RenderBlock *cb = containingBlock();
        int cw = containingBlockWidth();

        if (cw<0) cw = 0;

        m_marginLeft = 0;
        m_marginRight = 0;

        if (isInline() && !isInlineBlockOrInlineTable())
        {
            // just calculate margins
            m_marginLeft = ml.minWidth(cw);
            m_marginRight = mr.minWidth(cw);
            if (treatAsReplaced)
            {
                m_width = w.width(cw);
                m_width += paddingLeft() + paddingRight() + borderLeft() + borderRight();
                if(m_width < m_minWidth) m_width = m_minWidth;
            }

            return;
        }
        else {
            LengthType widthType, minWidthType, maxWidthType;
            if (treatAsReplaced) {
                m_width = w.width(cw);
                m_width += paddingLeft() + paddingRight() + borderLeft() + borderRight();
                widthType = w.type;
            } else {
                m_width = calcWidthUsing(Width, cw, widthType);
                int minW = calcWidthUsing(MinWidth, cw, minWidthType);
                int maxW = style()->maxWidth().value == UNDEFINED ?
                             m_width : calcWidthUsing(MaxWidth, cw, maxWidthType);
                
                if (m_width > maxW) {
                    m_width = maxW;
                    widthType = maxWidthType;
                }
                if (m_width < minW) {
                    m_width = minW;
                    widthType = minWidthType;
                }
            }
            
            if (widthType == Variable) {
    //          kdDebug( 6040 ) << "variable" << endl;
                m_marginLeft = ml.minWidth(cw);
                m_marginRight = mr.minWidth(cw);
            }
            else
            {
//          	kdDebug( 6040 ) << "non-variable " << w.type << ","<< w.value << endl;
                calcHorizontalMargins(ml,mr,cw);
            }
        }

        if (cw && cw != m_width + m_marginLeft + m_marginRight && !isFloating() && !isInline() &&
            !cb->isFlexibleBox())
        {
            if (cb->style()->direction()==LTR)
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

int RenderBox::calcWidthUsing(WidthType widthType, int cw, LengthType& lengthType)
{
    int width = m_width;
    Length w;
    if (widthType == Width)
        w = style()->width();
    else if (widthType == MinWidth)
        w = style()->minWidth();
    else
        w = style()->maxWidth();
        
    lengthType = w.type;
    
    if (lengthType == Variable) {
        int marginLeft = style()->marginLeft().minWidth(cw);
        int marginRight = style()->marginRight().minWidth(cw);
        if (cw) width = cw - marginLeft - marginRight;
        
        if (sizesToMaxWidth()) {
            if (width < m_minWidth) 
                width = m_minWidth;
            if (width > m_maxWidth) 
                width = m_maxWidth;
        }
    }
    else
    {
        width = w.width(cw);
        width += paddingLeft() + paddingRight() + borderLeft() + borderRight();
    }
    
    return width;
}

void RenderBox::calcHorizontalMargins(const Length& ml, const Length& mr, int cw)
{
    if (isFloating() || isInline()) // Inline blocks/tables and floats don't have their margins increased.
    {
        m_marginLeft = ml.minWidth(cw);
        m_marginRight = mr.minWidth(cw);
    }
    else
    {
        if ( (ml.type == Variable && mr.type == Variable) ||
             (ml.type != Variable && mr.type != Variable &&
                containingBlock()->style()->textAlign() == KHTML_CENTER) )
        {
            m_marginLeft = (cw - m_width)/2;
            if (m_marginLeft<0) m_marginLeft=0;
            m_marginRight = cw - m_width - m_marginLeft;
        }
        else if (mr.type == Variable ||
                 (ml.type != Variable && containingBlock()->style()->direction() == RTL &&
                  containingBlock()->style()->textAlign() == KHTML_LEFT))
        {
            m_marginLeft = ml.width(cw);
            m_marginRight = cw - m_width - m_marginLeft;
        }
        else if (ml.type == Variable ||
                 (mr.type != Variable && containingBlock()->style()->direction() == LTR &&
                  containingBlock()->style()->textAlign() == KHTML_RIGHT))
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
        bool inHorizontalBox = parent()->isFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL;
        bool stretching = parent()->style()->boxAlign() == BSTRETCH;
        
        // The parent box is flexing us, so it has increased or decreased our height.  We have to
        // grab our cached flexible height.
        if (parent()->isFlexibleBox() && parent()->style()->boxOrient() == VERTICAL
            && parent()->isFlexingChildren() && style()->boxFlexedHeight() != -1)
            h = Length(style()->boxFlexedHeight() - borderTop() - borderBottom() -
                       paddingTop() - paddingBottom(), Fixed);
        else if ( isReplaced() && !isInlineBlockOrInlineTable() &&
                  (!inHorizontalBox || !stretching )) {
            h = Length( calcReplacedHeight(), Fixed );
        }
        else
            h = style()->height();

        calcVerticalMargins();

        // for tables, calculate margins only
        if (isTable())
            return;

        // The parent box is flexing us, so it has increased or decreased our height.  We have to
        // grab our cached flexible height.
        if (parent()->isFlexibleBox() && parent()->style()->boxOrient() == VERTICAL
            && parent()->isFlexingChildren() && style()->boxFlexedHeight() != -1)
            h = Length(style()->boxFlexedHeight() - borderTop() - borderBottom() -
                       paddingTop() - paddingBottom(), Fixed);
        
        // Block children of horizontal flexible boxes fill the height of the box.
        if (h.isVariable() && parent()->isFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL
            && parent()->isStretchingChildren())
            h = Length(parent()->contentHeight() - marginTop() - marginBottom() -
                       borderTop() - paddingTop() - borderBottom() - paddingBottom(), Fixed);

        if (!h.isVariable())
        {
            int fh=-1;
            if (h.isFixed())
                fh = h.value + borderTop() + paddingTop() + borderBottom() + paddingBottom();
            else if (h.isPercent()) {
                // Handle a common case: nested 100% height <div>s.
                // This is kind of a one-off hack rather than doing it right.
                // Makes dbaron's z-index root bg testcases work. Bad dave. - dwh
                RenderBlock* cb = containingBlock();
                Length ch = containingBlock()->style()->height();
                while (cb && !cb->isTableCell() && ch.isPercent() && ch.value == 100) {
                    cb = cb->containingBlock();
                    ch = cb->style()->height();
                }

                if (cb->isCanvas()) {
                    // Don't allow this to affect the canvas' m_height member variable, since this
                    // can get called while the canvas is still laying out its kids.
                    // e.g., <html style="height:100%">etc. -dwh
                    int oldHeight = cb->height();
                    static_cast<RenderCanvas*>(cb)->calcHeight();
                    fh = h.width(cb->height()) + borderTop() + paddingTop() + borderBottom() + paddingBottom();
                    cb->setHeight(oldHeight);
                }
                else if (ch.isFixed())
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
    
    // Unfurling marquees override with the furled height.
    if (style()->overflow() == OMARQUEE && m_layer && m_layer->marquee() && 
        m_layer->marquee()->isUnfurlMarquee() && !m_layer->marquee()->isHorizontal()) {
        m_layer->marquee()->setEnd(m_height);
        m_height = kMin(m_height, m_layer->marquee()->unfurlPos());
    }
}

short RenderBox::calcReplacedWidth() const
{
    int width = calcReplacedWidthUsing(Width);
    int minW = calcReplacedWidthUsing(MinWidth);
    int maxW = style()->maxWidth().value == UNDEFINED ? width : calcReplacedWidthUsing(MaxWidth);

    if (width > maxW)
        width = maxW;
    
    if (width < minW)
        width = minW;

    return width;
}

int RenderBox::calcReplacedWidthUsing(WidthType widthType) const
{
    Length w;
    if (widthType == Width)
        w = style()->width();
    else if (widthType == MinWidth)
        w = style()->minWidth();
    else
        w = style()->maxWidth();
    
    switch (w.type) {
    case Fixed:
        return w.value;
    case Percent:
    {
        const int cw = containingBlockWidth();
        if (cw > 0) {
            int result = w.minWidth(cw);
            return result;
        }
    }
    // fall through
    default:
        return intrinsicWidth();
    }
}

int RenderBox::calcReplacedHeight() const
{
    int height = calcReplacedHeightUsing(Height);
    int minH = calcReplacedHeightUsing(MinHeight);
    int maxH = style()->maxHeight().value == UNDEFINED ? height : calcReplacedHeightUsing(MaxHeight);

    if (height > maxH)
        height = maxH;

    if (height < minH)
        height = minH;

    return height;
}

int RenderBox::calcReplacedHeightUsing(HeightType heightType) const
{
    Length h;
    if (heightType == Height)
        h = style()->height();
    else if (heightType == MinHeight)
        h = style()->minHeight();
    else
        h = style()->maxHeight();
    switch( h.type ) {
    case Percent:
        return availableHeightUsing(h);
    case Fixed:
        return h.value;
    default:
        return intrinsicHeight();
    };
}

int RenderBox::availableHeight() const
{
    return availableHeightUsing(style()->height());
}

int RenderBox::availableHeightUsing(const Length& h) const
{
    if (h.isFixed())
        return h.value;

    if (isCanvas())
        return static_cast<const RenderCanvas*>(this)->viewportHeight();

    // We need to stop here, since we don't want to increase the height of the table
    // artificially.  We're going to rely on this cell getting expanded to some new
    // height, and then when we lay out again we'll use the calculation below.
    if (isTableCell() && (h.isVariable() || h.isPercent())) {
        const RenderTableCell* tableCell = static_cast<const RenderTableCell*>(this);
        return tableCell->getCellPercentageHeight() - (borderLeft()+borderRight()+paddingLeft()+paddingRight());
    }
    
    if (h.isPercent())
       return h.width(containingBlock()->availableHeight());
       
    return containingBlock()->availableHeight();
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

void RenderBox::setStaticX(short staticX)
{
    m_staticX = staticX;
}

void RenderBox::setStaticY(int staticY)
{
    m_staticY = staticY;
}

void RenderBox::calcAbsoluteHorizontal()
{
    const int AUTO = -666666;
    int l,r,w,ml,mr,cw;

    int pab = borderLeft()+ borderRight()+ paddingLeft()+ paddingRight();

    l=r=ml=mr=w=AUTO;
 
    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    RenderObject* cb = container();
    cw = containingBlockWidth() + cb->paddingLeft() + cb->paddingRight();

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
    if ((parent()->style()->direction()==LTR && (l==AUTO && r==AUTO ))
            || style()->left().isStatic())
    {
        static_distance = m_staticX - cb->borderLeft(); // Should already have been set through layout of the parent().
        RenderObject* po = parent();
        for (; po && po != cb; po = po->parent())
            static_distance += po->xPos();

        if (l==AUTO || style()->left().isStatic())
            l = static_distance;
    }

    else if ((parent()->style()->direction()==RTL && (l==AUTO && r==AUTO ))
            || style()->right().isStatic())
    {
        RenderObject* po = parent();
        static_distance = m_staticX - cb->borderLeft(); // Should already have been set through layout of the parent().
        while (po && po!=containingBlock()) {
            static_distance+=po->xPos();
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
            // From section 10.3.7 of the CSS2.1 specification.
            // "The shrink-to-fit width is: min(max(preferred minimum width, available width), preferred width)."
            w = QMIN(QMAX(m_minWidth-pab, cw - ( r + ml + mr + pab)), m_maxWidth-pab);
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
            // From section 10.3.7 of the CSS2.1 specification.
            // "The shrink-to-fit width is: min(max(preferred minimum width, available width), preferred width)."
            w = QMIN(QMAX(m_minWidth-pab, cw - ( l + ml + mr + pab)), m_maxWidth-pab);
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
    m_x = l + ml + cb->borderLeft();
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

    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    RenderObject* cb = container();
    Length hl = cb->style()->height();
    if (hl.isFixed())
        ch = hl.value + cb->paddingTop() + cb->paddingBottom();
    else if (cb->isRoot())
        ch = cb->availableHeight();
    else
        ch = cb->height() - cb->borderTop() - cb->borderBottom();

    if(!style()->top().isVariable())
        t = style()->top().width(ch);
    if(!style()->bottom().isVariable())
        b = style()->bottom().width(ch);
    if (isTable() && style()->height().isVariable())
        // Height is never unsolved for tables. "auto" means shrink to fit.  Use our
        // height instead.
        h = m_height - pab;
    else if(!style()->height().isVariable())
    {
        h = style()->height().width(ch);
        
        if (m_height-pab > h) {
            setOverflowHeight(m_height + pab - (paddingBottom() + borderBottom()));
            m_height = h+pab;
        }
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
        static_top = m_staticY - cb->borderTop(); // Should already have been set through layout of the parent().
        RenderObject* po = parent();
        for (; po && po != cb; po = po->parent())
            static_top += po->yPos();

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

    if (style()->hidesOverflow() && m_height > h+pab)
        m_height = h+pab;
    
    // Do not allow the height to be negative.  This can happen when someone specifies both top and bottom
    // but the containing block height is less than top, e.g., top:20px, bottom:0, containing block height 16.
    m_height = kMax(0, m_height);
    
    m_marginTop = mt;
    m_marginBottom = mb;
    m_y = t + mt + cb->borderTop();
    
//    printf("v: h=%d, t=%d, b=%d, mt=%d, mb=%d, m_y=%d\n",h,t,b,mt,mb,m_y);

}


int RenderBox::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    return includeSelf ? m_height : 0;
}

int RenderBox::rightmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    return includeSelf ? m_width : 0;
}

int RenderBox::leftmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    return includeSelf ? 0 : m_width;
}

#undef DEBUG_LAYOUT
