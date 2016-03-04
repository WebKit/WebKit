/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#if ENABLE(SVG_FONTS)
#include "SVGTextRunRenderingContext.h"

#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "RenderObject.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGResourceSolidColor.h"
#include "SVGFontData.h"
#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGGlyphElement.h"
#include "SVGNames.h"
#include "WidthIterator.h"

namespace WebCore {

static inline const SVGFontData* svgFontAndFontFaceElementForFontData(const Font* font, SVGFontFaceElement*& svgFontFaceElement, SVGFontElement*& svgFontElement)
{
    ASSERT(font);
    ASSERT(font->isCustomFont());
    ASSERT(font->isSVGFont());

    auto* svgFontData = static_cast<const SVGFontData*>(font->svgData());

    svgFontFaceElement = svgFontData->svgFontFaceElement();
    ASSERT(svgFontFaceElement);

    svgFontElement = svgFontFaceElement->associatedFontElement();
    return svgFontData;
}

float SVGTextRunRenderingContext::floatWidthUsingSVGFont(const FontCascade& font, const TextRun& run, int& charsConsumed, String& glyphName) const
{
    WidthIterator it(&font, run);
    GlyphBuffer glyphBuffer;
    charsConsumed += it.advance(run.length(), &glyphBuffer);
    glyphName = it.lastGlyphName();
    return it.runWidthSoFar();
}

bool SVGTextRunRenderingContext::applySVGKerning(const Font* font, WidthIterator& iterator, GlyphBuffer* glyphBuffer, int from) const
{
    ASSERT(glyphBuffer);
    ASSERT(glyphBuffer->size() > 1);
    SVGFontElement* fontElement = 0;
    SVGFontFaceElement* fontFaceElement = 0;

    svgFontAndFontFaceElementForFontData(font, fontFaceElement, fontElement);
    if (!fontElement || !fontFaceElement)
        return false;

    if (fontElement->horizontalKerningMapIsEmpty())
        return true;

    float scale = scaleEmToUnits(font->platformData().size(), fontFaceElement->unitsPerEm());

    String lastGlyphName;
    String lastUnicodeString;
    int characterOffset = iterator.m_currentCharacter;
    String text = iterator.run().string();
    const int glyphCount = glyphBuffer->size() - from;
    GlyphBufferAdvance* advances = glyphBuffer->advances(from);

    for (int i = 0; i < glyphCount; ++i) {
        Glyph glyph = glyphBuffer->glyphAt(from + i);
        if (!glyph)
            continue;
        float kerning = 0;
        SVGGlyph svgGlyph = fontElement->svgGlyphForGlyph(glyph);
        String unicodeString = text.substring(characterOffset, svgGlyph.unicodeStringLength);
        if (i >= 1) {
            // FIXME: Support vertical text.
            kerning = fontElement->horizontalKerningForPairOfStringsAndGlyphs(lastUnicodeString, lastGlyphName, unicodeString, svgGlyph.glyphName);
            advances[i - 1].setWidth(advances[i - 1].width() - kerning * scale);
        }
        lastGlyphName = svgGlyph.glyphName;
        lastUnicodeString = unicodeString;
        characterOffset += svgGlyph.unicodeStringLength;
    }

    return true;
}

class SVGGlyphToPathTranslator final : public GlyphToPathTranslator {
public:
    SVGGlyphToPathTranslator(const TextRun* const, const GlyphBuffer&, const FloatPoint&, const SVGFontData&, SVGFontElement&, const int from, const int numGlyphs, float scale, bool isVerticalText);
private:
    bool containsMorePaths() override
    {
        return m_index != m_stoppingPoint;
    }

    Path path() override;
    std::pair<float, float> extents() override;
    GlyphUnderlineType underlineType() override;
    void advance() override;
    void moveToNextValidGlyph();
    AffineTransform transform();

    const TextRun* const m_textRun;
    const GlyphBuffer& m_glyphBuffer;
    const SVGFontData& m_svgFontData;
    FloatPoint m_currentPoint;
    FloatPoint m_glyphOrigin;
    SVGGlyph m_svgGlyph;
    int m_index;
    Glyph m_glyph;
    SVGFontElement& m_fontElement;
    const float m_stoppingPoint;
    const float m_scale;
    const bool m_isVerticalText;
};

SVGGlyphToPathTranslator::SVGGlyphToPathTranslator(const TextRun* const textRun, const GlyphBuffer& glyphBuffer, const FloatPoint& point, const SVGFontData& svgFontData, SVGFontElement& fontElement, const int from, const int numGlyphs, float scale, bool isVerticalText)
    : m_textRun(textRun)
    , m_glyphBuffer(glyphBuffer)
    , m_svgFontData(svgFontData)
    , m_currentPoint(point)
    , m_glyphOrigin(m_svgFontData.horizontalOriginX() * scale, m_svgFontData.horizontalOriginY() * scale)
    , m_index(from)
    , m_glyph(glyphBuffer.glyphAt(m_index))
    , m_fontElement(fontElement)
    , m_stoppingPoint(numGlyphs + from)
    , m_scale(scale)
    , m_isVerticalText(isVerticalText)
{
    ASSERT(glyphBuffer.size() > m_index);
    if (m_glyph) {
        m_svgGlyph = m_fontElement.svgGlyphForGlyph(m_glyph);
        ASSERT(!m_svgGlyph.isPartOfLigature);
        ASSERT(m_svgGlyph.tableEntry == m_glyph);
        SVGGlyphElement::inheritUnspecifiedAttributes(m_svgGlyph, &m_svgFontData);
    }
    moveToNextValidGlyph();
}

AffineTransform SVGGlyphToPathTranslator::transform()
{
    AffineTransform glyphPathTransform;
    glyphPathTransform.translate(m_currentPoint.x() + m_glyphOrigin.x(), m_currentPoint.y() + m_glyphOrigin.y());
    glyphPathTransform.scale(m_scale, -m_scale);
    return glyphPathTransform;
}

Path SVGGlyphToPathTranslator::path()
{
    Path glyphPath = m_svgGlyph.pathData;
    glyphPath.transform(transform());
    return glyphPath;
}

std::pair<float, float> SVGGlyphToPathTranslator::extents()
{
    AffineTransform glyphPathTransform = transform();
    FloatPoint beginning = glyphPathTransform.mapPoint(m_currentPoint);
    float width = narrowPrecisionToFloat(m_glyphBuffer.advanceAt(m_index).width() * glyphPathTransform.xScale());
    return std::make_pair(beginning.x(), beginning.x() + width);
}

auto SVGGlyphToPathTranslator::underlineType() -> GlyphUnderlineType
{
    ASSERT(m_textRun);
    return computeUnderlineType(*m_textRun, m_glyphBuffer, m_index);
}

void SVGGlyphToPathTranslator::moveToNextValidGlyph()
{
    if (m_glyph && !m_svgGlyph.pathData.isEmpty())
        return;
    advance();
}

void SVGGlyphToPathTranslator::advance()
{
    do {
        if (m_glyph) {
            float advance = m_glyphBuffer.advanceAt(m_index).width();
            if (m_isVerticalText)
                m_currentPoint.move(0, advance);
            else
                m_currentPoint.move(advance, 0);
        }

        ++m_index;
        if (m_index >= m_stoppingPoint || !m_glyphBuffer.fontAt(m_index)->isSVGFont())
            break;
        m_glyph = m_glyphBuffer.glyphAt(m_index);
        if (!m_glyph)
            continue;
        m_svgGlyph = m_fontElement.svgGlyphForGlyph(m_glyph);
        ASSERT(!m_svgGlyph.isPartOfLigature);
        ASSERT(m_svgGlyph.tableEntry == m_glyph);
        SVGGlyphElement::inheritUnspecifiedAttributes(m_svgGlyph, &m_svgFontData);
    } while ((!m_glyph || m_svgGlyph.pathData.isEmpty()) && m_index < m_stoppingPoint);

    if (containsMorePaths() && m_isVerticalText) {
        m_glyphOrigin.setX(m_svgGlyph.verticalOriginX * m_scale);
        m_glyphOrigin.setY(m_svgGlyph.verticalOriginY * m_scale);
    }
}

class DummyGlyphToPathTranslator final : public GlyphToPathTranslator {
    bool containsMorePaths() override
    {
        return false;
    }
    Path path() override
    {
        return Path();
    }
    std::pair<float, float> extents() override
    {
        return std::make_pair(0.f, 0.f);
    }
    GlyphUnderlineType underlineType() override
    {
        return GlyphUnderlineType::DrawOverGlyph;
    }
    void advance() override
    {
    }
};

std::unique_ptr<GlyphToPathTranslator> SVGTextRunRenderingContext::createGlyphToPathTranslator(const Font& font, const TextRun* textRun, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
{
    SVGFontElement* fontElement = nullptr;
    SVGFontFaceElement* fontFaceElement = nullptr;

    const SVGFontData* svgFontData = svgFontAndFontFaceElementForFontData(&font, fontFaceElement, fontElement);
    if (!fontElement || !fontFaceElement)
        return std::make_unique<DummyGlyphToPathTranslator>();

    auto& elementRenderer = is<RenderElement>(renderer()) ? downcast<RenderElement>(renderer()) : *renderer().parent();
    RenderStyle& style = elementRenderer.style();
    bool isVerticalText = style.svgStyle().isVerticalWritingMode();

    float scale = scaleEmToUnits(font.platformData().size(), fontFaceElement->unitsPerEm());

    return std::make_unique<SVGGlyphToPathTranslator>(textRun, glyphBuffer, point, *svgFontData, *fontElement, from, numGlyphs, scale, isVerticalText);
}

void SVGTextRunRenderingContext::drawSVGGlyphs(GraphicsContext& context, const Font& font, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
{
    auto activePaintingResource = this->activePaintingResource();
    if (!activePaintingResource) {
        // TODO: We're only supporting simple filled HTML text so far.
        RenderSVGResourceSolidColor* solidPaintingResource = RenderSVGResource::sharedSolidPaintingResource();
        solidPaintingResource->setColor(context.fillColor());
        activePaintingResource = solidPaintingResource;
    }

    auto& elementRenderer = is<RenderElement>(renderer()) ? downcast<RenderElement>(renderer()) : *renderer().parent();
    RenderStyle& style = elementRenderer.style();

    ASSERT(activePaintingResource);

    GraphicsContext* usedContext = &context;
    RenderSVGResourceMode resourceMode = context.textDrawingMode() == TextModeStroke ? ApplyToStrokeMode : ApplyToFillMode;
    for (auto translator = createGlyphToPathTranslator(font, nullptr, glyphBuffer, from, numGlyphs, point); translator->containsMorePaths(); translator->advance()) {
        Path glyphPath = translator->path();
        if (activePaintingResource->applyResource(elementRenderer, style, usedContext, resourceMode)) {
            float strokeThickness = context.strokeThickness();
            if (is<RenderSVGInlineText>(renderer()))
                usedContext->setStrokeThickness(strokeThickness * downcast<RenderSVGInlineText>(renderer()).scalingFactor());
            activePaintingResource->postApplyResource(elementRenderer, usedContext, resourceMode, &glyphPath, nullptr);
            usedContext->setStrokeThickness(strokeThickness);
        }
    }
}

static GlyphData missingGlyphForFont(const FontCascade& font)
{
    const Font& primaryFont = font.primaryFont();
    if (!primaryFont.isSVGFont())
        return GlyphData();
    SVGFontElement* fontElement;
    SVGFontFaceElement* fontFaceElement;
    svgFontAndFontFaceElementForFontData(&primaryFont, fontFaceElement, fontElement);
    return GlyphData(fontElement->missingGlyph(), &primaryFont);
}

GlyphData SVGTextRunRenderingContext::glyphDataForCharacter(const FontCascade& font, WidthIterator& iterator, UChar32 character, bool mirror, int currentCharacter, unsigned& advanceLength, String& normalizedSpacesStringCache)
{
    GlyphData glyphData = font.glyphDataForCharacter(character, mirror, AutoVariant);
    if (!glyphData.glyph)
        return missingGlyphForFont(font);

    ASSERT(glyphData.font);

    // Characters enclosed by an <altGlyph> element, may not be registered in the GlyphPage.
    if (!glyphData.font->isSVGFont()) {
        auto& elementRenderer = is<RenderElement>(renderer()) ? downcast<RenderElement>(renderer()) : *renderer().parent();
        if (Element* parentRendererElement = elementRenderer.element()) {
            if (is<SVGAltGlyphElement>(*parentRendererElement))
                glyphData.font = &font.primaryFont();
        }
    }

    if (!glyphData.font->isSVGFont())
        return glyphData;

    SVGFontElement* fontElement = nullptr;
    SVGFontFaceElement* fontFaceElement = nullptr;
    const SVGFontData* svgFontData = svgFontAndFontFaceElementForFontData(glyphData.font, fontFaceElement, fontElement);
    if (!svgFontData)
        return glyphData;

    // If we got here, we're dealing with a glyph defined in a SVG Font.
    // The returned glyph by glyphDataForCharacter() is a glyph stored in the SVG Font glyph table.
    // This doesn't necessarily mean the glyph is suitable for rendering/measuring in this context, its
    // arabic-form/orientation/... may not match, we have to apply SVG Glyph selection to discover that.
    if (svgFontData->applySVGGlyphSelection(iterator, glyphData, mirror, currentCharacter, advanceLength, normalizedSpacesStringCache))
        return glyphData;

    GlyphData missingGlyphData = missingGlyphForFont(font);
    if (missingGlyphData.glyph)
        return missingGlyphData;

    // SVG font context sensitive selection failed and there is no defined missing glyph. Drop down to a default font.
    // The behavior does not seem to be specified. For simplicity we don't try to resolve font fallbacks context-sensitively.
    auto fallbackDescription = font.fontDescription();
    fallbackDescription.setFamilies(Vector<AtomicString> { sansSerifFamily });
    FontCascade fallbackFont(fallbackDescription, font.letterSpacing(), font.wordSpacing());
    fallbackFont.update(font.fontSelector());

    return fallbackFont.glyphDataForCharacter(character, mirror, AutoVariant);
}

}

#endif
