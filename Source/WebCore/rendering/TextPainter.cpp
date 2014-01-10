/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "TextPainter.h"

#include "GraphicsContext.h"
#include "InlineTextBox.h"
#include "RenderCombineText.h"
#include "TextPaintStyle.h"

namespace WebCore {

TextPainter::TextPainter(GraphicsContext& context, bool paintSelectedTextOnly, bool paintSelectedTextSeparately, const Font& font,
    int startPositionInTextRun, int endPositionInTextBoxString, int length, const AtomicString& emphasisMark, RenderCombineText* combinedText, TextRun& textRun,
    FloatRect& boxRect, FloatPoint& textOrigin, int emphasisMarkOffset, const ShadowData* textShadow, const ShadowData* selectionShadow,
    bool textBoxIsHorizontal, TextPaintStyle& textPaintStyle, TextPaintStyle& selectionPaintStyle)
    : m_paintSelectedTextOnly(paintSelectedTextOnly)
    , m_paintSelectedTextSeparately(paintSelectedTextSeparately)
    , m_font(font)
    , m_startPositionInTextRun(startPositionInTextRun)
    , m_endPositionInTextRun(endPositionInTextBoxString)
    , m_length(length)
    , m_emphasisMark(emphasisMark)
    , m_combinedText(combinedText)
    , m_textRun(textRun)
    , m_boxRect(boxRect)
    , m_textOrigin(textOrigin)
    , m_emphasisMarkOffset(emphasisMarkOffset)
    , m_textBoxIsHorizontal(textBoxIsHorizontal)
    , m_savedDrawingStateForMask(&context, &textPaintStyle, &selectionPaintStyle, textShadow, selectionShadow)
{
}

static void drawTextOrEmphasisMarks(GraphicsContext& context, const Font& font, const TextRun& textRun, const AtomicString& emphasisMark,
    int emphasisMarkOffset, const FloatPoint& point, const int from, const int to)
{
    if (emphasisMark.isEmpty())
        context.drawText(font, textRun, point, from, to);
    else
        context.drawEmphasisMarks(font, textRun, emphasisMark, point + IntSize(0, emphasisMarkOffset), from, to);
}

static void paintTextWithShadows(GraphicsContext* context, const Font& font, const TextRun& textRun, const AtomicString& emphasisMark,
    int emphasisMarkOffset, int startOffset, int endOffset, int truncationPoint, const FloatPoint& textOrigin, const FloatRect& boxRect,
    const ShadowData* shadow, bool stroked, bool horizontal)
{
    Color fillColor = context->fillColor();
    ColorSpace fillColorSpace = context->fillColorSpace();
    bool opaque = !fillColor.hasAlpha();
    if (!opaque)
        context->setFillColor(Color::black, fillColorSpace);

    do {
        IntSize extraOffset;
        if (shadow)
            extraOffset = roundedIntSize(InlineTextBox::applyShadowToGraphicsContext(context, shadow, boxRect, stroked, opaque, horizontal));
        else if (!opaque)
            context->setFillColor(fillColor, fillColorSpace);

        if (startOffset <= endOffset)
            drawTextOrEmphasisMarks(*context, font, textRun, emphasisMark, emphasisMarkOffset, textOrigin + extraOffset, startOffset, endOffset);
        else {
            if (endOffset > 0)
                drawTextOrEmphasisMarks(*context, font, textRun, emphasisMark, emphasisMarkOffset, textOrigin + extraOffset, 0, endOffset);
            if (startOffset < truncationPoint)
                drawTextOrEmphasisMarks(*context, font, textRun, emphasisMark, emphasisMarkOffset, textOrigin + extraOffset, startOffset, truncationPoint);
        }

        if (!shadow)
            break;

        if (shadow->next() || stroked || !opaque)
            context->restore();
        else
            context->clearShadow();

        shadow = shadow->next();
    } while (shadow || stroked || !opaque);
}

void TextPainter::paintText()
{
    ASSERT(m_savedDrawingStateForMask.m_textPaintStyle);
    ASSERT(m_savedDrawingStateForMask.m_selectionPaintStyle);
    
    FloatPoint boxOrigin = m_boxRect.location();

    if (!m_paintSelectedTextOnly) {
        // For stroked painting, we have to change the text drawing mode. It's probably dangerous to leave that mutated as a side
        // effect, so only when we know we're stroking, do a save/restore.
        GraphicsContextStateSaver stateSaver(*m_savedDrawingStateForMask.m_context, m_savedDrawingStateForMask.m_textPaintStyle->strokeWidth > 0);

        updateGraphicsContext(*m_savedDrawingStateForMask.m_context, *m_savedDrawingStateForMask.m_textPaintStyle);
        if (!m_paintSelectedTextSeparately || m_endPositionInTextRun <= m_startPositionInTextRun) {
            // FIXME: Truncate right-to-left text correctly.
            paintTextWithShadows(m_savedDrawingStateForMask.m_context, m_font, m_textRun, nullAtom, 0, 0, m_length, m_length, m_textOrigin, m_boxRect, m_savedDrawingStateForMask.m_textShadow, m_savedDrawingStateForMask.m_textPaintStyle->strokeWidth > 0, m_textBoxIsHorizontal);
        } else
            paintTextWithShadows(m_savedDrawingStateForMask.m_context, m_font, m_textRun, nullAtom, 0, m_endPositionInTextRun, m_startPositionInTextRun, m_length, m_textOrigin, m_boxRect, m_savedDrawingStateForMask.m_textShadow, m_savedDrawingStateForMask.m_textPaintStyle->strokeWidth > 0, m_textBoxIsHorizontal);

        if (!m_emphasisMark.isEmpty()) {
            updateGraphicsContext(*m_savedDrawingStateForMask.m_context, *m_savedDrawingStateForMask.m_textPaintStyle, UseEmphasisMarkColor);

            DEFINE_STATIC_LOCAL(TextRun, objectReplacementCharacterTextRun, (&objectReplacementCharacter, 1));
            TextRun& emphasisMarkTextRun = m_combinedText ? objectReplacementCharacterTextRun : m_textRun;
            FloatPoint emphasisMarkTextOrigin = m_combinedText ? FloatPoint(boxOrigin.x() + m_boxRect.width() / 2, boxOrigin.y() + m_font.fontMetrics().ascent()) : m_textOrigin;
            if (m_combinedText)
                m_savedDrawingStateForMask.m_context->concatCTM(rotation(m_boxRect, Clockwise));

            if (!m_paintSelectedTextSeparately || m_endPositionInTextRun <= m_startPositionInTextRun) {
                // FIXME: Truncate right-to-left text correctly.
                paintTextWithShadows(m_savedDrawingStateForMask.m_context, m_combinedText ? m_combinedText->originalFont() : m_font, emphasisMarkTextRun, m_emphasisMark, m_emphasisMarkOffset, 0, m_length, m_length, emphasisMarkTextOrigin, m_boxRect, m_savedDrawingStateForMask.m_textShadow, m_savedDrawingStateForMask.m_textPaintStyle->strokeWidth > 0, m_textBoxIsHorizontal);
            } else
                paintTextWithShadows(m_savedDrawingStateForMask.m_context, m_combinedText ? m_combinedText->originalFont() : m_font, emphasisMarkTextRun, m_emphasisMark, m_emphasisMarkOffset, m_endPositionInTextRun, m_startPositionInTextRun, m_length, emphasisMarkTextOrigin, m_boxRect, m_savedDrawingStateForMask.m_textShadow, m_savedDrawingStateForMask.m_textPaintStyle->strokeWidth > 0, m_textBoxIsHorizontal);

            if (m_combinedText)
                m_savedDrawingStateForMask.m_context->concatCTM(rotation(m_boxRect, Counterclockwise));
        }
    }

    if ((m_paintSelectedTextOnly || m_paintSelectedTextSeparately) && m_startPositionInTextRun < m_endPositionInTextRun) {
        // paint only the text that is selected
        GraphicsContextStateSaver stateSaver(*m_savedDrawingStateForMask.m_context, m_savedDrawingStateForMask.m_selectionPaintStyle->strokeWidth > 0);

        updateGraphicsContext(*m_savedDrawingStateForMask.m_context, *m_savedDrawingStateForMask.m_selectionPaintStyle);
        paintTextWithShadows(m_savedDrawingStateForMask.m_context, m_font, m_textRun, nullAtom, 0, m_startPositionInTextRun, m_endPositionInTextRun, m_length, m_textOrigin, m_boxRect, m_savedDrawingStateForMask.m_selectionShadow, m_savedDrawingStateForMask.m_selectionPaintStyle->strokeWidth > 0, m_textBoxIsHorizontal);
        if (!m_emphasisMark.isEmpty()) {
            updateGraphicsContext(*m_savedDrawingStateForMask.m_context, *m_savedDrawingStateForMask.m_selectionPaintStyle, UseEmphasisMarkColor);

            DEFINE_STATIC_LOCAL(TextRun, objectReplacementCharacterTextRun, (&objectReplacementCharacter, 1));
            TextRun& emphasisMarkTextRun = m_combinedText ? objectReplacementCharacterTextRun : m_textRun;
            FloatPoint emphasisMarkTextOrigin = m_combinedText ? FloatPoint(boxOrigin.x() + m_boxRect.width() / 2, boxOrigin.y() + m_font.fontMetrics().ascent()) : m_textOrigin;
            if (m_combinedText)
                m_savedDrawingStateForMask.m_context->concatCTM(rotation(m_boxRect, Clockwise));

            paintTextWithShadows(m_savedDrawingStateForMask.m_context, m_combinedText ? m_combinedText->originalFont() : m_font, emphasisMarkTextRun, m_emphasisMark, m_emphasisMarkOffset, m_startPositionInTextRun, m_endPositionInTextRun, m_length, emphasisMarkTextOrigin, m_boxRect, m_savedDrawingStateForMask.m_selectionShadow, m_savedDrawingStateForMask.m_selectionPaintStyle->strokeWidth > 0, m_textBoxIsHorizontal);

            if (m_combinedText)
                m_savedDrawingStateForMask.m_context->concatCTM(rotation(m_boxRect, Counterclockwise));
        }
    }
}

void TextPainter::paintTextInContext(GraphicsContext& context, float amountToIncreaseStrokeWidthBy)
{
    SavedDrawingStateForMask savedDrawingStateForMask = m_savedDrawingStateForMask;
    
    ASSERT(m_savedDrawingStateForMask.m_textPaintStyle);
    ASSERT(m_savedDrawingStateForMask.m_selectionPaintStyle);
    m_savedDrawingStateForMask.m_context = &context;
    m_savedDrawingStateForMask.m_textPaintStyle->strokeWidth += amountToIncreaseStrokeWidthBy;
    m_savedDrawingStateForMask.m_selectionPaintStyle->strokeWidth += amountToIncreaseStrokeWidthBy;
    m_savedDrawingStateForMask.m_textShadow = nullptr;
    m_savedDrawingStateForMask.m_selectionShadow = nullptr;
    paintText();

    m_savedDrawingStateForMask = savedDrawingStateForMask;
}

#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
DashArray TextPainter::dashesForIntersectionsWithRect(const FloatRect& lineExtents)
{
    return m_font.dashesForIntersectionsWithRect(m_textRun, m_textOrigin, lineExtents);
}
#endif

} // namespace WebCore
