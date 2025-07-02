/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 */

#include "config.h"
#include "TextPainter.h"

#include "ControlFactory.h"
#include "DisplayListRecorderImpl.h"
#include "FilterOperations.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "InlineIteratorTextBox.h"
#include "LayoutIntegrationInlineContent.h"
#include "LegacyInlineTextBox.h"
#include "RenderCombineText.h"
#include "RenderLayer.h"
#include "RenderStyle.h"
#include "StyleTextShadow.h"
#include "TextBoxPainter.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

ShadowApplier::ShadowApplier(const RenderStyle& style, GraphicsContext& context, const Style::TextShadow* shadow, const FilterOperations* colorFilter, const FloatRect& textRect, bool isLastShadowIteration, bool lastShadowIterationShouldDrawText, bool opaque, bool ignoreWritingMode)
    : m_context { context }
    , m_shadow { shadow }
    , m_onlyDrawsShadow { !isLastShadowIteration || !lastShadowIterationShouldDrawText }
    , m_avoidDrawingShadow { shadowIsCompletelyCoveredByText(opaque) }
    , m_nothingToDraw { shadow && m_avoidDrawingShadow && m_onlyDrawsShadow }
    , m_didSaveContext { false }
{
    if (!shadow || m_nothingToDraw) {
        m_shadow = nullptr;
        return;
    }

    auto shadowOffset = TextBoxPainter::rotateShadowOffset(shadow->location, ignoreWritingMode ? WritingMode() : style.writingMode());
    auto shadowRadius = shadow->blur.value;
    auto shadowColor = style.colorResolvingCurrentColor(shadow->color);
    if (colorFilter)
        colorFilter->transformColor(shadowColor);

    // When drawing shadows, we usually clip the context to the area the shadow will reside, and then
    // draw the text itself outside the clipped area (so only the shadow shows up). However, we can
    // often draw the *last* shadow and the text itself in a single call.
    if (m_onlyDrawsShadow) {
        FloatRect shadowRect(textRect);
        shadowRect.inflate(Style::paintingExtent(*shadow) + 3 * textRect.height());
        shadowRect.move(shadowOffset);
        context.save();
        context.clip(shadowRect);

        m_didSaveContext = true;
        m_extraOffset = FloatSize(0, 2 * shadowRect.height() + std::max(0.0f, shadowOffset.height()) + shadowRadius);
        shadowOffset -= m_extraOffset;
    }

    if (!m_avoidDrawingShadow)
        context.setDropShadow({ shadowOffset, shadowRadius, shadowColor });
}

inline bool ShadowApplier::shadowIsCompletelyCoveredByText(bool textIsOpaque)
{
    return textIsOpaque
        && m_shadow
        && Style::isZero(m_shadow->location)
        && Style::isZero(m_shadow->blur);
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

TextPainter::TextPainter(GraphicsContext& context, const FontCascade& font, const RenderStyle& renderStyle, const TextPaintStyle& textPaintStyle, const Style::TextShadows& shadow, const FilterOperations* shadowColorFilter, const AtomString& emphasisMark, float emphasisMarkOffset, const RenderCombineText* combinedText)
    : m_context(context)
    , m_font(font)
    , m_renderStyle(renderStyle)
    , m_style(textPaintStyle)
    , m_emphasisMark(emphasisMark)
    , m_shadow(shadow)
    , m_shadowColorFilter(shadowColorFilter)
    , m_combinedText(combinedText)
    , m_emphasisMarkOffset(emphasisMarkOffset)
    , m_writingMode(renderStyle.writingMode())
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

    RefPtr glyphDisplayList = WTFMove(m_glyphDisplayList);
    if (!emphasisMark.isEmpty())
        m_context.drawEmphasisMarks(font, textRun, emphasisMark, textOrigin + FloatSize(0, emphasisMarkOffset), startOffset, endOffset);
    else if (startOffset || endOffset < textRun.length() || !glyphDisplayList)
        m_context.drawText(font, textRun, textOrigin, startOffset, endOffset);
    else {
        // Replaying back a whole cached glyph run to the GraphicsContext.
        m_context.translate(textOrigin);
        m_context.drawDisplayList(*glyphDisplayList);
        m_context.translate(-textOrigin);
    }
}

void TextPainter::paintTextWithShadows(const Style::TextShadows* shadows, const FilterOperations* colorFilter, const FontCascade& font, const TextRun& textRun, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset, const AtomString& emphasisMark, float emphasisMarkOffset, bool stroked)
{
    if (!shadows || shadows->isNone()) {
        paintTextOrEmphasisMarks(font, textRun, emphasisMark, emphasisMarkOffset, textOrigin, startOffset, endOffset);
        return;
    }

    Color fillColor = m_context.fillColor();
    bool opaque = fillColor.isOpaque();
    bool lastShadowIterationShouldDrawText = !stroked && opaque;
    if (!opaque)
        m_context.setFillColor(Color::black);
    for (const auto& shadow : *shadows) {
        ShadowApplier shadowApplier(m_renderStyle, m_context, &shadow, colorFilter, boxRect, &shadow == &shadows->last(), lastShadowIterationShouldDrawText, opaque, m_combinedText.get());
        if (!shadowApplier.nothingToDraw())
            paintTextOrEmphasisMarks(font, textRun, emphasisMark, emphasisMarkOffset, textOrigin + shadowApplier.extraOffset(), startOffset, endOffset);
    }

    if (!lastShadowIterationShouldDrawText) {
        if (!opaque)
            m_context.setFillColor(fillColor);
        paintTextOrEmphasisMarks(font, textRun, emphasisMark, emphasisMarkOffset, textOrigin, startOffset, endOffset);
    }
}

void TextPainter::paintTextAndEmphasisMarksIfNeeded(const TextRun& textRun, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned startOffset, unsigned endOffset,
    const TextPaintStyle& paintStyle, const Style::TextShadows& shadow, const FilterOperations* shadowColorFilter)
{
    if (paintStyle.paintOrder == PaintOrder::Normal) {
        // FIXME: Truncate right-to-left text correctly.
        paintTextWithShadows(&shadow, shadowColorFilter, m_font, textRun, boxRect, textOrigin, startOffset, endOffset, nullAtom(), 0, paintStyle.strokeWidth > 0);
    } else {
        auto textDrawingMode = m_context.textDrawingMode();
        auto paintOrder = RenderStyle::paintTypesForPaintOrder(paintStyle.paintOrder);
        auto shadowToUse = &shadow;

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
    static NeverDestroyed<TextRun> objectReplacementCharacterTextRun(StringView { span(objectReplacementCharacter) });
    CheckedRef emphasisMarkTextRun = m_combinedText ? objectReplacementCharacterTextRun.get() : textRun;
    FloatPoint emphasisMarkTextOrigin = m_combinedText ? FloatPoint(boxOrigin.x() + boxRect.width() / 2, boxOrigin.y() + m_font->metricsOfPrimaryFont().intAscent()) : textOrigin;

    if (m_combinedText)
        m_context.concatCTM(rotation(boxRect, RotationDirection::Clockwise));

    // FIXME: Truncate right-to-left text correctly.
    paintTextWithShadows(&shadow, shadowColorFilter, CheckedRef { m_combinedText ? m_combinedText->originalFont() : m_font.get() }, emphasisMarkTextRun, boxRect, emphasisMarkTextOrigin, startOffset, endOffset,
        m_emphasisMark, m_emphasisMarkOffset, paintStyle.strokeWidth > 0);

    if (m_combinedText)
        m_context.concatCTM(rotation(boxRect, RotationDirection::Counterclockwise));
}

void TextPainter::paintRange(const TextRun& textRun, const FloatRect& boxRect, const FloatPoint& textOrigin, unsigned start, unsigned end)
{
    ASSERT(start < end);
    paintTextAndEmphasisMarksIfNeeded(textRun, boxRect, textOrigin, start, end, m_style, m_shadow, m_shadowColorFilter);
}

bool TextPainter::shouldUseGlyphDisplayList(const PaintInfo& paintInfo, const RenderStyle& style)
{
    return !paintInfo.context().paintingDisabled() && paintInfo.enclosingSelfPaintingLayer() && FontCascade::canUseGlyphDisplayList(style);
}

void TextPainter::setForceUseGlyphDisplayListForTesting(bool enabled)
{
    GlyphDisplayListCache::singleton().setForceUseGlyphDisplayListForTesting(enabled);
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

    for (auto textBox : InlineIterator::textBoxesFor(*textNode.checkedRenderer())) {
        RefPtr<const DisplayList::DisplayList> displayList;
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
