/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#include "xml/dom_position.h"
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

void InlineTextBox::deleteLine(RenderArena* arena)
{
    static_cast<RenderText*>(m_object)->removeTextBox(this);
    detach(arena);
}

void InlineTextBox::extractLine()
{
    if (m_extracted)
        return;

    static_cast<RenderText*>(m_object)->extractTextBox(this);
}

void InlineTextBox::attachLine()
{
    if (!m_extracted)
        return;
    
    static_cast<RenderText*>(m_object)->attachTextBox(this);
}

int InlineTextBox::placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox)
{
    if (foundBox) {
        m_truncation = cFullTruncation;
        return -1;
    }

    int ellipsisX = ltr ? blockEdge - ellipsisWidth : blockEdge + ellipsisWidth;
    
    // For LTR, if the left edge of the ellipsis is to the left of our text run, then we are the run that will get truncated.
    if (ltr) {
        if (ellipsisX <= m_x) {
            // Too far.  Just set full truncation, but return -1 and let the ellipsis just be placed at the edge of the box.
            m_truncation = cFullTruncation;
            foundBox = true;
            return -1;
        }

        if (ellipsisX < m_x + m_width) {
            if (m_reversed)
                return -1; // FIXME: Support LTR truncation when the last run is RTL someday.

            foundBox = true;

            int offset = offsetForPosition(ellipsisX, false);
            if (offset == 0) {
                // No characters should be rendered.  Set ourselves to full truncation and place the ellipsis at the min of our start
                // and the ellipsis edge.
                m_truncation = cFullTruncation;
                return kMin(ellipsisX, m_x);
            }
            
            // Set the truncation index on the text run.  The ellipsis needs to be placed just after the last visible character.
            m_truncation = offset + m_start;
            return m_x + static_cast<RenderText*>(m_object)->width(m_start, offset, m_firstLine);
        }
    }
    else {
        // FIXME: Support RTL truncation someday, including both modes (when the leftmost run on the line is either RTL or LTR)
    }
    return -1;
}

void InlineTextBox::paintSelection(const Font *f, RenderText *text, QPainter *p, RenderStyle* style, int tx, int ty, int startPos, int endPos, bool extendSelection)
{
    int offset = m_start;
    int sPos = kMax(startPos - offset, 0);
    int ePos = kMin(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

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
    ty + m_baseline;
#endif
    
#if APPLE_CHANGES
    // Do the calculations to draw selections as tall as the line.
    // Use the bottom of the line above as the y position (if there is one, 
    // otherwise use the top of this renderer's line) and the height of the line as the height. 
    // This mimics Cocoa.
    RenderBlock *cb = object()->containingBlock();

    if (root()->prevRootBox())
        ty = root()->prevRootBox()->bottomOverflow();
    else
        ty = root()->topOverflow();

    int h = root()->bottomOverflow() - ty;

    int absx, absy;
    cb->absolutePosition(absx, absy);
    
    int x = m_x + tx;
    int minX = x;
    int maxX = x;
    if ((extendSelection || startPos < m_start) && root()->firstLeafChild() == this)
        minX = absx + kMax(cb->leftOffset(ty), cb->leftOffset(root()->blockHeight()));
    if ((extendSelection || endPos > m_start + m_len) && root()->lastLeafChild() == this)
        maxX = absx + kMin(cb->rightOffset(ty), cb->rightOffset(root()->blockHeight()));
    
    f->drawHighlightForText(p, x, minX, maxX, absy + ty, h, text->str->s, text->str->l, m_start, m_len,
		m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, style->visuallyOrdered(), sPos, ePos, c);
#else
    f->drawHighlightForText(p, m_x + tx, m_y + ty, text->str->s, text->str->l, m_start, m_len,
		m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, sPos, ePos, c);
#endif
    p->restore();
}

#ifdef APPLE_CHANGES
void InlineTextBox::paintDecoration( QPainter *pt, int _tx, int _ty, int deco)
{
    _tx += m_x;
    _ty += m_y;

    if (m_truncation == cFullTruncation)
        return;
    
    int width = (m_truncation == cNoTruncation) ? 
                m_width : static_cast<RenderText*>(m_object)->width(m_start, m_truncation - m_start, m_firstLine);
    
    // Get the text decoration colors.
    QColor underline, overline, linethrough;
    object()->getTextDecorationColors(deco, underline, overline, linethrough, true);
    
    // Use a special function for underlines to get the positioning exactly right.
    if (deco & UNDERLINE) {
        pt->setPen(underline);
        pt->drawLineForText(_tx, _ty, m_baseline, width);
    }
    if (deco & OVERLINE) {
        pt->setPen(overline);
        pt->drawLineForText(_tx, _ty, 0, width);
    }
    if (deco & LINE_THROUGH) {
        pt->setPen(linethrough);
        pt->drawLineForText(_tx, _ty, 2*m_baseline/3, width);
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

long InlineTextBox::caretMinOffset() const
{
    return m_start;
}

long InlineTextBox::caretMaxOffset() const
{
    return m_start + m_len;
}

unsigned long InlineTextBox::caretMaxRenderedOffset() const
{
    return m_start + m_len;
}

#define LOCAL_WIDTH_BUF_SIZE	1024

int InlineTextBox::offsetForPosition(int _x, bool includePartialGlyphs)
{
    RenderText* text = static_cast<RenderText*>(m_object);
    const Font* f = text->htmlFont(m_firstLine);
    return f->checkSelectionPoint(text->str->s, text->str->l, m_start, m_len, m_toAdd, _x - m_x, m_reversed, includePartialGlyphs);
}

// -------------------------------------------------------------------------------------

RenderText::RenderText(DOM::NodeImpl* node, DOMStringImpl *_str)
    : RenderObject(node), m_linesDirty(false)
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

    m_firstTextBox = m_lastTextBox = 0;
    
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
    if (!documentBeingDestroyed()) {
        if (firstTextBox())
            for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
                box->remove();
        else if (parent() && isBR())
            parent()->dirtyLinesFromChangedChild(this);
    }
    deleteTextBoxes();
    RenderObject::detach();
}

void RenderText::extractTextBox(InlineTextBox* box)
{
    m_lastTextBox = box->prevTextBox();
    if (box == m_firstTextBox)
        m_firstTextBox = 0;
    if (box->prevTextBox())
        box->prevTextBox()->setNextLineBox(0);
    box->setPreviousLineBox(0);
    for (InlineRunBox* curr = box; curr; curr = curr->nextLineBox())
        curr->setExtracted();
}

void RenderText::attachTextBox(InlineTextBox* box)
{
    if (m_lastTextBox) {
        m_lastTextBox->setNextLineBox(box);
        box->setPreviousLineBox(m_lastTextBox);
    }
    else
        m_firstTextBox = box;
    InlineTextBox* last = box;
    for (InlineTextBox* curr = box; curr; curr = curr->nextTextBox()) {
        curr->setExtracted(false);
        last = curr;
    }
    m_lastTextBox = last;
}

void RenderText::removeTextBox(InlineTextBox* box)
{
    if (box == m_firstTextBox)
        m_firstTextBox = box->nextTextBox();
    if (box == m_lastTextBox)
        m_lastTextBox = box->prevTextBox();
    if (box->nextTextBox())
        box->nextTextBox()->setPreviousLineBox(box->prevTextBox());
    if (box->prevTextBox())
        box->prevTextBox()->setNextLineBox(box->nextTextBox());
}

void RenderText::deleteTextBoxes()
{
    if (firstTextBox()) {
        RenderArena* arena = renderArena();
        InlineTextBox *curr = firstTextBox(), *next = 0;
        while (curr) {
            next = curr->nextTextBox();
            curr->detach(arena);
            curr = next;
        }
        m_firstTextBox = m_lastTextBox = 0;
    }
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
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        rects.append(QRect(_tx + box->xPos(), 
                           _ty + box->yPos(), 
                           box->width(), 
                           box->height()));
}

InlineTextBox* RenderText::findNextInlineTextBox(int offset, int &pos)
{
    // The text runs point to parts of the rendertext's str string
    // (they don't include '\n')
    // Find the text run that includes the character at @p offset
    // and return pos, which is the position of the char in the run.

    if (!m_firstTextBox)
        return 0;
    
    InlineTextBox* s = m_firstTextBox;
    int off = s->m_len;
    while (offset > off && s->nextTextBox())
    {
        s = s->nextTextBox();
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

    for (InlineTextBox *s = m_firstTextBox; s; s = s->nextTextBox()) {
        if((_y >=_ty + s->m_y) && (_y < _ty + s->m_y + s->height()) &&
           (_x >= _tx + s->m_x) && (_x <_tx + s->m_x + s->m_width) ) {
            inside = true;
            break;
        }
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

Position RenderText::positionForCoordinates(int _x, int _y)
{
    if (!firstTextBox() || stringLength() == 0)
        return Position(element(), 0);

    int absx, absy;
    containingBlock()->absolutePosition(absx, absy);

    if (firstTextBox() && _y < absy + firstTextBox()->root()->bottomOverflow() && _x < absx + firstTextBox()->m_x) {
        // at the y coordinate of the first line or above
        // and the x coordinate is to the left than the first text box left edge
        return Position(element(), firstTextBox()->m_start);
    }

    if (lastTextBox() && _y >= absy + lastTextBox()->root()->topOverflow() && _x >= absx + lastTextBox()->m_x + lastTextBox()->m_width) {
        // at the y coordinate of the last line or below
        // and the x coordinate is to the right than the last text box right edge
        return Position(element(), lastTextBox()->m_start + lastTextBox()->m_len);
    }

    for (InlineTextBox *box = firstTextBox(); box; box = box->nextTextBox()) {
        if (_y >= absy + box->root()->topOverflow() && _y < absy + box->root()->bottomOverflow()) {
            if (_x < absx + box->m_x + box->m_width) {
                // and the x coordinate is to the left of the right edge of this box
                // check to see if position goes in this box
                int offset = box->offsetForPosition(_x - absx);
                if (offset != -1) {
                    return Position(element(), offset + box->m_start);
                }
            }
            else if (!box->prevOnLine() && _x < absx + box->m_x)
                // box is first on line
                // and the x coordinate is to the left than the first text box left edge
                return Position(element(), box->m_start);
            else if (!box->nextOnLine() && _x >= absx + box->m_x + box->m_width)
                // box is last on line
                // and the x coordinate is to the right than the last text box right edge
                return Position(element(), box->m_start + box->m_len);
        }
    }
    
    return Position(element(), 0);
}

void RenderText::caretPos(int offset, bool override, int &_x, int &_y, int &width, int &height)
{
    if (!firstTextBox() || stringLength() == 0) {
        _x = _y = height = -1;
        return;
    }

    // Find the text box for the given offset
    InlineTextBox *box = 0;
    for (box = firstTextBox(); box; box = box->nextTextBox()) {
        if (offset <= box->m_start + box->m_len)
            break;
    }
    
    if (!box) {
        _x = _y = height = -1;
        return;
    }

    height = box->root()->bottomOverflow() - box->root()->topOverflow();
    _y = box->root()->topOverflow();

    const QFontMetrics &fm = metrics(box->isFirstLineStyle());
    QString string(str->s + box->m_start, box->m_len);
    long pos = offset - box->m_start; // the number of characters we are into the string
    _x = box->m_x + (fm.boundingRect(string, pos)).right();

#if 0
    // EDIT FIXME
    if (pos)
        _x += fm.rightBearing(*(str->s + box->m_start + offset));
#endif

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

void RenderText::paint(PaintInfo& i, int tx, int ty)
{
    if (i.phase != PaintActionForeground && i.phase != PaintActionSelection)
        return;
    
    if (!shouldPaintWithinRoot(i))
        return;
        
    if (style()->visibility() != VISIBLE || !firstTextBox())
        return;
    
    if (ty + firstTextBox()->yPos() > i.r.y() + i.r.height()) return;
    if (ty + lastTextBox()->yPos() + lastTextBox()->height() < i.r.y()) return;
    
    QPainter* p = i.p;
    RenderStyle* pseudoStyle = style(true);
    if (pseudoStyle == style()) pseudoStyle = 0;
    int d = style()->textDecorationsInEffect();
    bool isPrinting = (p->device()->devType() == QInternal::Printer);
    
    // Walk forward until we hit the first line that needs to be painted.
    InlineTextBox* s = firstTextBox();
    for (; s && !s->checkVerticalPoint(i.r.y(), ty, i.r.height()); s = s->nextTextBox());
    if (!s) return;
    
    // Now calculate startPos and endPos, for painting selection.
    // We paint selection while endPos > 0
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

    const Font *font = &style()->htmlFont();

#if APPLE_CHANGES
    // Do one pass for the selection, then another for the rest.
    bool haveSelection = startPos != endPos && !isPrinting && selectionState() != SelectionNone;
    if (!haveSelection && i.phase == PaintActionSelection) {
        // When only painting the selection, don't bother to paint if there is none.
        return;
    }

    InlineTextBox* startBox = s;
    for (int pass = 0; pass < (haveSelection ? 2 : 1); pass++) {
        s = startBox;
        bool drawSelectionBackground = haveSelection && pass == 0 && i.phase != PaintActionSelection;
        bool drawText = !haveSelection || pass == 1;
#endif

    // run until we find one that is outside the range, then we
    // know we can stop
    do {
        if (isPrinting)
        {
            if (ty+s->m_y+s->height() > i.r.y() + i.r.height())
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

        if (s->m_truncation == cFullTruncation)
            continue;
        
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
            bool paintSelectedTextOnly = (i.phase == PaintActionSelection);
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
                // FIXME: Handle RTL direction, handle reversed strings.  For now truncation can only be turned on
                // for non-reversed LTR strings.
                int endPoint = s->m_len;
                if (s->m_truncation != cNoTruncation)
                    endPoint = s->m_truncation - s->m_start;
                font->drawText(p, s->m_x + tx, s->m_y + ty + s->m_baseline,
                               str->s, str->l, s->m_start, endPoint,
                               s->m_toAdd, s->m_reversed ? QPainter::RTL : QPainter::LTR, style()->visuallyOrdered());
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
        
        if (d != TDNONE && i.phase == PaintActionForeground &&
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
            s->paintSelection(font, this, p, _style, tx, ty, startPos, endPos, selectionState() == SelectionInside);

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

    } while (((s = s->nextTextBox()) != 0) && s->checkVerticalPoint(i.r.y(), ty, i.r.height()));

#if APPLE_CHANGES
    } // end of for loop
#endif
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

void RenderText::trimmedMinMaxWidth(int& beginMinW, bool& beginWS, 
                                    int& endMinW, bool& endWS,
                                    bool& hasBreakableChar, bool& hasBreak,
                                    int& beginMaxW, int& endMaxW,
                                    int& minW, int& maxW, bool& stripFrontSpaces)
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
            
        if (ignoringSpaces || (i > 0 && c.unicode() == SOFT_HYPHEN)) // Ignore spaces and soft hyphens
            continue;
        
        int wordlen = 0;
        while (i+wordlen < len && str->s[i+wordlen] != '\n' && str->s[i+wordlen] != ' ' &&
               (i+wordlen == 0 || str->s[i+wordlen].unicode() != SOFT_HYPHEN) && // Skip soft hyphens
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
    if (!m_firstTextBox) return 0;
    int retval=6666666;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
	retval = kMin(retval, (int)box->m_x);
    return retval;
}

int RenderText::xPos() const
{
    return m_firstTextBox ? m_firstTextBox->m_x : 0;
}

int RenderText::yPos() const
{
    return m_firstTextBox ? m_firstTextBox->m_y : 0;
}

const QFont &RenderText::font()
{
    return style()->font();
}

void RenderText::setTextWithOffset(DOMStringImpl *text, uint offset, uint len, bool force)
{
    uint oldLen = str ? str->l : 0;
    uint newLen = text ? text->l : 0;
    int delta = newLen - oldLen;
    uint end = len ? offset+len-1 : offset;

    RootInlineBox* firstRootBox = 0;
    RootInlineBox* lastRootBox = 0;
    
    bool dirtiedLines = false;
    
    // Dirty all text boxes that include characters in between offset and offset+len.
    for (InlineTextBox* curr = firstTextBox(); curr; curr = curr->nextTextBox()) {
        // Text run is entirely before the affected range.
        if (curr->end() < offset)
            continue;
        
        // Text run is entirely after the affected range.
        if (curr->start() > end) {
            curr->offsetRun(delta);
            RootInlineBox* root = curr->root();
            if (!firstRootBox) {
                firstRootBox = root;
                if (!dirtiedLines) { // The affected area was in between two runs. Go ahead and mark the root box of the run after the affected area as dirty.
                    firstRootBox->markDirty();
                    dirtiedLines = true;
                }
            }
            lastRootBox = root;
        }
        else if (curr->end() >= offset && curr->end() <= end) {
            curr->dirtyLineBoxes(); // Text run overlaps with the left end of the affected range.
            dirtiedLines = true;
        }
        else if (curr->start() <= offset && curr->end() >= end) {
            curr->dirtyLineBoxes(); // Text run subsumes the affected range.
            dirtiedLines = true;
        }
        else if (curr->start() <= end && curr->end() >= end) {
            curr->dirtyLineBoxes(); // Text run overlaps with right end of the affected range.
            dirtiedLines = true;
        }
    }
    
    // Now we have to walk all of the clean lines and adjust their cached line break information
    // to reflect our updated offsets.
    if (lastRootBox)
        lastRootBox = lastRootBox->nextRootBox();
    if (firstRootBox) {
        RootInlineBox* prev = firstRootBox->prevRootBox();
        if (prev)
            firstRootBox = prev;
    }
    for (RootInlineBox* curr = firstRootBox; curr && curr != lastRootBox; curr = curr->nextRootBox()) {
        if (!curr->isDirty() && curr->lineBreakObj() == this && curr->lineBreakPos() > end)
            curr->setLineBreakPos(curr->lineBreakPos()+delta);
    }
    
    m_linesDirty = dirtiedLines;
    setText(text, force);
}

void RenderText::setText(DOMStringImpl *text, bool force)
{
    if (!text)
        return;
    if (!force && str == text)
        return;
    if (str)
        str->deref();

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

    setNeedsLayoutAndMinMaxRecalc();
    
#ifdef BIDI_DEBUG
    QConstString cstr(str->s, str->l);
    kdDebug( 6040 ) << "RenderText::setText( " << cstr.string().length() << " ) '" << cstr.string() << "'" << endl;
#endif
}

int RenderText::height() const
{
    // FIXME: Why use line-height? Shouldn't we be adding in the height of the last text box? -dwh
    int retval = 0;
    if (firstTextBox())
        retval = lastTextBox()->m_y + lineHeight(false) - firstTextBox()->m_y;
    return retval;
}

short RenderText::lineHeight(bool firstLine, bool) const
{
    // Always use the interior line height of the parent (e.g., if our parent is an inline block).
    return parent()->lineHeight(firstLine, true);
}

short RenderText::baselinePosition( bool firstLine, bool ) const
{
    const QFontMetrics &fm = metrics( firstLine );
    return fm.ascent() +
        ( lineHeight( firstLine ) - fm.height() ) / 2;
}

void RenderText::dirtyLineBoxes(bool fullLayout, bool)
{
    if (fullLayout)
        deleteTextBoxes();
    else if (!m_linesDirty) {
        for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
            box->dirtyLineBoxes();
    }
    m_linesDirty = false;
}

InlineBox* RenderText::createInlineBox(bool, bool isRootLineBox, bool)
{
    KHTMLAssert(!isRootLineBox);
    InlineTextBox* textBox = new (renderArena()) InlineTextBox(this);
    if (!m_firstTextBox)
        m_firstTextBox = m_lastTextBox = textBox;
    else {
        m_lastTextBox->setNextLineBox(textBox);
        textBox->setPreviousLineBox(m_lastTextBox);
        m_lastTextBox = textBox;
    }
    return textBox;
}

void RenderText::position(InlineBox* box, int from, int len, bool reverse)
{
    InlineTextBox *s = static_cast<InlineTextBox*>(box);
    
    // ### should not be needed!!!
    if (len == 0) {
        // We want the box to be destroyed.
        s->remove();
        s->detach(renderArena());
        m_firstTextBox = m_lastTextBox = 0;
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

int RenderText::width() const
{
    int w;
    int minx = 100000000;
    int maxx = 0;
    // slooow
    for (InlineTextBox* s = firstTextBox(); s; s = s->nextTextBox()) {
        if(s->m_x < minx)
            minx = s->m_x;
        if(s->m_x + s->m_width > maxx)
            maxx = s->m_x + s->m_width;
    }

    w = kMax(0, maxx-minx);

    return w;
}

QRect RenderText::getAbsoluteRepaintRect()
{
    RenderObject *cb = containingBlock();
    return cb->getAbsoluteRepaintRect();
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

long RenderText::caretMinOffset() const
{
    if (!firstTextBox()) 
        return 0;
    // EDIT FIXME: it is *not* guaranteed that the first run contains the lowest offset
    // Either make this a linear search (slow),
    // or maintain an index (needs much mem),
    // or calculate and store it in bidi.cpp (needs calculation even if not needed)
    return firstTextBox()->m_start;
}

long RenderText::caretMaxOffset() const
{
    if (!firstTextBox()) 
        return str->l;
    // EDIT FIXME: it is *not* guaranteed that the last run contains the highest offset
    // Either make this a linear search (slow),
    // or maintain an index (needs much mem),
    // or calculate and store it in bidi.cpp (needs calculation even if not needed)
    return lastTextBox()->m_start + lastTextBox()->m_len;
}

unsigned long RenderText::caretMaxRenderedOffset() const
{
    int l = 0;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        l += box->m_len;
    return l;
}

InlineBox *RenderText::inlineBox(long offset)
{
    for (InlineTextBox *box = firstTextBox(); box; box = box->nextTextBox()) {
        if (offset >= box->m_start && offset <= box->m_start + box->m_len) {
            return box;
        }
        else if (offset < box->m_start) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not rendered.
            return box->prevTextBox() ? box->prevTextBox() : firstTextBox();
        }
    }
    
    return 0;
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
