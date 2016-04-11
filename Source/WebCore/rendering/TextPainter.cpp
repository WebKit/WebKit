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
#include <wtf/NeverDestroyed.h>

namespace WebCore {

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
        context.setShadow(shadowOffset, shadowRadius, shadowColor);
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

TextPainter::TextPainter(GraphicsContext& context)
    : m_context(context)
{
}

void TextPainter::drawTextOrEmphasisMarks(const FontCascade& font, const TextRun& textRun, const AtomicString& emphasisMark,
    int emphasisMarkOffset, const FloatPoint& textOrigin, int startOffset, int endOffset)
{
    ASSERT(startOffset < endOffset);
    if (emphasisMark.isEmpty())
        m_context.drawText(font, textRun, textOrigin, startOffset, endOffset);
    else
        m_context.drawEmphasisMarks(font, textRun, emphasisMark, textOrigin + IntSize(0, emphasisMarkOffset), startOffset, endOffset);
}

void TextPainter::paintTextWithShadows(const ShadowData* shadow, const FontCascade& font, const TextRun& textRun, const FloatRect& boxRect, const FloatPoint& textOrigin,
    int startOffset, int endOffset, const AtomicString& emphasisMark, int emphasisMarkOffset, bool stroked)
{
    if (!shadow) {
        drawTextOrEmphasisMarks(font, textRun, emphasisMark, emphasisMarkOffset, textOrigin, startOffset, endOffset);
        return;
    }

    Color fillColor = m_context.fillColor();
    bool opaque = !fillColor.hasAlpha();
    bool lastShadowIterationShouldDrawText = !stroked && opaque;
    if (!opaque)
        m_context.setFillColor(Color::black);
    while (shadow) {
        ShadowApplier shadowApplier(m_context, shadow, boxRect, lastShadowIterationShouldDrawText, opaque, m_textBoxIsHorizontal ? Horizontal : Vertical);
        if (!shadowApplier.nothingToDraw())
            drawTextOrEmphasisMarks(font, textRun, emphasisMark, emphasisMarkOffset, textOrigin + shadowApplier.extraOffset(), startOffset, endOffset);
        shadow = shadow->next();
    }

    if (!lastShadowIterationShouldDrawText) {
        if (!opaque)
            m_context.setFillColor(fillColor);
        drawTextOrEmphasisMarks(font, textRun, emphasisMark, emphasisMarkOffset, textOrigin, startOffset, endOffset);
    }
}

void TextPainter::paintTextAndEmphasisMarksIfNeeded(const TextRun& textRun, const FloatRect& boxRect, const FloatPoint& textOrigin, int startOffset, int endOffset,
    const TextPaintStyle& paintStyle, const ShadowData* shadow)
{
    // FIXME: Truncate right-to-left text correctly.
    paintTextWithShadows(shadow, *m_font, textRun, boxRect, textOrigin, startOffset, endOffset, nullAtom, 0, paintStyle.strokeWidth > 0);

    if (m_emphasisMark.isEmpty())
        return;

    FloatPoint boxOrigin = boxRect.location();
    updateGraphicsContext(m_context, paintStyle, UseEmphasisMarkColor);
    static NeverDestroyed<TextRun> objectReplacementCharacterTextRun(StringView(&objectReplacementCharacter, 1));
    const TextRun& emphasisMarkTextRun = m_combinedText ? objectReplacementCharacterTextRun.get() : textRun;
    FloatPoint emphasisMarkTextOrigin = m_combinedText ? FloatPoint(boxOrigin.x() + boxRect.width() / 2, boxOrigin.y() + m_font->fontMetrics().ascent()) : textOrigin;
    if (m_combinedText)
        m_context.concatCTM(rotation(boxRect, Clockwise));

    // FIXME: Truncate right-to-left text correctly.
    paintTextWithShadows(shadow, m_combinedText ? m_combinedText->originalFont() : *m_font, emphasisMarkTextRun, boxRect, emphasisMarkTextOrigin, startOffset, endOffset,
        m_emphasisMark, m_emphasisMarkOffset, paintStyle.strokeWidth > 0);

    if (m_combinedText)
        m_context.concatCTM(rotation(boxRect, Counterclockwise));
}
    
void TextPainter::paintText(const TextRun& textRun, int length, const FloatRect& boxRect, const FloatPoint& textOrigin, int selectionStart, int selectionEnd,
    bool paintSelectedTextOnly, bool paintSelectedTextSeparately)
{
    ASSERT(m_font);
    if (!paintSelectedTextOnly) {
        // For stroked painting, we have to change the text drawing mode. It's probably dangerous to leave that mutated as a side
        // effect, so only when we know we're stroking, do a save/restore.
        GraphicsContextStateSaver stateSaver(m_context, m_textPaintStyle.strokeWidth > 0);
        updateGraphicsContext(m_context, m_textPaintStyle);
        bool fullPaint = !paintSelectedTextSeparately || selectionEnd <= selectionStart;
        if (fullPaint)
            paintTextAndEmphasisMarksIfNeeded(textRun, boxRect, textOrigin, 0, length, m_textPaintStyle, m_textShadow);
        else {
            // Paint the before and after selection parts.
            if (selectionStart > 0)
                paintTextAndEmphasisMarksIfNeeded(textRun, boxRect, textOrigin, 0, selectionStart, m_textPaintStyle, m_textShadow);
            if (selectionEnd < length)
                paintTextAndEmphasisMarksIfNeeded(textRun, boxRect, textOrigin, selectionEnd, length, m_textPaintStyle, m_textShadow);
        }
    }
    // Paint only the text that is selected.
    if ((paintSelectedTextOnly || paintSelectedTextSeparately) && selectionStart < selectionEnd) {
        GraphicsContextStateSaver stateSaver(m_context, m_selectionPaintStyle.strokeWidth > 0);
        updateGraphicsContext(m_context, m_selectionPaintStyle);
        paintTextAndEmphasisMarksIfNeeded(textRun, boxRect, textOrigin, selectionStart, selectionEnd, m_selectionPaintStyle, m_selectionShadow);
    }
}

} // namespace WebCore
