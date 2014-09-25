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
#include <wtf/NeverDestroyed.h>

namespace WebCore {

TextPainter::TextPainter(GraphicsContext& context, bool paintSelectedTextOnly, bool paintSelectedTextSeparately, const Font& font,
    int startPositionInTextRun, int endPositionInTextBoxString, int length, const AtomicString& emphasisMark, RenderCombineText* combinedText, TextRun& textRun,
    FloatRect& boxRect, FloatPoint& textOrigin, int emphasisMarkOffset, const ShadowData* textShadow, const ShadowData* selectionShadow,
    bool textBoxIsHorizontal, TextPaintStyle& textPaintStyle, TextPaintStyle& selectionPaintStyle)
        : m_context(context)
        , m_textPaintStyle(textPaintStyle)
        , m_selectionPaintStyle(selectionPaintStyle)
        , m_textShadow(textShadow)
        , m_selectionShadow(selectionShadow)
        , m_paintSelectedTextOnly(paintSelectedTextOnly)
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

ShadowApplier::ShadowApplier(GraphicsContext& context, const ShadowData* shadow, const FloatRect& textRect, bool lastShadowIterationShouldDrawText, bool opaque, FontOrientation orientation)
    : m_context(context)
    , m_shadow(shadow)
    , m_onlyDrawsShadow(!isLastShadowIteration() || !lastShadowIterationShouldDrawText)
    , m_avoidDrawingShadow(shadowIsCompletelyCoveredByText(opaque))
    , m_nothingToDraw(shadow && m_avoidDrawingShadow && m_onlyDrawsShadow)
    , m_didSaveContext(false)
{
    if (!shadow || m_nothingToDraw) {
        m_shadow = nullptr;
        return;
    }

    int shadowX = orientation == Horizontal ? shadow->x() : shadow->y();
    int shadowY = orientation == Horizontal ? shadow->y() : -shadow->x();
    FloatSize shadowOffset(shadowX, shadowY);
    int shadowRadius = shadow->radius();
    const Color& shadowColor = shadow->color();

    // When drawing shadows, we usually clip the context to the area the shadow will reside, and then
    // draw the text itself outside the clipped area (so only the shadow shows up). However, we can
    // often draw the *last* shadow and the text itself in a single call.
    if (m_onlyDrawsShadow) {
        FloatRect shadowRect(textRect);
        shadowRect.inflate(shadow->paintingExtent());
        shadowRect.move(shadowOffset);
        context.save();
        context.clip(shadowRect);

        m_didSaveContext = true;
        m_extraOffset = FloatSize(0, 2 * textRect.height() + std::max(0.0f, shadowOffset.height()) + shadowRadius);
        shadowOffset -= m_extraOffset;
    }

    if (!m_avoidDrawingShadow)
        context.setShadow(shadowOffset, shadowRadius, shadowColor, context.fillColorSpace());
}

ShadowApplier::~ShadowApplier()
{
    if (!m_shadow)
        return;
    if (m_onlyDrawsShadow)
        m_context.restore();
    else if (!m_avoidDrawingShadow)
        m_context.clearShadow();
}

static void paintTextWithShadows(GraphicsContext& context, const Font& font, const TextRun& textRun, const AtomicString& emphasisMark,
    int emphasisMarkOffset, int startOffset, int endOffset, int truncationPoint, const FloatPoint& textOrigin, const FloatRect& boxRect,
    const ShadowData* shadow, bool stroked, bool horizontal)
{
    Color fillColor = context.fillColor();
    ColorSpace fillColorSpace = context.fillColorSpace();
    bool opaque = !fillColor.hasAlpha();
    bool lastShadowIterationShouldDrawText = !stroked && opaque;
    if (!opaque)
        context.setFillColor(Color::black, fillColorSpace);

    do {
        ShadowApplier shadowApplier(context, shadow, boxRect, lastShadowIterationShouldDrawText, opaque, horizontal ? Horizontal : Vertical);
        if (shadowApplier.nothingToDraw()) {
            shadow = shadow->next();
            continue;
        }

        IntSize extraOffset = roundedIntSize(shadowApplier.extraOffset());
        if (!shadow && !opaque)
            context.setFillColor(fillColor, fillColorSpace);

        if (startOffset <= endOffset)
            drawTextOrEmphasisMarks(context, font, textRun, emphasisMark, emphasisMarkOffset, textOrigin + extraOffset, startOffset, endOffset);
        else {
            if (endOffset > 0)
                drawTextOrEmphasisMarks(context, font, textRun, emphasisMark, emphasisMarkOffset, textOrigin + extraOffset, 0, endOffset);
            if (startOffset < truncationPoint)
                drawTextOrEmphasisMarks(context, font, textRun, emphasisMark, emphasisMarkOffset, textOrigin + extraOffset, startOffset, truncationPoint);
        }

        if (!shadow)
            break;

        shadow = shadow->next();
    } while (shadow || !lastShadowIterationShouldDrawText);
}

void TextPainter::paintText()
{
    FloatPoint boxOrigin = m_boxRect.location();

    if (!m_paintSelectedTextOnly) {
        // For stroked painting, we have to change the text drawing mode. It's probably dangerous to leave that mutated as a side
        // effect, so only when we know we're stroking, do a save/restore.
        GraphicsContextStateSaver stateSaver(m_context, m_textPaintStyle.strokeWidth > 0);

        updateGraphicsContext(m_context, m_textPaintStyle);
        if (!m_paintSelectedTextSeparately || m_endPositionInTextRun <= m_startPositionInTextRun) {
            // FIXME: Truncate right-to-left text correctly.
            paintTextWithShadows(m_context, m_font, m_textRun, nullAtom, 0, 0, m_length, m_length, m_textOrigin, m_boxRect, m_textShadow, m_textPaintStyle.strokeWidth > 0, m_textBoxIsHorizontal);
        } else
            paintTextWithShadows(m_context, m_font, m_textRun, nullAtom, 0, m_endPositionInTextRun, m_startPositionInTextRun, m_length, m_textOrigin, m_boxRect, m_textShadow, m_textPaintStyle.strokeWidth > 0, m_textBoxIsHorizontal);

        if (!m_emphasisMark.isEmpty()) {
            updateGraphicsContext(m_context, m_textPaintStyle, UseEmphasisMarkColor);

            static NeverDestroyed<TextRun> objectReplacementCharacterTextRun(&objectReplacementCharacter, 1);
            TextRun& emphasisMarkTextRun = m_combinedText ? objectReplacementCharacterTextRun.get() : m_textRun;
            FloatPoint emphasisMarkTextOrigin = m_combinedText ? FloatPoint(boxOrigin.x() + m_boxRect.width() / 2, boxOrigin.y() + m_font.fontMetrics().ascent()) : m_textOrigin;
            if (m_combinedText)
                m_context.concatCTM(rotation(m_boxRect, Clockwise));

            if (!m_paintSelectedTextSeparately || m_endPositionInTextRun <= m_startPositionInTextRun) {
                // FIXME: Truncate right-to-left text correctly.
                paintTextWithShadows(m_context, m_combinedText ? m_combinedText->originalFont() : m_font, emphasisMarkTextRun, m_emphasisMark, m_emphasisMarkOffset, 0, m_length, m_length, emphasisMarkTextOrigin, m_boxRect, m_textShadow, m_textPaintStyle.strokeWidth > 0, m_textBoxIsHorizontal);
            } else
                paintTextWithShadows(m_context, m_combinedText ? m_combinedText->originalFont() : m_font, emphasisMarkTextRun, m_emphasisMark, m_emphasisMarkOffset, m_endPositionInTextRun, m_startPositionInTextRun, m_length, emphasisMarkTextOrigin, m_boxRect, m_textShadow, m_textPaintStyle.strokeWidth > 0, m_textBoxIsHorizontal);

            if (m_combinedText)
                m_context.concatCTM(rotation(m_boxRect, Counterclockwise));
        }
    }

    if ((m_paintSelectedTextOnly || m_paintSelectedTextSeparately) && m_startPositionInTextRun < m_endPositionInTextRun) {
        // paint only the text that is selected
        GraphicsContextStateSaver stateSaver(m_context, m_selectionPaintStyle.strokeWidth > 0);

        updateGraphicsContext(m_context, m_selectionPaintStyle);
        paintTextWithShadows(m_context, m_font, m_textRun, nullAtom, 0, m_startPositionInTextRun, m_endPositionInTextRun, m_length, m_textOrigin, m_boxRect, m_selectionShadow, m_selectionPaintStyle.strokeWidth > 0, m_textBoxIsHorizontal);
        if (!m_emphasisMark.isEmpty()) {
            updateGraphicsContext(m_context, m_selectionPaintStyle, UseEmphasisMarkColor);

            DEPRECATED_DEFINE_STATIC_LOCAL(TextRun, objectReplacementCharacterTextRun, (&objectReplacementCharacter, 1));
            TextRun& emphasisMarkTextRun = m_combinedText ? objectReplacementCharacterTextRun : m_textRun;
            FloatPoint emphasisMarkTextOrigin = m_combinedText ? FloatPoint(boxOrigin.x() + m_boxRect.width() / 2, boxOrigin.y() + m_font.fontMetrics().ascent()) : m_textOrigin;
            if (m_combinedText)
                m_context.concatCTM(rotation(m_boxRect, Clockwise));

            paintTextWithShadows(m_context, m_combinedText ? m_combinedText->originalFont() : m_font, emphasisMarkTextRun, m_emphasisMark, m_emphasisMarkOffset, m_startPositionInTextRun, m_endPositionInTextRun, m_length, emphasisMarkTextOrigin, m_boxRect, m_selectionShadow, m_selectionPaintStyle.strokeWidth > 0, m_textBoxIsHorizontal);

            if (m_combinedText)
                m_context.concatCTM(rotation(m_boxRect, Counterclockwise));
        }
    }
}

#if ENABLE(CSS3_TEXT_DECORATION_SKIP_INK)
DashArray TextPainter::dashesForIntersectionsWithRect(const FloatRect& lineExtents)
{
    return m_font.dashesForIntersectionsWithRect(m_textRun, m_textOrigin, lineExtents);
}
#endif

} // namespace WebCore
