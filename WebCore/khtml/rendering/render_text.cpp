/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
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
//#define DEBUG_LAYOUT
//#define BIDI_DEBUG

#include "rendering/render_canvas.h"
#include "rendering/render_text.h"
#include "rendering/break_lines.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "render_arena.h"

#include "misc/loader.h"

#include <qpainter.h>
#include <kdebug.h>
#include <assert.h>

using namespace khtml;
using namespace DOM;

#ifndef NDEBUG
static bool inInlineTextBoxDetach;
#endif

void InlineTextBox::detach(RenderArena* renderArena)
{
#ifndef NDEBUG
    inInlineTextBoxDetach = true;
#endif
    delete this;
#ifndef NDEBUG
    inInlineTextBoxDetach = false;
#endif
    
    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

void* InlineTextBox::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void InlineTextBox::operator delete(void* ptr, size_t sz)
{
    assert(inInlineTextBoxDetach);
    
    // Stash size where detach can find it.
    *(size_t *)ptr = sz;
}

void InlineTextBox::paintSelection(const Font *f, RenderText *text, QPainter *p, RenderStyle* style, int tx, int ty, int startPos, int endPos)
{
    if(startPos > m_len) return;
    if(startPos < 0) startPos = 0;

    p->save();
#if APPLE_CHANGES
    // Macintosh-style text highlighting is to draw with a particular background color, not invert.
    QColor textColor = style->color();
    QColor c = p->selectedTextBackgroundColor();
    
    // if text color and selection background color are identical, invert background color.
    if (textColor == c)
        c = QColor(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    RenderStyle* pseudoStyle = object()->getPseudoStyle(RenderStyle::SELECTION);
    if (pseudoStyle && pseudoStyle->backgroundColor().isValid())
        c = pseudoStyle->backgroundColor();
    p->setPen(c); // Don't draw text at all!
    
#else
    QColor c = style->color();
    p->setPen(QColor(0xff-c.red(),0xff-c.green(),0xff-c.blue()));
#endif
    ty += m_baseline;

#if APPLE_CHANGES
    //kdDebug( 6040 ) << "InlineTextBox::painting(" << s.string() << ") at(" << x+_tx << "/" << y+_ty << ")" << endl;
    f->drawHighlightForText(p, m_x + tx, m_y + ty, text->str->s, text->str->l, m_start, m_len,
		m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, style->visuallyOrdered(), startPos, endPos, c);
#else
    f->drawHighlightForText(p, m_x + tx, m_y + ty, text->str->s, text->str->l, m_start, m_len,
		m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, startPos, endPos, c);
#endif
    p->restore();
}

#ifdef APPLE_CHANGES
void InlineTextBox::paintDecoration( QPainter *pt, int _tx, int _ty, int deco)
{
    _tx += m_x;
    _ty += m_y;

    // Get the text decoration colors.
    QColor underline, overline, linethrough;
    object()->getTextDecorationColors(deco, underline, overline, linethrough, true);
    
    // Use a special function for underlines to get the positioning exactly right.
    if (deco & UNDERLINE) {
        pt->setPen(underline);
        pt->drawLineForText(_tx, _ty, m_baseline, m_width);
    }
    if (deco & OVERLINE) {
        pt->setPen(overline);
        pt->drawLineForText(_tx, _ty, 0, m_width);
    }
    if (deco & LINE_THROUGH) {
        pt->setPen(linethrough);
        pt->drawLineForText(_tx, _ty, 2*m_baseline/3, m_width);
    }
}
#else
void InlineTextBox::paintDecoration( QPainter *pt, int _tx, int _ty, int decoration)
{
    _tx += m_x;
    _ty += m_y;

    int width = m_width - 1;

    QColor underline, overline, linethrough;
    object()->getTextDecorationColors(decoration, underline, overline, linethrough, true);
    
    int underlineOffset = ( pt->fontMetrics().height() + m_baseline ) / 2;
    if(underlineOffset <= m_baseline) underlineOffset = m_baseline+1;

    if(deco & UNDERLINE){
        pt->setPen(underline);
        pt->drawLine(_tx, _ty + underlineOffset, _tx + width, _ty + underlineOffset );
    }
    if (deco & OVERLINE) {
        pt->setPen(overline);
        pt->drawLine(_tx, _ty, _tx + width, _ty );
    }
    if(deco & LINE_THROUGH) {
        pt->setPen(linethrough);
        pt->drawLine(_tx, _ty + 2*m_baseline/3, _tx + width, _ty + 2*m_baseline/3 );
    }
    // NO! Do NOT add BLINK! It is the most annouing feature of Netscape, and IE has a reason not to
    // support it. Lars
}
#endif

#define LOCAL_WIDTH_BUF_SIZE	1024

FindSelectionResult InlineTextBox::checkSelectionPoint(int _x, int _y, int _tx, int _ty, const Font *f, RenderText *text, int & offset, short lineHeight)
{
//     kdDebug(6040) << "InlineTextBox::checkSelectionPoint " << this << " _x=" << _x << " _y=" << _y
//                   << " _tx+m_x=" << _tx+m_x << " _ty+m_y=" << _ty+m_y << endl;
    offset = 0;

    if ( _y < _ty + m_y )
        return SelectionPointBefore; // above -> before

    if ( _y > _ty + m_y + m_height ) {
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
    int pos = f->checkSelectionPoint (text->str->s, text->str->l, m_start, m_len, m_toAdd, _x - (_tx + m_x), m_reversed);
#else
    int delta = _x - (_tx + m_x);
    //kdDebug(6040) << "InlineTextBox::checkSelectionPoint delta=" << delta << endl;
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

InlineTextBoxArray::InlineTextBoxArray()
{
    setAutoDelete(false);
}

int InlineTextBoxArray::compareItems( Item d1, Item d2 )
{
    assert(d1);
    assert(d2);

    return static_cast<InlineTextBox*>(d1)->m_y - static_cast<InlineTextBox*>(d2)->m_y;
}

// remove this once QVector::bsearch is fixed
int InlineTextBoxArray::findFirstMatching(Item d) const
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

#ifdef APPLE_CHANGES
    m_monospaceCharacterWidth = 0;
    m_allAsciiChecked = false;
    m_allAscii = false;
#endif

    str = _str;
    if (str) {
        str = str->replace('\\', backslashAsCurrencySymbol());
        str->ref();
    }
    KHTMLAssert(!str || !str->l || str->s);

    m_selectionState = SelectionNone;

#ifdef DEBUG_LAYOUT
    QConstString cstr(str->s, str->l);
    kdDebug( 6040 ) << "RenderText ctr( "<< cstr.string().length() << " )  '" << cstr.string() << "'" << endl;
#endif
}

void RenderText::setStyle(RenderStyle *_style)
{
    if ( style() != _style ) {
        bool needToTransformText = (!style() && _style->textTransform() != TTNONE) ||
                                   (style() && style()->textTransform() != _style->textTransform());

        RenderObject::setStyle( _style );
        m_lineHeight = RenderObject::lineHeight(false);

        if (needToTransformText) {
            DOM::DOMStringImpl* textToTransform = originalString();
            if (textToTransform)
                setText(textToTransform, true);
        }
#if APPLE_CHANGES
        // setText also calls cacheWidths(), so there is no need to call it again in that case.
        else
            cacheWidths();
#endif
    }
}

RenderText::~RenderText()
{
    if(str) str->deref();
}

void RenderText::detach()
{
    deleteRuns();
    RenderObject::detach();
}

void RenderText::deleteRuns()
{
    // this is a slight variant of QArray::clear().
    // We don't delete the array itself here because its
    // likely to be used in the same size later again, saves
    // us resize() calls
    unsigned int len = m_lines.size();
    if (len) {
        RenderArena* arena = renderArena();
        for(unsigned int i=0; i < len; i++) {
            InlineTextBox* s = m_lines.at(i);
            if (s)
                s->detach(arena);
            m_lines.remove(i);
        }
    }
    
    KHTMLAssert(m_lines.count() == 0);
}

bool RenderText::isTextFragment() const
{
    return false;
}

DOM::DOMStringImpl* RenderText::originalString() const
{
    return element() ? element()->string() : 0;
}

void RenderText::absoluteRects(QValueList<QRect>& rects, int _tx, int _ty)
{
    for (unsigned int i = 0; i < m_lines.count(); i++)
        rects.append(QRect(_tx + m_lines[i]->xPos(), 
                           _ty + m_lines[i]->yPos(), 
                           m_lines[i]->width(), 
                           m_lines[i]->height()));
}

InlineTextBox * RenderText::findNextInlineTextBox( int offset, int &pos )
{
    // The text runs point to parts of the rendertext's str string
    // (they don't include '\n')
    // Find the text run that includes the character at @p offset
    // and return pos, which is the position of the char in the run.

    if ( m_lines.isEmpty() )
        return 0L;

    InlineTextBox* s = m_lines[0];
    uint si = 1;
    int off = s->m_len;
    while(offset > off && si < m_lines.count())
    {
        s = m_lines[si++];
        off = s->m_start + s->m_len;
    }
    // we are now in the correct text run
    pos = (offset > off ? s->m_len : s->m_len - (off - offset) );
    return s;
}

bool RenderText::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty,
                             HitTestAction hitTestAction, bool inside)
{
    assert(parent());

    InlineTextBox *s = m_lines.count() ? m_lines[0] : 0;
    int si = 0;
    while(s) {
        if((_y >=_ty + s->m_y) && (_y < _ty + s->m_y + s->height()) &&
           (_x >= _tx + s->m_x) && (_x <_tx + s->m_x + s->m_width) ) {
            inside = true;
            break;
        }

        s = si < (int) m_lines.count()-1 ? m_lines[++si] : 0;
    }

    if (inside && element()) {
        if (info.innerNode() && info.innerNode()->renderer() && 
            !info.innerNode()->renderer()->isInline()) {
            // Within the same layer, inlines are ALWAYS fully above blocks.  Change inner node.
            info.setInnerNode(element());
            
            // Clear everything else.
            info.setInnerNonSharedNode(0);
            info.setURLElement(0);
        }
        
        if (!info.innerNode())
            info.setInnerNode(element());

        if (!info.innerNonSharedNode())
            info.setInnerNonSharedNode(element());
    }

    return inside;
}

FindSelectionResult RenderText::checkSelectionPointIgnoringContinuations(int _x, int _y, int _tx, int _ty, DOM::NodeImpl*& node, int &offset)
{
//     kdDebug(6040) << "RenderText::checkSelectionPoint " << this << " _x=" << _x << " _y=" << _y
//                   << " _tx=" << _tx << " _ty=" << _ty << endl;
    InlineTextBox *lastPointAfterInline=0;

    for(unsigned int si = 0; si < m_lines.count(); si++)
    {
        InlineTextBox* s = m_lines[si];
        int result;
        const Font *f = htmlFont( si==0 );
        result = s->checkSelectionPoint(_x, _y, _tx, _ty, f, this, offset, m_lineHeight);

//         kdDebug(6040) << "RenderText::checkSelectionPoint " << this << " line " << si << " result=" << result << " offset=" << offset << endl;
        if ( result == SelectionPointInside ) // x,y is inside the InlineTextBox
        {
            offset += s->m_start; // add the offset from the previous lines
            //kdDebug(6040) << "RenderText::checkSelectionPoint inside -> " << offset << endl;
            node = element();
            return SelectionPointInside;
        } else if ( result == SelectionPointBefore ) {
            // x,y is before the InlineTextBox -> stop here
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
  InlineTextBox * s = findNextInlineTextBox( offset, pos );
  _y = s->m_y;
  height = s->m_height;

  const QFontMetrics &fm = metrics( s->m_firstLine );
  QString tekst(str->s + s->m_start, s->m_len);
  _x = s->m_x + (fm.boundingRect(tekst, pos)).right();
  if(pos)
      _x += fm.rightBearing( *(str->s + s->m_start + pos - 1 ) );

  int absx, absy;
  absolutePosition(absx,absy);
  _x += absx;
  _y += absy;
}

void RenderText::posOfChar(int chr, int &x, int &y)
{
    absolutePosition( x, y, false );

    //if( chr > (int) str->l )
    //chr = str->l;

    int pos;
    InlineTextBox * s = findNextInlineTextBox( chr, pos );

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

static int
simpleDifferenceBetweenColors(QColor c1, QColor c2)
{
    // a distance could be computed by squaring the differences between components, but
    // this is faster and so far seems good enough for our purposes.
    return abs(c1.red() - c2.red()) + abs(c1.green() - c2.green()) + abs(c1.blue() - c2.blue());
}

static QColor 
correctedTextColor(QColor textColor, QColor backgroundColor) 
{
    // Adjust the text color if it is too close to the background color,
    // by darkening or lightening it to move it further away.
    
    int d = simpleDifferenceBetweenColors(textColor, backgroundColor);
    // semi-arbitrarily chose 255 value here after a few tests; 
    if (d > 255) {
        return textColor;
    }
    
    int distanceFromWhite = simpleDifferenceBetweenColors(textColor, Qt::white);
    int distanceFromBlack = simpleDifferenceBetweenColors(textColor, Qt::black);

    if (distanceFromWhite < distanceFromBlack) {
        return textColor.dark();
    }
    
    return textColor.light();
}

void RenderText::paintObject(QPainter *p, int /*x*/, int y, int /*w*/, int h,
                             int tx, int ty, PaintAction paintAction)
{
    RenderStyle* pseudoStyle = style(true);
    if (pseudoStyle == style()) pseudoStyle = 0;
    int d = style()->textDecorationsInEffect();
    InlineTextBox f(0, y-ty);
    int si = m_lines.findFirstMatching(&f);
    // something matching found, find the first one to print
    bool isPrinting = (p->device()->devType() == QInternal::Printer);
    if(si >= 0)
    {
        // Move up until out of area to be printed
        while(si > 0 && m_lines[si-1]->checkVerticalPoint(y, ty, h))
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

        InlineTextBox* s;

	const Font *font = &style()->htmlFont();

#if APPLE_CHANGES
        // Do one pass for the selection, then another for the rest.
        bool haveSelection = startPos != endPos && !isPrinting && selectionState() != SelectionNone;
        if (!haveSelection && paintAction == PaintActionSelection) {
            // When only painting the selection, don't bother to paint if there is none.
            return;
        }
        int startLine = si;
        for (int pass = 0; pass < (haveSelection ? 2 : 1); pass++) {
            si = startLine;
            
            bool drawSelectionBackground = haveSelection && pass == 0 && paintAction != PaintActionSelection;
            bool drawText = !haveSelection || pass == 1;
#endif

        // run until we find one that is outside the range, then we
        // know we can stop
        do {
            s = m_lines[si];

	    if (isPrinting)
	    {
                // FIXME: Need to understand what this section is doing.
                if (ty+s->m_y < y)
                {
                   // This has been printed already we suppose.
                   continue;
                }

                if (ty+s->m_y+s->height() > y+h)
                {
                   RenderCanvas* canvasObj = canvas();
                   if (ty+s->m_y < canvasObj->truncatedAt())
#if APPLE_CHANGES
                       canvasObj->setBestTruncatedAt(ty+s->m_y, this);
#else
                       canvasObj->setTruncatedAt(ty+s->m_y);
#endif
                   // Let's stop here.
                   break;
                }
            }

            RenderStyle* _style = pseudoStyle && s->m_firstLine ? pseudoStyle : style();

            if (_style->font() != p->font())
                p->setFont(_style->font());

            font = &_style->htmlFont(); // Always update, since smallCaps is not stored in the QFont.

#if APPLE_CHANGES
            if (drawText) {
#endif
            
            QColor textColor = _style->color();
            if (_style->shouldCorrectTextColor()) {
                textColor = correctedTextColor(textColor, _style->backgroundColor());
            }

            if(textColor != p->pen().color())
                p->setPen(textColor);

#if APPLE_CHANGES
            // Set a text shadow if we have one.
            // FIXME: Support multiple shadow effects.  Need more from the CG API before
            // we can do this.
            bool setShadow = false;
            if (_style->textShadow()) {
                p->setShadow(_style->textShadow()->x, _style->textShadow()->y,
                             _style->textShadow()->blur, _style->textShadow()->color);
                setShadow = true;
            }
#endif
            
            if (s->m_len > 0) {
                bool paintSelectedTextOnly = (paintAction == PaintActionSelection);
                bool paintSelectedTextSeparately = false; // Whether or not we have to do multiple paints.  Only
                                               // necessary when a custom ::selection foreground color is applied.
                QColor selectionColor = p->pen().color();
                ShadowData* selectionTextShadow = 0;
                if (haveSelection) {
                    RenderStyle* pseudoStyle = getPseudoStyle(RenderStyle::SELECTION);
                    if (pseudoStyle) {
                        if (pseudoStyle->color() != selectionColor || pseudoStyle->textShadow()) {
                            if (!paintSelectedTextOnly)
                                paintSelectedTextSeparately = true;
                            if (pseudoStyle->color() != selectionColor)
                                selectionColor = pseudoStyle->color();
                            if (pseudoStyle->textShadow())
                                selectionTextShadow = pseudoStyle->textShadow();
                        }
                    }
                }
                
                if (!paintSelectedTextOnly && !paintSelectedTextSeparately) {
#if APPLE_CHANGES
                    font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline,
                                   str->s, str->l, s->m_start, s->m_len,
                                   s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR, style()->visuallyOrdered());
#else
                    font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline,
                                   str->s, str->l, s->m_start, s->m_len,
                                   s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR);
#endif
                }
                else {
                    int offset = s->m_start;
                    int sPos = QMAX( startPos - offset, 0 );
                    int ePos = QMIN( endPos - offset, s->m_len );
                    if (paintSelectedTextSeparately) {
                        if (sPos >= ePos)
#if APPLE_CHANGES
                            font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline,
                                           str->s, str->l, s->m_start, s->m_len,
                                           s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR, style()->visuallyOrdered());
#else
                            font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline,
                                           str->s, str->l, s->m_start, s->m_len,
                                           s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR);
#endif
                        else {
                            if (sPos-1 >= 0)
#if APPLE_CHANGES
                                font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline, str->s,
                                            str->l, s->m_start, s->m_len,
                                            s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR, style()->visuallyOrdered(), 0, sPos);
#else
                                font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline, str->s,
                                            str->l, s->m_start, s->m_len,
                                            s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR, 0, sPos);
#endif
                            if (ePos < s->m_start+s->m_len)
#if APPLE_CHANGES
                                font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline, str->s,
                                            str->l, s->m_start, s->m_len,
                                            s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR, style()->visuallyOrdered(), ePos, -1);
#else
                                font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline, str->s,
                                            str->l, s->m_start, s->m_len,
                                            s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR, ePos, -1);
#endif
                        }
                    }
                    
                    if ( sPos < ePos ) {
                        if (selectionColor != p->pen().color())
                            p->setPen(selectionColor);

#if APPLE_CHANGES
                        if (selectionTextShadow)
                            p->setShadow(selectionTextShadow->x,
                                         selectionTextShadow->y,
                                         selectionTextShadow->blur,
                                         selectionTextShadow->color);
#endif                       

#if APPLE_CHANGES
                        font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline, str->s,
                                       str->l, s->m_start, s->m_len,
                                       s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR, style()->visuallyOrdered(), sPos, ePos);
#else
                        font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline, str->s,
                                       str->l, s->m_start, s->m_len,
                                       s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR, sPos, ePos);
#endif

#if APPLE_CHANGES
                        if (selectionTextShadow)
                            p->clearShadow();
#endif
                    }
                } 
            }
            
            if (d != TDNONE && paintAction == PaintActionForeground &&
                style()->htmlHacks()) {
                p->setPen(_style->color());
                s->paintDecoration(p, tx, ty, d);
            }

#if APPLE_CHANGES
            if (setShadow)
                p->clearShadow();
            
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
                //kdDebug(6040) << this << " paintSelection with startPos=" << sPos << " endPos=" << ePos << endl;
		if ( sPos < ePos )
		    s->paintSelection(font, this, p, _style, tx, ty, sPos, ePos);

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

        } while (++si < (int)m_lines.count() && m_lines[si]->checkVerticalPoint(y, ty, h));

#if APPLE_CHANGES
        } // end of for loop
#endif
    }
}

void RenderText::paint(QPainter *p, int x, int y, int w, int h,
                       int tx, int ty, PaintAction paintAction)
{
    if (paintAction != PaintActionForeground && paintAction != PaintActionSelection)
        return;
    
    if (style()->visibility() != VISIBLE)
        return;

    int s = m_lines.count() - 1;
    if ( s < 0 )
        return;

    if (ty + m_lines[0]->yPos() > y + h) return;
    if (ty + m_lines[s]->yPos() + m_lines[s]->height() < y ) return;

    paintObject(p, x, y, w, h, tx, ty, paintAction);
}

#ifdef APPLE_CHANGES

bool RenderText::allAscii() const
{
    if (m_allAsciiChecked)
        return m_allAscii;
    m_allAsciiChecked = true;
    
    unsigned int i;
    for (i = 0; i < str->l; i++){
        if (str->s[i].unicode() >= 0x7f){
            m_allAscii = false;
            return m_allAscii;
        }
    }
    
    m_allAscii = true;
    
    return m_allAscii;
}

bool RenderText::shouldUseMonospaceCache(const Font *f) const
{
    return (f && f->isFixedPitch() && allAscii() && !style()->htmlFont().isSmallCaps());
}

// We cache the width of the ' ' character for <pre> text.  We could go futher
// and cache a widths array for all styles, at the expense of increasing the size of the
// RenderText.
void RenderText::cacheWidths()
{
    const Font *f = htmlFont( false );
    
    if (shouldUseMonospaceCache(f)){	
        float fw;
        QChar c(' ');
        f->floatCharacterWidths( &c, 1, 0, 1, 0, &fw);
        m_monospaceCharacterWidth = (int)fw;
    }
    else
        m_monospaceCharacterWidth = 0;
}


inline int RenderText::widthFromCache(const Font *f, int start, int len) const
{
    if (m_monospaceCharacterWidth != 0){
        int i, w = 0;
        for (i = start; i < start+len; i++){
            int dir = str->s[i].direction();
            if (dir != QChar::DirNSM && dir != QChar::DirBN)
                w += m_monospaceCharacterWidth;
        }
        return w;
    }
    
    return f->width(str->s, str->l, start, len);
}
#ifdef XXX
inline int RenderText::widthFromCache(const Font *f, int start, int len) const
{
    if (m_monospaceCharacterWidth != 0){
        return len * m_monospaceCharacterWidth;
    }

    return f->width(str->s, str->l, start, len);
}
#endif

#endif

void RenderText::trimmedMinMaxWidth(short& beginMinW, bool& beginWS, 
                                    short& endMinW, bool& endWS,
                                    bool& hasBreakableChar, bool& hasBreak,
                                    short& beginMaxW, short& endMaxW,
                                    short& minW, short& maxW, bool& stripFrontSpaces)
{
    bool isPre = style()->whiteSpace() == PRE;
    if (isPre)
        stripFrontSpaces = false;
    
    int len = str->l;
    if (len == 0 || (stripFrontSpaces && str->containsOnlyWhitespace())) {
        maxW = 0;
        hasBreak = false;
        return;
    }
    
    minW = m_minWidth;
    maxW = m_maxWidth;
    beginWS = stripFrontSpaces ? false : m_hasBeginWS;
    endWS = m_hasEndWS;
    
    beginMinW = m_beginMinWidth;
    endMinW = m_endMinWidth;
    
    hasBreakableChar = m_hasBreakableChar;
    hasBreak = m_hasBreak;

    if (stripFrontSpaces && (str->s[0] == ' ' || (!isPre && str->s[0] == '\n'))) {
        const Font *f = htmlFont( false );
        QChar space[1]; space[0] = ' ';
        int spaceWidth = f->width(space, 1, 0);
        maxW -= spaceWidth;
    }
    
    stripFrontSpaces = !isPre && m_hasEndWS;
    
    if (style()->whiteSpace() == NOWRAP)
        minW = maxW;
    else if (minW > maxW)
        minW = maxW;
        
    // Compute our max widths by scanning the string for newlines.
    if (hasBreak) {
        const Font *f = htmlFont( false );
        bool firstLine = true;
        beginMaxW = endMaxW = maxW;
        for (int i = 0; i < len; i++)
        {
            int linelen = 0;
            while (i+linelen < len && str->s[i+linelen] != '\n')
                linelen++;
                
            if (linelen)
            {
#if !APPLE_CHANGES
                endMaxW = f->width(str->s, str->l, i, linelen);
#else
                endMaxW = widthFromCache(f, i, linelen);
#endif
                if (firstLine) {
                    firstLine = false;
                    beginMaxW = endMaxW;
                }
                i += linelen;
            }
            else if (firstLine) {
                beginMaxW = 0;
                firstLine = false;
            }
	    
	    if (i == len-1)
	        // A <pre> run that ends with a newline, as in, e.g.,
	        // <pre>Some text\n\n<span>More text</pre>
	        endMaxW = 0;
        }
    }
}

void RenderText::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    // ### calc Min and Max width...
    m_minWidth = m_beginMinWidth = m_endMinWidth = 0;
    m_maxWidth = 0;

    if (isBR())
        return;
        
    int currMinWidth = 0;
    int currMaxWidth = 0;
    m_hasBreakableChar = m_hasBreak = m_hasBeginWS = m_hasEndWS = false;
    
    // ### not 100% correct for first-line
    const Font *f = htmlFont( false );
    int wordSpacing = style()->wordSpacing();
    int len = str->l;
    bool ignoringSpaces = false;
    bool isSpace = false;
    bool isPre = style()->whiteSpace() == PRE;
    bool firstWord = true;
    bool firstLine = true;
    for(int i = 0; i < len; i++)
    {
        const QChar c = str->s[i];
        
        bool previousCharacterIsSpace = isSpace;
        
        bool isNewline = false;
        if (c == '\n') {
            if (isPre) {
                m_hasBreak = true;
                isNewline = true;
                isSpace = false;
            }
            else
                isSpace = true;
        } else {
            isSpace = c == ' ';
        }
        
        if ((isSpace || isNewline) && i == 0)
            m_hasBeginWS = true;
        if ((isSpace || isNewline) && i == len-1)
            m_hasEndWS = true;
            
        if (!ignoringSpaces && !isPre && previousCharacterIsSpace && isSpace)
            ignoringSpaces = true;
        
        if (ignoringSpaces && !isSpace)
            ignoringSpaces = false;
            
        if (ignoringSpaces)
            continue;
        
        int wordlen = 0;
        while (i+wordlen < len && str->s[i+wordlen] != '\n' && str->s[i+wordlen] != ' ' &&
               (wordlen == 0 || !isBreakable( str->s, i+wordlen, str->l)))
            wordlen++;
            
        if (wordlen)
        {
#if !APPLE_CHANGES
            int w = f->width(str->s, str->l, i, wordlen);
#else
            int w = widthFromCache(f, i, wordlen);
#endif
            currMinWidth += w;
            currMaxWidth += w;
            
            bool isBreakableCharSpace = (i+wordlen < len) ? ((!isPre && str->s[i+wordlen] == '\n') || 
                                                             str->s[i+wordlen] == ' ') : false;

            if (i+wordlen < len && style()->whiteSpace() == NORMAL)
                m_hasBreakableChar = true;
            
            // Add in wordspacing to our maxwidth, but not if this is the last word on a line or the
            // last word in the run.
            if (wordSpacing && isBreakableCharSpace && !containsOnlyWhitespace(i+wordlen, len-(i+wordlen)))
                currMaxWidth += wordSpacing;

            if (firstWord) {
                firstWord = false;
                // If the first character in the run is breakable, then we consider ourselves to have a beginning
                // minimum width of 0, since a break could occur right before our run starts, preventing us from ever
                // being appended to a previous text run when considering the total minimum width of the containing block.
                bool hasBreak = isBreakable(str->s, i, str->l);
                if (hasBreak)
                    m_hasBreakableChar = true;
                m_beginMinWidth = hasBreak ? 0 : w;
            }
            m_endMinWidth = w;
            
            if (currMinWidth > m_minWidth) m_minWidth = currMinWidth;
            currMinWidth = 0;
            
            i += wordlen-1;
        }
        else {
            // Nowrap can never be broken, so don't bother setting the
            // breakable character boolean. Pre can only be broken if we encounter a newline.
            if (style()->whiteSpace() == NORMAL || isNewline)
                m_hasBreakableChar = true;

            if (currMinWidth > m_minWidth) m_minWidth = currMinWidth;
            currMinWidth = 0;
            
            if (isNewline) // Only set if isPre was true and we saw a newline.
            {
                if (firstLine) {
                    firstLine = false;
                    m_beginMinWidth = currMaxWidth;
                }
                
                if (currMaxWidth > m_maxWidth) m_maxWidth = currMaxWidth;
                currMaxWidth = 0;
            }
            else
            {
                currMaxWidth += f->width( str->s, str->l, i + wordlen );
            }
        }
    }
    
    if(currMinWidth > m_minWidth) m_minWidth = currMinWidth;
    if(currMaxWidth > m_maxWidth) m_maxWidth = currMaxWidth;

    if (style()->whiteSpace() != NORMAL)
        m_minWidth = m_maxWidth;

    if (isPre) {
        if (firstLine)
            m_beginMinWidth = m_maxWidth;
        m_endMinWidth = currMaxWidth;
    }
    
    setMinMaxKnown();
    //kdDebug( 6040 ) << "Text::calcMinMaxWidth(): min = " << m_minWidth << " max = " << m_maxWidth << endl;
}

bool RenderText::containsOnlyWhitespace(unsigned int from, unsigned int len) const
{
    unsigned int currPos;
    for (currPos = from; 
         currPos < from+len && (str->s[currPos] == '\n' || str->s[currPos].unicode() == ' '); 
         currPos++);
    return currPos >= (from+len);
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
    if (str) {
        str = str->replace('\\', backslashAsCurrencySymbol());
        if ( style() ) {
            switch(style()->textTransform()) {
                case CAPITALIZE:   str = str->capitalize();  break;
                case UPPERCASE:   str = str->upper();       break;
                case LOWERCASE:  str = str->lower();       break;
                case NONE:
                default:;
            }
        }
        str->ref();
    }

#if APPLE_CHANGES
    cacheWidths();
#endif
    // ### what should happen if we change the text of a
    // RenderBR object ?
    KHTMLAssert(!isBR() || (str->l == 1 && (*str->s) == '\n'));
    KHTMLAssert(!str->l || str->s);

    setNeedsLayout(true);
    
#ifdef BIDI_DEBUG
    QConstString cstr(str->s, str->l);
    kdDebug( 6040 ) << "RenderText::setText( " << cstr.string().length() << " ) '" << cstr.string() << "'" << endl;
#endif
}

int RenderText::height() const
{
    int retval = 0;
    if ( m_lines.count() )
        retval = m_lines[m_lines.count()-1]->m_y + m_lineHeight - m_lines[0]->m_y;
    return retval;
}

short RenderText::lineHeight( bool firstLine, bool ) const
{
    if ( firstLine )
 	return RenderObject::lineHeight( firstLine );

    return m_lineHeight;
}

short RenderText::baselinePosition( bool firstLine, bool ) const
{
    const QFontMetrics &fm = metrics( firstLine );
    return fm.ascent() +
        ( lineHeight( firstLine ) - fm.height() ) / 2;
}

InlineBox* RenderText::createInlineBox(bool, bool isRootLineBox)
{
    // FIXME: Either ditch the array or get this object into it.
    KHTMLAssert(!isRootLineBox);
    return new (renderArena()) InlineTextBox(this);
}

void RenderText::position(InlineBox* box, int from, int len, bool reverse)
{
    InlineTextBox *s = static_cast<InlineTextBox*>(box);
    
    // ### should not be needed!!!
    if (len == 0 || isBR()) {
        // We want the box to be destroyed.  This is a <br>, and we don't
        // need <br>s to be included.
        s->detach(renderArena());
        return;
    }
    
    reverse = reverse && !style()->visuallyOrdered();

#ifdef DEBUG_LAYOUT
    QChar *ch = str->s+from;
    QConstString cstr(ch, len);
    qDebug("setting run text to *%s*, len=%d, w)=%d" , cstr.string().latin1(), len, width );//" << y << ")" << " height=" << lineHeight(false) << " fontHeight=" << metrics(false).height() << " ascent =" << metrics(false).ascent() << endl;
#endif

    s->m_reversed = reverse;
    s->m_start = from;
    s->m_len = len;
    
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
#if APPLE_CHANGES
    else if (f == &style()->htmlFont())
        w = widthFromCache (f, from, len);
#endif
    else
	w = f->width(str->s, str->l, from, len );

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
        InlineTextBox* s = m_lines[si];
        if(s->m_x < minx)
            minx = s->m_x;
        if(s->m_x + s->m_width > maxx)
            maxx = s->m_x + s->m_width;
    }

    w = QMAX(0, maxx-minx);

    return w;
}

QRect RenderText::getAbsoluteRepaintRect()
{
    RenderObject *cb = containingBlock();
    return cb->getAbsoluteRepaintRect();
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
    return style(firstLine)->fontMetrics();
}

const Font *RenderText::htmlFont(bool firstLine) const
{
    return &style(firstLine)->htmlFont();
}

RenderTextFragment::RenderTextFragment(DOM::NodeImpl* _node, DOM::DOMStringImpl* _str,
                                       int startOffset, int endOffset)
:RenderText(_node, _str->substring(startOffset, endOffset)), 
m_start(startOffset), m_end(endOffset), m_generatedContentStr(0)
{}

RenderTextFragment::RenderTextFragment(DOM::NodeImpl* _node, DOM::DOMStringImpl* _str)
:RenderText(_node, _str), m_start(0)
{
    m_generatedContentStr = _str;
    if (_str) {
        _str->ref();
        m_end = _str->l;
    }
    else
        m_end = 0;
}
    
RenderTextFragment::~RenderTextFragment()
{
    if (m_generatedContentStr)
        m_generatedContentStr->deref();
}

bool RenderTextFragment::isTextFragment() const
{
    return true;
}

DOM::DOMStringImpl* RenderTextFragment::originalString() const
{
    DOM::DOMStringImpl* result = 0;
    if (element())
        result = element()->string();
    else
        result = contentString();
    if (result && (start() > 0 || start() < result->l))
        result = result->substring(start(), end());
    return result;
}
#undef BIDI_DEBUG
#undef DEBUG_LAYOUT
