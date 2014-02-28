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

static inline const SVGFontData* svgFontAndFontFaceElementForFontData(const SimpleFontData* fontData, SVGFontFaceElement*& fontFace, SVGFontElement*& font)
{
    ASSERT(fontData);
    ASSERT(fontData->isCustomFont());
    ASSERT(fontData->isSVGFont());

    const SVGFontData* svgFontData = static_cast<const SVGFontData*>(fontData->fontData());

    fontFace = svgFontData->svgFontFaceElement();
    ASSERT(fontFace);

    font = fontFace->associatedFontElement();
    return svgFontData;
}

float SVGTextRunRenderingContext::floatWidthUsingSVGFont(const Font& font, const TextRun& run, int& charsConsumed, String& glyphName) const
{
    WidthIterator it(&font, run);
    GlyphBuffer glyphBuffer;
    charsConsumed += it.advance(run.length(), &glyphBuffer);
    glyphName = it.lastGlyphName();
    return it.runWidthSoFar();
}

bool SVGTextRunRenderingContext::applySVGKerning(const SimpleFontData* fontData, WidthIterator& iterator, GlyphBuffer* glyphBuffer, int from) const
{
    ASSERT(glyphBuffer);
    ASSERT(glyphBuffer->size() > 1);
    SVGFontElement* fontElement = 0;
    SVGFontFaceElement* fontFaceElement = 0;

    svgFontAndFontFaceElementForFontData(fontData, fontFaceElement, fontElement);
    if (!fontElement || !fontFaceElement)
        return false;

    float scale = scaleEmToUnits(fontData->platformData().size(), fontFaceElement->unitsPerEm());

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
    SVGGlyphToPathTranslator(const GlyphBuffer& glyphBuffer, const FloatPoint& point, const SVGFontData& svgFontData, SVGFontElement& fontElement, const int from, const int numGlyphs, float scale, bool isVerticalText);
private:
    virtual bool containsMorePaths() override
    {
        return m_index != m_stoppingPoint;
    }
    virtual Path nextPath() override;
    void moveToNextValidGlyph();
    void incrementIndex();

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

SVGGlyphToPathTranslator::SVGGlyphToPathTranslator(const GlyphBuffer& glyphBuffer, const FloatPoint& point, const SVGFontData& svgFontData, SVGFontElement& fontElement, const int from, const int numGlyphs, float scale, bool isVerticalText)
    : m_glyphBuffer(glyphBuffer)
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

Path SVGGlyphToPathTranslator::nextPath()
{
    if (m_isVerticalText) {
        m_glyphOrigin.setX(m_svgGlyph.verticalOriginX * m_scale);
        m_glyphOrigin.setY(m_svgGlyph.verticalOriginY * m_scale);
    }

    AffineTransform glyphPathTransform;
    glyphPathTransform.translate(m_currentPoint.x() + m_glyphOrigin.x(), m_currentPoint.y() + m_glyphOrigin.y());
    glyphPathTransform.scale(m_scale, -m_scale);

    Path glyphPath = m_svgGlyph.pathData;
    glyphPath.transform(glyphPathTransform);
    incrementIndex();
    return glyphPath;
}

void SVGGlyphToPathTranslator::moveToNextValidGlyph()
{
    if (m_glyph && !m_svgGlyph.pathData.isEmpty())
        return;
    incrementIndex();
}

void SVGGlyphToPathTranslator::incrementIndex()
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
        if (m_index >= m_stoppingPoint)
            break;
        m_glyph = m_glyphBuffer.glyphAt(m_index);
        if (!m_glyph)
            continue;
        m_svgGlyph = m_fontElement.svgGlyphForGlyph(m_glyph);
        ASSERT(!m_svgGlyph.isPartOfLigature);
        ASSERT(m_svgGlyph.tableEntry == m_glyph);
        SVGGlyphElement::inheritUnspecifiedAttributes(m_svgGlyph, &m_svgFontData);
    } while ((!m_glyph || m_svgGlyph.pathData.isEmpty()) && m_index < m_stoppingPoint);
}

class DummyGlyphToPathTranslator final : public GlyphToPathTranslator {
    virtual bool containsMorePaths() override
    {
        return false;
    }
    virtual Path nextPath() override
    {
        return Path();
    }
};

std::unique_ptr<GlyphToPathTranslator> SVGTextRunRenderingContext::createGlyphToPathTranslator(const SimpleFontData& fontData, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
{
    SVGFontElement* fontElement = 0;
    SVGFontFaceElement* fontFaceElement = 0;

    const SVGFontData* svgFontData = svgFontAndFontFaceElementForFontData(&fontData, fontFaceElement, fontElement);
    if (!fontElement || !fontFaceElement)
        return std::make_unique<DummyGlyphToPathTranslator>();

    auto& elementRenderer = renderer().isRenderElement() ? toRenderElement(renderer()) : *renderer().parent();
    RenderStyle& style = elementRenderer.style();
    bool isVerticalText = style.svgStyle().isVerticalWritingMode();

    float scale = scaleEmToUnits(fontData.platformData().size(), fontFaceElement->unitsPerEm());

    return std::make_unique<SVGGlyphToPathTranslator>(glyphBuffer, point, *svgFontData, *fontElement, from, numGlyphs, scale, isVerticalText);
}

void SVGTextRunRenderingContext::drawSVGGlyphs(GraphicsContext* context, const SimpleFontData* fontData, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
{
    auto activePaintingResource = this->activePaintingResource();
    if (!activePaintingResource) {
        // TODO: We're only supporting simple filled HTML text so far.
        RenderSVGResourceSolidColor* solidPaintingResource = RenderSVGResource::sharedSolidPaintingResource();
        solidPaintingResource->setColor(context->fillColor());
        activePaintingResource = solidPaintingResource;
    }

    auto& elementRenderer = renderer().isRenderElement() ? toRenderElement(renderer()) : *renderer().parent();
    RenderStyle& style = elementRenderer.style();

    ASSERT(activePaintingResource);

    RenderSVGResourceMode resourceMode = context->textDrawingMode() == TextModeStroke ? ApplyToStrokeMode : ApplyToFillMode;
    auto translator(createGlyphToPathTranslator(*fontData, glyphBuffer, from, numGlyphs, point));
    while (translator->containsMorePaths()) {
        Path glyphPath = translator->nextPath();
        if (activePaintingResource->applyResource(elementRenderer, style, context, resourceMode)) {
            float strokeThickness = context->strokeThickness();
            if (renderer().isSVGInlineText())
                context->setStrokeThickness(strokeThickness * toRenderSVGInlineText(renderer()).scalingFactor());
            activePaintingResource->postApplyResource(elementRenderer, context, resourceMode, &glyphPath, 0);
            context->setStrokeThickness(strokeThickness);
        }
    }
}

GlyphData SVGTextRunRenderingContext::glyphDataForCharacter(const Font& font, WidthIterator& iterator, UChar32 character, bool mirror, int currentCharacter, unsigned& advanceLength)
{
    const SimpleFontData* primaryFont = font.primaryFont();
    ASSERT(primaryFont);

    std::pair<GlyphData, GlyphPage*> pair = font.glyphDataAndPageForCharacter(character, mirror, AutoVariant);
    GlyphData glyphData = pair.first;

    // Check if we have the missing glyph data, in which case we can just return.
    GlyphData missingGlyphData = primaryFont->missingGlyphData();
    if (glyphData.glyph == missingGlyphData.glyph && glyphData.fontData == missingGlyphData.fontData) {
        ASSERT(glyphData.fontData);
        return glyphData;
    }

    // Save data fromt he font fallback list because we may modify it later. Do this before the
    // potential change to glyphData.fontData below.
    FontGlyphs* glyph = font.glyphs();
    ASSERT(glyph);
    FontGlyphs::GlyphPagesStateSaver glyphPagesSaver(*glyph);

    // Characters enclosed by an <altGlyph> element, may not be registered in the GlyphPage.
    const SimpleFontData* originalFontData = glyphData.fontData;
    if (glyphData.fontData && !glyphData.fontData->isSVGFont()) {
        auto& elementRenderer = renderer().isRenderElement() ? toRenderElement(renderer()) : *renderer().parent();
        if (Element* parentRendererElement = elementRenderer.element()) {
            if (parentRendererElement->hasTagName(SVGNames::altGlyphTag))
                glyphData.fontData = primaryFont;
        }
    }

    const SimpleFontData* fontData = glyphData.fontData;
    if (fontData) {
        if (!fontData->isSVGFont())
            return glyphData;

        SVGFontElement* fontElement = 0;
        SVGFontFaceElement* fontFaceElement = 0;

        const SVGFontData* svgFontData = svgFontAndFontFaceElementForFontData(fontData, fontFaceElement, fontElement);
        if (!fontElement || !fontFaceElement)
            return glyphData;

        // If we got here, we're dealing with a glyph defined in a SVG Font.
        // The returned glyph by glyphDataAndPageForCharacter() is a glyph stored in the SVG Font glyph table.
        // This doesn't necessarily mean the glyph is suitable for rendering/measuring in this context, its
        // arabic-form/orientation/... may not match, we have to apply SVG Glyph selection to discover that.
        if (svgFontData->applySVGGlyphSelection(iterator, glyphData, mirror, currentCharacter, advanceLength))
            return glyphData;
    }

    GlyphPage* page = pair.second;
    ASSERT(page);

    // No suitable glyph found that is compatible with the requirments (same language, arabic-form, orientation etc.)
    // Even though our GlyphPage contains an entry for eg. glyph "a", it's not compatible. So we have to temporarily
    // remove the glyph data information from the GlyphPage, and retry the lookup, which handles font fallbacks correctly.
    page->setGlyphDataForCharacter(character, 0, 0);

    // Assure that the font fallback glyph selection worked, aka. the fallbackGlyphData font data is not the same as before.
    GlyphData fallbackGlyphData = font.glyphDataForCharacter(character, mirror);
    ASSERT(fallbackGlyphData.fontData != fontData);

    // Restore original state of the SVG Font glyph table and the current font fallback list,
    // to assure the next lookup of the same glyph won't immediately return the fallback glyph.
    page->setGlyphDataForCharacter(character, glyphData.glyph, originalFontData);
    ASSERT(fallbackGlyphData.fontData);
    return fallbackGlyphData;
}

}

#endif
