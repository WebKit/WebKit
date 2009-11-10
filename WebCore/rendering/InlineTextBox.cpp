/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "ChromeClient.h"
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "Page.h"
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
    RenderObject::SelectionState state = renderer()->selectionState();
    if (state == RenderObject::SelectionStart || state == RenderObject::SelectionEnd || state == RenderObject::SelectionBoth) {
        int startPos, endPos;
        renderer()->selectionStartEnd(startPos, endPos);
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

    RenderText* textObj = textRenderer();
    int selTop = selectionTop();
    int selHeight = selectionHeight();
    const Font& f = textObj->style(m_firstLine)->font();

    IntRect r = enclosingIntRect(f.selectionRectForText(TextRun(textObj->text()->characters() + m_start, m_len, textObj->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride),
                                                        IntPoint(tx + m_x, ty + selTop), selHeight, sPos, ePos));
    if (r.x() > tx + m_x + m_width)
        r.setWidth(0);
    else if (r.right() - 1 > tx + m_x + m_width)
        r.setWidth(tx + m_x + m_width - r.x());
    return r;
}

void InlineTextBox::deleteLine(RenderArena* arena)
{
    toRenderText(renderer())->removeTextBox(this);
    destroy(arena);
}

void InlineTextBox::extractLine()
{
    if (m_extracted)
        return;

    toRenderText(renderer())->extractTextBox(this);
}

void InlineTextBox::attachLine()
{
    if (!m_extracted)
        return;
    
    toRenderText(renderer())->attachTextBox(this);
}

int InlineTextBox::placeEllipsisBox(bool flowIsLTR, int visibleLeftEdge, int visibleRightEdge, int ellipsisWidth, bool& foundBox)
{
    if (foundBox) {
        m_truncation = cFullTruncation;
        return -1;
    }

    // For LTR this is the left edge of the box, for RTL, the right edge in parent coordinates.
    int ellipsisX = flowIsLTR ? visibleRightEdge - ellipsisWidth : visibleLeftEdge + ellipsisWidth;
    
    // Criteria for full truncation:
    // LTR: the left edge of the ellipsis is to the left of our text run.
    // RTL: the right edge of the ellipsis is to the right of our text run.
    bool ltrFullTruncation = flowIsLTR && ellipsisX <= m_x;
    bool rtlFullTruncation = !flowIsLTR && ellipsisX >= (m_x + m_width);
    if (ltrFullTruncation || rtlFullTruncation) {
        // Too far.  Just set full truncation, but return -1 and let the ellipsis just be placed at the edge of the box.
        m_truncation = cFullTruncation;
        foundBox = true;
        return -1;
    }

    bool ltrEllipsisWithinBox = flowIsLTR && (ellipsisX < m_x + m_width);
    bool rtlEllipsisWithinBox = !flowIsLTR && (ellipsisX > m_x);
    if (ltrEllipsisWithinBox || rtlEllipsisWithinBox) {
        foundBox = true;

        // The inline box may have different directionality than it's parent.  Since truncation
        // behavior depends both on both the parent and the inline block's directionality, we
        // must keep track of these separately.
        bool ltr = direction() == LTR;
        if (ltr != flowIsLTR) {
          // Width in pixels of the visible portion of the box, excluding the ellipsis.
          int visibleBoxWidth = visibleRightEdge - visibleLeftEdge  - ellipsisWidth;
          ellipsisX = ltr ? m_x + visibleBoxWidth : m_x + m_width - visibleBoxWidth;
        }

        int offset = offsetForPosition(ellipsisX, false);
        if (offset == 0) {
            // No characters should be rendered.  Set ourselves to full truncation and place the ellipsis at the min of our start
            // and the ellipsis edge.
            m_truncation = cFullTruncation;
            return min(ellipsisX, m_x);
        }

        // Set the truncation index on the text run.
        m_truncation = offset;

        // If we got here that means that we were only partially truncated and we need to return the pixel offset at which
        // to place the ellipsis.
        int widthOfVisibleText = toRenderText(renderer())->width(m_start, offset, textPos(), m_firstLine);

        // The ellipsis needs to be placed just after the last visible character.
        // Where "after" is defined by the flow directionality, not the inline
        // box directionality.
        // e.g. In the case of an LTR inline box truncated in an RTL flow then we can
        // have a situation such as |Hello| -> |...He|
        if (flowIsLTR)
            return m_x + widthOfVisibleText;
        else
            return (m_x + m_width) - widthOfVisibleText - ellipsisWidth;
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

void updateGraphicsContext(GraphicsContext* context, const Color& fillColor, const Color& strokeColor, float strokeThickness, ColorSpace colorSpace)
{
    int mode = context->textDrawingMode();
    if (strokeThickness > 0) {
        int newMode = mode | cTextStroke;
        if (mode != newMode) {
            context->setTextDrawingMode(newMode);
            mode = newMode;
        }
    }
    
    if (mode & cTextFill && (fillColor != context->fillColor() || colorSpace != context->fillColorSpace()))
        context->setFillColor(fillColor, colorSpace);

    if (mode & cTextStroke) {
        if (strokeColor != context->strokeColor())
            context->setStrokeColor(strokeColor, colorSpace);
        if (strokeThickness != context->strokeThickness())
            context->setStrokeThickness(strokeThickness);
    }
}

bool InlineTextBox::isLineBreak() const
{
    return renderer()->isBR() || (renderer()->style()->preserveNewline() && len() == 1 && (*textRenderer()->text())[start()] == '\n');
}

bool InlineTextBox::nodeAtPoint(const HitTestRequest&, HitTestResult& result, int x, int y, int tx, int ty)
{
    if (isLineBreak())
        return false;

    IntRect rect(tx + m_x, ty + m_y, m_width, height());
    if (m_truncation != cFullTruncation && visibleToHitTesting() && rect.contains(x, y)) {
        renderer()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
        return true;
    }
    return false;
}

static void paintTextWithShadows(GraphicsContext* context, const Font& font, const TextRun& textRun, int startOffset, int endOffset, const IntPoint& textOrigin, int x, int y, int w, int h, ShadowData* shadow, bool stroked)
{
    Color fillColor = context->fillColor();
    ColorSpace fillColorSpace = context->fillColorSpace();
    bool opaque = fillColor.alpha() == 255;
    if (!opaque)
        context->setFillColor(Color::black, fillColorSpace);

    do {
        IntSize extraOffset;

        if (shadow) {
            IntSize shadowOffset(shadow->x, shadow->y);
            int shadowBlur = shadow->blur;
            const Color& shadowColor = shadow->color;

            if (shadow->next || stroked || !opaque) {
                IntRect shadowRect(x, y, w, h);
                shadowRect.inflate(shadowBlur);
                shadowRect.move(shadowOffset);
                context->save();
                context->clip(shadowRect);

                extraOffset = IntSize(0, 2 * h + max(0, shadowOffset.height()) + shadowBlur);
                shadowOffset -= extraOffset;
            }
            context->setShadow(shadowOffset, shadowBlur, shadowColor);
        } else if (!opaque)
            context->setFillColor(fillColor, fillColorSpace);

        if (startOffset <= endOffset)
            context->drawText(font, textRun, textOrigin + extraOffset, startOffset, endOffset);
        else {
            if (endOffset > 0)
                context->drawText(font, textRun, textOrigin + extraOffset,  0, endOffset);
            if (startOffset < textRun.length())
                context->drawText(font, textRun, textOrigin + extraOffset, startOffset);
        }

        if (!shadow)
            break;

        if (shadow->next || stroked || !opaque)
            context->restore();
        else
            context->clearShadow();

        shadow = shadow->next;
    } while (shadow || stroked || !opaque);
}

void InlineTextBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (isLineBreak() || !renderer()->shouldPaintWithinRoot(paintInfo) || renderer()->style()->visibility() != VISIBLE ||
        m_truncation == cFullTruncation || paintInfo.phase == PaintPhaseOutline)
        return;

    ASSERT(paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines);

    // FIXME: Technically we're potentially incorporating other visual overflow that had nothing to do with us.
    // Would it be simpler to just check our own shadow and stroke overflow by hand here?
    int leftOverflow = parent()->x() - parent()->leftVisualOverflow();
    int rightOverflow = parent()->rightVisualOverflow() - (parent()->x() + parent()->width());
    int xPos = tx + m_x - leftOverflow;
    int w = width() + leftOverflow + rightOverflow;
    if (xPos >= paintInfo.rect.right() || xPos + w <= paintInfo.rect.x())
        return;

    bool isPrinting = textRenderer()->document()->printing();
    
    // Determine whether or not we're selected.
    bool haveSelection = !isPrinting && paintInfo.phase != PaintPhaseTextClip && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && paintInfo.phase == PaintPhaseSelection)
        // When only painting the selection, don't bother to paint if there is none.
        return;

    if (m_truncation != cNoTruncation) {
        TextDirection flowDirection = renderer()->containingBlock()->style()->direction();
        if (flowDirection != direction()) {
            // Make the visible fragment of text hug the edge closest to the rest of the run by moving the origin
            // at which we start drawing text.
            // e.g. In the case of LTR text truncated in an RTL Context, the correct behavior is:
            // |Hello|CBA| -> |...He|CBA|
            // In order to draw the fragment "He" aligned to the right edge of it's box, we need to start drawing
            // farther to the right.
            // NOTE: WebKit's behavior differs from that of IE which appears to just overlay the ellipsis on top of the
            // truncated string i.e.  |Hello|CBA| -> |...lo|CBA|
            int widthOfVisibleText = toRenderText(renderer())->width(m_start, m_truncation, textPos(), m_firstLine);
            int widthOfHiddenText = m_width - widthOfVisibleText;
            // FIXME: The hit testing logic also needs to take this translation int account.
            tx += direction() == LTR ? widthOfHiddenText : -widthOfHiddenText;
        }
    }

    GraphicsContext* context = paintInfo.context;

    // Determine whether or not we have composition underlines to draw.
    bool containsComposition = renderer()->node() && renderer()->document()->frame()->editor()->compositionNode() == renderer()->node();
    bool useCustomUnderlines = containsComposition && renderer()->document()->frame()->editor()->compositionUsesCustomUnderlines();

    // Set our font.
    RenderStyle* styleToUse = renderer()->style(m_firstLine);
    int d = styleToUse->textDecorationsInEffect();
    const Font& font = styleToUse->font();

    // 1. Paint backgrounds behind text if needed. Examples of such backgrounds include selection
    // and composition underlines.
    if (paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseTextClip && !isPrinting) {
#if PLATFORM(MAC)
        // Custom highlighters go behind everything else.
        if (styleToUse->highlight() != nullAtom && !context->paintingDisabled())
            paintCustomHighlight(tx, ty, styleToUse->highlight());
#endif

        if (containsComposition && !useCustomUnderlines)
            paintCompositionBackground(context, tx, ty, styleToUse, font,
                renderer()->document()->frame()->editor()->compositionStart(),
                renderer()->document()->frame()->editor()->compositionEnd());

        paintDocumentMarkers(context, tx, ty, styleToUse, font, true);

        if (haveSelection && !useCustomUnderlines)
            paintSelection(context, tx, ty, styleToUse, font);
    }

    // 2. Now paint the foreground, including text and decorations like underline/overline (in quirks mode only).
    if (m_len <= 0)
        return;

    Color textFillColor;
    Color textStrokeColor;
    float textStrokeWidth = styleToUse->textStrokeWidth();
    ShadowData* textShadow = paintInfo.forceBlackText ? 0 : styleToUse->textShadow();

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

    bool paintSelectedTextOnly = (paintInfo.phase == PaintPhaseSelection);
    bool paintSelectedTextSeparately = false;

    Color selectionFillColor = textFillColor;
    Color selectionStrokeColor = textStrokeColor;
    float selectionStrokeWidth = textStrokeWidth;
    ShadowData* selectionShadow = textShadow;
    if (haveSelection) {
        // Check foreground color first.
        Color foreground = paintInfo.forceBlackText ? Color::black : renderer()->selectionForegroundColor();
        if (foreground.isValid() && foreground != selectionFillColor) {
            if (!paintSelectedTextOnly)
                paintSelectedTextSeparately = true;
            selectionFillColor = foreground;
        }

        if (RenderStyle* pseudoStyle = renderer()->getCachedPseudoStyle(SELECTION)) {
            ShadowData* shadow = paintInfo.forceBlackText ? 0 : pseudoStyle->textShadow();
            if (shadow != selectionShadow) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionShadow = shadow;
            }

            float strokeWidth = pseudoStyle->textStrokeWidth();
            if (strokeWidth != selectionStrokeWidth) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeWidth = strokeWidth;
            }

            Color stroke = paintInfo.forceBlackText ? Color::black : pseudoStyle->textStrokeColor();
            if (!stroke.isValid())
                stroke = pseudoStyle->color();
            if (stroke != selectionStrokeColor) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeColor = stroke;
            }
        }
    }

    int baseline = renderer()->style(m_firstLine)->font().ascent();
    IntPoint textOrigin(m_x + tx, m_y + ty + baseline);
    TextRun textRun(textRenderer()->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || styleToUse->visuallyOrdered());

    int sPos = 0;
    int ePos = 0;
    if (paintSelectedTextOnly || paintSelectedTextSeparately)
        selectionStartEnd(sPos, ePos);

    if (!paintSelectedTextOnly) {
        // For stroked painting, we have to change the text drawing mode.  It's probably dangerous to leave that mutated as a side
        // effect, so only when we know we're stroking, do a save/restore.
        if (textStrokeWidth > 0)
            context->save();

        updateGraphicsContext(context, textFillColor, textStrokeColor, textStrokeWidth, styleToUse->colorSpace());
        if (!paintSelectedTextSeparately || ePos <= sPos) {
            // FIXME: Truncate right-to-left text correctly.
            paintTextWithShadows(context, font, textRun, 0, m_truncation == cNoTruncation ? m_len : m_truncation, textOrigin, m_x + tx, m_y + ty, width(), height(), textShadow, textStrokeWidth > 0);
        } else
            paintTextWithShadows(context, font, textRun, ePos, sPos, textOrigin, m_x + tx, m_y + ty, width(), height(), textShadow, textStrokeWidth > 0);

        if (textStrokeWidth > 0)
            context->restore();
    }

    if ((paintSelectedTextOnly || paintSelectedTextSeparately) && sPos < ePos) {
        // paint only the text that is selected
        if (selectionStrokeWidth > 0)
            context->save();

        updateGraphicsContext(context, selectionFillColor, selectionStrokeColor, selectionStrokeWidth, styleToUse->colorSpace());
        paintTextWithShadows(context, font, textRun, sPos, ePos, textOrigin, m_x + tx, m_y + ty, width(), height(), selectionShadow, selectionStrokeWidth > 0);

        if (selectionStrokeWidth > 0)
            context->restore();
    }

    // Paint decorations
    if (d != TDNONE && paintInfo.phase != PaintPhaseSelection && styleToUse->htmlHacks()) {
        context->setStrokeColor(styleToUse->color(), styleToUse->colorSpace());
        paintDecoration(context, tx, ty, d, textShadow);
    }

    if (paintInfo.phase == PaintPhaseForeground) {
        paintDocumentMarkers(context, tx, ty, styleToUse, font, false);

        if (useCustomUnderlines) {
            const Vector<CompositionUnderline>& underlines = renderer()->document()->frame()->editor()->customCompositionUnderlines();
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
                    paintCompositionUnderline(context, tx, ty, underline);
                    if (underline.endOffset > end() + 1)
                        // underline also runs into the next run. Bail now, no more marker advancement.
                        break;
                } else
                    // underline is completely after this run, bail.  A later run will paint it.
                    break;
            }
        }
    }
}

void InlineTextBox::selectionStartEnd(int& sPos, int& ePos)
{
    int startPos, endPos;
    if (renderer()->selectionState() == RenderObject::SelectionInside) {
        startPos = 0;
        endPos = textRenderer()->textLength();
    } else {
        textRenderer()->selectionStartEnd(startPos, endPos);
        if (renderer()->selectionState() == RenderObject::SelectionStart)
            endPos = textRenderer()->textLength();
        else if (renderer()->selectionState() == RenderObject::SelectionEnd)
            startPos = 0;
    }

    sPos = max(startPos - m_start, 0);
    ePos = min(endPos - m_start, (int)m_len);
}

void InlineTextBox::paintSelection(GraphicsContext* context, int tx, int ty, RenderStyle* style, const Font& font)
{
    // See if we have a selection to paint at all.
    int sPos, ePos;
    selectionStartEnd(sPos, ePos);
    if (sPos >= ePos)
        return;

    Color textColor = style->color();
    Color c = renderer()->selectionBackgroundColor();
    if (!c.isValid() || c.alpha() == 0)
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.  This should basically never happen, since the selection has transparency.
    if (textColor == c)
        c = Color(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    context->save();
    updateGraphicsContext(context, c, c, 0, style->colorSpace());  // Don't draw text at all!
    int y = selectionTop();
    int h = selectionHeight();
    context->clip(IntRect(m_x + tx, y + ty, m_width, h));
    context->drawHighlightForText(font, TextRun(textRenderer()->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd, 
                                  direction() == RTL, m_dirOverride || style->visuallyOrdered()),
                                  IntPoint(m_x + tx, y + ty), h, c, style->colorSpace(), sPos, ePos);
    context->restore();
}

void InlineTextBox::paintCompositionBackground(GraphicsContext* context, int tx, int ty, RenderStyle* style, const Font& font, int startPos, int endPos)
{
    int offset = m_start;
    int sPos = max(startPos - offset, 0);
    int ePos = min(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

    context->save();

    Color c = Color(225, 221, 85);
    
    updateGraphicsContext(context, c, c, 0, style->colorSpace()); // Don't draw text at all!

    int y = selectionTop();
    int h = selectionHeight();
    context->drawHighlightForText(font, TextRun(textRenderer()->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd,
                                  direction() == RTL, m_dirOverride || style->visuallyOrdered()),
                                  IntPoint(m_x + tx, y + ty), h, c, style->colorSpace(), sPos, ePos);
    context->restore();
}

#if PLATFORM(MAC)

void InlineTextBox::paintCustomHighlight(int tx, int ty, const AtomicString& type)
{
    Frame* frame = renderer()->document()->frame();
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;

    RootInlineBox* r = root();
    FloatRect rootRect(tx + r->x(), ty + selectionTop(), r->width(), selectionHeight());
    FloatRect textRect(tx + x(), rootRect.y(), width(), rootRect.height());

    page->chrome()->client()->paintCustomHighlight(renderer()->node(), type, textRect, rootRect, true, false);
}

#endif

void InlineTextBox::paintDecoration(GraphicsContext* context, int tx, int ty, int deco, ShadowData* shadow)
{
    tx += m_x;
    ty += m_y;

    if (m_truncation == cFullTruncation)
        return;

    int width = m_width;
    if (m_truncation != cNoTruncation) {
        width = toRenderText(renderer())->width(m_start, m_truncation, textPos(), m_firstLine);
        if (direction() == RTL)
            tx += (m_width - width);
    }
    
    // Get the text decoration colors.
    Color underline, overline, linethrough;
    renderer()->getTextDecorationColors(deco, underline, overline, linethrough, true);
    
    // Use a special function for underlines to get the positioning exactly right.
    bool isPrinting = textRenderer()->document()->printing();
    context->setStrokeThickness(1.0f); // FIXME: We should improve this rule and not always just assume 1.

    bool linesAreOpaque = !isPrinting && (!(deco & UNDERLINE) || underline.alpha() == 255) && (!(deco & OVERLINE) || overline.alpha() == 255) && (!(deco & LINE_THROUGH) || linethrough.alpha() == 255);

    int baseline = renderer()->style(m_firstLine)->font().ascent();

    bool setClip = false;
    int extraOffset = 0;
    if (!linesAreOpaque && shadow && shadow->next) {
        context->save();
        IntRect clipRect(tx, ty, width, baseline + 2);
        for (ShadowData* s = shadow; s; s = s->next) {
            IntRect shadowRect(tx, ty, width, baseline + 2);
            shadowRect.inflate(s->blur);
            shadowRect.move(s->x, s->y);
            clipRect.unite(shadowRect);
            extraOffset = max(extraOffset, max(0, s->y) + s->blur);
        }
        context->save();
        context->clip(clipRect);
        extraOffset += baseline + 2;
        ty += extraOffset;
        setClip = true;
    }

    bool setShadow = false;
    
    do {
        if (shadow) {
            if (!shadow->next) {
                // The last set of lines paints normally inside the clip.
                ty -= extraOffset;
                extraOffset = 0;
            }
            context->setShadow(IntSize(shadow->x, shadow->y - extraOffset), shadow->blur, shadow->color);
            setShadow = true;
            shadow = shadow->next;
        }

        if (deco & UNDERLINE) {
            context->setStrokeColor(underline, renderer()->style()->colorSpace());
            context->setStrokeStyle(SolidStroke);
            // Leave one pixel of white between the baseline and the underline.
            context->drawLineForText(IntPoint(tx, ty + baseline + 1), width, isPrinting);
        }
        if (deco & OVERLINE) {
            context->setStrokeColor(overline, renderer()->style()->colorSpace());
            context->setStrokeStyle(SolidStroke);
            context->drawLineForText(IntPoint(tx, ty), width, isPrinting);
        }
        if (deco & LINE_THROUGH) {
            context->setStrokeColor(linethrough, renderer()->style()->colorSpace());
            context->setStrokeStyle(SolidStroke);
            context->drawLineForText(IntPoint(tx, ty + 2 * baseline / 3), width, isPrinting);
        }
    } while (shadow);

    if (setClip)
        context->restore();
    else if (setShadow)
        context->clearShadow();
}

void InlineTextBox::paintSpellingOrGrammarMarker(GraphicsContext* pt, int tx, int ty, DocumentMarker marker, RenderStyle* style, const Font& font, bool grammar)
{
    // Never print spelling/grammar markers (5327887)
    if (textRenderer()->document()->printing())
        return;

    if (m_truncation == cFullTruncation)
        return;

    int start = 0;                  // start of line to draw, relative to tx
    int width = m_width;            // how much line to draw

    // Determine whether we need to measure text
    bool markerSpansWholeBox = true;
    if (m_start <= (int)marker.startOffset)
        markerSpansWholeBox = false;
    if ((end() + 1) != marker.endOffset)      // end points at the last char, not past it
        markerSpansWholeBox = false;
    if (m_truncation != cNoTruncation)
        markerSpansWholeBox = false;

    if (!markerSpansWholeBox || grammar) {
        int startPosition = max<int>(marker.startOffset - m_start, 0);
        int endPosition = min<int>(marker.endOffset - m_start, m_len);
        
        if (m_truncation != cNoTruncation)
            endPosition = min<int>(endPosition, m_truncation);

        // Calculate start & width
        IntPoint startPoint(tx + m_x, ty + selectionTop());
        TextRun run(textRenderer()->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || style->visuallyOrdered());
        int h = selectionHeight();
        
        IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, startPoint, h, startPosition, endPosition));
        start = markerRect.x() - startPoint.x();
        width = markerRect.width();
        
        // Store rendered rects for bad grammar markers, so we can hit-test against it elsewhere in order to
        // display a toolTip. We don't do this for misspelling markers.
        if (grammar)
            renderer()->document()->setRenderedRectForMarker(renderer()->node(), marker, markerRect);
    }
    
    // IMPORTANT: The misspelling underline is not considered when calculating the text bounds, so we have to
    // make sure to fit within those bounds.  This means the top pixel(s) of the underline will overlap the
    // bottom pixel(s) of the glyphs in smaller font sizes.  The alternatives are to increase the line spacing (bad!!)
    // or decrease the underline thickness.  The overlap is actually the most useful, and matches what AppKit does.
    // So, we generally place the underline at the bottom of the text, but in larger fonts that's not so good so
    // we pin to two pixels under the baseline.
    int lineThickness = cMisspellingLineThickness;
    int baseline = renderer()->style(m_firstLine)->font().ascent();
    int descent = height() - baseline;
    int underlineOffset;
    if (descent <= (2 + lineThickness)) {
        // Place the underline at the very bottom of the text in small/medium fonts.
        underlineOffset = height() - lineThickness;
    } else {
        // In larger fonts, though, place the underline up near the baseline to prevent a big gap.
        underlineOffset = baseline + 2;
    }
    pt->drawLineForMisspellingOrBadGrammar(IntPoint(tx + m_x + start, ty + m_y + underlineOffset), width, grammar);
}

void InlineTextBox::paintTextMatchMarker(GraphicsContext* pt, int tx, int ty, DocumentMarker marker, RenderStyle* style, const Font& font)
{
    // Use same y positioning and height as for selection, so that when the selection and this highlight are on
    // the same word there are no pieces sticking out.
    int y = selectionTop();
    int h = selectionHeight();
    
    int sPos = max(marker.startOffset - m_start, (unsigned)0);
    int ePos = min(marker.endOffset - m_start, (unsigned)m_len);    
    TextRun run(textRenderer()->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || style->visuallyOrdered());
    IntPoint startPoint = IntPoint(m_x + tx, y + ty);
    
    // Always compute and store the rect associated with this marker
    IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, startPoint, h, sPos, ePos));
    renderer()->document()->setRenderedRectForMarker(renderer()->node(), marker, markerRect);
     
    // Optionally highlight the text
    if (renderer()->document()->frame()->markedTextMatchesAreHighlighted()) {
        Color color = marker.activeMatch ?
            renderer()->theme()->platformActiveTextSearchHighlightColor() :
            renderer()->theme()->platformInactiveTextSearchHighlightColor();
        pt->save();
        updateGraphicsContext(pt, color, color, 0, style->colorSpace());  // Don't draw text at all!
        pt->clip(IntRect(tx + m_x, ty + y, m_width, h));
        pt->drawHighlightForText(font, run, startPoint, h, color, style->colorSpace(), sPos, ePos);
        pt->restore();
    }
}

void InlineTextBox::computeRectForReplacementMarker(int tx, int ty, DocumentMarker marker, RenderStyle* style, const Font& font)
{
    // Replacement markers are not actually drawn, but their rects need to be computed for hit testing.
    int y = selectionTop();
    int h = selectionHeight();
    
    int sPos = max(marker.startOffset - m_start, (unsigned)0);
    int ePos = min(marker.endOffset - m_start, (unsigned)m_len);    
    TextRun run(textRenderer()->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || style->visuallyOrdered());
    IntPoint startPoint = IntPoint(m_x + tx, y + ty);
    
    // Compute and store the rect associated with this marker.
    IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, startPoint, h, sPos, ePos));
    renderer()->document()->setRenderedRectForMarker(renderer()->node(), marker, markerRect);
}
    
void InlineTextBox::paintDocumentMarkers(GraphicsContext* pt, int tx, int ty, RenderStyle* style, const Font& font, bool background)
{
    if (!renderer()->node())
        return;

    Vector<DocumentMarker> markers = renderer()->document()->markersForNode(renderer()->node());
    Vector<DocumentMarker>::iterator markerIt = markers.begin();

    // Give any document markers that touch this run a chance to draw before the text has been drawn.
    // Note end() points at the last char, not one past it like endOffset and ranges do.
    for ( ; markerIt != markers.end(); markerIt++) {
        DocumentMarker marker = *markerIt;
        
        // Paint either the background markers or the foreground markers, but not both
        switch (marker.type) {
            case DocumentMarker::Grammar:
            case DocumentMarker::Spelling:
            case DocumentMarker::Replacement:
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
                paintSpellingOrGrammarMarker(pt, tx, ty, marker, style, font, false);
                break;
            case DocumentMarker::Grammar:
                paintSpellingOrGrammarMarker(pt, tx, ty, marker, style, font, true);
                break;
            case DocumentMarker::TextMatch:
                paintTextMatchMarker(pt, tx, ty, marker, style, font);
                break;
            case DocumentMarker::Replacement:
                computeRectForReplacementMarker(tx, ty, marker, style, font);
                break;
            default:
                ASSERT_NOT_REACHED();
        }

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
        start = toRenderText(renderer())->width(m_start, paintStart - m_start, textPos(), m_firstLine);
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
        width = toRenderText(renderer())->width(paintStart, paintEnd - paintStart, textPos() + start, m_firstLine);
    }

    // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
    // All other marked text underlines are 1px thick.
    // If there's not enough space the underline will touch or overlap characters.
    int lineThickness = 1;
    int baseline = renderer()->style(m_firstLine)->font().ascent();
    if (underline.thick && height() - baseline >= 2)
        lineThickness = 2;

    // We need to have some space between underlines of subsequent clauses, because some input methods do not use different underline styles for those.
    // We make each line shorter, which has a harmless side effect of shortening the first and last clauses, too.
    start += 1;
    width -= 2;

    ctx->setStrokeColor(underline.color, renderer()->style()->colorSpace());
    ctx->setStrokeThickness(lineThickness);
    ctx->drawLineForText(IntPoint(tx + start, ty + height() - lineThickness), width, textRenderer()->document()->printing());
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
    if (x() == 0)
        return 0;
        
    RenderBlock *blockElement = renderer()->containingBlock();
    return direction() == RTL ? x() - blockElement->borderRight() - blockElement->paddingRight()
                      : x() - blockElement->borderLeft() - blockElement->paddingLeft();
}

int InlineTextBox::offsetForPosition(int _x, bool includePartialGlyphs) const
{
    if (isLineBreak())
        return 0;

    RenderText* text = toRenderText(renderer());
    RenderStyle *style = text->style(m_firstLine);
    const Font* f = &style->font();
    return f->offsetForPosition(TextRun(textRenderer()->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || style->visuallyOrdered()),
                                _x - m_x, includePartialGlyphs);
}

int InlineTextBox::positionForOffset(int offset) const
{
    ASSERT(offset >= m_start);
    ASSERT(offset <= m_start + m_len);

    if (isLineBreak())
        return m_x;

    RenderText* text = toRenderText(renderer());
    const Font& f = text->style(m_firstLine)->font();
    int from = direction() == RTL ? offset - m_start : 0;
    int to = direction() == RTL ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    return enclosingIntRect(f.selectionRectForText(TextRun(text->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride),
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

typedef HashMap<InlineTextBox*, Vector<const SimpleFontData*> > FallbackFontsMap;
static FallbackFontsMap* gFallbackFontsMap;

void InlineTextBox::setFallbackFonts(const HashSet<const SimpleFontData*>& fallbackFonts)
{
    if (!gFallbackFontsMap)
        gFallbackFontsMap = new FallbackFontsMap;

    FallbackFontsMap::iterator it = gFallbackFontsMap->set(this, Vector<const SimpleFontData*>()).first;
    ASSERT(it->second.isEmpty());
    copyToVector(fallbackFonts, it->second);
}

void InlineTextBox::takeFallbackFonts(Vector<const SimpleFontData*>& fallbackFonts)
{
    if (!gFallbackFontsMap)
        return;

    FallbackFontsMap::iterator it = gFallbackFontsMap->find(this);
    if (it == gFallbackFontsMap->end())
        return;

    fallbackFonts.swap(it->second);
    gFallbackFontsMap->remove(it);
}

} // namespace WebCore
