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
 * $Id$
 */
//#define DEBUG_LAYOUT
//#define BIDI_DEBUG

#include "render_text.h"
#include "break_lines.h"
#include "render_style.h"

#include "misc/loader.h"
#include "misc/helper.h"

#include <qfontmetrics.h>
#include <qfontinfo.h>
#include <qfont.h>
#include <qpainter.h>
#include <qstring.h>
#include <qcolor.h>
#include <qrect.h>
#include <kdebug.h>

#define QT_ALLOC_QCHAR_VEC( N ) (QChar*) new char[ sizeof(QChar)*( N ) ]
#define QT_DELETE_QCHAR_VEC( P ) delete[] ((char*)( P ))

using namespace khtml;
using namespace DOM;

TextSlave::~TextSlave()
{
    if(m_reversed)
        QT_DELETE_QCHAR_VEC(m_text);
}

void TextSlave::print( QPainter *p, int _tx, int _ty)
{
    if (!m_text || m_len <= 0)
        return;

    QConstString s(m_text, m_len);
    //kdDebug( 6040 ) << "textSlave::printing(" << s.string() << ") at(" << x+_tx << "/" << y+_ty << ")" << endl;
    p->drawText(m_x + _tx, m_y + _ty + m_baseline, s.string());
}

void TextSlave::printSelection(QPainter *p, RenderStyle* style, int tx, int ty, int startPos, int endPos)
{
    if(startPos > m_len) return;
    if(startPos < 0) startPos = 0;

    int _len = m_len;
    int _width = m_width;
    if(endPos > 0 && endPos < m_len) {
        _len = endPos;
    }
    _len -= startPos;

    //kdDebug(6040) << "TextSlave::printSelection startPos (relative)=" << startPos << " len (of selection)=" << _len << "  (m_len=" << m_len << ")" << endl;
    QConstString s(m_text+startPos , _len);

    if (_len != m_len)
        _width = p->fontMetrics().width(s.string());

    int _offset = 0;
    if ( startPos > 0 ) {
        QConstString aStr(m_text, startPos);
        _offset = p->fontMetrics().width(aStr.string());
    }

    p->save();
    QColor c = style->color();
    p->setPen(QColor(0xff-c.red(),0xff-c.green(),0xff-c.blue()));
    QFontMetrics fm = p->fontMetrics();
    p->fillRect(m_x + tx + _offset, m_y + ty + m_baseline - fm.ascent(), _width, fm.height(), c);

    ty += m_baseline;

    //kdDebug( 6040 ) << "textSlave::printing(" << s.string() << ") at(" << x+_tx << "/" << y+_ty << ")" << endl;
    p->drawText(m_x + tx + _offset, m_y + ty, s.string());
    p->restore();
}

void TextSlave::printDecoration( QPainter *pt, RenderText* p, int _tx, int _ty, int deco, bool begin, bool end)
{
    _tx += m_x;
    _ty += m_y;

    int width = m_width;

    if( begin )
 	    width -= p->paddingLeft() + p->borderLeft();

    if ( end )
        width -= p->paddingRight() + p->borderRight();

    int underlineOffset = ( pt->fontMetrics().height() + m_baseline ) / 2;
    if(underlineOffset <= m_baseline) underlineOffset = m_baseline+1;

    // FIXME: [kocienda]
    // This hack gets the underline in a better place but we should be
    // doing something better.
    underlineOffset+=2;

    if(deco & UNDERLINE){
        //fprintf (stderr, "UNDERLINE (%d, %d) to (%d, %d)\n", _tx, _ty + underlineOffset, _tx + width, _ty + underlineOffset );
        pt->drawLine(_tx, _ty + underlineOffset, _tx + width, _ty + underlineOffset );
    }
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

    _tx += m_x;
    _ty += m_y + m_baseline - pt->fontMetrics().ascent() - topExtra;

    int width = m_width;

    // the height of the decorations is:  topBorder + topPadding + fm.height() + bottomPadding + bottomBorder
    int height = pt->fontMetrics().height() + topExtra + bottomExtra;

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

    if(style->hasBorder())
        p->printBorder(pt, _tx, _ty, width, height, style, begin, end);

#ifdef BIDI_DEBUG
    int h = p->lineHeight( false ) + p->paddingTop() + p->paddingBottom() + p->borderTop() + p->borderBottom();
    QColor c2 = QColor("#0000ff");
    p->drawBorder(pt, _tx, _ty, _tx, _ty + h, 1,
                  RenderObject::BSLeft, c2, c2, SOLID, false, false, 0, 0);
    p->drawBorder(pt, _tx + m_width, _ty, _tx + m_width, _ty + h, 1,
                  RenderObject::BSRight, c2, c2, SOLID, false, false, 0, 0);
#endif
}

bool TextSlave::checkPoint(int _x, int _y, int _tx, int _ty, int height)
{
    if((_ty + m_y > _y) || (_ty + m_y + height < _y) ||
       (_tx + m_x > _x) || (_tx + m_x + m_width < _x))
        return false;
    return true;
}

FindSelectionResult TextSlave::checkSelectionPoint(int _x, int _y, int _tx, int _ty, QFontMetrics * fm, int & offset, int lineHeight)
{
    //kdDebug(6040) << "TextSlave::checkSelectionPoint " << this << " _x=" << _x << " _y=" << _y
    //              << " _tx+m_x=" << _tx+m_x << " _ty+m_y=" << _ty+m_y << endl;
    offset = 0;

    if ( _y < _ty + m_y )
        return SelectionPointBefore; // above -> before

    if ( _y > _ty + m_y + lineHeight || _x > _tx + m_x + m_width )
    {
        // below or on the right -> after
        // Set the offset to the max
        offset = m_len;
        return SelectionPointAfter;
    }

    // The Y matches, check if we're on the left
    if ( _x < _tx + m_x )
        return SelectionPointBefore; // on the left (and not below) -> before

    if ( m_reversed )
        return SelectionPointBefore; // Abort if RTL (TODO)

    int delta = _x - (_tx + m_x);
    //kdDebug(6040) << "TextSlave::checkSelectionPoint delta=" << delta << endl;
    int pos = 0;
    while(pos < m_len)
    {
        // ### this will produce wrong results for RTL text!!!
        int w = fm->width(*(m_text+pos));
        int w2 = w/2;
        w -= w2;
        delta -= w2;
        if(delta <= 0) break;
        pos++;
        delta -= w;
    }
    //kdDebug( 6040 ) << " Text  --> inside at position " << pos << endl;
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
    ASSERT(d1);
    ASSERT(d2);

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

RenderText::RenderText(DOMStringImpl *_str)
    : RenderObject()
{
    // init RenderObject attributes
    setRenderText();   // our object inherits from RenderText
    setInline(true);   // our object is Inline

    m_minWidth = -1;
    m_maxWidth = -1;
    str = _str;
    if(str) str->ref();
    assert(!str || !str->l || str->s);

    m_selectionState = SelectionNone;
    m_hasReturn = true;
    fm = 0;

#ifdef DEBUG_LAYOUT
    QConstString cstr(str->s, str->l);
    kdDebug( 6040 ) << "RenderText::setText '" << (const char *)cstr.string().utf8() << "'" << endl;
#endif
}

void RenderText::setStyle(RenderStyle *_style)
{
    bool fontchanged = ( !style() || style()->font() != _style->font() );
    RenderObject::setStyle(_style);
    if ( !fm || fontchanged ) {
        delete fm;
        fm = new QFontMetrics( style()->font() );
    }
    m_lineHeight = style()->lineHeight().width(metrics( false ).height());

    if(m_lineHeight<=0)
        m_lineHeight = metrics( false ).height();

    if ( style()->fontVariant() == SMALL_CAPS ) {
	setText( str->upper() );
    } else {
	// ### does not work if texttransform is set to None again!
	switch(style()->textTransform()) {
	    case CAPITALIZE:  setText(str->capitalize());  break;
	    case UPPERCASE:   setText(str->upper());       break;
	    case LOWERCASE:   setText(str->lower());       break;
	    case NONE:
	    default:;
	}
    }
}

RenderText::~RenderText()
{
    deleteSlaves();
    if(str) str->deref();
    delete fm;
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

    ASSERT(m_lines.count() == 0);
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
    uint si = 0;
    int off = s->m_len;
    while(offset > off && si < m_lines.count())
    {
        s = m_lines[++si];
        if ( s->m_reversed )
            return 0L; // Abort if RTL (TODO)
        // ### only for visuallyOrdered !
        off = s->m_text - str->s + s->m_len;
    }
    // we are now in the correct text slave
    pos = (offset > off ? s->m_len : s->m_len - (off - offset) );
    return s;
}

bool RenderText::checkPoint(int _x, int _y, int _tx, int _ty)
{
    TextSlave *s = m_lines.count() ? m_lines[0] : 0;
    int si = 0;
    while(s)
    {
        if( s->checkPoint(_x, _y, _tx, _ty, m_lineHeight) )
            return true;
        // ### this might be wrong, if combining chars are used ( eg arabic )
        s = si < (int)m_lines.count()-1 ? m_lines[++si] : 0;
    }
    return false;
}

FindSelectionResult RenderText::checkSelectionPoint(int _x, int _y, int _tx, int _ty, int &offset)
{
    //kdDebug(6040) << "RenderText::checkSelectionPoint " << this << " _x=" << _x << " _y=" << _y
    //              << " _tx=" << _tx << " _ty=" << _ty << endl;
    for(unsigned int si = 0; si < m_lines.count(); si++)
    {
        TextSlave* s = m_lines[si];
        if ( s->m_reversed )
            return SelectionPointBefore; // abort if RTL (TODO)
        int result;
	if ( khtml::printpainter || hasFirstLine() ) {
	    QFontMetrics _fm = metrics( ( si == 0) );
	    result = s->checkSelectionPoint(_x, _y, _tx, _ty, &_fm, offset, m_lineHeight);
	} else {
	    result = s->checkSelectionPoint(_x, _y, _tx, _ty, fm, offset, m_lineHeight);
	}
        //kdDebug(6040) << "RenderText::checkSelectionPoint " << this << " line " << si << " result=" << result << " offset=" << offset << endl;
        if ( result == SelectionPointInside ) // x,y is inside the textslave
        {
            // ### only for visuallyOrdered !
            offset += s->m_text - str->s; // add the offset from the previous lines
            //kdDebug(6040) << "RenderText::checkSelectionPoint inside -> " << offset << endl;
            return SelectionPointInside;
        } else if ( result == SelectionPointBefore )
        {
            // x,y is before the textslave -> stop here
            if ( si > 0 )
            {
                // ### only for visuallyOrdered !
                offset = s->m_text - str->s - 1;
                //kdDebug(6040) << "RenderText::checkSelectionPoint before -> " << offset << endl;
                return SelectionPointInside;
            } else
            {
                offset = 0;
                //kdDebug(6040) << "RenderText::checkSelectionPoint before us -> returning Before" << endl;
                return SelectionPointBefore;
            }
        }
    }

    // set offset to max
    offset = str->l;
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

  QFontMetrics fm = metrics( false ); // #### wrong for first-line!
  QString tekst(s->m_text, s->m_len);
  _x = s->m_x + (fm.boundingRect(tekst, pos)).right();
  if(pos)
      _x += fm.rightBearing( *(s->m_text + pos - 1 ) );

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
    if(parent() && parent()->absolutePosition(xPos, yPos, false)) {
        if ( m_lines.count() ) {
            TextSlave* s = m_lines[0];
            xPos += s->m_x;
            yPos += s->m_y;
        }
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

void RenderText::printObject( QPainter *p, int /*x*/, int y, int /*w*/, int h,
                      int tx, int ty)
{
    RenderStyle* pseudoStyle = style()->getPseudoStyle(RenderStyle::FIRST_LINE);
    int d = style()->textDecoration();
    TextSlave f(0, y-ty);
    int si = m_lines.findFirstMatching(&f);
    // something matching found, find the first one to print
    if(si >= 0)
    {
        // Move up until out of area to be printed
        while(si > 0 && m_lines[si-1]->checkVerticalPoint(y, ty, h, m_lineHeight))
            si--;

        //QConstString cstr(str->s, str->l);
        //kdDebug(6040) << this << " RenderText text '" << (const char *)cstr.string().utf8() << "'" << endl;
        //kdDebug(6040) << this << " RenderText::printObject y=" << y << " ty=" << ty << " h=" << h << " first line is " << si << endl;

        // Now calculate startPos and endPos, for printing selection.
        // We print selection while endPos > 0
        int endPos, startPos;
        if (selectionState() != SelectionNone)
        {
            if (selectionState() == SelectionInside)
            {
                //kdDebug(6040) << this << " SelectionInside -> 0 to end" << endl;
                startPos = 0;
                endPos = str->l;
            }
            else
            {
                selectionStartEnd(startPos, endPos);
                if(selectionState() == SelectionStart)
                    endPos = str->l;
                else if(selectionState() == SelectionEnd)
                    startPos = 0;
            }
            //kdDebug(6040) << this << " Selection from " << startPos << " to " << endPos << endl;

            // Eat the lines we don't print (startPos and endPos are from line 0!)
            // Note that we do this even if si==0, because we may have \n as the first char,
            // and this takes care of it too (David)
            if ( m_lines[si]->m_reversed )
                endPos = -1; // No selection if RTL (TODO)
            else
            {
                // ## Only valid for visuallyOrdered
                int len = m_lines[si]->m_text - str->s;
                //kdDebug(6040) << this << " RenderText::printObject adjustement si=" << si << " len=" << len << endl;
                startPos -= len;
                endPos -= len;
            }
        }

        TextSlave* s;
        int minx =  1000000;
        int maxx = -1000000;
        int outlinebox_y = m_lines[si]->m_y;

        RenderStyle* outlineStyle = 0;
        if (!outlineStyle && style()->outlineWidth())
            outlineStyle = style();

        // run until we find one that is outside the range, then we
        // know we can stop
        do {
            s = m_lines[si];
            RenderStyle* _style = pseudoStyle && s->m_firstLine ? pseudoStyle : style();

            if(_style->font() != p->font())
                p->setFont(_style->font());

            if((hasSpecialObjects()  &&
                (parent()->isInline() || pseudoStyle)) &&
               (!pseudoStyle || s->m_firstLine))
                s->printBoxDecorations(p, _style, this, tx, ty, si == 0, si == (int)m_lines.count()-1);

            if(_style->color() != p->pen().color())
                p->setPen(_style->color());

            s->print(p, tx, ty);

            if(d != TDNONE)
            {
                p->setPen(_style->textDecorationColor());
                s->printDecoration(p, this, tx, ty, d, si == 0, si == ( int ) m_lines.count()-1);
            }

            if (selectionState() != SelectionNone && endPos > 0)
            {
                //kdDebug(6040) << this << " printSelection with startPos=" << startPos << " endPos=" << endPos << endl;
                s->printSelection(p, _style, tx, ty, startPos, endPos);

                int diff;
                if(si < (int)m_lines.count()-1)
                    // ### only for visuallyOrdered ! (we disabled endPos for RTL, so we can't go here in RTL mode)
                    diff = m_lines[si+1]->m_text - s->m_text;
                else
                    diff = s->m_len;
                //kdDebug(6040) << this << " RenderText::printSelection eating the line si=" << si << " length=" << diff << endl;
                endPos -= diff;
                startPos -= diff;
                //kdDebug(6040) << this << " startPos now " << startPos << ", endPos now " << endPos << endl;
            }
            if(outlineStyle) {
                if(outlinebox_y == s->m_y) {
                    if(minx > s->m_x)  minx = s->m_x;
                    if(maxx < s->m_x+s->m_width) maxx = s->m_x+s->m_width;
                }
                else {
                    printOutline(p, tx+minx, ty+outlinebox_y, maxx-minx, m_lineHeight, outlineStyle);
                    outlinebox_y = s->m_y;
                    minx = s->m_x;
                    maxx = s->m_x+s->m_width;
                }
            }
        } while (++si < (int)m_lines.count() && m_lines[si]->checkVerticalPoint(y, ty, h, m_lineHeight));

        if(outlineStyle)
            printOutline(p, tx+minx, ty+outlinebox_y, maxx-minx, m_lineHeight, outlineStyle);
    }
}

void RenderText::print( QPainter *p, int x, int y, int w, int h,
                      int tx, int ty)
{
    //if ( !isVisible() )
    //    return;

    int s = m_lines.count() - 1;
    if ( s < 0 ) return;
   if ( ty + m_lines[0]->m_y > y + h ) return;
   if ( ty + m_lines[s]->m_y + m_lines[s]->m_baseline + m_lineHeight < y ) return;

    printObject(p, x, y, w, h, tx, ty);
}

void RenderText::calcMinMaxWidth()
{
    //kdDebug( 6040 ) << "Text::calcMinMaxWidth(): known=" << minMaxKnown() << endl;
    if(minMaxKnown()) return;

    // ### calc Min and Max width...
    m_minWidth = 0;
    m_maxWidth = 0;

    int currMinWidth = 0;
    int currMaxWidth = 0;
    m_hasReturn = false;
    m_hasBreakableChar = false;

    // ### not 100% correct for first-line
    QFontMetrics _fm = khtml::printpainter ? metrics( false ) : *fm;
    int len = str->l;
    if ( len == 1 && str->s->latin1() == '\n' )
	m_hasReturn = true;
    for(int i = 0; i < len; i++)
    {
        int wordlen = 0;
        do {
            wordlen++;
        } while( i+wordlen < len && !(isBreakable( str->s, i+wordlen, str->l )) );
        if (wordlen)
        {
            int w = _fm.width(QConstString(str->s+i, wordlen).string());
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
                currMaxWidth += _fm.width( *(str->s+i+wordlen) );
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
    setMinMaxKnown();
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
    return parent()->style()->font();
}

void RenderText::setText(DOMStringImpl *text)
{
    if( str == text ) return;
    if(str) str->deref();
    str = text;
    if(str) str->ref();

    // ### what should happen if we change the text of a
    // RenderBR object ?
    assert(!isBR() || (str->l == 1 && (*str->s) == '\n'));
    assert(!str->l || str->s);

    setLayouted(false);
    RenderObject* cb = containingBlock();
    if ( cb != this )
    {
        cb->setLayouted(false);
        cb->layout();
    }
#ifdef DEBUG_LAYOUT
    QConstString cstr(str->s, str->l);
    kdDebug( 6040 ) << "RenderText::setText '" << (const char *)cstr.string().utf8() << "'" << endl;
#endif
}

int RenderText::height() const
{
    // ### does this make sense??
    int retval = metrics( false ).height() + paddingTop() + paddingBottom() + borderTop() + borderBottom();
    if (m_lines.count())
	retval += m_lineHeight;
    return retval;
}

int RenderText::lineHeight( bool firstLine ) const
{
    if ( firstLine ) 
	return RenderObject::lineHeight( firstLine );
    return m_lineHeight;
}

// #### fix for printpainter and :first-line needed
short RenderText::baselinePosition( bool firstLine ) const
{
    return metrics( firstLine ).ascent();
}

void RenderText::position(int x, int y, int from, int len, int width, bool reverse, bool firstLine)
{
    // ### should not be needed!!!
    if(len == 0 || (len == 1 && *(str->s+from) == '\n') ) return;

    QChar *ch;
    reverse = reverse && !style()->visuallyOrdered();
    if ( reverse ) {
        // reverse String
        QString aStr = QConstString(str->s+from, len).string();
        //kdDebug( 6040 ) << "reversing '" << (const char *)aStr.utf8() << "' len=" << aStr.length() << " oldlen=" << len << endl;
        len = aStr.length();
        ch = QT_ALLOC_QCHAR_VEC(len);
        int half =  len/2;
        const QChar *s = aStr.unicode();
        for(int i = 0; i <= half; i++)
        {
            ch[len-1-i] = s[i];
            ch[i] = s[len-1-i];
            if(ch[i].mirrored() && !style()->visuallyOrdered())
                ch[i] = ch[i].mirroredChar();
            if(ch[len-1-i].mirrored() && !style()->visuallyOrdered() && i != len-1-i)
                ch[len-1-i] = ch[len-1-i].mirroredChar();
        }
    }
    else
        ch = str->s+from;

    // ### margins and RTL
    if(from == 0 && parent()->isInline() && parent()->firstChild()==this)
    {
        x += paddingLeft() + borderLeft() + marginLeft();
        width -= marginLeft();
    }

    if(from + len == int(str->l) && parent()->isInline() && parent()->lastChild()==this)
        width -= marginRight();

    // add half leading to vertiaclly center it.
    y += ( m_lineHeight - metrics( firstLine ).height() )/2;

#ifdef DEBUG_LAYOUT
    QConstString cstr(ch, len);
    kdDebug( 6040 ) << "setting slave text to '" << (const char *)cstr.string().utf8() << "' len=" << len << " width=" << width << " at (" << x << "/" << y << ")" << " height=" << lineHeight() << " fontHeight=" << metrics().height() << " ascent =" << metrics().ascent() << endl;
#endif

    TextSlave *s = new TextSlave(x, y, ch, len,
                                 baselinePosition( firstLine ),
                                 width, reverse, firstLine);

    if(m_lines.count() == m_lines.size())
        m_lines.resize(m_lines.size()*2+1);

    m_lines.insert(m_lines.count(), s);
}

unsigned int RenderText::width(unsigned int from, unsigned int len, bool firstLine) const
{
    if(!str->s || from > str->l ) return 0;

    if ( from + len > str->l ) len = str->l - from;

    if ( khtml::printpainter || ( firstLine && hasFirstLine() ) ) {
	QFontMetrics _fm = metrics( firstLine );
	return width( from, len, &_fm );
    }

    return width( from, len, fm );
}

unsigned int RenderText::width(unsigned int from, unsigned int len, QFontMetrics *_fm) const
{
    if(!str->s || from > str->l ) return 0;
    if ( from + len > str->l ) len = str->l - from;

    int w;
    if ( _fm == fm && from == 0 && len == str->l )
 	 w = m_maxWidth;
    if( len == 1)
        w = _fm->width( *(str->s+from) );
    else
        w = _fm->width(QConstString(str->s+from, len).string());

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

QFontMetrics RenderText::metrics(bool firstLine) const
{
    if( firstLine && hasFirstLine() ) {
	RenderStyle *pseudoStyle  = style()->getPseudoStyle(RenderStyle::FIRST_LINE);
	if ( pseudoStyle )
	    return fontMetrics( pseudoStyle->font() );
    }
    if ( khtml::printpainter )
	return fontMetrics(style()->font());
    return *fm;
}

#undef BIDI_DEBUG
