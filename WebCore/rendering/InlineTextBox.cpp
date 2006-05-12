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

#include "Document.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "RenderBlock.h"
#include "break_lines.h"
#include "Range.h"
#include "RenderArena.h"
#include <wtf/AlwaysInline.h>

using namespace std;

namespace WebCore {

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
    *static_cast<size_t*>(ptr) = sz;
}

bool InlineTextBox::isSelected(int startPos, int endPos) const
{
    int sPos = max(startPos - m_start, 0);
    int ePos = min(endPos - m_start, (int)m_len);
    return (sPos < ePos);
}

RenderObject::SelectionState InlineTextBox::selectionState()
{
    RenderObject::SelectionState state = object()->selectionState();
    if (state == RenderObject::SelectionStart || state == RenderObject::SelectionEnd || state == RenderObject::SelectionBoth) {
        int startPos, endPos;
        object()->selectionStartEnd(startPos, endPos);
        
        // If we're at a line wrap, then the selection is going to extend onto the next line (and thus needs to be thought of as
        // extending beyond our box.
        if (textObject()->atLineWrap(this, endPos))
            endPos++;

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
    int sPos = max(startPos - m_start, 0);
    int ePos = min(endPos - m_start, (int)m_len);
    
    if (sPos >= ePos)
        return IntRect();

    RootInlineBox* rootBox = root();
    RenderText* textObj = textObject();
    int selTop = rootBox->selectionTop();
    int selHeight = rootBox->selectionHeight();
    const Font *f = textObj->font(m_firstLine);

    IntRect r = enclosingIntRect(f->selectionRectForText(TextRun(textObj->string(), m_start, m_len, sPos, ePos),
                                        IntPoint(tx + m_x, ty + selTop), selHeight, textObj->tabWidth(), textPos(), 
                                        m_toAdd, m_reversed, m_dirOverride));
    if (r.x() > tx + m_x + m_width)
        r.setWidth(0);
    else if (r.right() - 1 > tx + m_x + m_width)
        r.setWidth(tx + m_x + m_width - r.x());
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
                return min(ellipsisX, m_x);
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

static Color 
correctedTextColor(Color textColor, Color backgroundColor) 
{
    // Adjust the text color if it is too close to the background color,
    // by darkening or lightening it to move it further away.
    
    int d = differenceSquared(textColor, backgroundColor);
    // semi-arbitrarily chose 65025 (255^2) value here after a few tests; 
    if (d > 65025) {
        return textColor;
    }
    
    int distanceFromWhite = differenceSquared(textColor, Color::white);
    int distanceFromBlack = differenceSquared(textColor, Color::black);

    if (distanceFromWhite < distanceFromBlack) {
        return textColor.dark();
    }
    
    return textColor.light();
}

bool InlineTextBox::isLineBreak() const
{
    return object()->isBR() || (object()->style()->preserveNewline() && len() == 1 && (*textObject()->string())[start()] == '\n');
}

bool InlineTextBox::nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty)
{
    if (isLineBreak())
        return false;

    IntRect rect(tx + m_x, ty + m_y, m_width, m_height);
    if (m_truncation != cFullTruncation && object()->style()->visibility() == VISIBLE && rect.contains(x, y)) {
        object()->setInnerNode(i);
        return true;
    }
    return false;
}

void InlineTextBox::paint(RenderObject::PaintInfo& i, int tx, int ty)
{
    if (isLineBreak() || !object()->shouldPaintWithinRoot(i) || object()->style()->visibility() != VISIBLE ||
        m_truncation == cFullTruncation || i.phase == PaintPhaseOutline)
        return;
    
    assert(i.phase != PaintPhaseSelfOutline && i.phase != PaintPhaseChildOutlines);

    int xPos = tx + m_x - parent()->maxHorizontalShadow();
    int w = width() + 2 * parent()->maxHorizontalShadow();
    if (xPos >= i.r.right() || xPos + w <= i.r.x())
        return;

    bool isPrinting = textObject()->document()->printing();
    
    // Determine whether or not we're selected.
    bool haveSelection = !isPrinting && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && i.phase == PaintPhaseSelection)
        // When only painting the selection, don't bother to paint if there is none.
        return;

    // Determine whether or not we have marked text.
    Range *markedTextRange = object()->document()->frame()->markedTextRange();
    int exception = 0;
    bool haveMarkedText = markedTextRange && markedTextRange->startContainer(exception) == object()->node();
    bool markedTextUsesUnderlines = object()->document()->frame()->markedTextUsesUnderlines();

    // Set our font.
    RenderStyle* styleToUse = object()->style(m_firstLine);
    int d = styleToUse->textDecorationsInEffect();
    const Font* font = &styleToUse->font();
    if (*font != i.p->font())
        i.p->setFont(*font);

    // 1. Paint backgrounds behind text if needed.  Examples of such backgrounds include selection
    // and marked text.
    if (i.phase != PaintPhaseSelection && !isPrinting) {
        if (haveMarkedText  && !markedTextUsesUnderlines)
            paintMarkedTextBackground(i.p, tx, ty, styleToUse, font, markedTextRange->startOffset(exception), markedTextRange->endOffset(exception));

        paintAllMarkersOfType(i.p, tx, ty, DocumentMarker::TextMatch, styleToUse, font);

        if (haveSelection)
            paintSelection(i.p, tx, ty, styleToUse, font);
    }
    
    // 2. Now paint the foreground, including text and decorations like underline/overline (in quirks mode only).
    if (m_len <= 0) return;
    
    DeprecatedValueList<MarkedTextUnderline> underlines;
    if (haveMarkedText && markedTextUsesUnderlines) {
        underlines = object()->document()->frame()->markedTextUnderlines();
    }
    DeprecatedValueListIterator<MarkedTextUnderline> underlineIt = underlines.begin();

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
        i.p->setShadow(IntSize(styleToUse->textShadow()->x, styleToUse->textShadow()->y),
            styleToUse->textShadow()->blur, styleToUse->textShadow()->color);
        setShadow = true;
    }

    bool paintSelectedTextOnly = (i.phase == PaintPhaseSelection);
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

    StringImpl* textStr = textObject()->string();

    if (!paintSelectedTextOnly && !paintSelectedTextSeparately) {
        // paint all the text
        // FIXME: Handle RTL direction, handle reversed strings.  For now truncation can only be turned on
        // for non-reversed LTR strings.
        int endPoint = m_len;
        if (m_truncation != cNoTruncation)
            endPoint = m_truncation - m_start;
        i.p->drawText(TextRun(textStr, m_start, endPoint),
                      IntPoint(m_x + tx, m_y + ty + m_baseline), textObject()->tabWidth(), textPos(),
                      m_toAdd, m_reversed ? RTL : LTR, m_dirOverride || styleToUse->visuallyOrdered());
    } else {
        int sPos, ePos;
        selectionStartEnd(sPos, ePos);
        if (paintSelectedTextSeparately) {
            // paint only the text that is not selected
            if (sPos >= ePos)
                i.p->drawText(TextRun(textStr, m_start, m_len), IntPoint(m_x + tx, m_y + ty + m_baseline), textObject()->tabWidth(), textPos(),
                              m_toAdd, m_reversed ? RTL : LTR, m_dirOverride || styleToUse->visuallyOrdered());
            else {
                if (sPos - 1 >= 0)
                    i.p->drawText(TextRun(textStr, m_start, m_len, 0, sPos), IntPoint(m_x + tx, m_y + ty + m_baseline), textObject()->tabWidth(), textPos(),
                                  m_toAdd, m_reversed ? RTL : LTR, m_dirOverride || styleToUse->visuallyOrdered());
                if (ePos < m_start + m_len)
                    i.p->drawText(TextRun(textStr, m_start, m_len, ePos), IntPoint(m_x + tx, m_y + ty + m_baseline), textObject()->tabWidth(), textPos(),
                                  m_toAdd, m_reversed ? RTL : LTR, m_dirOverride || styleToUse->visuallyOrdered());
            }
        }
            
        if (sPos < ePos) {
            // paint only the text that is selected
            if (selectionColor != i.p->pen().color())
                i.p->setPen(selectionColor);
            
            if (selectionTextShadow)
                i.p->setShadow(IntSize(selectionTextShadow->x, selectionTextShadow->y),
                               selectionTextShadow->blur,
                               selectionTextShadow->color);
            i.p->drawText(TextRun(textStr, m_start, m_len, sPos, ePos), IntPoint(m_x + tx, m_y + ty + m_baseline), textObject()->tabWidth(), textPos(),
                          m_toAdd, m_reversed ? RTL : LTR, m_dirOverride || styleToUse->visuallyOrdered());
            if (selectionTextShadow)
                i.p->clearShadow();
        }
    }

    // Paint decorations
    if (d != TDNONE && i.phase != PaintPhaseSelection && styleToUse->htmlHacks()) {
        i.p->setPen(styleToUse->color());
        paintDecoration(i.p, tx, ty, d);
    }

    if (i.phase != PaintPhaseSelection) {
        paintAllMarkersOfType(i.p, tx, ty, DocumentMarker::Spelling, styleToUse, font);

        for ( ; underlineIt != underlines.end(); underlineIt++) {
            MarkedTextUnderline underline = *underlineIt;

            if (underline.endOffset <= start())
                // underline is completely before this run.  This might be an underline that sits
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
        endPos = textObject()->string()->length();
    } else {
        textObject()->selectionStartEnd(startPos, endPos);
        if (object()->selectionState() == RenderObject::SelectionStart)
            endPos = textObject()->string()->length();
        else if (object()->selectionState() == RenderObject::SelectionEnd)
            startPos = 0;
    }

    sPos = max(startPos - m_start, 0);
    ePos = min(endPos - m_start, (int)m_len);
}

void InlineTextBox::paintSelection(GraphicsContext* p, int tx, int ty, RenderStyle* style, const Font* f)
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
    p->drawHighlightForText(TextRun(textObject()->string(), m_start, m_len, sPos, ePos), IntPoint(m_x + tx, y + ty), h, textObject()->tabWidth(), textPos(), 
                            m_toAdd, m_reversed ? RTL : LTR, m_dirOverride || style->visuallyOrdered(), c);
    p->restore();
}

void InlineTextBox::paintMarkedTextBackground(GraphicsContext* p, int tx, int ty, RenderStyle* style, const Font* f, int startPos, int endPos)
{
    int offset = m_start;
    int sPos = max(startPos - offset, 0);
    int ePos = min(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

    p->save();

    Color c = Color(225, 221, 85);
    
    p->setPen(c); // Don't draw text at all!

    RootInlineBox* r = root();
    int y = r->selectionTop();
    int h = r->selectionHeight();
    p->drawHighlightForText(TextRun(textObject()->string(), m_start, m_len, sPos, ePos),
                            IntPoint(m_x + tx, y + ty), h, textObject()->tabWidth(), textPos(),
                            m_toAdd, m_reversed ? RTL : LTR, m_dirOverride || style->visuallyOrdered(), c);
    p->restore();
}

void InlineTextBox::paintDecoration(GraphicsContext *pt, int _tx, int _ty, int deco)
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
    bool isPrinting = textObject()->document()->printing();
    if (deco & UNDERLINE) {
        pt->setPen(underline);
        pt->drawLineForText(IntPoint(_tx, _ty), m_baseline, width, isPrinting);
    }
    if (deco & OVERLINE) {
        pt->setPen(overline);
        pt->drawLineForText(IntPoint(_tx, _ty), 0, width, isPrinting);
    }
    if (deco & LINE_THROUGH) {
        pt->setPen(linethrough);
        pt->drawLineForText(IntPoint(_tx, _ty), 2*m_baseline/3, width, isPrinting);
    }
}

void InlineTextBox::paintSpellingMarker(GraphicsContext* pt, int _tx, int _ty, DocumentMarker marker)
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
        paintEnd = min(paintEnd, marker.endOffset);
        useWholeWidth = false;
    }
    if (m_truncation != cNoTruncation) {
        paintEnd = min(paintEnd, (unsigned)m_truncation);
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
    pt->drawLineForMisspelling(IntPoint(_tx + start, _ty + underlineOffset), width);
}

void InlineTextBox::paintTextMatchMarker(GraphicsContext* pt, int _tx, int _ty, DocumentMarker marker, RenderStyle* style, const Font* f)
{
    Color yellow = Color(255, 255, 0);
    pt->save();
    pt->setPen(yellow); // Don't draw text at all!
   // Use same y positioning and height as for selection, so that when the selection and this highlight are on
   // the same word there are no pieces sticking out.
    RootInlineBox* r = root();
    int y = r->selectionTop();
    int h = r->selectionHeight();
    pt->addClip(IntRect(_tx + m_x, _ty + y, m_width, h));
    int sPos = max(marker.startOffset - m_start, (unsigned)0);
    int ePos = min(marker.endOffset - m_start, (unsigned)m_len);
    
    pt->drawHighlightForText(TextRun(textObject()->string(), m_start, m_len, sPos, ePos), IntPoint(m_x + _tx, y + _ty), h, textObject()->tabWidth(), textPos(), 
                             m_toAdd, m_reversed ? RTL : LTR, m_dirOverride || style->visuallyOrdered(), yellow);
    pt->restore();
}

void InlineTextBox::paintAllMarkersOfType(GraphicsContext* pt, int _tx, int _ty, DocumentMarker::MarkerType markerType, RenderStyle* style, const Font* f)
{
    DeprecatedValueList<DocumentMarker> markers = object()->document()->markersForNode(object()->node());
    DeprecatedValueListIterator <DocumentMarker> markerIt = markers.begin();

    // Give any document markers that touch this run a chance to draw before the text has been drawn.
    // Note end() points at the last char, not one past it like endOffset and ranges do.
    for ( ; markerIt != markers.end(); markerIt++) {
        DocumentMarker marker = *markerIt;
        
        if (marker.type != markerType)
            continue;
        
        if (marker.endOffset <= start())
            // marker is completely before this run.  This might be a marker that sits before the
            // first run we draw, or markers that were within runs we skipped due to truncation.
            continue;
        
        if (marker.startOffset > end())
            // marker is completely after this run, bail.  A later run will paint it.
            break;
        
        // marker intersects this run.  Paint it.
        switch (markerType) {
            case DocumentMarker::Spelling:
                paintSpellingMarker(pt, _tx, _ty, marker);
                break;
            case DocumentMarker::TextMatch:
                paintTextMatchMarker(pt, _tx, _ty, marker, style, f);
                break;
            default:
                assert(false);
        }

        if (marker.endOffset > end() + 1)
            // marker also runs into the next run. Bail now, no more marker advancement.
            break;
    }
}


void InlineTextBox::paintMarkedTextUnderline(GraphicsContext* pt, int _tx, int _ty, MarkedTextUnderline& underline)
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
        paintEnd = min(paintEnd, (unsigned)underline.endOffset);
        useWholeWidth = false;
    }
    if (m_truncation != cNoTruncation) {
        paintEnd = min(paintEnd, (unsigned)m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = static_cast<RenderText*>(m_object)->width(paintStart, paintEnd - paintStart, textPos() + start, m_firstLine);
    }

    int underlineOffset = m_height - 3;
    pt->setPen(Pen(underline.color, underline.thick ? 2 : 0));
    pt->drawLineForText(IntPoint(_tx + start, _ty), underlineOffset, width, textObject()->document()->printing());
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
    if (isLineBreak())
        return 0;

    RenderText* text = static_cast<RenderText*>(m_object);
    RenderStyle *style = text->style(m_firstLine);
    const Font* f = &style->font();
    return f->checkSelectionPoint(TextRun(textObject()->string(), m_start, m_len),
                                  m_toAdd, text->tabWidth(), textPos(), _x - m_x,
                                  m_reversed ? RTL : LTR, m_dirOverride || style->visuallyOrdered(), includePartialGlyphs);
}

int InlineTextBox::positionForOffset(int offset) const
{
    if (isLineBreak())
        return m_x;

    RenderText *text = static_cast<RenderText *>(m_object);
    const Font *f = text->font(m_firstLine);
    int from = m_reversed ? offset - m_start : 0;
    int to = m_reversed ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    return enclosingIntRect(f->selectionRectForText(TextRun(text->string(), m_start, m_len, from, to), IntPoint(m_x, 0), 0, text->tabWidth(), textPos(), m_toAdd, m_reversed, m_dirOverride)).right();
}

}
