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

#include "render_list.h"
#include "rendering/render_canvas.h"

#include "xml/dom_docimpl.h"
#include "misc/htmltags.h"

#include <qpainter.h>

#include "misc/helper.h"

#include <kdebug.h>

//#define BOX_DEBUG

using DOM::DocumentImpl;
using namespace khtml;

static QString toRoman( int number, bool upper )
{
    QString roman;
    QChar ldigits[] = { 'i', 'v', 'x', 'l', 'c', 'd', 'm' };
    QChar udigits[] = { 'I', 'V', 'X', 'L', 'C', 'D', 'M' };
    QChar *digits = upper ? udigits : ldigits;
    int i, d = 0;

    do
    {
        int num = number % 10;

        if ( num % 5 < 4 )
            for ( i = num % 5; i > 0; i-- )
                roman.insert( 0, digits[ d ] );

        if ( num >= 4 && num <= 8)
            roman.insert( 0, digits[ d+1 ] );

        if ( num == 9 )
            roman.insert( 0, digits[ d+2 ] );

        if ( num % 5 == 4 )
            roman.insert( 0, digits[ d ] );

        number /= 10;
        d += 2;
    }
    while ( number );

    return roman;
}

static QString toLetter( int number, int base ) {
    number--;
    QString letter = (QChar) (base + (number % 24));
    // Add a "'" at the end of the alphabet
    for (int i = 0; i < (number / 24); i++) {
       letter += QString::fromLatin1("'");
    }
    return letter;
}

static QString toHebrew( int number ) {
    const QChar tenDigit[] = {1497, 1499, 1500, 1502, 1504, 1505, 1506, 1508, 1510};

    QString letter;
    if (number>999) {
  	letter = toHebrew(number/1000) + QString::fromLatin1("'");
   	number = number%1000;
    }

    int hunderts = (number/400);
    if (hunderts > 0) {
	for(int i=0; i<hunderts; i++) {
	    letter += QChar(1511 + 3);
	}
    }
    number = number % 400;
    if ((number / 100) != 0) {
        letter += QChar (1511 + (number / 100) -1);
    }
    number = number % 100;
    int tens = number/10;
    if (tens > 0 && !(number == 15 || number == 16)) {
	letter += tenDigit[tens-1];
    }
    if (number == 15 || number == 16) { // special because of religious
	letter += QChar(1487 + 9);       // reasons
    	letter += QChar(1487 + number - 9);
    } else {
        number = number % 10;
        if (number != 0) {
            letter += QChar (1487 + number);
        }
    }
    return letter;
}

// -------------------------------------------------------------------------

RenderListItem::RenderListItem(DOM::NodeImpl* node)
    : RenderBlock(node), _notInList(false)
{
    // init RenderObject attributes
    setInline(false);   // our object is not Inline

    predefVal = -1;
    m_marker = 0;
}

void RenderListItem::setStyle(RenderStyle *_style)
{
    RenderBlock::setStyle(_style);

    if (style()->listStyleType() != LNONE ||
        (style()->listStyleImage() && !style()->listStyleImage()->isErrorImage())) {
        RenderStyle *newStyle = new RenderStyle();
        newStyle->ref();
        newStyle->inheritFrom(style());
        if (!m_marker) {
            m_marker = new (renderArena()) RenderListMarker();
            m_marker->setStyle(newStyle);
            m_marker->setListItem(this);
            _markerInstalledInParent = false;
        }
        else
            m_marker->setStyle(newStyle);
        newStyle->deref();
    } else if (m_marker) {
        m_marker->detach(renderArena());
        m_marker = 0;
    }
}

RenderListItem::~RenderListItem()
{
}

void RenderListItem::detach(RenderArena* renderArena)
{    
    if (m_marker && !_markerInstalledInParent) {
        m_marker->detach(renderArena);
        m_marker = 0;
    }
    RenderBlock::detach(renderArena);
}

void RenderListItem::calcListValue()
{
    // only called from the marker so..
    KHTMLAssert(m_marker);

    if(predefVal != -1)
        m_marker->m_value = predefVal;
    else if(!previousSibling())
        m_marker->m_value = 1;
    else {
	RenderObject *o = previousSibling();
	while ( o && (!o->isListItem() || o->style()->listStyleType() == LNONE) )
	    o = o->previousSibling();
        if( o && o->isListItem() && o->style()->listStyleType() != LNONE ) {
            RenderListItem *item = static_cast<RenderListItem *>(o);
            m_marker->m_value = item->value() + 1;
        }
        else
            m_marker->m_value = 1;
    }
}

static RenderObject* getParentOfFirstLineBox(RenderObject* curr, RenderObject* marker)
{
    RenderObject* firstChild = curr->firstChild();
    if (!firstChild)
        return 0;
        
    for (RenderObject* currChild = firstChild;
         currChild; currChild = currChild->nextSibling()) {
        if (currChild == marker)
            continue;
            
        if (currChild->isInline())
            return curr;
        
        if (currChild->isFloating() || currChild->isPositioned())
            continue;
            
        if (currChild->isTable() || !currChild->isRenderBlock())
            break;
        
        if (currChild->style()->htmlHacks() && currChild->element() &&
            (currChild->element()->id() == ID_UL || currChild->element()->id() == ID_OL))
            break;
            
        RenderObject* lineBox = getParentOfFirstLineBox(currChild, marker);
        if (lineBox)
            return lineBox;
    }
    
    return 0;
}

void RenderListItem::updateMarkerLocation()
{
    // Sanity check the location of our marker.
    if (m_marker) {
        RenderObject* markerPar = m_marker->parent();
        RenderObject* lineBoxParent = getParentOfFirstLineBox(this, m_marker);
        if (!lineBoxParent) {
            // If the marker is currently contained inside an anonymous box,
            // then we are the only item in that anonymous box (since no line box
            // parent was found).  It's ok to just leave the marker where it is
            // in this case.
            if (markerPar && markerPar->isAnonymousBox())
                lineBoxParent = markerPar;
            else
                lineBoxParent = this;
        }
        if (markerPar != lineBoxParent)
        {
            if (markerPar)
                markerPar->removeChild(m_marker);
            if (!lineBoxParent)
                lineBoxParent = this;
            lineBoxParent->addChild(m_marker, lineBoxParent->firstChild());
            _markerInstalledInParent = true;
            if (!m_marker->minMaxKnown())
                m_marker->calcMinMaxWidth();
            recalcMinMaxWidths();
        }
    }
}

void RenderListItem::calcMinMaxWidth()
{
    // Make sure our marker is in the correct location.
    updateMarkerLocation();
    if (!minMaxKnown())
        RenderBlock::calcMinMaxWidth();
}

void RenderListItem::layout( )
{
    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

    updateMarkerLocation();    
    RenderBlock::layout();
}

void RenderListItem::paint(QPainter *p, int _x, int _y, int _w, int _h,
                           int _tx, int _ty, PaintAction paintAction)
{
    if ( !m_height )
        return;

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << nodeName().string() << "(LI)::paint()" << endl;
#endif
    RenderBlock::paint(p, _x, _y, _w, _h, _tx, _ty, paintAction);
}

void RenderListItem::paintObject(QPainter *p, int _x, int _y,
                                    int _w, int _h, int _tx, int _ty, PaintAction paintAction)
{
    // ### this should scale with the font size in the body... possible?
    //m_marker->printIcon(p, _tx, _ty);
    RenderBlock::paintObject(p, _x, _y, _w, _h, _tx, _ty, paintAction);
}

// -----------------------------------------------------------

RenderListMarker::RenderListMarker()
    : RenderBox(0), m_listImage(0), m_value(-1)
{
    // init RenderObject attributes
    setInline(true);   // our object is Inline
    setReplaced(true); // pretend to be replaced
    // val = -1;
    // m_listImage = 0;
}

RenderListMarker::~RenderListMarker()
{
    if(m_listImage)
        m_listImage->deref(this);
}

void RenderListMarker::setStyle(RenderStyle *s)
{
    if ( s && style() && s->listStylePosition() != style()->listStylePosition() )
        setNeedsLayoutAndMinMaxRecalc();
    
    RenderBox::setStyle(s);

    if ( m_listImage != style()->listStyleImage() ) {
	if(m_listImage)  m_listImage->deref(this);
	m_listImage = style()->listStyleImage();
	if(m_listImage)  m_listImage->ref(this);
    }
}


void RenderListMarker::paint(QPainter *p, int _x, int _y, int _w, int _h,
                             int _tx, int _ty, PaintAction paintAction)
{
    if (paintAction != PaintActionForeground)
        return;
    
    if (style()->visibility() != VISIBLE)  return;

    _tx += m_x;
    _ty += m_y;

    if((_ty > _y + _h) || (_ty + m_height < _y))
        return;

    if(shouldPaintBackgroundOrBorder()) 
        paintBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);

    paintObject(p, _x, _y, _w, _h, _tx, _ty, paintAction);
}

void RenderListMarker::paintObject(QPainter *p, int, int _y,
                                    int, int _h, int _tx, int _ty, PaintAction paintAction)
{
    if (style()->visibility() != VISIBLE) return;

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << nodeName().string() << "(ListMarker)::paintObject(" << _tx << ", " << _ty << ")" << endl;
#endif
    p->setFont(style()->font());
    const QFontMetrics fm = p->fontMetrics();
    int offset = fm.ascent()*2/3;

    // The marker needs to adjust its tx, for the case where it's an outside marker.
    RenderObject* listItem = 0;
    int leftLineOffset = 0;
    int rightLineOffset = 0;
    if (!isInside()) {
        listItem = this;
        int yOffset = 0;
        int xOffset = 0;
        while (listItem && listItem != m_listItem) {
            yOffset += listItem->yPos();
            xOffset += listItem->xPos();
            listItem = listItem->parent();
        }
        
        // Now that we have our xoffset within the listbox, we need to adjust ourselves by the delta
        // between our current xoffset and our desired position (which is just outside the border box
        // of the list item).
        if (style()->direction() == LTR) {
            leftLineOffset = m_listItem->leftRelOffset(yOffset, m_listItem->leftOffset(yOffset));
            _tx -= (xOffset - leftLineOffset) + m_listItem->paddingLeft() + m_listItem->borderLeft();
        }
        else {
            rightLineOffset = m_listItem->rightRelOffset(yOffset, m_listItem->rightOffset(yOffset));
            _tx += (rightLineOffset-xOffset) + m_listItem->paddingRight() + m_listItem->borderRight();
        }
    }

    bool isPrinting = (p->device()->devType() == QInternal::Printer);
    if (isPrinting)
    {
        if (_ty < _y)
        {
            // This has been printed already we suppose.
            return;
        }
        if (_ty + m_height + paddingBottom() + borderBottom() >= _y+_h)
        {
            RenderCanvas *rootObj = canvas();
            if (_ty < rootObj->truncatedAt())
#if APPLE_CHANGES
                rootObj->setBestTruncatedAt(_ty, this);
#else
                rootObj->setTruncatedAt(_ty);
#endif
            // Let's print this on the next page.
            return; 
        }
    }
    

    int xoff = 0;
    int yoff = fm.ascent() - offset;

    if (!isInside())
        if (listItem->style()->direction() == LTR)
            xoff = -7 - offset;
        else 
            xoff = offset;
        
            
    if ( m_listImage && !m_listImage->isErrorImage()) {
        // For OUTSIDE bullets shrink back to only a 0.3em margin. 0.67 em is too
        // much.  This brings the margin back to MacIE/Gecko/WinIE levels.  
        // For LTR don't forget to add in the width of the image to the offset as
        // well (you are moving the image left, so you have to also add in the width
        // of the image's border box as well). -dwh
        if (!isInside()) {
            if (style()->direction() == LTR)
                xoff -= m_listImage->pixmap().width() - fm.ascent()*1/3;
            else
                xoff -= fm.ascent()*1/3;
        }
        
        p->drawPixmap( QPoint( _tx + xoff, _ty ), m_listImage->pixmap());
        return;
    }

#ifdef BOX_DEBUG
    p->setPen( Qt::red );
    p->drawRect( _tx + xoff, _ty + yoff, offset, offset );
#endif

    const QColor color( style()->color() );
    p->setPen( color );

    switch(style()->listStyleType()) {
    case DISC:
        p->setBrush( color );
        p->drawEllipse( _tx + xoff, _ty + (3 * yoff)/2, (offset>>1)+1, (offset>>1)+1 );
        return;
    case CIRCLE:
        p->setBrush( Qt::NoBrush );
        p->drawEllipse( _tx + xoff, _ty + (3 * yoff)/2, (offset>>1)+1, (offset>>1)+1 );
        return;
    case SQUARE:
        p->setBrush( color );
        p->drawRect( _tx + xoff, _ty + (3 * yoff)/2, (offset>>1)+1, (offset>>1)+1 );
        return;
    case LNONE:
        return;
    default:
        if (!m_item.isEmpty()) {
#if APPLE_CHANGES
            // Text should be drawn on the baseline, so we add in the ascent of the font. 
            // For some inexplicable reason, this works in Konqueror.  I'm not sure why.
            // - dwh
       	    _ty += fm.ascent();
#else
       	    //_ty += fm.ascent() - fm.height()/2 + 1;
#endif
            if (isInside()) {
            	if( style()->direction() == LTR)
                    p->drawText(_tx, _ty, 0, 0, Qt::AlignLeft|Qt::DontClip, m_item);
            	else
            	    p->drawText(_tx, _ty, 0, 0, Qt::AlignRight|Qt::DontClip, m_item);
            } else {
                if(style()->direction() == LTR)
            	    p->drawText(_tx-offset/2, _ty, 0, 0, Qt::AlignRight|Qt::DontClip, m_item);
            	else
            	    p->drawText(_tx+offset/2, _ty, 0, 0, Qt::AlignLeft|Qt::DontClip, m_item);
            }
        }
    }
}

void RenderListMarker::layout()
{
    KHTMLAssert( needsLayout() );
    // ### KHTMLAssert( minMaxKnown() );
    if ( !minMaxKnown() )
	calcMinMaxWidth();
    setNeedsLayout(false);
}

void RenderListMarker::setPixmap( const QPixmap &p, const QRect& r, CachedImage *o)
{
    if(o != m_listImage) {
        RenderBox::setPixmap(p, r, o);
        return;
    }

    if(m_width != m_listImage->pixmap_size().width() || m_height != m_listImage->pixmap_size().height())
        setNeedsLayoutAndMinMaxRecalc();
    else
        repaintRectangle(0, 0, m_width, m_height);
}

void RenderListMarker::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    m_width = 0;

    if(m_listImage) {
        if (isInside())
            m_width = m_listImage->pixmap().width() + 5;
        m_height = m_listImage->pixmap().height();
        m_minWidth = m_maxWidth = m_width;
        setMinMaxKnown();
        return;
    }

    if (m_value < 0) // not yet calculated
        m_listItem->calcListValue();

    const QFontMetrics &fm = style()->fontMetrics();
    m_height = fm.ascent();

    switch(style()->listStyleType())
    {
    case DISC:
    case CIRCLE:
    case SQUARE:
        if (isInside()) {
            m_width = m_height; //fm.ascent();
        }
    	goto end;
    case ARMENIAN:
    case GEORGIAN:
    case CJK_IDEOGRAPHIC:
    case HIRAGANA:
    case KATAKANA:
    case HIRAGANA_IROHA:
    case KATAKANA_IROHA:
    case DECIMAL_LEADING_ZERO:
        // ### unsupported, we use decimal instead
    case LDECIMAL:
        m_item.sprintf( "%2ld", m_value );
        break;
    case LOWER_ROMAN:
        m_item = toRoman( m_value, false );
        break;
    case UPPER_ROMAN:
        m_item = toRoman( m_value, true );
        break;
    case LOWER_GREEK:
     {
    	int number = m_value - 1;
      	int l = (number % 24);

	if (l>16) {l++;} // Skip GREEK SMALL LETTER FINAL SIGMA

   	m_item = QChar(945 + l);
    	for (int i = 0; i < (number / 24); i++) {
       	    m_item += QString::fromLatin1("'");
    	}
	break;
     }
    case HEBREW:
     	m_item = toHebrew( m_value );
	break;
    case LOWER_ALPHA:
    case LOWER_LATIN:
        m_item = toLetter( m_value, 'a' );
        break;
    case UPPER_ALPHA:
    case UPPER_LATIN:
        m_item = toLetter( m_value, 'A' );
        break;
    case LNONE:
        break;
    }
    m_item += QString::fromLatin1(". ");

    if (isInside())
        m_width = fm.width(m_item);

end:

    m_minWidth = m_width;
    m_maxWidth = m_width;

    setMinMaxKnown();
}

void RenderListMarker::calcWidth()
{
    RenderBox::calcWidth();
}

short RenderListMarker::lineHeight(bool, bool) const
{
    return height();
}

short RenderListMarker::baselinePosition(bool, bool) const
{
    return height();
}

bool RenderListMarker::isInside() const
{
    return m_listItem->notInList() || style()->listStylePosition() == INSIDE;
}

#undef BOX_DEBUG
