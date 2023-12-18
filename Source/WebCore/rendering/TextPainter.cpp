/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

#include "DisplayListRecorderImpl.h"
#include "DisplayListReplayer.h"
#include "FilterOperations.h"
#include "GraphicsContext.h"
#include "InlineIteratorTextBox.h"
#include "LayoutIntegrationInlineContent.h"
#include "LegacyInlineTextBox.h"
#include "RenderCombineText.h"
#include "RenderLayer.h"
#include "ShadowData.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

ShadowApplier::ShadowApplier(const RenderStyle& style, GraphicsContext& context, const ShadowData* shadow, const FilterOperations* colorFilter, const FloatRect& textRect, bool lastShadowIterationShouldDrawText, bool opaque, FontOrientation orientation)
    : m_context { context }
    , m_shadow { shadow }
    , m_onlyDrawsShadow { !isLastShadowIteration() || !lastShadowIterationShouldDrawText }
    , m_avoidDrawingShadow { shadowIsCompletelyCoveredByText(opaque) }
    , m_nothingToDraw { shadow && m_avoidDrawingShadow && m_onlyDrawsShadow }
    , m_didSaveContext { false }
{
    if (!shadow || m_nothingToDraw) {
        m_shadow = nullptr;
        return;
    }

    float shadowX = orientation == FontOrientation::Horizontal ? shadow->x().value() : shadow->y().value();
    float shadowY = orientation == FontOrientation::Horizontal ? shadow->y().value() : -shadow->x().value();
    FloatSize shadowOffset(shadowX, shadowY);
    auto shadowRadius = shadow->radius();
    auto shadowColor = style.colorResolvingCurrentColor(shadow->color());
    if (colorFilter)
        colorFilter->transformColor(shadowColor);

    // When drawing shadows, we usually clip the context to the area the shadow will reside, and then
    // draw the text itself outside the clipped area (so only the shadow shows up). However, we can
    // often draw the *last* shadow and the text itself in a single call.
    if (m_onlyDrawsShadow) {
        FloatRect shadowRect(textRect);
        shadowRect.inflate(shadow->paintingExtent() + 3 * textRect.height());
        shadowRect.move(shadowOffset);
        context.save();
        context.clip(shadowRect);

        m_didSaveContext = true;
        m_extraOffset = FloatSize(0, 2 * shadowRect.height() + std::max(0.0f, shadowOffset.height()) + shadowRadius.value());
        shadowOffset -= m_extraOffset;
    }

    if (!m_avoidDrawingShadow)
        context.setDropShadow({ shadowOffset, shadowRadius.value(), shadowColor });
}

inline bool ShadowApplier::isLastShadowIteration()
{
    return m_shadow && !m_shadow->next();
}

inline bool ShadowApplier::shadowIsCompletelyCoveredByText(bool textIsOpaque)
{
    return textIsOpaque && m_shadow && m_shadow->location().isZero() && m_shadow->radius().isZero();
}

ShadowApplier::~ShadowApplier()
{
    if (!m_shadow)
        return;
    if (m_onlyDrawsShadow)
        m_context.restore();
    else if (!m_avoidDrawingShadow)
        m_context.clearDropShadow();
}

TextPainter::TextPainter(GraphicsContext& context, const FontCascade& font, const RenderStyle& renderStyle)
    : m_context(context)
    , m_font(font)
    , m_renderStyle(renderStyle)
{
}

void TextPainter::paintTextOrEmphasisMarks(const FontCascade& font, const TextRun& textRun, const AtomString& emphasisMark,
    float emphasisMarkOffset, const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset)
{
    ASSERT(startOffset < endOffset);

    if (m_context.detectingContentfulPaint()) {
        if (!textRun.text().containsOnly<isASCIIWhitespace>())
            m_context.setContentfulPaintDetected();
        return;
    }

    if (!emphasisMark.isEmpty())
        m_context.drawEmphasisMarks(font, textRun, emphasisMark, textOrigin + FloatSize(0, emphasisMarkOffset), startOffset, endOffset);
    else if (startOffset || endOffset < textRun.length() || !m_glyphDisplayList)
        m_context.drawText(font, textRun, textOrigin, startOffset, endOffset);
    else {
        // Replaying back a whole cached glyph run to the GraphicsContext.
        m_context.drawDisplayListItems(m_glyphDisplayList->items(), m_glyphDisplayList->resourceHeap(), textOrigin);
    }
    m_glyphDisplayList = nullptr;
}

void TextPainter::paintTextWithShadows(const ShadowData* shadow, const FilterOperations* colorFilter, const FontCascade& font, const TextRun& textRun, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset, const AtomString& emphasisMark, float emphasisMarkOffset, bool stroked)
{
    if (!shadow) {
        paintTextOrEmphasisMarks(font, textRun, emphasisMark, emphasisMarkOffset, textOrigin, startOffset, endOffset);
        return;
    }

    Color fillColor = m_context.fillColor();
    bool opaque = fillColor.isOpaque();
    bool lastShadowIterationShouldDrawText = !stroked && opaque;
    if (!opaque)
        m_context.setFillColor(Color::black);
    while (shadow) {
        ShadowApplier shadowApplier(m_renderStyle, m_context, shadow, colorFilter, boxRect, lastShadowIterationShouldDrawText, opaque, (m_textBoxIsHorizontal || m_combinedText) ? FontOrientation::Horizontal : FontOrientation::Vertical);
        if (!shadowApplier.nothingToDraw())
            paintTextOrEmphasisMarks(font, textRun, emphasisMark, emphasisMarkOffset, textOrigin + shadowApplier.extraOffset(), startOffset, endOffset);
        shadow = shadow->next();
    }

    if (!lastShadowIterationShouldDrawText) {
        if (!opaque)
            m_context.setFillColor(fillColor);
        paintTextOrEmphasisMarks(font, textRun, emphasisMark, emphasisMarkOffset, textOrigin, startOffset, endOffset);
    }
}

void TextPainter::paintTextAndEmphasisMarksIfNeeded(const TextRun& textRun, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset,
    const TextPaintStyle& paintStyle, const ShadowData* shadow, const FilterOperations* shadowColorFilter)
{
    if (paintStyle.paintOrder == PaintOrder::Normal) {
        // FIXME: Truncate right-to-left text correctly.
        paintTextWithShadows(shadow, shadowColorFilter, m_font, textRun, boxRect, textOrigin, startOffset, endOffset, nullAtom(), 0, paintStyle.strokeWidth > 0);
    } else {
        auto textDrawingMode = m_context.textDrawingMode();
        auto paintOrder = RenderStyle::paintTypesForPaintOrder(paintStyle.paintOrder);
        auto shadowToUse = shadow;

        for (auto order : paintOrder) {
            switch (order) {
            case PaintType::Fill: {
                auto textDrawingModeWithoutStroke = textDrawingMode;
                textDrawingModeWithoutStroke.remove(TextDrawingMode::Stroke);
                m_context.setTextDrawingMode(textDrawingModeWithoutStroke);
                paintTextWithShadows(shadowToUse, shadowColorFilter, m_font, textRun, boxRect, textOrigin, startOffset, endOffset, nullAtom(), 0, false);
                shadowToUse = nullptr;
                m_context.setTextDrawingMode(textDrawingMode);
                break;
            }
            case PaintType::Stroke: {
                auto textDrawingModeWithoutFill = textDrawingMode;
                textDrawingModeWithoutFill.remove(TextDrawingMode::Fill);
                m_context.setTextDrawingMode(textDrawingModeWithoutFill);
                paintTextWithShadows(shadowToUse, shadowColorFilter, m_font, textRun, boxRect, textOrigin, startOffset, endOffset, nullAtom(), 0, paintStyle.strokeWidth > 0);
                shadowToUse = nullptr;
                m_context.setTextDrawingMode(textDrawingMode);
            }
                break;
            case PaintType::Markers:
                continue;
            }
        }
    }
    
    if (m_emphasisMark.isEmpty())
        return;

    FloatPoint boxOrigin = boxRect.location();
    updateGraphicsContext(m_context, paintStyle, UseEmphasisMarkColor);
    static NeverDestroyed<TextRun> objectReplacementCharacterTextRun(StringView(&objectReplacementCharacter, 1));
    const TextRun& emphasisMarkTextRun = m_combinedText ? objectReplacementCharacterTextRun.get() : textRun;
    FloatPoint emphasisMarkTextOrigin = m_combinedText ? FloatPoint(boxOrigin.x() + boxRect.width() / 2, boxOrigin.y() + m_font.metricsOfPrimaryFont().ascent()) : textOrigin;
    if (m_combinedText)
        m_context.concatCTM(rotation(boxRect, RotationDirection::Clockwise));

    // FIXME: Truncate right-to-left text correctly.
    paintTextWithShadows(shadow, shadowColorFilter, m_combinedText ? m_combinedText->originalFont() : m_font, emphasisMarkTextRun, boxRect, emphasisMarkTextOrigin, startOffset, endOffset,
        m_emphasisMark, m_emphasisMarkOffset, paintStyle.strokeWidth > 0);

    if (m_combinedText)
        m_context.concatCTM(rotation(boxRect, RotationDirection::Counterclockwise));
}

void TextPainter::paintRange(const TextRun& textRun, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned start, unsigned end)
{
    ASSERT(start < end);
    paintTextAndEmphasisMarksIfNeeded(textRun, boxRect, textOrigin, start, end, m_style, m_shadow, m_shadowColorFilter);
}

static bool forceUseGlyphDisplayListForTesting = false;

bool TextPainter::shouldUseGlyphDisplayList(const PaintInfo& paintInfo)
{
#if USE(GLYPH_DISPLAY_LIST_CACHE)
    return !paintInfo.context().paintingDisabled() && paintInfo.enclosingSelfPaintingLayer() && (paintInfo.enclosingSelfPaintingLayer()->paintingFrequently() || forceUseGlyphDisplayListForTesting);
#else
    return !paintInfo.context().paintingDisabled() && paintInfo.enclosingSelfPaintingLayer() && forceUseGlyphDisplayListForTesting;
#endif
}

void TextPainter::setForceUseGlyphDisplayListForTesting(bool enabled)
{
    forceUseGlyphDisplayListForTesting = enabled;
}

void TextPainter::clearGlyphDisplayListCacheForTesting()
{
    GlyphDisplayListCache::singleton().clear();
}

String TextPainter::cachedGlyphDisplayListsForTextNodeAsText(Text& textNode, OptionSet<DisplayList::AsTextFlag> flags)
{
    if (!textNode.renderer())
        return String();

    StringBuilder builder;

    for (auto textBox : InlineIterator::textBoxesFor(*textNode.renderer())) {
        DisplayList::DisplayList* displayList = nullptr;
        if (auto* legacyInlineBox = textBox.legacyInlineBox())
            displayList = TextPainter::glyphDisplayListIfExists(*legacyInlineBox);
        else
            displayList = TextPainter::glyphDisplayListIfExists(*textBox.inlineBox());
        if (displayList) {
            builder.append(displayList->asText(flags));
            builder.append('\n');
        }
    }

    return builder.toString();
}

} // namespace WebCore
