/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "InlineTextBox.h"

#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "RenderArena.h"
#include "RenderBlock.h"
#include "RenderTheme.h"
#include "Text.h"
#include "break_lines.h"
#include <wtf/AlwaysInline.h>

using namespace std;

namespace WebCore {

int InlineTextBox::selectionTop()
{
    return root()->selectionTop();
}

int InlineTextBox::selectionHeight()
{
    return root()->selectionHeight();
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
        // The position after a hard line break is considered to be past its end.
        int lastSelectable = start() + len() - (isLineBreak() ? 1 : 0);

        bool start = (state != RenderObject::SelectionEnd && startPos >= m_start && startPos < m_start + m_len);
        bool end = (state != RenderObject::SelectionStart && endPos > m_start && endPos <= lastSelectable);
        if (start && end)
            state = RenderObject::SelectionBoth;
        else if (start)
            state = RenderObject::SelectionStart;
        else if (end)
            state = RenderObject::SelectionEnd;
        else if ((state == RenderObject::SelectionEnd || startPos < m_start) &&
                 (state == RenderObject::SelectionStart || endPos > lastSelectable))
            state = RenderObject::SelectionInside;
        else if (state == RenderObject::SelectionBoth)
            state = RenderObject::SelectionNone;
    }
    return state;
}

IntRect InlineTextBox::selectionRect(int tx, int ty, int startPos, int endPos)
{
    int sPos = max(startPos - m_start, 0);
    int ePos = min(endPos - m_start, (int)m_len);
    
    if (sPos >= ePos)
        return IntRect();

    RenderText* textObj = textObject();
    int selTop = selectionTop();
    int selHeight = selectionHeight();
    const Font& f = textObj->style(m_firstLine)->font();

    IntRect r = enclosingIntRect(f.selectionRectForText(TextRun(textObj->text()->characters() + m_start, m_len, textObj->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride),
                                                        IntPoint(tx + m_x, ty + selTop), selHeight, sPos, ePos));
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
            m_truncation = offset;
            return m_x + static_cast<RenderText*>(m_object)->width(m_start, offset, textPos(), m_firstLine);
        }
    }
    else {
        // FIXME: Support RTL truncation someday, including both modes (when the leftmost run on the line is either RTL or LTR)
    }
    return -1;
}

Color correctedTextColor(Color textColor, Color backgroundColor) 
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

void updateGraphicsContext(GraphicsContext* context, const Color& fillColor, const Color& strokeColor, float strokeThickness)
{
    int mode = context->textDrawingMode();
    if (strokeThickness > 0) {
        int newMode = mode | cTextStroke;
        if (mode != newMode) {
            context->setTextDrawingMode(newMode);
            mode = newMode;
        }
    }
    
    if (mode & cTextFill && fillColor != context->fillColor())
        context->setFillColor(fillColor);

    if (mode & cTextStroke) {
        if (strokeColor != context->strokeColor())
            context->setStrokeColor(strokeColor);
        if (strokeThickness != context->strokeThickness())
            context->setStrokeThickness(strokeThickness);
    }
}

bool InlineTextBox::isLineBreak() const
{
    return object()->isBR() || (object()->style()->preserveNewline() && len() == 1 && (*textObject()->text())[start()] == '\n');
}

bool InlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    if (isLineBreak())
        return false;

    IntRect rect(tx + m_x, ty + m_y, m_width, m_height);
    if (m_truncation != cFullTruncation && object()->style()->visibility() == VISIBLE && rect.contains(x, y)) {
        object()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
        return true;
    }
    return false;
}

void InlineTextBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (isLineBreak() || !object()->shouldPaintWithinRoot(paintInfo) || object()->style()->visibility() != VISIBLE ||
            m_truncation == cFullTruncation || paintInfo.phase == PaintPhaseOutline)
        return;
    
    ASSERT(paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines);

    int xPos = tx + m_x - parent()->maxHorizontalVisualOverflow();
    int w = width() + 2 * parent()->maxHorizontalVisualOverflow();
    if (xPos >= paintInfo.rect.right() || xPos + w <= paintInfo.rect.x())
        return;

    bool isPrinting = textObject()->document()->printing();
    
    // Determine whether or not we're selected.
    bool haveSelection = !isPrinting && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && paintInfo.phase == PaintPhaseSelection)
        // When only painting the selection, don't bother to paint if there is none.
        return;

    // Determine whether or not we have composition underlines to draw.
    bool containsComposition = object()->document()->frame()->editor()->compositionNode() == object()->node();
    bool useCustomUnderlines = containsComposition && object()->document()->frame()->editor()->compositionUsesCustomUnderlines();

    // Set our font.
    RenderStyle* styleToUse = object()->style(m_firstLine);
    int d = styleToUse->textDecorationsInEffect();
    const Font* font = &styleToUse->font();
    if (*font != paintInfo.context->font())
        paintInfo.context->setFont(*font);

    // 1. Paint backgrounds behind text if needed. Examples of such backgrounds include selection
    // and composition underlines.
    if (paintInfo.phase != PaintPhaseSelection && !isPrinting) {
#if PLATFORM(MAC)
        // Custom highlighters go behind everything else.
        if (styleToUse->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
            paintCustomHighlight(tx, ty, styleToUse->highlight());
#endif

        if (containsComposition && !useCustomUnderlines)
            paintCompositionBackground(paintInfo.context, tx, ty, styleToUse, font,
                object()->document()->frame()->editor()->compositionStart(),
                object()->document()->frame()->editor()->compositionEnd());

        paintDocumentMarkers(paintInfo.context, tx, ty, styleToUse, font, true);

        if (haveSelection && !useCustomUnderlines)
            paintSelection(paintInfo.context, tx, ty, styleToUse, font);
    }

    // 2. Now paint the foreground, including text and decorations like underline/overline (in quirks mode only).
    if (m_len <= 0)
        return;

    Color textFillColor;
    Color textStrokeColor;
    float textStrokeWidth = styleToUse->textStrokeWidth();

    if (paintInfo.forceBlackText) {
        textFillColor = Color::black;
        textStrokeColor = Color::black;
    } else {
        textFillColor = styleToUse->textFillColor();
        if (!textFillColor.isValid())
            textFillColor = styleToUse->color();
        
        // Make the text fill color legible against a white background
        if (styleToUse->forceBackgroundsToWhite())
            textFillColor = correctedTextColor(textFillColor, Color::white);
            
        textStrokeColor = styleToUse->textStrokeColor();
        if (!textStrokeColor.isValid())
            textStrokeColor = styleToUse->color();
        
        // Make the text stroke color legible against a white background
        if (styleToUse->forceBackgroundsToWhite())
            textStrokeColor = correctedTextColor(textStrokeColor, Color::white);
    }

    // For stroked painting, we have to change the text drawing mode.  It's probably dangerous to leave that mutated as a side
    // effect, so only when we know we're stroking, do a save/restore.
    if (textStrokeWidth > 0)
        paintInfo.context->save();

    updateGraphicsContext(paintInfo.context, textFillColor, textStrokeColor, textStrokeWidth);
    
    // Set a text shadow if we have one.
    // FIXME: Support multiple shadow effects.  Need more from the CG API before
    // we can do this.
    bool setShadow = false;
    if (styleToUse->textShadow()) {
        paintInfo.context->setShadow(IntSize(styleToUse->textShadow()->x, styleToUse->textShadow()->y),
                                     styleToUse->textShadow()->blur, styleToUse->textShadow()->color);
        setShadow = true;
    }

    bool paintSelectedTextOnly = (paintInfo.phase == PaintPhaseSelection);
    bool paintSelectedTextSeparately = false; // Whether or not we have to do multiple paints.  Only
                                              // necessary when a custom ::selection foreground color is applied.
    Color selectionFillColor = textFillColor;
    Color selectionStrokeColor = textStrokeColor;
    float selectionStrokeWidth = textStrokeWidth;
    ShadowData* selectionTextShadow = 0;
    if (haveSelection) {
        // Check foreground color first.
        Color foreground = object()->selectionForegroundColor();
        if (foreground.isValid() && foreground != selectionFillColor) {
            if (!paintSelectedTextOnly)
                paintSelectedTextSeparately = true;
            selectionFillColor = foreground;
        }
        RenderStyle* pseudoStyle = object()->getPseudoStyle(RenderStyle::SELECTION);
        if (pseudoStyle) {
            if (pseudoStyle->textShadow()) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                if (pseudoStyle->textShadow())
                    selectionTextShadow = pseudoStyle->textShadow();
            }
            
            float strokeWidth = pseudoStyle->textStrokeWidth();
            if (strokeWidth != selectionStrokeWidth) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeWidth = strokeWidth;
            }

            Color stroke = pseudoStyle->textStrokeColor();
            if (!stroke.isValid())
                stroke = pseudoStyle->color();
            if (stroke != selectionStrokeColor) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeColor = stroke;
            }
        }
    }

    StringImpl* textStr = textObject()->text();

    if (!paintSelectedTextOnly && !paintSelectedTextSeparately) {
        // paint all the text
        // FIXME: Handle RTL direction, handle reversed strings.  For now truncation can only be turned on
        // for non-reversed LTR strings.
        int endPoint = m_len;
        if (m_truncation != cNoTruncation)
            endPoint = m_truncation;
        paintInfo.context->drawText(TextRun(textStr->characters() + m_start, endPoint, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || styleToUse->visuallyOrdered()),
                                    IntPoint(m_x + tx, m_y + ty + m_baseline));
    } else {
        int sPos, ePos;
        selectionStartEnd(sPos, ePos);
        if (paintSelectedTextSeparately) {
            // paint only the text that is not selected
            if (sPos >= ePos)
                paintInfo.context->drawText(TextRun(textStr->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || styleToUse->visuallyOrdered()),
                                            IntPoint(m_x + tx, m_y + ty + m_baseline));
            else {
                if (sPos - 1 >= 0)
                    paintInfo.context->drawText(TextRun(textStr->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || styleToUse->visuallyOrdered()),
                                                IntPoint(m_x + tx, m_y + ty + m_baseline),  0, sPos);
                if (ePos < m_start + m_len)
                    paintInfo.context->drawText(TextRun(textStr->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || styleToUse->visuallyOrdered()),
                                                IntPoint(m_x + tx, m_y + ty + m_baseline), ePos);
            }
        }

        if (sPos < ePos) {
            // paint only the text that is selected
            if (selectionStrokeWidth > 0)
                paintInfo.context->save();
        
            updateGraphicsContext(paintInfo.context, selectionFillColor, selectionStrokeColor, selectionStrokeWidth);

            if (selectionTextShadow)
                paintInfo.context->setShadow(IntSize(selectionTextShadow->x, selectionTextShadow->y),
                                             selectionTextShadow->blur, selectionTextShadow->color);
            paintInfo.context->drawText(TextRun(textStr->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || styleToUse->visuallyOrdered()),
                                        IntPoint(m_x + tx, m_y + ty + m_baseline), sPos, ePos);
            if (selectionTextShadow)
                paintInfo.context->clearShadow();
                
            if (selectionStrokeWidth > 0)
                paintInfo.context->restore();
        }
    }

    // Paint decorations
    if (d != TDNONE && paintInfo.phase != PaintPhaseSelection && styleToUse->htmlHacks()) {
        paintInfo.context->setStrokeColor(styleToUse->color());
        paintDecoration(paintInfo.context, tx, ty, d);
    }

    if (paintInfo.phase != PaintPhaseSelection) {
        paintDocumentMarkers(paintInfo.context, tx, ty, styleToUse, font, false);

        if (useCustomUnderlines) {
            const Vector<CompositionUnderline>& underlines = object()->document()->frame()->editor()->customCompositionUnderlines();
            size_t numUnderlines = underlines.size();

            for (size_t index = 0; index < numUnderlines; ++index) {
                const CompositionUnderline& underline = underlines[index];

                if (underline.endOffset <= start())
                    // underline is completely before this run.  This might be an underline that sits
                    // before the first run we draw, or underlines that were within runs we skipped 
                    // due to truncation.
                    continue;
                
                if (underline.startOffset <= end()) {
                    // underline intersects this run.  Paint it.
                    paintCompositionUnderline(paintInfo.context, tx, ty, underline);
                    if (underline.endOffset > end() + 1)
                        // underline also runs into the next run. Bail now, no more marker advancement.
                        break;
                } else
                    // underline is completely after this run, bail.  A later run will paint it.
                    break;
            }
        }
    }

    if (setShadow)
        paintInfo.context->clearShadow();
        
    if (textStrokeWidth > 0)
        paintInfo.context->restore();
}

void InlineTextBox::selectionStartEnd(int& sPos, int& ePos)
{
    int startPos, endPos;
    if (object()->selectionState() == RenderObject::SelectionInside) {
        startPos = 0;
        endPos = textObject()->textLength();
    } else {
        textObject()->selectionStartEnd(startPos, endPos);
        if (object()->selectionState() == RenderObject::SelectionStart)
            endPos = textObject()->textLength();
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

    Color textColor = style->color();
    Color c = object()->selectionBackgroundColor();
    if (!c.isValid() || c.alpha() == 0)
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.  This should basically never happen, since the selection has transparency.
    if (textColor == c)
        c = Color(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    p->save();
    updateGraphicsContext(p, c, c, 0);  // Don't draw text at all!
    int y = selectionTop();
    int h = selectionHeight();
    p->clip(IntRect(m_x + tx, y + ty, m_width, h));
    p->drawHighlightForText(TextRun(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || style->visuallyOrdered()),
                            IntPoint(m_x + tx, y + ty), h, c, sPos, ePos);
    p->restore();
}

void InlineTextBox::paintCompositionBackground(GraphicsContext* p, int tx, int ty, RenderStyle* style, const Font* f, int startPos, int endPos)
{
    int offset = m_start;
    int sPos = max(startPos - offset, 0);
    int ePos = min(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

    p->save();

    Color c = Color(225, 221, 85);
    
    updateGraphicsContext(p, c, c, 0); // Don't draw text at all!

    int y = selectionTop();
    int h = selectionHeight();
    p->drawHighlightForText(TextRun(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || style->visuallyOrdered()),
                            IntPoint(m_x + tx, y + ty), h, c, sPos, ePos);
    p->restore();
}

#if PLATFORM(MAC)
void InlineTextBox::paintCustomHighlight(int tx, int ty, const AtomicString& type)
{
    RootInlineBox* r = root();
    FloatRect rootRect(tx + r->xPos(), ty + selectionTop(), r->width(), selectionHeight());
    FloatRect textRect(tx + xPos(), rootRect.y(), width(), rootRect.height());

    object()->document()->frame()->paintCustomHighlight(type, textRect, rootRect, true, false, object()->node());
}
#endif

void InlineTextBox::paintDecoration(GraphicsContext* context, int tx, int ty, int deco)
{
    tx += m_x;
    ty += m_y;

    if (m_truncation == cFullTruncation)
        return;
    
    int width = (m_truncation == cNoTruncation) ? m_width
        : static_cast<RenderText*>(m_object)->width(m_start, m_truncation, textPos(), m_firstLine);
    
    // Get the text decoration colors.
    Color underline, overline, linethrough;
    object()->getTextDecorationColors(deco, underline, overline, linethrough, true);
    
    // Use a special function for underlines to get the positioning exactly right.
    bool isPrinting = textObject()->document()->printing();
    context->setStrokeThickness(1.0f); // FIXME: We should improve this rule and not always just assume 1.
    if (deco & UNDERLINE) {
        context->setStrokeColor(underline);
        // Leave one pixel of white between the baseline and the underline.
        context->drawLineForText(IntPoint(tx, ty + m_baseline + 1), width, isPrinting);
    }
    if (deco & OVERLINE) {
        context->setStrokeColor(overline);
        context->drawLineForText(IntPoint(tx, ty), width, isPrinting);
    }
    if (deco & LINE_THROUGH) {
        context->setStrokeColor(linethrough);
        context->drawLineForText(IntPoint(tx, ty + 2 * m_baseline / 3), width, isPrinting);
    }
}

void InlineTextBox::paintSpellingOrGrammarMarker(GraphicsContext* pt, int tx, int ty, DocumentMarker marker, RenderStyle* style, const Font* f, bool grammar)
{
    // Never print spelling/grammar markers (5327887)
    if (textObject()->document()->printing())
        return;

    if (m_truncation == cFullTruncation)
        return;

    tx += m_x;
    ty += m_y;
        
    int start = 0;                  // start of line to draw, relative to tx
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
        paintEnd = min(paintEnd, (unsigned)m_start + m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = static_cast<RenderText*>(m_object)->width(paintStart, paintEnd - paintStart, textPos() + start, m_firstLine);
    }
    
    // Store rendered rects for bad grammar markers, so we can hit-test against it elsewhere in order to
    // display a toolTip. We don't do this for misspelling markers.
    if (grammar) {
        int y = selectionTop();
        IntPoint startPoint = IntPoint(m_x + tx, y + ty);
        int startPosition = max(marker.startOffset - m_start, (unsigned)0);
        int endPosition = min(marker.endOffset - m_start, (unsigned)m_len);    
        TextRun run(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || style->visuallyOrdered());
        IntRect markerRect = enclosingIntRect(f->selectionRectForText(run, startPoint, selectionHeight(), startPosition, endPosition));
        object()->document()->setRenderedRectForMarker(object()->node(), marker, markerRect);
    }
    
    // IMPORTANT: The misspelling underline is not considered when calculating the text bounds, so we have to
    // make sure to fit within those bounds.  This means the top pixel(s) of the underline will overlap the
    // bottom pixel(s) of the glyphs in smaller font sizes.  The alternatives are to increase the line spacing (bad!!)
    // or decrease the underline thickness.  The overlap is actually the most useful, and matches what AppKit does.
    // So, we generally place the underline at the bottom of the text, but in larger fonts that's not so good so
    // we pin to two pixels under the baseline.
    int lineThickness = cMisspellingLineThickness;
    int descent = m_height - m_baseline;
    int underlineOffset;
    if (descent <= (2 + lineThickness)) {
        // place the underline at the very bottom of the text in small/medium fonts
        underlineOffset = m_height - lineThickness;
    } else {
        // in larger fonts, tho, place the underline up near the baseline to prevent big gap
        underlineOffset = m_baseline + 2;
    }
    pt->drawLineForMisspellingOrBadGrammar(IntPoint(tx + start, ty + underlineOffset), width, grammar);
}

void InlineTextBox::paintTextMatchMarker(GraphicsContext* pt, int tx, int ty, DocumentMarker marker, RenderStyle* style, const Font* f)
{
   // Use same y positioning and height as for selection, so that when the selection and this highlight are on
   // the same word there are no pieces sticking out.
    int y = selectionTop();
    int h = selectionHeight();
    
    int sPos = max(marker.startOffset - m_start, (unsigned)0);
    int ePos = min(marker.endOffset - m_start, (unsigned)m_len);    
    TextRun run(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || style->visuallyOrdered());
    IntPoint startPoint = IntPoint(m_x + tx, y + ty);
    
    // Always compute and store the rect associated with this marker
    IntRect markerRect = enclosingIntRect(f->selectionRectForText(run, startPoint, h, sPos, ePos));
    object()->document()->setRenderedRectForMarker(object()->node(), marker, markerRect);
     
    // Optionally highlight the text
    if (object()->document()->frame()->markedTextMatchesAreHighlighted()) {
        Color color = theme()->platformTextSearchHighlightColor();
        pt->save();
        updateGraphicsContext(pt, color, color, 0);  // Don't draw text at all!
        pt->clip(IntRect(tx + m_x, ty + y, m_width, h));
        pt->drawHighlightForText(run, startPoint, h, color, sPos, ePos);
        pt->restore();
    }
}

void InlineTextBox::paintDocumentMarkers(GraphicsContext* pt, int tx, int ty, RenderStyle* style, const Font* f, bool background)
{
    Vector<DocumentMarker> markers = object()->document()->markersForNode(object()->node());
    Vector<DocumentMarker>::iterator markerIt = markers.begin();

    // Give any document markers that touch this run a chance to draw before the text has been drawn.
    // Note end() points at the last char, not one past it like endOffset and ranges do.
    for ( ; markerIt != markers.end(); markerIt++) {
        DocumentMarker marker = *markerIt;
        
        // Paint either the background markers or the foreground markers, but not both
        switch (marker.type) {
            case DocumentMarker::Grammar:
            case DocumentMarker::Spelling:
                if (background)
                    continue;
                break;
                
            case DocumentMarker::TextMatch:
                if (!background)
                    continue;
                break;
            
            default:
                ASSERT_NOT_REACHED();
        }

        if (marker.endOffset <= start())
            // marker is completely before this run.  This might be a marker that sits before the
            // first run we draw, or markers that were within runs we skipped due to truncation.
            continue;
        
        if (marker.startOffset > end())
            // marker is completely after this run, bail.  A later run will paint it.
            break;
        
        // marker intersects this run.  Paint it.
        switch (marker.type) {
            case DocumentMarker::Spelling:
                paintSpellingOrGrammarMarker(pt, tx, ty, marker, style, f, false);
                break;
            case DocumentMarker::Grammar:
                paintSpellingOrGrammarMarker(pt, tx, ty, marker, style, f, true);
                break;
            case DocumentMarker::TextMatch:
                paintTextMatchMarker(pt, tx, ty, marker, style, f);
                break;
            default:
                ASSERT_NOT_REACHED();
        }

        if (marker.endOffset > end() + 1)
            // marker also runs into the next run. Bail now, no more marker advancement.
            break;
    }
}


void InlineTextBox::paintCompositionUnderline(GraphicsContext* ctx, int tx, int ty, const CompositionUnderline& underline)
{
    tx += m_x;
    ty += m_y;

    if (m_truncation == cFullTruncation)
        return;
    
    int start = 0;                 // start of line to draw, relative to tx
    int width = m_width;           // how much line to draw
    bool useWholeWidth = true;
    unsigned paintStart = m_start;
    unsigned paintEnd = end() + 1; // end points at the last char, not past it
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
        paintEnd = min(paintEnd, (unsigned)m_start + m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = static_cast<RenderText*>(m_object)->width(paintStart, paintEnd - paintStart, textPos() + start, m_firstLine);
    }

    // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
    // All other marked text underlines are 1px thick.
    // If there's not enough space the underline will touch or overlap characters.
    int lineThickness = 1;
    if (underline.thick && m_height - m_baseline >= 2)
        lineThickness = 2;

    ctx->setStrokeColor(underline.color);
    ctx->setStrokeThickness(lineThickness);
    ctx->drawLineForText(IntPoint(tx + start, ty + m_height - lineThickness), width, textObject()->document()->printing());
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
    return f->offsetForPosition(TextRun(textObject()->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride || style->visuallyOrdered()),
                                _x - m_x, includePartialGlyphs);
}

int InlineTextBox::positionForOffset(int offset) const
{
    if (isLineBreak())
        return m_x;

    RenderText* text = static_cast<RenderText*>(m_object);
    const Font& f = text->style(m_firstLine)->font();
    int from = m_reversed ? offset - m_start : 0;
    int to = m_reversed ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    return enclosingIntRect(f.selectionRectForText(TextRun(text->text()->characters() + m_start, m_len, textObject()->allowTabs(), textPos(), m_toAdd, m_reversed, m_dirOverride),
                                                   IntPoint(m_x, 0), 0, from, to)).right();
}

bool InlineTextBox::containsCaretOffset(int offset) const
{
    // Offsets before the box are never "in".
    if (offset < m_start)
        return false;

    int pastEnd = m_start + m_len;

    // Offsets inside the box (not at either edge) are always "in".
    if (offset < pastEnd)
        return true;

    // Offsets outside the box are always "out".
    if (offset > pastEnd)
        return false;

    // Offsets at the end are "out" for line breaks (they are on the next line).
    if (isLineBreak())
        return false;

    // Offsets at the end are "in" for normal boxes (but the caller has to check affinity).
    return true;
}

} // namespace WebCore
