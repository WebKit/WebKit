/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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

#include "config.h"
#include "render_list.h"

#include "GraphicsContext.h"
#include "render_canvas.h"

#include "DocumentImpl.h"
#include "CachedImage.h"

#include "htmlnames.h"
#include "html_listimpl.h"
#include "helper.h"

//#define BOX_DEBUG

namespace WebCore {

using namespace HTMLNames;

const int cMarkerPadding = 7;

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
    QString letter = (QChar) (base + (number % 26));
    // Add a single quote at the end of the alphabet.
    for (int i = 0; i < (number / 26); i++) {
       letter += '\'';
    }
    return letter;
}

static QString toHebrew( int number ) {
    const QChar tenDigit[] = {1497, 1499, 1500, 1502, 1504, 1505, 1506, 1508, 1510};

    QString letter;
    if (number>999) {
  	letter = toHebrew(number/1000) + "'";
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
    : RenderBlock(node), predefVal(-1), m_marker(0), _notInList(false), m_value(-1)
{
    // init RenderObject attributes
    setInline(false);   // our object is not Inline
}

void RenderListItem::setStyle(RenderStyle *_style)
{
    RenderBlock::setStyle(_style);

    if (style()->listStyleType() != LNONE ||
        (style()->listStyleImage() && !style()->listStyleImage()->isErrorImage())) {
        RenderStyle *newStyle = new (renderArena()) RenderStyle();
        newStyle->ref();
        // The marker always inherits from the list item, regardless of where it might end
        // up (e.g., in some deeply nested line box).  See CSS3 spec.
        newStyle->inheritFrom(style()); 
        if (!m_marker) {
            m_marker = new (renderArena()) RenderListMarker(document());
            m_marker->setStyle(newStyle);
            m_marker->setListItem(this);
        } else
            m_marker->setStyle(newStyle);
        newStyle->deref(renderArena());
    } else if (m_marker) {
        m_marker->destroy();
        m_marker = 0;
    }
}

void RenderListItem::destroy()
{    
    if (m_marker) {
        m_marker->destroy();
        m_marker = 0;
    }
    RenderBlock::destroy();
}

static NodeImpl* enclosingList(NodeImpl* node)
{
    for (NodeImpl* n = node->parentNode(); n; n = n->parentNode())
        if (n->hasTagName(ulTag) || n->hasTagName(olTag))
            return n;
    return 0;
}

static RenderListItem* previousListItem(NodeImpl* list, RenderListItem* item)
{
    if (!list)
        return 0;
    for (NodeImpl* n = item->node()->traversePreviousNode(); n != list; n = n->traversePreviousNode()) {
        RenderObject* o = n->renderer();
        if (o && o->isListItem()) {
            NodeImpl* otherList = enclosingList(n);
            // This item is part of our current list, so it's what we're looking for.
            if (list == otherList)
                return static_cast<RenderListItem*>(o);
            // We found ourself inside another list; lets skip the rest of it.
            if (otherList)
                n = otherList;
        }
    }
    return 0;
}

void RenderListItem::calcValue()
{
    if (predefVal != -1)
        m_value = predefVal;
    else {
        NodeImpl* list = enclosingList(node());
        RenderListItem* item = previousListItem(list, this);
        if (item) {
            // FIXME: This recurses to a possible depth of the length of the list.
            // That's not good -- we need to change this to an iterative algorithm.
            if (item->value() == -1)
                item->calcValue();
            m_value = item->value() + 1;
        } else if (list && list->hasTagName(olTag))
            m_value = static_cast<HTMLOListElementImpl*>(list)->start();
        else
            m_value = 1;
    }
}

bool RenderListItem::isEmpty() const
{
    return lastChild() == m_marker;
}

static RenderObject* getParentOfFirstLineBox(RenderObject* curr, RenderObject* marker)
{
    RenderObject* firstChild = curr->firstChild();
    if (!firstChild)
        return 0;
        
    for (RenderObject* currChild = firstChild; currChild; currChild = currChild->nextSibling()) {
        if (currChild == marker)
            continue;
            
        if (currChild->isInline())
            return curr;
        
        if (currChild->isFloating() || currChild->isPositioned())
            continue;
            
        if (currChild->isTable() || !currChild->isRenderBlock())
            break;
        
        if (currChild->style()->htmlHacks() && currChild->element() &&
            (currChild->element()->hasTagName(ulTag)|| currChild->element()->hasTagName(olTag)))
            break;
            
        RenderObject* lineBox = getParentOfFirstLineBox(currChild, marker);
        if (lineBox)
            return lineBox;
    }
    
    return 0;
}

void RenderListItem::resetValue()
{
    m_value = -1;
    if (m_marker)
        m_marker->setNeedsLayoutAndMinMaxRecalc();
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
            if (markerPar && markerPar->isAnonymousBlock())
                lineBoxParent = markerPar;
            else
                lineBoxParent = this;
        }
        
        if (markerPar != lineBoxParent || !m_marker->minMaxKnown()) {
            if (markerPar)
                markerPar->removeChild(m_marker);
            if (!lineBoxParent)
                lineBoxParent = this;
            lineBoxParent->addChild(m_marker, lineBoxParent->firstChild());
            if (!m_marker->minMaxKnown())
                m_marker->calcMinMaxWidth();
            recalcMinMaxWidths();
        }
    }
}

// We need to override RenderBlock::nodeAtPoint so that a point over an outside list marker will return the list item.
// We should remove this when we improve the way list markers are stored in the render tree.
bool RenderListItem::nodeAtPoint(NodeInfo& i, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    if (RenderBlock::nodeAtPoint(i, x, y, tx, ty, hitTestAction))
        return true;
    
    if (m_marker && !m_marker->isInside() && hitTestAction == HitTestForeground) {
        IntRect markerRect = m_marker->getRelativeMarkerRect();
        markerRect.move(tx + m_x, ty + m_y);
        if (markerRect.contains(x, y)) {
            setInnerNode(i);
            return true;
        }
    }
    
    return false;
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

void RenderListItem::paint(PaintInfo& i, int _tx, int _ty)
{
    if (!m_height)
        return;

    RenderBlock::paint(i, _tx, _ty);
}

IntRect RenderListItem::getAbsoluteRepaintRect()
{
    IntRect result = RenderBlock::getAbsoluteRepaintRect();
    if (m_marker && m_marker->parent() == this && !m_marker->isInside()) {
        IntRect markerRect = m_marker->getRelativeMarkerRect();
        int x, y;
        absolutePosition(x, y);
        markerRect.move(x, y);
        result.unite(markerRect);
    }
    return result;
}

// -----------------------------------------------------------

RenderListMarker::RenderListMarker(DocumentImpl* document)
    : RenderBox(document), m_listImage(0)
{
    // init RenderObject attributes
    setInline(true);   // our object is Inline
    setReplaced(true); // pretend to be replaced
}

RenderListMarker::~RenderListMarker()
{
    if (m_listImage)
        m_listImage->deref(this);
}

void RenderListMarker::setStyle(RenderStyle *s)
{
    if (s && style() && s->listStylePosition() != style()->listStylePosition())
        setNeedsLayoutAndMinMaxRecalc();
    
    RenderBox::setStyle(s);

    if ( m_listImage != style()->listStyleImage() ) {
	if (m_listImage)
	    m_listImage->deref(this);
	m_listImage = style()->listStyleImage();
	if (m_listImage)
	    m_listImage->ref(this);
    }
}

InlineBox* RenderListMarker::createInlineBox(bool, bool isRootLineBox, bool)
{
    KHTMLAssert(!isRootLineBox);
    ListMarkerBox* box = new (renderArena()) ListMarkerBox(this);
    m_inlineBoxWrapper = box;
    return box;
}

void RenderListMarker::paint(PaintInfo& i, int _tx, int _ty)
{
    if (i.phase != PaintActionForeground)
        return;
    
    if (style()->visibility() != VISIBLE)  return;

    IntRect marker = getRelativeMarkerRect();
    marker.move(IntPoint(_tx, _ty));

    IntRect box(_tx + m_x, _ty + m_y, m_width, m_height);

    if (box.y() > i.r.bottom() || box.y() + box.height() < i.r.y())
        return;

    if (shouldPaintBackgroundOrBorder()) 
        paintBoxDecorations(i, box.x(), box.y());

    GraphicsContext* p = i.p;
    p->setFont(style()->font());

    if (p->printing()) {
        if (box.y() < i.r.y())
            // This has been printed already we suppose.
            return;
        
        RenderCanvas* c = canvas();
        if (box.y() + box.height() + paddingBottom() + borderBottom() >= c->printRect().bottom()) {
            if (box.y() < c->truncatedAt())
                c->setBestTruncatedAt(box.y(), this);
            // Let's print this on the next page.
            return; 
        }
    }

    if (m_listImage && !m_listImage->isErrorImage()) {
        p->drawImageAtPoint(m_listImage->image(), marker.location());
        return;
    }

#ifdef BOX_DEBUG
    p->setPen( red );
    p->drawRect(box.x(), box.y(), box.width(), box.height());
#endif

    const Color color( style()->color() );
    p->setPen( color );

    switch(style()->listStyleType()) {
    case DISC:
        p->setBrush(color);
        p->drawEllipse(marker.x(), marker.y(), marker.width(), marker.height());
        return;
    case CIRCLE:
        p->setBrush(WebCore::Brush::NoBrush);
        p->drawEllipse(marker.x(), marker.y(), marker.width(), marker.height());
        return;
    case SQUARE:
        p->setBrush(color);
        p->drawRect(marker.x(), marker.y(), marker.width(), marker.height());
        return;
    case LNONE:
        return;
    default:
        if (!m_item.isEmpty()) {
            const Font& font = style()->font();
            if (isInside()) {
            	if( style()->direction() == LTR) {
                    p->drawText(marker.x(), marker.y(), 0, 0, 0, 0, AlignLeft, m_item);
                    p->drawText(marker.x() + font.width(m_item), marker.y(), 0, 0, 0, 0, AlignLeft, ". ");
                } else {
                    p->drawText(marker.x(), marker.y(), 0, 0, 0, 0, AlignLeft, " .");
            	    p->drawText(marker.x() + font.width(" ."), marker.y(), 0, 0, 0, 0, AlignLeft, m_item);
                }
            } else {
                if (style()->direction() == LTR) {
                    p->drawText(marker.x(), marker.y(), 0, 0, 0, 0, AlignRight, ". ");
                    p->drawText(marker.x() - font.width(". "), marker.y(), 0, 0, 0, 0, AlignRight, m_item);
                } else {
            	    p->drawText(marker.x(), marker.y(), 0, 0, 0, 0, AlignLeft, " .");
                    p->drawText(marker.x() + font.width(" ."), marker.y(), 0, 0, 0, 0, AlignLeft, m_item);
                }
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

void RenderListMarker::imageChanged(CachedImage *o)
{
    if (o != m_listImage) {
        RenderBox::imageChanged(o);
        return;
    }

    if (m_width != m_listImage->imageSize().width() || m_height != m_listImage->imageSize().height())
        setNeedsLayoutAndMinMaxRecalc();
    else
        repaint();
}

void RenderListMarker::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    m_width = 0;

    if (m_listImage) {
        if (isInside())
            m_width = m_listImage->image()->width() + cMarkerPadding;
        m_height = m_listImage->image()->height();
        m_minWidth = m_maxWidth = m_width;
        setMinMaxKnown();
        return;
    }

    if (m_listItem->value() < 0) // not yet calculated
        m_listItem->calcValue();

    const Font& font = style()->font();
    m_height = font.ascent();

    switch(style()->listStyleType())
    {
    case DISC:
    case CIRCLE:
    case SQUARE:
        if (isInside())
            m_width = m_height;
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
        m_item.sprintf( "%d", m_listItem->value() );
        break;
    case LOWER_ROMAN:
        m_item = toRoman( m_listItem->value(), false );
        break;
    case UPPER_ROMAN:
        m_item = toRoman( m_listItem->value(), true );
        break;
    case LOWER_GREEK:
     {
    	int number = m_listItem->value() - 1;
      	int l = (number % 24);

	if (l>16) {l++;} // Skip GREEK SMALL LETTER FINAL SIGMA

   	m_item = QChar(945 + l);
    	for (int i = 0; i < (number / 24); i++) {
       	    m_item += "'";
    	}
	break;
     }
    case HEBREW:
     	m_item = toHebrew( m_listItem->value() );
	break;
    case LOWER_ALPHA:
    case LOWER_LATIN:
        m_item = toLetter( m_listItem->value(), 'a' );
        break;
    case UPPER_ALPHA:
    case UPPER_LATIN:
        m_item = toLetter( m_listItem->value(), 'A' );
        break;
    case LNONE:
        break;
    }

    if (isInside())
        m_width = font.width(m_item) + font.width(". ");

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
    if (!m_listImage)
        return m_listItem->lineHeight(false, true);
    return height();
}

short RenderListMarker::baselinePosition(bool, bool) const
{
    if (!m_listImage) {
        const Font& font = style()->font();
        return font.ascent() + (lineHeight(false) - font.height())/2;
    }
    return height();
}

bool RenderListMarker::isInside() const
{
    return m_listItem->notInList() || style()->listStylePosition() == INSIDE;
}

IntRect RenderListMarker::getRelativeMarkerRect()
{
    int x = m_x;
    int y = m_y;
    
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
            x -= (xOffset - leftLineOffset) + m_listItem->paddingLeft() + m_listItem->borderLeft();
        } else {
            rightLineOffset = m_listItem->rightRelOffset(yOffset, m_listItem->rightOffset(yOffset));
            x += (rightLineOffset-xOffset) + m_listItem->paddingRight() + m_listItem->borderRight();
        }
    }
    
    const Font& font = style()->font();
    
    int offset = font.ascent()*2/3;
    bool haveImage = m_listImage && !m_listImage->isErrorImage();
    if (haveImage)
        offset = m_listImage->image()->width();
    
    int xoff = 0;
    int yoff = font.ascent() - offset;

    int bulletWidth = offset/2;
    if (offset%2)
        bulletWidth++;
    if (!isInside()) {
        if (listItem->style()->direction() == LTR)
            xoff = -cMarkerPadding - offset;
        else
            xoff = cMarkerPadding + (haveImage ? 0 : (offset - bulletWidth));
    } else if (style()->direction() == RTL)
        xoff += haveImage ? cMarkerPadding : (m_width - bulletWidth);
    
    if (m_listImage && !m_listImage->isErrorImage())
        return IntRect(x + xoff, y,  m_listImage->imageSize().width(), m_listImage->imageSize().height());
    
    switch(style()->listStyleType()) {
    case DISC:
    case CIRCLE:
    case SQUARE:
        return IntRect(x + xoff, y + (3 * yoff)/2, bulletWidth, bulletWidth);
    case LNONE:
        return IntRect();
    default:
        if (m_item.isEmpty())
            return IntRect();
            
        y += font.ascent();

        if (isInside())
            return IntRect(x, y, font.width(m_item) + font.width(". "), font.height());
        else {
            if (style()->direction() == LTR)
                return IntRect(x - offset / 2, y, font.width(m_item) + font.width(". "), font.height());
            else
                return IntRect(x + offset / 2, y, font.width(m_item) + font.width(". "), font.height());
        }
    }
}

}
