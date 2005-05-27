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

#include "rendering/render_text.h"

#include "rendering/render_canvas.h"
#include "rendering/render_object.h"
#include "rendering/break_lines.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_position.h"
#include "render_arena.h"

#include "misc/loader.h"

#include "khtml_part.h"

#include <qpainter.h>
#include <kdebug.h>
#include <assert.h>

#include <unicode/ubrk.h>
#include <unicode/uloc.h>
#include <unicode/utypes.h>
#include <unicode/parseerr.h>

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

RenderText* InlineTextBox::textObject()
{
    return static_cast<RenderText*>(m_object);
}

bool InlineTextBox::checkVerticalPoint(int _y, int _ty, int _h)
{
    int topY = m_y;
    int bottomY = m_y + m_height;
    if (root()->hasSelectedChildren()) {
        topY = kMin(root()->selectionTop(), topY);
        bottomY = kMax(bottomY, root()->bottomOverflow());
    }
    if ((_ty + topY >= _y + _h) || (_ty + bottomY <= _y))
        return false;
    return true;
}

bool InlineTextBox::isSelected(int startPos, int endPos) const
{
    int sPos = kMax(startPos - m_start, 0);
    int ePos = kMin(endPos - m_start, (int)m_len);
    return (sPos < ePos);
}

RenderObject::SelectionState InlineTextBox::selectionState()
{
    RenderObject::SelectionState state = object()->selectionState();
    if (state == RenderObject::SelectionStart || state == RenderObject::SelectionEnd ||
        state == RenderObject::SelectionBoth) {
        int startPos, endPos;
        object()->selectionStartEnd(startPos, endPos);
        
        bool start = (state != RenderObject::SelectionEnd && startPos >= m_start && startPos < m_start + m_len);
        bool end = (state != RenderObject::SelectionStart && endPos > m_start && endPos <= m_start + m_len);
        if (start && end)
            state = RenderObject::SelectionBoth;
        else if (start)
            state = RenderObject::SelectionStart;
        else if (end)
            state = RenderObject::SelectionEnd;
        else if ((state == RenderObject::SelectionEnd || startPos < m_start) &&
                 (state == RenderObject::SelectionStart || endPos > m_start + m_len))
            state = RenderObject::SelectionInside;
    }
    return state;
}

QRect InlineTextBox::selectionRect(int tx, int ty, int startPos, int endPos)
{
    int sPos = kMax(startPos - m_start, 0);
    int ePos = kMin(endPos - m_start, (int)m_len);
    
    if (sPos >= ePos)
        return QRect();

    RootInlineBox* rootBox = root();
    int selStart = m_reversed ? m_x + m_width : m_x;
    int selEnd = selStart;
    int selTop = rootBox->selectionTop();
    int selHeight = rootBox->selectionHeight();
    
    // FIXME: For justified text, just return the entire text box's rect.  At the moment there's still no easy
    // way to get the width of a run including the justification padding.
    if (sPos > 0 && !m_toAdd) {
        // The selection begins in the middle of our run.
        int w = textObject()->width(m_start, sPos, m_firstLine);
        if (m_reversed)
            selStart -= w;
        else
            selStart += w;
    }

    if (m_toAdd || (sPos == 0 && ePos == m_len)) {
        if (m_reversed)
            selEnd = m_x;
        else
            selEnd = m_x + m_width;
    }
    else {
        // Our run is partially selected, and so we have to actually do a measurement.
        int w = textObject()->width(sPos + m_start, ePos - sPos, m_firstLine);
        if (m_reversed)
            selEnd = selStart - w;
        else
            selEnd = selStart + w;
    }

    int selLeft = m_reversed ? selEnd : selStart;
    int selRight = m_reversed ? selStart : selEnd;

    return QRect(selLeft + tx, selTop + ty, selRight - selLeft, selHeight);
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

bool InlineTextBox::nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty)
{
    if (object()->isBR())
        return false;

    QRect rect(tx + m_x, ty + m_y, m_width, m_height);
    if (m_truncation != cFullTruncation && 
        object()->style()->visibility() == VISIBLE && rect.contains(x, y)) {
        object()->setInnerNode(i);
        return true;
    }
    return false;
}

void InlineTextBox::paint(RenderObject::PaintInfo& i, int tx, int ty)
{
    if (object()->isBR() || !object()->shouldPaintWithinRoot(i) || object()->style()->visibility() != VISIBLE ||
        m_truncation == cFullTruncation || i.phase == PaintActionOutline)
        return;

    int xPos = tx + m_x;
    int w = width();
    if ((xPos >= i.r.x() + i.r.width()) || (xPos + w <= i.r.x()))
        return;
        
    bool isPrinting = (i.p->device()->devType() == QInternal::Printer);

    // Determine whether or not we're selected.
    bool haveSelection = !isPrinting && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && i.phase == PaintActionSelection)
        // When only painting the selection, don't bother to paint if there is none.
        return;

    // Determine whether or not we have marked text.
    RangeImpl *markedTextRange = KWQ(object()->document()->part())->markedTextRange();
    int exception = 0;
    bool haveMarkedText = markedTextRange && markedTextRange->startContainer(exception) == object()->node();
    bool markedTextUsesUnderlines = KWQ(object()->document()->part())->markedTextUsesUnderlines();

    // Set our font.
    RenderStyle* styleToUse = object()->style(m_firstLine);
    int d = styleToUse->textDecorationsInEffect();
    if (styleToUse->font() != i.p->font())
        i.p->setFont(styleToUse->font());
    const Font *font = &styleToUse->htmlFont();

    // 1. Paint backgrounds behind text if needed.  Examples of such backgrounds include selection
    // and marked text.
    if ((haveSelection || haveMarkedText) && !markedTextUsesUnderlines && i.phase != PaintActionSelection && !isPrinting) {
        if (haveMarkedText)
            paintMarkedTextBackground(i.p, tx, ty, styleToUse, font, markedTextRange->startOffset(exception), markedTextRange->endOffset(exception));

        if (haveSelection)
            paintSelection(i.p, tx, ty, styleToUse, font);
    }

    // 2. Now paint the foreground, including text and decorations like underline/overline (in quirks mode only).
    if (m_len <= 0) return;
    QValueList<DocumentMarker> markers = object()->document()->markersForNode(object()->node());
    QValueListIterator <DocumentMarker> markerIt = markers.begin();

    QValueList<KWQKHTMLPart::MarkedTextUnderline> underlines;
    if (haveMarkedText && markedTextUsesUnderlines) {
        underlines = KWQ(object()->document()->part())->markedTextUnderlines();
    }
    QValueListIterator<KWQKHTMLPart::MarkedTextUnderline> underlineIt = underlines.begin();

    QColor textColor = styleToUse->color();
    
    // Make the text color legible against a white background
    if (styleToUse->forceBackgroundsToWhite())
        textColor = correctedTextColor(textColor, Qt::white);

    if (textColor != i.p->pen().color())
        i.p->setPen(textColor);

    // Set a text shadow if we have one.
    // FIXME: Support multiple shadow effects.  Need more from the CG API before
    // we can do this.
    bool setShadow = false;
    if (styleToUse->textShadow()) {
        i.p->setShadow(styleToUse->textShadow()->x, styleToUse->textShadow()->y,
                        styleToUse->textShadow()->blur, styleToUse->textShadow()->color);
        setShadow = true;
    }

    bool paintSelectedTextOnly = (i.phase == PaintActionSelection);
    bool paintSelectedTextSeparately = false; // Whether or not we have to do multiple paints.  Only
                                              // necessary when a custom ::selection foreground color is applied.
    QColor selectionColor = i.p->pen().color();
    ShadowData* selectionTextShadow = 0;
    if (haveSelection) {
        RenderStyle* pseudoStyle = object()->getPseudoStyle(RenderStyle::SELECTION);
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
        // paint all the text
        // FIXME: Handle RTL direction, handle reversed strings.  For now truncation can only be turned on
        // for non-reversed LTR strings.
        int endPoint = m_len;
        if (m_truncation != cNoTruncation)
            endPoint = m_truncation - m_start;
        font->drawText(i.p, m_x + tx, m_y + ty + m_baseline,
                       textObject()->string()->s, textObject()->string()->l, m_start, endPoint,
                       m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, styleToUse->visuallyOrdered());
    } else {
        int sPos, ePos;
        selectionStartEnd(sPos, ePos);
        if (paintSelectedTextSeparately) {
            // paint only the text that is not selected
            if (sPos >= ePos) {
                font->drawText(i.p, m_x + tx, m_y + ty + m_baseline,
                               textObject()->string()->s, textObject()->string()->l, m_start, m_len,
                               m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, styleToUse->visuallyOrdered());
            } else {
                if (sPos - 1 >= 0) {
                    font->drawText(i.p, m_x + tx, m_y + ty + m_baseline, textObject()->string()->s,
                                   textObject()->string()->l, m_start, m_len,
                                   m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, styleToUse->visuallyOrdered(), 0, sPos);
                }
                if (ePos < m_start + m_len) {
                    font->drawText(i.p, m_x + tx, m_y + ty + m_baseline, textObject()->string()->s,
                                   textObject()->string()->l, m_start, m_len,
                                   m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, styleToUse->visuallyOrdered(), ePos, -1);
                }
            }
        }
            
        if (sPos < ePos) {
            // paint only the text that is selected
            if (selectionColor != i.p->pen().color())
                i.p->setPen(selectionColor);
            
            if (selectionTextShadow)
                i.p->setShadow(selectionTextShadow->x,
                               selectionTextShadow->y,
                               selectionTextShadow->blur,
                               selectionTextShadow->color);
            font->drawText(i.p, m_x + tx, m_y + ty + m_baseline, textObject()->string()->s,
                           textObject()->string()->l, m_start, m_len,
                           m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, styleToUse->visuallyOrdered(), sPos, ePos);
            if (selectionTextShadow)
                i.p->clearShadow();
        }
    }

    // Paint decorations
    if (d != TDNONE && i.phase != PaintActionSelection && styleToUse->htmlHacks()) {
        i.p->setPen(styleToUse->color());
        paintDecoration(i.p, tx, ty, d);
    }

    // Draw any doc markers that touch this run
    // Note end() points at the last char, not one past it like endOffset and ranges do
    if (i.phase != PaintActionSelection) {
        for ( ; markerIt != markers.end(); markerIt++) {
            DocumentMarker marker = *markerIt;

            if (marker.endOffset <= start())
                // marker is completely before this run.  This might be a marker that sits before the
                // first run we draw, or markers that were within runs we skipped due to truncation.
                continue;
            
            if (marker.startOffset <= end()) {
                // marker intersects this run.  Paint it.
                paintMarker(i.p, tx, ty, marker);
                if (marker.endOffset > end() + 1)
                    // marker also runs into the next run. Bail now, no more marker advancement.
                    break;
            } else
                // marker is completely after this run, bail.  A later run will paint it.
                break;
        }


        for ( ; underlineIt != underlines.end(); underlineIt++) {
            KWQKHTMLPart::MarkedTextUnderline underline = *underlineIt;

            if (underline.endOffset <= start())
                // underline is completely before this run.  This might be an underlinethat sits
                // before the first run we draw, or underlines that were within runs we skipped 
                // due to truncation.
                continue;
            
            if (underline.startOffset <= end()) {
                // underline intersects this run.  Paint it.
                paintMarkedTextUnderline(i.p, tx, ty, underline);
                if (underline.endOffset > end() + 1)
                    // underline also runs into the next run. Bail now, no more marker advancement.
                    break;
            } else
                // underline is completely after this run, bail.  A later run will paint it.
                break;
        }



    }

    if (setShadow)
        i.p->clearShadow();
}

void InlineTextBox::selectionStartEnd(int& sPos, int& ePos)
{
    int startPos, endPos;
    if (object()->selectionState() == RenderObject::SelectionInside) {
        startPos = 0;
        endPos = textObject()->string()->l;
    } else {
        textObject()->selectionStartEnd(startPos, endPos);
        if (object()->selectionState() == RenderObject::SelectionStart)
            endPos = textObject()->string()->l;
        else if (object()->selectionState() == RenderObject::SelectionEnd)
            startPos = 0;
    }

    sPos = kMax(startPos - m_start, 0);
    ePos = kMin(endPos - m_start, (int)m_len);
}

void InlineTextBox::paintSelection(QPainter* p, int tx, int ty, RenderStyle* style, const Font* f)
{
    // See if we have a selection to paint at all.
    int sPos, ePos;
    selectionStartEnd(sPos, ePos);
    if (sPos >= ePos)
        return;

    // Macintosh-style text highlighting is to draw with a particular background color, not invert.
    QColor textColor = style->color();
    QColor c = object()->selectionColor(p);
    if (!c.isValid())
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.  This should basically never happen, since the selection has transparency.
    if (textColor == c)
        c = QColor(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    p->save();
    p->setPen(c); // Don't draw text at all!
    RootInlineBox* r = root();
    int x = m_x + tx;
    int y = r->selectionTop();
    int h = r->selectionHeight();
    f->drawHighlightForText(p, x, y + ty, h,
                            textObject()->str->s, textObject()->str->l, m_start, m_len,
                            m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, style->visuallyOrdered(), sPos, ePos, c);
    p->restore();
}

void InlineTextBox::paintMarkedTextBackground(QPainter* p, int tx, int ty, RenderStyle* style, const Font* f, int startPos, int endPos)
{
    int offset = m_start;
    int sPos = kMax(startPos - offset, 0);
    int ePos = kMin(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

    p->save();

    QColor c = QColor(225, 221, 85);
    
    p->setPen(c); // Don't draw text at all!

    RootInlineBox* r = root();
    int x = m_x + tx;
    int y = r->selectionTop();
    int h = r->selectionHeight();
    f->drawHighlightForText(p, x, y + ty, h, textObject()->str->s, textObject()->str->l, m_start, m_len,
		m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, style->visuallyOrdered(), sPos, ePos, c);
    p->restore();
}

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

void InlineTextBox::paintMarker(QPainter *pt, int _tx, int _ty, DocumentMarker marker)
{
    _tx += m_x;
    _ty += m_y;

    if (m_truncation == cFullTruncation)
        return;
    
    int start = 0;                  // start of line to draw, relative to _tx
    int width = m_width;            // how much line to draw
    bool useWholeWidth = true;
    ulong paintStart = m_start;
    ulong paintEnd = end()+1;      // end points at the last char, not past it
    if (paintStart <= marker.startOffset) {
        paintStart = marker.startOffset;
        useWholeWidth = false;
        start = static_cast<RenderText*>(m_object)->width(m_start, paintStart - m_start, m_firstLine);
    }
    if (paintEnd != marker.endOffset) {      // end points at the last char, not past it
        paintEnd = kMin(paintEnd, marker.endOffset);
        useWholeWidth = false;
    }
    if (m_truncation != cNoTruncation) {
        paintEnd = kMin(paintEnd, (ulong)m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = static_cast<RenderText*>(m_object)->width(paintStart, paintEnd - paintStart, m_firstLine);
    }

    // IMPORTANT: The misspelling underline is not considered when calculating the text bounds, so we have to
    // make sure to fit within those bounds.  This means the top pixel(s) of the underline will overlap the
    // bottom pixel(s) of the glyphs in smaller font sizes.  The alternatives are to increase the line spacing (bad!!)
    // or decrease the underline thickness.  The overlap is actually the most useful, and matches what AppKit does.
    // So, we generally place the underline at the bottom of the text, but in larger fonts that's not so good so
    // we pin to two pixels under the baseline.
    int lineThickness = pt->misspellingLineThickness();
    int descent = m_height - m_baseline;
    int underlineOffset;
    if (descent <= (2 + lineThickness)) {
        // place the underline at the very bottom of the text in small/medium fonts
        underlineOffset = m_height - lineThickness;
    } else {
        // in larger fonts, tho, place the underline up near the baseline to prevent big gap
        underlineOffset = m_baseline + 2;
    }
    pt->drawLineForMisspelling(_tx + start, _ty + underlineOffset, width);
}

void InlineTextBox::paintMarkedTextUnderline(QPainter *pt, int _tx, int _ty, KWQKHTMLPart::MarkedTextUnderline underline)
{
    _tx += m_x;
    _ty += m_y;

    if (m_truncation == cFullTruncation)
        return;
    
    int start = 0;                  // start of line to draw, relative to _tx
    int width = m_width;            // how much line to draw
    bool useWholeWidth = true;
    ulong paintStart = m_start;
    ulong paintEnd = end()+1;      // end points at the last char, not past it
    if (paintStart <= underline.startOffset) {
        paintStart = underline.startOffset;
        useWholeWidth = false;
        start = static_cast<RenderText*>(m_object)->width(m_start, paintStart - m_start, m_firstLine);
    }
    if (paintEnd != underline.endOffset) {      // end points at the last char, not past it
        paintEnd = kMin(paintEnd, (ulong)underline.endOffset);
        useWholeWidth = false;
    }
    if (m_truncation != cNoTruncation) {
        paintEnd = kMin(paintEnd, (ulong)m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = static_cast<RenderText*>(m_object)->width(paintStart, paintEnd - paintStart, m_firstLine);
    }

    int underlineOffset = m_height - 3;
    pt->setPen(QPen(underline.color, underline.thick ? 2 : 0));
    pt->drawLineForText(_tx + start, _ty, underlineOffset, width);
}

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

static UBreakIterator *getCharacterBreakIterator(const DOMStringImpl *i)
{
    // The locale is currently ignored when determining character cluster breaks.  This may change
    // in the future (according to Deborah Goldsmith).
    static bool createdIterator = false;
    static UBreakIterator *iterator;
    UErrorCode status;
    if (!createdIterator) {
        status = U_ZERO_ERROR;
        iterator = ubrk_open(UBRK_CHARACTER, "en_us", NULL, 0, &status);
        createdIterator = true;
    }
    if (!iterator) {
        return NULL;
    }
    status = U_ZERO_ERROR;
    ubrk_setText(iterator, reinterpret_cast<const UChar *>(i->s), i->l, &status);
    if (status != U_ZERO_ERROR) {
        return NULL;
    }
    return iterator;
}

long RenderText::previousOffset (long current) const
{
    UBreakIterator *iterator = getCharacterBreakIterator(str);
    if (iterator) {
        return ubrk_preceding(iterator, current);
    }
    return current - 1;
}

long RenderText::nextOffset (long current) const
{
    UBreakIterator *iterator = getCharacterBreakIterator(str);
    if (iterator) {
        return ubrk_following(iterator, current);
    }
    return current + 1;
}

int InlineTextBox::offsetForPosition(int _x, bool includePartialGlyphs) const
{
    RenderText* text = static_cast<RenderText*>(m_object);
    const Font* f = text->htmlFont(m_firstLine);
    return f->checkSelectionPoint(text->str->s, text->str->l, m_start, m_len, m_toAdd, _x - m_x, m_reversed, includePartialGlyphs);
}

int InlineTextBox::positionForOffset(int offset) const
{
    RenderText *text = static_cast<RenderText *>(m_object);
    const QFontMetrics &fm = text->metrics(m_firstLine);

    int left;
    if (m_reversed) {
	long len = m_start + m_len - offset;
	QString string(text->str->s + offset, len);
	left = m_x + fm.boundingRect(string, len).right();
    } else {
	long len = offset - m_start;
	QString string(text->str->s + m_start, len);
	left = m_x + fm.boundingRect(string, len).right();
    }
    // FIXME: Do we need to add rightBearing here?
    return left;
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
        if (firstTextBox()) {
            if (isBR()) {
                RootInlineBox* next = firstTextBox()->root()->nextRootBox();
                if (next)
                    next->markDirty();
            }
            for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
                box->remove();
        }
        else if (parent())
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

InlineTextBox* RenderText::findNextInlineTextBox(int offset, int &pos) const
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

VisiblePosition RenderText::positionForCoordinates(int _x, int _y)
{
    if (!firstTextBox() || stringLength() == 0)
        return VisiblePosition(element(), 0, DOWNSTREAM);

    int absx, absy;
    containingBlock()->absolutePosition(absx, absy);

    if (firstTextBox() && _y < absy + firstTextBox()->root()->bottomOverflow() && _x < absx + firstTextBox()->m_x) {
        // at the y coordinate of the first line or above
        // and the x coordinate is to the left than the first text box left edge
        return VisiblePosition(element(), firstTextBox()->m_start, DOWNSTREAM);
    }

    if (lastTextBox() && _y >= absy + lastTextBox()->root()->topOverflow() && _x >= absx + lastTextBox()->m_x + lastTextBox()->m_width) {
        // at the y coordinate of the last line or below
        // and the x coordinate is to the right than the last text box right edge
        return VisiblePosition(element(), lastTextBox()->m_start + lastTextBox()->m_len, DOWNSTREAM);
    }

    for (InlineTextBox *box = firstTextBox(); box; box = box->nextTextBox()) {
        if (_y >= absy + box->root()->topOverflow() && _y < absy + box->root()->bottomOverflow()) {
            if (_x < absx + box->m_x + box->m_width) {
                // and the x coordinate is to the left of the right edge of this box
                // check to see if position goes in this box
                int offset = box->offsetForPosition(_x - absx);
                if (offset != -1) {
                    EAffinity affinity = offset >= box->m_len && !box->nextOnLine() ? UPSTREAM : DOWNSTREAM;
                    return VisiblePosition(element(), offset + box->m_start, affinity);
                }
            }
            else if (!box->prevOnLine() && _x < absx + box->m_x) {
                // box is first on line
                // and the x coordinate is to the left of the first text box left edge
                return VisiblePosition(element(), box->m_start, DOWNSTREAM);
            }
            else if (!box->nextOnLine() && _x >= absx + box->m_x + box->m_width)
                // box is last on line
                // and the x coordinate is to the right of the last text box right edge
                return VisiblePosition(element(), box->m_start + box->m_len, UPSTREAM);
        }
    }
    
    return VisiblePosition(element(), 0, DOWNSTREAM);
}

static RenderObject *firstRendererOnNextLine(InlineBox *box)
{
    if (!box)
        return 0;

    RootInlineBox *root = box->root();
    if (!root)
        return 0;
        
    if (root->endsWithBreak())
        return 0;
    
    RootInlineBox *nextRoot = root->nextRootBox();
    if (!nextRoot)
        return 0;
    
    InlineBox *firstChild = nextRoot->firstChild();
    if (!firstChild)
        return 0;

    return firstChild->object();
}

static RenderObject *lastRendererOnPrevLine(InlineBox *box)
{
    if (!box)
        return 0;
    
    RootInlineBox *root = box->root();
    if (!root)
        return 0;
    
    if (root->endsWithBreak())
        return 0;
    
    RootInlineBox *prevRoot = root->prevRootBox();
    if (!prevRoot)
        return 0;
    
    InlineBox *lastChild = prevRoot->lastChild();
    if (!lastChild)
        return 0;
    
    return lastChild->object();
}

QRect RenderText::caretRect(int offset, EAffinity affinity, int *extraWidthToEndOfLine)
{
    if (!firstTextBox() || stringLength() == 0) {
        return QRect();
    }

    // Find the text box for the given offset
    InlineTextBox *box = 0;
    for (box = firstTextBox(); box; box = box->nextTextBox()) {
        if ((offset >= box->m_start) && (offset <= box->m_start + box->m_len)) {
            // Check if downstream affinity would make us move to the next line.
            InlineTextBox *nextBox = box->nextTextBox();
            if (offset == box->m_start + box->m_len && affinity == DOWNSTREAM  && nextBox &&  !box->nextOnLine()) {
                // We're at the end of a line broken on a word boundary and affinity is downstream.
                // Try to jump down to the next line.
                if (nextBox) {
                    // Use the next text box
                    box = nextBox;
                    offset = box->m_start;
                } else {
                    // Look on the next line
                    RenderObject *object = firstRendererOnNextLine(box);
                    if (object)
                        return object->caretRect(0, affinity);
                }
            } else {
		InlineTextBox *prevBox = box->prevTextBox();
		if (offset == box->m_start && affinity == UPSTREAM && prevBox && !box->prevOnLine()) {
		    if (prevBox) {
			box = prevBox;
			offset = box->m_start + box->m_len;
		    } else {
			RenderObject *object = lastRendererOnPrevLine(box);
			if (object)
			    return object->caretRect(0, affinity);
		    }
		}
	    }
            break;
        }
    }
    
    if (!box) {
        return QRect();
    }

    int height = box->root()->bottomOverflow() - box->root()->topOverflow();
    int top = box->root()->topOverflow();

    int left = box->positionForOffset(offset);

    // FIXME: should we use the width of the root inline box or the
    // width of the containing block for this?
    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = (box->root()->width() + box->root()->xPos()) - (left + 1);

    int absx, absy;
    absolutePosition(absx,absy);
    left += absx;
    top += absy;

    // FIXME: Need the +1 to match caret position of other programs on Macintosh.
    // Would be better to somehow derive it once we understand exactly why it's needed.
    left += 1;

    RenderBlock *cb = containingBlock();
    int availableWidth = cb->lineWidth(top);
    if (style()->whiteSpace() == NORMAL)
        left = kMin(left, absx + box->m_x + availableWidth - 1);
    
    return QRect(left, top, 1, height);
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

void RenderText::setSelectionState(SelectionState s)
{
    InlineTextBox* box;
    
    m_selectionState = s;
    if (s == SelectionStart || s == SelectionEnd || s == SelectionBoth) {
        int startPos, endPos;
        selectionStartEnd(startPos, endPos);
        if(selectionState() == SelectionStart) {
            endPos = str->l;
            
            // to handle selection from end of text to end of line
            if (startPos != 0 && startPos == endPos) {
                startPos = endPos - 1;
            }
        } else if(selectionState() == SelectionEnd)
            startPos = 0;
        
        for (box = firstTextBox(); box; box = box->nextTextBox()) {
            if (box->isSelected(startPos, endPos)) {
                RootInlineBox* line = box->root();
                if (line)
                    line->setHasSelectedChildren(true);
            }
        }
    }
    else {
        for (box = firstTextBox(); box; box = box->nextTextBox()) {
            RootInlineBox* line = box->root();
            if (line)
                line->setHasSelectedChildren(s == SelectionInside);
        }
    }
    
    containingBlock()->setSelectionState(s);
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
        if (curr->lineBreakObj() == this && curr->lineBreakPos() > end)
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

#ifdef APPLE_CHANGES
    m_allAsciiChecked = false;
#endif

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

QRect RenderText::selectionRect()
{
    QRect rect;
    if (selectionState() == SelectionNone)
        return rect;
    RenderBlock* cb =  containingBlock();
    if (!cb)
        return rect;
    
    // Now calculate startPos and endPos for painting selection.
    // We include a selection while endPos > 0
    int startPos, endPos;
    if (selectionState() == SelectionInside) {
        // We are fully selected.
        startPos = 0;
        endPos = str->l;
    } else {
        selectionStartEnd(startPos, endPos);
        if (selectionState() == SelectionStart)
            endPos = str->l;
        else if (selectionState() == SelectionEnd)
            startPos = 0;
    }
    
    if (startPos == endPos)
        return rect;

    int absx, absy;
    cb->absolutePosition(absx, absy);
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        QRect r = box->selectionRect(absx, absy, startPos, endPos);
        if (!r.isEmpty()) {
            if (rect.isEmpty())
                rect = r;
            else
                rect = rect.unite(r);
        }
    }

    return rect;
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
    InlineTextBox *box = firstTextBox();
    if (!box)
        return 0;
    int minOffset = box->m_start;
    for (box = box->nextTextBox(); box; box = box->nextTextBox())
        minOffset = kMin(minOffset, box->m_start);
    return minOffset;
}

long RenderText::caretMaxOffset() const
{
    InlineTextBox* box = lastTextBox();
    if (!box) 
        return str->l;
    int maxOffset = box->m_start + box->m_len;
    for (box = box->prevTextBox(); box; box = box->prevTextBox())
	maxOffset = kMax(maxOffset,box->m_start + box->m_len);
    return maxOffset;
}

unsigned long RenderText::caretMaxRenderedOffset() const
{
    int l = 0;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        l += box->m_len;
    return l;
}

InlineBox *RenderText::inlineBox(long offset, EAffinity affinity)
{
    for (InlineTextBox *box = firstTextBox(); box; box = box->nextTextBox()) {
        if (offset >= box->m_start && offset <= box->m_start + box->m_len) {
            if (affinity == DOWNSTREAM && box->nextTextBox() && offset == box->m_start + box->m_len)
                return box->nextTextBox();
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
