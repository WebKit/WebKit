/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
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
//#define DEBUG_LAYOUT
//#define BIDI_DEBUG

#include "rendering/render_root.h"
#include "rendering/render_text.h"
#include "rendering/render_root.h"
#include "rendering/break_lines.h"
#include "xml/dom_nodeimpl.h"

#include "misc/loader.h"

#include <qpainter.h>
#include <kdebug.h>
#include <assert.h>

using namespace khtml;
using namespace DOM;

void TextSlave::printSelection(const Font *f, RenderText *text, QPainter *p, RenderStyle* style, int tx, int ty, int startPos, int endPos)
{
    if(startPos > m_len) return;
    if(startPos < 0) startPos = 0;

    p->save();
#if APPLE_CHANGES
    // Macintosh-style text highlighting is to draw with a particular background color, not invert.
    QColor textColor = style->color();
    QColor c = QPainter::selectedTextBackgroundColor();
    if (textColor == c)
        c = QColor(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());
    p->setPen(c); // Don't draw text at all!
#else
    QColor c = style->color();
    p->setPen(QColor(0xff-c.red(),0xff-c.green(),0xff-c.blue()));
#endif
    ty += m_baseline;

    //kdDebug( 6040 ) << "textSlave::printing(" << s.string() << ") at(" << x+_tx << "/" << y+_ty << ")" << endl;
    f->drawText(p, m_x + tx, m_y + ty, text->str->s, text->str->l, m_start, m_len,
		m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, startPos, endPos, c);
    p->restore();
}

void TextSlave::printDecoration( QPainter *pt, RenderText* p, int _tx, int _ty, int deco, bool begin, bool end)
{
    _tx += m_x;
    _ty += m_y;

    int width = m_width - 1;

    if( begin )
 	width -= p->paddingLeft() + p->borderLeft();

    if ( end )
        width -= p->paddingRight() + p->borderRight();

#if APPLE_CHANGES
    // Use a special function for underlines to get the positioning exactly right.
    if(deco & UNDERLINE)
        pt->drawUnderlineForText(_tx, _ty + m_baseline, p->str->s + m_start, m_len);
#else
    int underlineOffset = ( pt->fontMetrics().height() + m_baseline ) / 2;
    if(underlineOffset <= m_baseline) underlineOffset = m_baseline+1;

    if(deco & UNDERLINE){
        pt->drawLine(_tx, _ty + underlineOffset, _tx + width, _ty + underlineOffset );
    }
#endif
    if(deco & OVERLINE)
        pt->drawLine(_tx, _ty, _tx + width, _ty );
    if(deco & LINE_THROUGH)
        pt->drawLine(_tx, _ty + 2*m_baseline/3, _tx + width, _ty + 2*m_baseline/3 );
    // NO! Do NOT add BLINK! It is the most annouing feature of Netscape, and IE has a reason not to
    // support it. Lars
}

void TextSlave::printBoxDecorations(QPainter *pt, RenderStyle* style, RenderText *p, int _tx, int _ty, bool begin, bool end)
{
    int topExtra = p->borderTop() + p->paddingTop();
    int bottomExtra = p->borderBottom() + p->paddingBottom();
    // ### firstline
    int halfleading = (p->m_lineHeight - style->font().pixelSize() ) / 2;

    _tx += m_x;
    _ty += m_y + halfleading - topExtra;

    int width = m_width;

    // the height of the decorations is:  topBorder + topPadding + CSS font-size + bottomPadding + bottomBorder
    int height = style->font().pixelSize() + topExtra + bottomExtra;

    if( begin )
	_tx -= p->paddingLeft() + p->borderLeft();

    QColor c = style->backgroundColor();
    CachedImage *i = style->backgroundImage();
    if(c.isValid() && (!i || i->tiled_pixmap(c).mask()))
         pt->fillRect(_tx, _ty, width, height, c);

    if(i) {
        // ### might need to add some correct offsets
        // ### use paddingX/Y
        pt->drawTiledPixmap(_tx, _ty, width, height, i->tiled_pixmap(c));
    }

#ifdef DEBUG_VALIGN
    pt->fillRect(_tx, _ty, width, height, Qt::cyan );
#endif

    if(style->hasBorder())
        p->printBorder(pt, _tx, _ty, width, height, style, begin, end);
}

FindSelectionResult TextSlave::checkSelectionPoint(int _x, int _y, int _tx, int _ty, const Font *f, RenderText *text, int & offset, short lineHeight)
{
//     kdDebug(6040) << "TextSlave::checkSelectionPoint " << this << " _x=" << _x << " _y=" << _y
//                   << " _tx+m_x=" << _tx+m_x << " _ty+m_y=" << _ty+m_y << endl;
    offset = 0;

    if ( _y < _ty + m_y )
        return SelectionPointBefore; // above -> before

    if ( _y > _ty + m_y + lineHeight ) {
        // below -> after
        // Set the offset to the max
        offset = m_len;
        return SelectionPointAfter;
    }
    if ( _x > _tx + m_x + m_width ) {
	// to the right
	return m_reversed ? SelectionPointBeforeInLine : SelectionPointAfterInLine;
    }

    // The Y matches, check if we're on the left
    if ( _x < _tx + m_x ) {
        return m_reversed ? SelectionPointAfterInLine : SelectionPointBeforeInLine;
    }

#if APPLE_CHANGES
    // Floating point version needed for best results with Mac OS X text.
    float delta = _x - (_tx + m_x);
    float widths[m_len]; 
    
    // Do width calculations for whole run once.
    f->floatCharacterWidths( text->str->s, text->str->l, m_start, m_len, m_toAdd, &widths[0]);
    int pos = 0;
    if ( m_reversed ) {
	delta -= m_width;
	while(pos < m_len) {
	    float w = widths[pos];
	    float w2 = w/2;
	    w -= w2;
	    delta += w2;
	    if(delta >= 0)
	        break;
	    pos++;
	    delta += w;
	}
    } else {
	while(pos < m_len) {
	    float w = widths[pos];
	    float w2 = w/2;
	    w -= w2;
	    delta -= w2;
	    if(delta <= 0) 
	        break;
	    pos++;
	    delta -= w;
	}
    }
#else
    int delta = _x - (_tx + m_x);
    //kdDebug(6040) << "TextSlave::checkSelectionPoint delta=" << delta << endl;
    int pos = 0;
    if ( m_reversed ) {
	delta -= m_width;
	while(pos < m_len) {
	    int w = f->width( text->str->s, text->str->l, m_start + pos);
	    int w2 = w/2;
	    w -= w2;
	    delta += w2;
	    if(delta >= 0) break;
	    pos++;
	    delta += w;
	}
    } else {
	while(pos < m_len) {
	    int w = f->width( text->str->s, text->str->l, m_start + pos);
	    int w2 = w/2;
	    w -= w2;
	    delta -= w2;
	    if(delta <= 0) break;
	    pos++;
	    delta -= w;
	}
    }
#endif
//     kdDebug( 6040 ) << " Text  --> inside at position " << pos << endl;
    offset = pos;
    return SelectionPointInside;
}

// -----------------------------------------------------------------------------

TextSlaveArray::TextSlaveArray()
{
    setAutoDelete(true);
}

int TextSlaveArray::compareItems( Item d1, Item d2 )
{
    assert(d1);
    assert(d2);

    return static_cast<TextSlave*>(d1)->m_y - static_cast<TextSlave*>(d2)->m_y;
}

// remove this once QVector::bsearch is fixed
int TextSlaveArray::findFirstMatching(Item d) const
{
    int len = count();

    if ( !len )
	return -1;
    if ( !d )
	return -1;
    int n1 = 0;
    int n2 = len - 1;
    int mid = 0;
    bool found = FALSE;
    while ( n1 <= n2 ) {
	int  res;
	mid = (n1 + n2)/2;
	if ( (*this)[mid] == 0 )			// null item greater
	    res = -1;
	else
	    res = ((QGVector*)this)->compareItems( d, (*this)[mid] );
	if ( res < 0 )
	    n2 = mid - 1;
	else if ( res > 0 )
	    n1 = mid + 1;
	else {					// found it
	    found = TRUE;
	    break;
	}
    }
    /* if ( !found )
	return -1; */
    // search to first one equal or bigger
    while ( found && (mid > 0) && !((QGVector*)this)->compareItems(d, (*this)[mid-1]) )
	mid--;
    return mid;
}

// -------------------------------------------------------------------------------------

RenderText::RenderText(DOM::NodeImpl* node, DOMStringImpl *_str)
    : RenderObject(node)
{
    // init RenderObject attributes
    setRenderText();   // our object inherits from RenderText

    m_minWidth = -1;
    m_maxWidth = -1;
    str = _str;
    if(str) str->ref();
    KHTMLAssert(!str || !str->l || str->s);

    m_selectionState = SelectionNone;
    m_hasReturn = true;

#ifdef DEBUG_LAYOUT
    QConstString cstr(str->s, str->l);
    kdDebug( 6040 ) << "RenderText ctr( "<< cstr.string().length() << " )  '" << cstr.string() << "'" << endl;
#endif
}

void RenderText::setStyle(RenderStyle *_style)
{
    if ( style() != _style ) {
        // ### fontvariant being implemented as text-transform: upper. sucks!
        bool changedText = (!style() && (_style->fontVariant() != FVNORMAL || _style->textTransform() != TTNONE)) ||
            ((style() && style()->textTransform() != _style->textTransform()) ||
             (style() && style()->fontVariant() != _style->fontVariant()));

        RenderObject::setStyle( _style );
        m_lineHeight = RenderObject::lineHeight(false);

        if (changedText && element() && element()->string())
            setText(element()->string(), changedText);
    }
}

RenderText::~RenderText()
{
    deleteSlaves();
    if(str) str->deref();
}

void RenderText::deleteSlaves()
{
    // this is a slight variant of QArray::clear().
    // We don't delete the array itself here because its
    // likely to be used in the same size later again, saves
    // us resize() calls
    unsigned int len = m_lines.size();
    for(unsigned int i=0; i < len; i++)
        m_lines.remove(i);

    KHTMLAssert(m_lines.count() == 0);
}

TextSlave * RenderText::findTextSlave( int offset, int &pos )
{
    // The text slaves point to parts of the rendertext's str string
    // (they don't include '\n')
    // Find the text slave that includes the character at @p offset
    // and return pos, which is the position of the char in the slave.

    if ( m_lines.isEmpty() )
        return 0L;

    TextSlave* s = m_lines[0];
    uint si = 1;
    int off = s->m_len;
    while(offset > off && si < m_lines.count())
    {
        s = m_lines[si++];
        off = s->m_start + s->m_len;
    }
    // we are now in the correct text slave
    pos = (offset > off ? s->m_len : s->m_len - (off - offset) );
    return s;
}

bool RenderText::nodeAtPoint(NodeInfo& /*info*/, int _x, int _y, int _tx, int _ty)
{
    assert(parent());

    _tx -= paddingLeft() + borderLeft();
    _ty -= borderTop() + paddingTop();

    int height = m_lineHeight + borderTop() + paddingTop() +
                 borderBottom() + paddingBottom();

    bool inside = false;
    TextSlave *s = m_lines.count() ? m_lines[0] : 0;
    int si = 0;
    while(s) {
        if((_y >=_ty + s->m_y) && (_y < _ty + s->m_y + height) &&
           (_x >= _tx + s->m_x) && (_x <_tx + s->m_x + s->m_width) ) {
            inside = true;
            break;
        }

        s = si < (int) m_lines.count()-1 ? m_lines[++si] : 0;
    }

    setMouseInside(inside);

    return inside;
}

FindSelectionResult RenderText::checkSelectionPoint(int _x, int _y, int _tx, int _ty, DOM::NodeImpl*& node, int &offset)
{
//     kdDebug(6040) << "RenderText::checkSelectionPoint " << this << " _x=" << _x << " _y=" << _y
//                   << " _tx=" << _tx << " _ty=" << _ty << endl;
    TextSlave *lastPointAfterInline=0;

    for(unsigned int si = 0; si < m_lines.count(); si++)
    {
        TextSlave* s = m_lines[si];
        int result;
        const Font *f = htmlFont( si==0 );
        result = s->checkSelectionPoint(_x, _y, _tx, _ty, f, this, offset, m_lineHeight);

//         kdDebug(6040) << "RenderText::checkSelectionPoint " << this << " line " << si << " result=" << result << " offset=" << offset << endl;
        if ( result == SelectionPointInside ) // x,y is inside the textslave
        {
            offset += s->m_start; // add the offset from the previous lines
            //kdDebug(6040) << "RenderText::checkSelectionPoint inside -> " << offset << endl;
            node = element();
            return SelectionPointInside;
        } else if ( result == SelectionPointBefore ) {
            // x,y is before the textslave -> stop here
            if ( si > 0 && lastPointAfterInline ) {
                offset = lastPointAfterInline->m_start + lastPointAfterInline->m_len;
                //kdDebug(6040) << "RenderText::checkSelectionPoint before -> " << offset << endl;
                node = element();
                return SelectionPointInside;
            } else {
                offset = 0;
                //kdDebug(6040) << "RenderText::checkSelectionPoint " << this << "before us -> returning Before" << endl;
                node = element();
                return SelectionPointBefore;
            }
        } else if ( result == SelectionPointAfterInLine ) {
	    lastPointAfterInline = s;
	}

    }

    // set offset to max
    offset = str->l;
    //qDebug("setting node to %p", element());
    node = element();
    return SelectionPointAfter;
}

void RenderText::cursorPos(int offset, int &_x, int &_y, int &height)
{
  if (!m_lines.count()) {
    _x = _y = height = -1;
    return;
  }

  int pos;
  TextSlave * s = findTextSlave( offset, pos );
  _y = s->m_y;
  height = m_lineHeight; // ### firstLine!!! s->m_height;

  const QFontMetrics &fm = metrics( false ); // #### wrong for first-line!
  QString tekst(str->s + s->m_start, s->m_len);
  _x = s->m_x + (fm.boundingRect(tekst, pos)).right();
  if(pos)
      _x += fm.rightBearing( *(str->s + s->m_start + pos - 1 ) );

  int absx, absy;

  RenderObject *cb = containingBlock();

  if (cb && cb != this && cb->absolutePosition(absx,absy))
  {
    _x += absx;
    _y += absy;
  } else {
    // we don't know our absolute position, and there is not point returning
    // just a relative one
    _x = _y = -1;
  }
}

bool RenderText::absolutePosition(int &xPos, int &yPos, bool)
{
    return RenderObject::absolutePosition(xPos, yPos, false);

    if(parent() && parent()->absolutePosition(xPos, yPos, false)) {
        xPos -= paddingLeft() + borderLeft();
        yPos -= borderTop() + paddingTop();
        return true;
    }
    xPos = yPos = 0;
    return false;
}

void RenderText::posOfChar(int chr, int &x, int &y)
{
    if (!parent())
    {
       x = -1;
       y = -1;
       return;
    }
    parent()->absolutePosition( x, y, false );

    //if( chr > (int) str->l )
    //chr = str->l;

    int pos;
    TextSlave * s = findTextSlave( chr, pos );

    if ( s )
    {
        // s is the line containing the character
        x += s->m_x; // this is the x of the beginning of the line, but it's good enough for now
        y += s->m_y;
    }
}

int RenderText::rightmostPosition() const
{
    if (style()->whiteSpace() != NORMAL)
        return maxWidth();

    return 0;
}

void RenderText::printObject( QPainter *p, int /*x*/, int y, int /*w*/, int h,
                      int tx, int ty)
{
    int ow = style()->outlineWidth();
    RenderStyle* pseudoStyle = style()->getPseudoStyle(RenderStyle::FIRST_LINE);
    int d = style()->textDecoration();
    TextSlave f(0, y-ty);
    int si = m_lines.findFirstMatching(&f);
    // something matching found, find the first one to print
    bool isPrinting = (p->device()->devType() == QInternal::Printer);
    if(si >= 0)
    {
        // Move up until out of area to be printed
        while(si > 0 && m_lines[si-1]->checkVerticalPoint(y, ty, h, m_lineHeight))
            si--;

        // Now calculate startPos and endPos, for printing selection.
        // We print selection while endPos > 0
        int endPos, startPos;
        if (!isPrinting && (selectionState() != SelectionNone)) {
            if (selectionState() == SelectionInside) {
                //kdDebug(6040) << this << " SelectionInside -> 0 to end" << endl;
                startPos = 0;
                endPos = str->l;
            } else {
                selectionStartEnd(startPos, endPos);
                if(selectionState() == SelectionStart)
                    endPos = str->l;
                else if(selectionState() == SelectionEnd)
                    startPos = 0;
            }
            //kdDebug(6040) << this << " Selection from " << startPos << " to " << endPos << endl;
        }

        TextSlave* s;
        int minx =  1000000;
        int maxx = -1000000;
        int outlinebox_y = m_lines[si]->m_y;
	QPtrList <QRect> linerects;
        linerects.setAutoDelete(true);
        linerects.append(new QRect());

	bool renderOutline = style()->outlineWidth()!=0;

	const Font *font = &style()->htmlFont();

#if APPLE_CHANGES
        // Do one pass for the selection, then another for the rest.
        bool haveSelection = startPos != endPos && !isPrinting && selectionState() != SelectionNone;
        int startLine = si;
        for (int pass = 0; pass < (haveSelection ? 2 : 1); pass++) {
            si = startLine;
            
            bool drawDecorations = !haveSelection || pass == 0;
            bool drawSelectionBackground = haveSelection && pass == 0;
            bool drawText = !haveSelection || pass == 1;
#endif

        // run until we find one that is outside the range, then we
        // know we can stop
        do {
            s = m_lines[si];

	    if (isPrinting)
	    {
                int lh = lineHeight( false ) + paddingBottom() + borderBottom();
                if (ty+s->m_y < y)
                {
                   // This has been printed already we suppose.
                   continue;
                }

                if (ty+lh+s->m_y > y+h)
                {
                   RenderRoot *rootObj = root();
                   if (ty+s->m_y < rootObj->truncatedAt())
                      rootObj->setTruncatedAt(ty+s->m_y);
                   // Let's stop here.
                   break;
                }
            }

            RenderStyle* _style = pseudoStyle && s->m_firstLine ? pseudoStyle : style();

            if(_style->font() != p->font()) {
                p->setFont(_style->font());
		font = &_style->htmlFont();
	    }

#if APPLE_CHANGES
            if (drawDecorations)
#endif
            if((hasSpecialObjects()  &&
                (parent()->isInline() || pseudoStyle)) &&
               (!pseudoStyle || s->m_firstLine))
                s->printBoxDecorations(p, _style, this, tx, ty, si == 0, si == (int)m_lines.count()-1);


#if APPLE_CHANGES
            if (drawText) {
#endif

            if(_style->color() != p->pen().color())
                p->setPen(_style->color());

	    if (s->m_len > 0)
		font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline, str->s, str->l, s->m_start, s->m_len,
			       s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR);

            if(d != TDNONE)
            {
                p->setPen(_style->textDecorationColor());
                s->printDecoration(p, this, tx, ty, d, si == 0, si == ( int ) m_lines.count()-1);
            }

#if APPLE_CHANGES
            } // drawText
#endif

#if APPLE_CHANGES
            if (drawSelectionBackground)
#endif
            if (!isPrinting && (selectionState() != SelectionNone))
            {
		int offset = s->m_start;
		int sPos = QMAX( startPos - offset, 0 );
		int ePos = QMIN( endPos - offset, s->m_len );
                //kdDebug(6040) << this << " printSelection with startPos=" << sPos << " endPos=" << ePos << endl;
		if ( sPos < ePos )
		    s->printSelection(font, this, p, _style, tx, ty, sPos, ePos);

            }

#if APPLE_CHANGES
            if (drawText)
#endif
            if(renderOutline) {
                if(outlinebox_y == s->m_y) {
                    if(minx > s->m_x)  minx = s->m_x;
                    int newmaxx = s->m_x+s->m_width;
                    //if (parent()->isInline() && si==0) newmaxx-=paddingLeft();
                    if (parent()->isInline() && si==int(m_lines.count())-1) newmaxx-=paddingRight();
                    if(maxx < newmaxx) maxx = newmaxx;
                }
                else {
                    QRect *curLine = new QRect(minx, outlinebox_y, maxx-minx, m_lineHeight);
                    linerects.append(curLine);

                    outlinebox_y = s->m_y;
                    minx = s->m_x;
                    maxx = s->m_x+s->m_width;
                    //if (parent()->isInline() && si==0) maxx-=paddingLeft();
                    if (parent()->isInline() && si==int(m_lines.count())-1) maxx-=paddingRight();
                }
            }
#ifdef BIDI_DEBUG
            {
                int h = lineHeight( false ) + paddingTop() + paddingBottom() + borderTop() + borderBottom();
                QColor c2 = QColor("#0000ff");
                drawBorder(p, tx, ty, tx+1, ty + h,
                              RenderObject::BSLeft, c2, c2, SOLID, 1, 1);
                drawBorder(p, tx + s->m_width, ty, tx + s->m_width + 1, ty + h,
                              RenderObject::BSRight, c2, c2, SOLID, 1, 1);
            }
#endif

        } while (++si < (int)m_lines.count() && m_lines[si]->checkVerticalPoint(y-ow, ty, h, m_lineHeight));

#if APPLE_CHANGES
        } // end of for loop
#endif

        if(renderOutline)
	  {
	    linerects.append(new QRect(minx, outlinebox_y, maxx-minx, m_lineHeight));
	    linerects.append(new QRect());
	    for (unsigned int i = 1; i < linerects.count() - 1; i++)
                printTextOutline(p, tx, ty, *linerects.at(i-1), *linerects.at(i), *linerects.at(i+1));
	  }
    }
}

void RenderText::print( QPainter *p, int x, int y, int w, int h,
                      int tx, int ty)
{
    if (style()->visibility() != VISIBLE) return;

    int s = m_lines.count() - 1;
    if ( s < 0 ) return;

    // ### incorporate padding/border here!
    if ( ty + m_lines[0]->m_y > y + h + 64 ) return;
    if ( ty + m_lines[s]->m_y + m_lines[s]->m_baseline + m_lineHeight + 64 < y ) return;

    printObject(p, x, y, w, h, tx, ty);
}

void RenderText::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    // ### calc Min and Max width...
    m_minWidth = 0;
    m_maxWidth = 0;

    int currMinWidth = 0;
    int currMaxWidth = 0;
    m_hasReturn = false;
    m_hasBreakableChar = false;

    // ### not 100% correct for first-line
    const Font *f = htmlFont( false );
    int len = str->l;
    if ( len == 1 && str->s->latin1() == '\n' )
	m_hasReturn = true;
    for(int i = 0; i < len; i++)
    {
        int wordlen = 0;
        while( i+wordlen < len && !(isBreakable( str->s, i+wordlen, str->l )) )
            wordlen++;
        if (wordlen)
        {
            int w = f->width(str->s, str->l, i, wordlen);
            currMinWidth += w;
            currMaxWidth += w;
        }
        if(i+wordlen < len)
        {
	    m_hasBreakableChar = true;
            if ( (*(str->s+i+wordlen)).latin1() == '\n' )
            {
		m_hasReturn = true;
                if(currMinWidth > m_minWidth) m_minWidth = currMinWidth;
                currMinWidth = 0;
                if(currMaxWidth > m_maxWidth) m_maxWidth = currMaxWidth;
                currMaxWidth = 0;
            }
            else
            {
                if(currMinWidth > m_minWidth) m_minWidth = currMinWidth;
                currMinWidth = 0;
                currMaxWidth += f->width( str->s, str->l, i + wordlen );
            }
            /* else if( c == '-')
            {
                currMinWidth += minus_width;
                if(currMinWidth > m_minWidth) m_minWidth = currMinWidth;
                currMinWidth = 0;
                currMaxWidth += minus_width;
            }*/
        }
        i += wordlen;
    }
    if(currMinWidth > m_minWidth) m_minWidth = currMinWidth;
    if(currMaxWidth > m_maxWidth) m_maxWidth = currMaxWidth;

    if (style()->whiteSpace() == NOWRAP)
        m_minWidth = m_maxWidth;

    setMinMaxKnown();
    //kdDebug( 6040 ) << "Text::calcMinMaxWidth(): min = " << m_minWidth << " max = " << m_maxWidth << endl;
}

int RenderText::minXPos() const
{
    if (!m_lines.count())
	return 0;
    int retval=6666666;
    for (unsigned i=0;i < m_lines.count(); i++)
    {
	retval = QMIN ( retval, m_lines[i]->m_x);
    }
    return retval;
}

int RenderText::xPos() const
{
    if (m_lines.count())
	return m_lines[0]->m_x;
    else
	return 0;
}

int RenderText::yPos() const
{
    if (m_lines.count())
        return m_lines[0]->m_y;
    else
        return 0;
}

const QFont &RenderText::font()
{
    return style()->font();
}

void RenderText::setText(DOMStringImpl *text, bool force)
{
#if APPLE_CHANGES
    if (!text)
        return;
#endif
    if( !force && str == text ) return;
    if(str) str->deref();
    str = text;

    if ( str && style() ) {
        if ( style()->fontVariant() == SMALL_CAPS )
            str = str->upper();
        else
            switch(style()->textTransform()) {
            case CAPITALIZE:   str = str->capitalize();  break;
            case UPPERCASE:   str = str->upper();       break;
            case LOWERCASE:  str = str->lower();       break;
            case NONE:
            default:;
            }
        str->ref();
    }

    // ### what should happen if we change the text of a
    // RenderBR object ?
    KHTMLAssert(!isBR() || (str->l == 1 && (*str->s) == '\n'));
    KHTMLAssert(!str->l || str->s);

    setLayouted(false);
#ifdef BIDI_DEBUG
    QConstString cstr(str->s, str->l);
    kdDebug( 6040 ) << "RenderText::setText( " << cstr.string().length() << " ) '" << cstr.string() << "'" << endl;
#endif
}

int RenderText::height() const
{
    int retval;
    if ( m_lines.count() )
        retval = m_lines[m_lines.count()-1]->m_y + m_lineHeight - m_lines[0]->m_y;
    else
        retval = metrics( false ).height();

    retval += paddingTop() + paddingBottom() + borderTop() + borderBottom();

    return retval;
}

short RenderText::lineHeight( bool firstLine ) const
{
    if ( firstLine )
 	return RenderObject::lineHeight( firstLine );

    return m_lineHeight;
}

short RenderText::baselinePosition( bool firstLine ) const
{
    const QFontMetrics &fm = metrics( firstLine );
    return fm.ascent() +
        ( lineHeight( firstLine ) - fm.height() ) / 2;
}

void RenderText::position(int x, int y, int from, int len, int width, bool reverse, bool firstLine, int spaceAdd)
{
    // ### should not be needed!!!
    assert(!(len == 0 || (str->l && len == 1 && *(str->s+from) == '\n') ));

    reverse = reverse && !style()->visuallyOrdered();

    // ### margins and RTL
    if(from == 0 && parent()->isInline() && parent()->firstChild()==this)
    {
        x += paddingLeft() + borderLeft() + marginLeft();
        width -= marginLeft();
    }

    if(from + len >= int(str->l) && parent()->isInline() && parent()->lastChild()==this)
        width -= marginRight();

#ifdef DEBUG_LAYOUT
    QChar *ch = str->s+from;
    QConstString cstr(ch, len);
    qDebug("setting slave text to *%s*, len=%d, w)=%d" , cstr.string().latin1(), len, width );//" << y << ")" << " height=" << lineHeight(false) << " fontHeight=" << metrics(false).height() << " ascent =" << metrics(false).ascent() << endl;
#endif

    TextSlave *s = new TextSlave(x, y, from, len,
                                 baselinePosition( firstLine ),
                                 width+spaceAdd, reverse, spaceAdd, firstLine);

    if(m_lines.count() == m_lines.size())
        m_lines.resize(m_lines.size()*2+1);

    m_lines.insert(m_lines.count(), s);
}

unsigned int RenderText::width(unsigned int from, unsigned int len, bool firstLine) const
{
    if(!str->s || from > str->l ) return 0;
    if ( from + len > str->l ) len = str->l - from;

    const Font *f = htmlFont( firstLine );
    return width( from, len, f );
}

unsigned int RenderText::width(unsigned int from, unsigned int len, const Font *f) const
{
    if(!str->s || from > str->l ) return 0;
    if ( from + len > str->l ) len = str->l - from;

    int w;
    if ( f == &style()->htmlFont() && from == 0 && len == str->l )
 	 w = m_maxWidth;
    else
	w = f->width(str->s, str->l, from, len );

    // ### add margins and support for RTL

    if(parent()->isInline())
    {
        if(from == 0 && parent()->firstChild() == static_cast<const RenderObject*>(this))
            w += borderLeft() + paddingLeft() + marginLeft();
        if(from + len == str->l &&
           parent()->lastChild() == static_cast<const RenderObject*>(this))
            w += borderRight() + paddingRight() +marginRight();
    }

    //kdDebug( 6040 ) << "RenderText::width(" << from << ", " << len << ") = " << w << endl;
    return w;
}

short RenderText::width() const
{
    int w;
    int minx = 100000000;
    int maxx = 0;
    // slooow
    for(unsigned int si = 0; si < m_lines.count(); si++) {
        TextSlave* s = m_lines[si];
        if(s->m_x < minx)
            minx = s->m_x;
        if(s->m_x + s->m_width > maxx)
            maxx = s->m_x + s->m_width;
    }

    w = QMAX(0, maxx-minx);

    if(parent()->isInline())
    {
        if(parent()->firstChild() == static_cast<const RenderObject*>(this))
            w += borderLeft() + paddingLeft();
        if(parent()->lastChild() == static_cast<const RenderObject*>(this))
            w += borderRight() + paddingRight();
    }

    return w;
}

void RenderText::repaint()
{
    RenderObject *cb = containingBlock();
    if(cb != this)
        cb->repaint();
}

bool RenderText::isFixedWidthFont() const
{
    return QFontInfo(style()->font()).fixedPitch();
}

short RenderText::verticalPositionHint( bool firstLine ) const
{
    return parent()->verticalPositionHint( firstLine );
}

const QFontMetrics &RenderText::metrics(bool firstLine) const
{
    if( firstLine && hasFirstLine() ) {
	RenderStyle *pseudoStyle  = style()->getPseudoStyle(RenderStyle::FIRST_LINE);
	if ( pseudoStyle )
	    return pseudoStyle->fontMetrics();
    }
    return style()->fontMetrics();
}

const Font *RenderText::htmlFont(bool firstLine) const
{
    const Font *f = 0;
    if( firstLine && hasFirstLine() ) {
	RenderStyle *pseudoStyle  = style()->getPseudoStyle(RenderStyle::FIRST_LINE);
	if ( pseudoStyle )
	    f = &pseudoStyle->htmlFont();
    } else {
	f = &style()->htmlFont();
    }
    return f;
}

void RenderText::printTextOutline(QPainter *p, int tx, int ty, const QRect &lastline, const QRect &thisline, const QRect &nextline)
{
  int ow = style()->outlineWidth();
  EBorderStyle os = style()->outlineStyle();
  QColor oc = style()->outlineColor();

  int t = ty + thisline.top();
  int l = tx + thisline.left();
  int b = ty + thisline.bottom() + 1;
  int r = tx + thisline.right() + 1;

  // left edge
  drawBorder(p,
	     l - ow,
	     t - (lastline.isEmpty() || thisline.left() < lastline.left() || lastline.right() <= thisline.left() ? ow : 0),
	     l,
	     b + (nextline.isEmpty() || thisline.left() <= nextline.left() || nextline.right() <= thisline.left() ? ow : 0),
	     BSLeft,
	     oc, style()->color(), os,
	     (lastline.isEmpty() || thisline.left() < lastline.left() || lastline.right() <= thisline.left() ? ow : -ow),
	     (nextline.isEmpty() || thisline.left() <= nextline.left() || nextline.right() <= thisline.left() ? ow : -ow),
	     true);

  // right edge
  drawBorder(p,
	     r,
	     t - (lastline.isEmpty() || lastline.right() < thisline.right() || thisline.right() <= lastline.left() ? ow : 0),
	     r + ow,
	     b + (nextline.isEmpty() || nextline.right() <= thisline.right() || thisline.right() <= nextline.left() ? ow : 0),
	     BSRight,
	     oc, style()->color(), os,
	     (lastline.isEmpty() || lastline.right() < thisline.right() || thisline.right() <= lastline.left() ? ow : -ow),
	     (nextline.isEmpty() || nextline.right() <= thisline.right() || thisline.right() <= nextline.left() ? ow : -ow),
	     true);
  // upper edge
  if ( thisline.left() < lastline.left())
      drawBorder(p,
		 l - ow,
		 t - ow,
		 QMIN(r+ow, (lastline.isValid()? tx+lastline.left() : 1000000)),
		 t ,
		 BSTop, oc, style()->color(), os,
		 ow,
		 (lastline.isValid() && tx+lastline.left()+1<r+ow ? -ow : ow),
		 true);

  if (lastline.right() < thisline.right())
      drawBorder(p,
		 QMAX(lastline.isValid()?tx + lastline.right() + 1:-1000000, l - ow),
		 t - ow,
		 r + ow,
		 t ,
		 BSTop, oc, style()->color(), os,
		 (lastline.isValid() && l-ow < tx+lastline.right()+1 ? -ow : ow),
		 ow,
		 true);

  // lower edge
  if ( thisline.left() < nextline.left())
      drawBorder(p,
		 l - ow,
		 b,
		 QMIN(r+ow, nextline.isValid()? tx+nextline.left()+1 : 1000000),
		 b + ow,
		 BSBottom, oc, style()->color(), os,
		 ow,
		 (nextline.isValid() && tx+nextline.left()+1<r+ow? -ow : ow),
		 true);

  if (nextline.right() < thisline.right())
      drawBorder(p,
		 QMAX(nextline.isValid()?tx+nextline.right()+1:-1000000 , l-ow),
		 b,
		 r + ow,
		 b + ow,
		 BSBottom, oc, style()->color(), os,
		 (nextline.isValid() && l-ow < tx+nextline.right()+1? -ow : ow),
		 ow,
		 true);
}

#undef BIDI_DEBUG
#undef DEBUG_LAYOUT
