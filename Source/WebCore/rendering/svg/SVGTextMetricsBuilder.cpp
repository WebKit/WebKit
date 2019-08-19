/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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
#include "SVGTextMetricsBuilder.h"

#include "RenderChildIterator.h"
#include "RenderSVGInline.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGText.h"

namespace WebCore {

SVGTextMetricsBuilder::SVGTextMetricsBuilder()
    : m_text(0)
    , m_run(StringView())
    , m_textPosition(0)
    , m_isComplexText(false)
    , m_totalWidth(0)
{
}

inline bool SVGTextMetricsBuilder::currentCharacterStartsSurrogatePair() const
{
    return U16_IS_LEAD(m_run[m_textPosition]) && (m_textPosition + 1) < m_run.length() && U16_IS_TRAIL(m_run[m_textPosition + 1]);
}

bool SVGTextMetricsBuilder::advance()
{
    m_textPosition += m_currentMetrics.length();
    if (m_textPosition >= m_run.length())
        return false;

    if (m_isComplexText)
        advanceComplexText();
    else
        advanceSimpleText();

    return m_currentMetrics.length() > 0;
}

void SVGTextMetricsBuilder::advanceSimpleText()
{
    GlyphBuffer glyphBuffer;
    unsigned metricsLength = m_simpleWidthIterator->advance(m_textPosition + 1, &glyphBuffer);
    if (!metricsLength) {
        m_currentMetrics = SVGTextMetrics();
        return;
    }

    float currentWidth = m_simpleWidthIterator->runWidthSoFar() - m_totalWidth;
    m_totalWidth = m_simpleWidthIterator->runWidthSoFar();

    m_currentMetrics = SVGTextMetrics(*m_text, metricsLength, currentWidth);
}

void SVGTextMetricsBuilder::advanceComplexText()
{
    unsigned metricsLength = currentCharacterStartsSurrogatePair() ? 2 : 1;
    m_currentMetrics = SVGTextMetrics::measureCharacterRange(*m_text, m_textPosition, metricsLength);
    m_complexStartToCurrentMetrics = SVGTextMetrics::measureCharacterRange(*m_text, 0, m_textPosition + metricsLength);
    ASSERT(m_currentMetrics.length() == metricsLength);

    // Frequent case for Arabic text: when measuring a single character the arabic isolated form is taken
    // when rendering the glyph "in context" (with it's surrounding characters) it changes due to shaping.
    // So whenever currentWidth != currentMetrics.width(), we are processing a text run whose length is
    // not equal to the sum of the individual lengths of the glyphs, when measuring them isolated.
    float currentWidth = m_complexStartToCurrentMetrics.width() - m_totalWidth;
    if (currentWidth != m_currentMetrics.width())
        m_currentMetrics.setWidth(currentWidth);

    m_totalWidth = m_complexStartToCurrentMetrics.width();
}

void SVGTextMetricsBuilder::initializeMeasurementWithTextRenderer(RenderSVGInlineText& text)
{
    m_text = &text;
    m_textPosition = 0;
    m_currentMetrics = SVGTextMetrics();
    m_complexStartToCurrentMetrics = SVGTextMetrics();
    m_totalWidth = 0;

    const FontCascade& scaledFont = text.scaledFont();
    m_run = SVGTextMetrics::constructTextRun(text);
    m_isComplexText = scaledFont.codePath(m_run) == FontCascade::Complex;

    if (m_isComplexText)
        m_simpleWidthIterator = nullptr;
    else
        m_simpleWidthIterator = makeUnique<WidthIterator>(&scaledFont, m_run);
}

struct MeasureTextData {
    MeasureTextData(SVGCharacterDataMap* characterDataMap)
        : allCharactersMap(characterDataMap)
        , lastCharacter(0)
        , processRenderer(false)
        , valueListPosition(0)
        , skippedCharacters(0)
    {
    }

    SVGCharacterDataMap* allCharactersMap;
    UChar lastCharacter;
    bool processRenderer;
    unsigned valueListPosition;
    unsigned skippedCharacters;
};

void SVGTextMetricsBuilder::measureTextRenderer(RenderSVGInlineText& text, MeasureTextData* data)
{
    SVGTextLayoutAttributes* attributes = text.layoutAttributes();
    Vector<SVGTextMetrics>* textMetricsValues = &attributes->textMetricsValues();
    if (data->processRenderer) {
        if (data->allCharactersMap)
            attributes->clear();
        else
            textMetricsValues->clear();
    }

    initializeMeasurementWithTextRenderer(text);
    bool preserveWhiteSpace = text.style().whiteSpace() == WhiteSpace::Pre;
    int surrogatePairCharacters = 0;

    while (advance()) {
        UChar currentCharacter = m_run[m_textPosition];
        if (currentCharacter == ' ' && !preserveWhiteSpace && (!data->lastCharacter || data->lastCharacter == ' ')) {
            if (data->processRenderer)
                textMetricsValues->append(SVGTextMetrics(SVGTextMetrics::SkippedSpaceMetrics));
            if (data->allCharactersMap)
                data->skippedCharacters += m_currentMetrics.length();
            continue;
        }

        if (data->processRenderer) {
            if (data->allCharactersMap) {
                const SVGCharacterDataMap::const_iterator it = data->allCharactersMap->find(data->valueListPosition + m_textPosition - data->skippedCharacters - surrogatePairCharacters + 1);
                if (it != data->allCharactersMap->end())
                    attributes->characterDataMap().set(m_textPosition + 1, it->value);
            }
            textMetricsValues->append(m_currentMetrics);
        }

        if (data->allCharactersMap && currentCharacterStartsSurrogatePair())
            surrogatePairCharacters++;

        data->lastCharacter = currentCharacter;
    }

    if (!data->allCharactersMap)
        return;

    data->valueListPosition += m_textPosition - data->skippedCharacters;
    data->skippedCharacters = 0;
}

void SVGTextMetricsBuilder::walkTree(RenderElement& start, RenderSVGInlineText* stopAtLeaf, MeasureTextData* data)
{
    for (auto& child : childrenOfType<RenderObject>(start)) {
        if (is<RenderSVGInlineText>(child)) {
            auto& text = downcast<RenderSVGInlineText>(child);
            if (stopAtLeaf && stopAtLeaf != &text) {
                data->processRenderer = false;
                measureTextRenderer(text, data);
                continue;
            }

            data->processRenderer = true;
            measureTextRenderer(text, data);
            if (stopAtLeaf)
                return;

            continue;
        }

        if (!is<RenderSVGInline>(child))
            continue;

        walkTree(downcast<RenderSVGInline>(child), stopAtLeaf, data);
    }
}

void SVGTextMetricsBuilder::measureTextRenderer(RenderSVGInlineText& text)
{
    auto* textRoot = RenderSVGText::locateRenderSVGTextAncestor(text);
    if (!textRoot)
        return;

    MeasureTextData data(nullptr);
    walkTree(*textRoot, &text, &data);
}

void SVGTextMetricsBuilder::buildMetricsAndLayoutAttributes(RenderSVGText& textRoot, RenderSVGInlineText* stopAtLeaf, SVGCharacterDataMap& allCharactersMap)
{
    MeasureTextData data(&allCharactersMap);
    walkTree(textRoot, stopAtLeaf, &data);
}

}
