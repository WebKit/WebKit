/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "InlineTextBox.h"

#include "break_lines.h"
#include "kxmlcore/AlwaysInline.h"
#include "render_arena.h"
#include "RenderBlock.h"
#include "xml/dom2_rangeimpl.h"
#include "DocumentImpl.h"
#include "Pen.h"

#include "Frame.h"

using namespace DOM;

namespace khtml {

#ifndef NDEBUG
static bool inInlineTextBoxDetach;
#endif

void InlineTextBox::destroy(RenderArena* renderArena)
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
    
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
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

IntRect InlineTextBox::selectionRect(int tx, int ty, int startPos, int endPos)
{
    int sPos = kMax(startPos - m_start, 0);
    int ePos = kMin(endPos - m_start, (int)m_len);
    
    if (sPos >= ePos)
        return IntRect();

    RootInlineBox* rootBox = root();
    RenderText* textObj = textObject();
    int selTop = rootBox->selectionTop();
    int selHeight = rootBox->selectionHeight();
    const Font *f = textObj->htmlFont(m_firstLine);

    IntRect r = f->selectionRectForText(tx + m_x, ty + selTop, selHeight, textObj->tabWidth(), textPos(), textObj->str->s, textObj->str->l, m_start, m_len, m_toAdd, m_reversed, m_dirOverride, sPos, ePos);
    if (r.left() > tx + m_x + m_width)
        r.setWidth(0);
    else if (r.right() > tx + m_x + m_width)
        r.setWidth(tx + m_x + m_width - r.left());
    return r;
}

void InlineTextBox::deleteLine(RenderArena* arena)
{
    static_cast<RenderText*>(m_object)->removeTextBox(this);
    destroy(arena);
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
            return m_x + static_cast<RenderText*>(m_object)->width(m_start, offset, textPos(), m_firstLine);
        }
    }
    else {
        // FIXME: Support RTL truncation someday, including both modes (when the leftmost run on the line is either RTL or LTR)
    }
    return -1;
}

static int
simpleDifferenceBetweenColors(Color c1, Color c2)
{
    // a distance could be computed by squaring the differences between components, but
    // this is faster and so far seems good enough for our purposes.
    return abs(c1.red() - c2.red()) + abs(c1.green() - c2.green()) + abs(c1.blue() - c2.blue());
}

static Color 
correctedTextColor(Color textColor, Color backgroundColor) 
{
    // Adjust the text color if it is too close to the background color,
    // by darkening or lightening it to move it further away.
    
    int d = simpleDifferenceBetweenColors(textColor, backgroundColor);
    // semi-arbitrarily chose 255 value here after a few tests; 
    if (d > 255) {
        return textColor;
    }
    
    int distanceFromWhite = simpleDifferenceBetweenColors(textColor, Color::white);
    int distanceFromBlack = simpleDifferenceBetweenColors(textColor, Color::black);

    if (distanceFromWhite < distanceFromBlack) {
        return textColor.dark();
    }
    
    return textColor.light();
}

bool InlineTextBox::nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty)
{
    if (object()->isBR())
        return false;

    IntRect rect(tx + m_x, ty + m_y, m_width, m_height);
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
        
    bool isPrinting = i.p->printing();
    
    // Determine whether or not we're selected.
    bool haveSelection = !isPrinting && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && i.phase == PaintActionSelection)
        // When only painting the selection, don't bother to paint if there is none.
        return;

    // Determine whether or not we have marked text.
    RangeImpl *markedTextRange = object()->document()->frame()->markedTextRange();
    int exception = 0;
    bool haveMarkedText = markedTextRange && markedTextRange->startContainer(exception) == object()->node();
    bool markedTextUsesUnderlines = object()->document()->frame()->markedTextUsesUnderlines();

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

    QValueList<MarkedTextUnderline> underlines;
    if (haveMarkedText && markedTextUsesUnderlines) {
        underlines = object()->document()->frame()->markedTextUnderlines();
    }
    QValueListIterator<MarkedTextUnderline> underlineIt = underlines.begin();

    Color textColor = styleToUse->color();
    
    // Make the text color legible against a white background
    if (styleToUse->forceBackgroundsToWhite())
        textColor = correctedTextColor(textColor, Color::white);

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
    Color selectionColor = i.p->pen().color();
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
        font->drawText(i.p, m_x + tx, m_y + ty + m_baseline, textObject()->tabWidth(), textPos(),
                       textObject()->string()->s, textObject()->string()->l, m_start, endPoint,
                       m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, m_dirOverride || styleToUse->visuallyOrdered());
    } else {
        int sPos, ePos;
        selectionStartEnd(sPos, ePos);
        if (paintSelectedTextSeparately) {
            // paint only the text that is not selected
            if (sPos >= ePos) {
                font->drawText(i.p, m_x + tx, m_y + ty + m_baseline, textObject()->tabWidth(), textPos(),
                               textObject()->string()->s, textObject()->string()->l, m_start, m_len,
                               m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, m_dirOverride || styleToUse->visuallyOrdered());
            } else {
                if (sPos - 1 >= 0) {
                    font->drawText(i.p, m_x + tx, m_y + ty + m_baseline, textObject()->tabWidth(), textPos(),
                                   textObject()->string()->s, textObject()->string()->l, m_start, m_len,
                                   m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, m_dirOverride || styleToUse->visuallyOrdered(), 0, sPos);
                }
                if (ePos < m_start + m_len) {
                    font->drawText(i.p, m_x + tx, m_y + ty + m_baseline, textObject()->tabWidth(), textPos(),
                                   textObject()->string()->s, textObject()->string()->l, m_start, m_len,
                                   m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, m_dirOverride || styleToUse->visuallyOrdered(), ePos, -1);
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
            font->drawText(i.p, m_x + tx, m_y + ty + m_baseline, textObject()->tabWidth(), textPos(),
                           textObject()->string()->s, textObject()->string()->l, m_start, m_len,
                           m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, m_dirOverride || styleToUse->visuallyOrdered(), sPos, ePos);
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
            MarkedTextUnderline underline = *underlineIt;

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
    Color textColor = style->color();
    Color c = object()->selectionColor(p);
    if (!c.isValid())
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.  This should basically never happen, since the selection has transparency.
    if (textColor == c)
        c = Color(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    p->save();
    p->setPen(c); // Don't draw text at all!
    RootInlineBox* r = root();
    int y = r->selectionTop();
    int h = r->selectionHeight();
    p->addClip(IntRect(m_x + tx, y + ty, m_width, h));
    f->drawHighlightForText(p, m_x + tx, y + ty, h, textObject()->tabWidth(), textPos(), 
                            textObject()->str->s, textObject()->str->l, m_start, m_len,
                            m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, m_dirOverride || style->visuallyOrdered(), sPos, ePos, c);
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

    Color c = Color(225, 221, 85);
    
    p->setPen(c); // Don't draw text at all!

    RootInlineBox* r = root();
    int y = r->selectionTop();
    int h = r->selectionHeight();
    f->drawHighlightForText(p, m_x + tx, y + ty, h, textObject()->tabWidth(), textPos(), textObject()->str->s, textObject()->str->l, m_start, m_len,
            m_toAdd, m_reversed ? QPainter::RTL : QPainter::LTR, m_dirOverride || style->visuallyOrdered(), sPos, ePos, c);
    p->restore();
}

void InlineTextBox::paintDecoration( QPainter *pt, int _tx, int _ty, int deco)
{
    _tx += m_x;
    _ty += m_y;

    if (m_truncation == cFullTruncation)
        return;
    
    int width = (m_truncation == cNoTruncation) ? 
                m_width : static_cast<RenderText*>(m_object)->width(m_start, m_truncation - m_start, textPos(), m_firstLine);
    
    // Get the text decoration colors.
    Color underline, overline, linethrough;
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
    unsigned paintStart = m_start;
    unsigned paintEnd = end()+1;      // end points at the last char, not past it
    if (paintStart <= marker.startOffset) {
        paintStart = marker.startOffset;
        useWholeWidth = false;
        start = static_cast<RenderText*>(m_object)->width(m_start, paintStart - m_start, textPos(), m_firstLine);
    }
    if (paintEnd != marker.endOffset) {      // end points at the last char, not past it
        paintEnd = kMin(paintEnd, marker.endOffset);
        useWholeWidth = false;
    }
    if (m_truncation != cNoTruncation) {
        paintEnd = kMin(paintEnd, (unsigned)m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = static_cast<RenderText*>(m_object)->width(paintStart, paintEnd - paintStart, textPos() + start, m_firstLine);
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

void InlineTextBox::paintMarkedTextUnderline(QPainter *pt, int _tx, int _ty, MarkedTextUnderline& underline)
{
    _tx += m_x;
    _ty += m_y;

    if (m_truncation == cFullTruncation)
        return;
    
    int start = 0;                  // start of line to draw, relative to _tx
    int width = m_width;            // how much line to draw
    bool useWholeWidth = true;
    unsigned paintStart = m_start;
    unsigned paintEnd = end()+1;      // end points at the last char, not past it
    if (paintStart <= underline.startOffset) {
        paintStart = underline.startOffset;
        useWholeWidth = false;
        start = static_cast<RenderText*>(m_object)->width(m_start, paintStart - m_start, textPos(), m_firstLine);
    }
    if (paintEnd != underline.endOffset) {      // end points at the last char, not past it
        paintEnd = kMin(paintEnd, (unsigned)underline.endOffset);
        useWholeWidth = false;
    }
    if (m_truncation != cNoTruncation) {
        paintEnd = kMin(paintEnd, (unsigned)m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = static_cast<RenderText*>(m_object)->width(paintStart, paintEnd - paintStart, textPos() + start, m_firstLine);
    }

    int underlineOffset = m_height - 3;
    pt->setPen(Pen(underline.color, underline.thick ? 2 : 0));
    pt->drawLineForText(_tx + start, _ty, underlineOffset, width);
}

int InlineTextBox::caretMinOffset() const
{
    return m_start;
}

int InlineTextBox::caretMaxOffset() const
{
    return m_start + m_len;
}

unsigned InlineTextBox::caretMaxRenderedOffset() const
{
    return m_start + m_len;
}

int InlineTextBox::textPos() const
{
    if (xPos() == 0)
        return 0;
        
    RenderBlock *blockElement = object()->containingBlock();
    return m_reversed ? xPos() - blockElement->borderRight() - blockElement->paddingRight()
                      : xPos() - blockElement->borderLeft() - blockElement->paddingLeft();
}

int InlineTextBox::offsetForPosition(int _x, bool includePartialGlyphs) const
{
    RenderText* text = static_cast<RenderText*>(m_object);
    RenderStyle *style = text->style(m_firstLine);
    const Font* f = &style->htmlFont();
    return f->checkSelectionPoint(text->str->s, text->str->l, m_start, m_len, m_toAdd, text->tabWidth(), textPos(), _x - m_x, m_reversed ? QPainter::RTL : QPainter::LTR, m_dirOverride || style->visuallyOrdered(), includePartialGlyphs);
}

int InlineTextBox::positionForOffset(int offset) const
{
    RenderText *text = static_cast<RenderText *>(m_object);
    const Font *f = text->htmlFont(m_firstLine);
    int from = m_reversed ? offset - m_start : 0;
    int to = m_reversed ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    return f->selectionRectForText(m_x, 0, 0, text->tabWidth(), textPos(), text->str->s, text->str->l, m_start, m_len, m_toAdd, m_reversed, m_dirOverride, from, to).right();
}

}
