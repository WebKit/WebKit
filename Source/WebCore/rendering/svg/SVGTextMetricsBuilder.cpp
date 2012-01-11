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

#if ENABLE(SVG)
#include "SVGTextMetricsBuilder.h"

#include "RenderSVGInlineText.h"
#include "RenderSVGText.h"
#include "SVGTextRunRenderingContext.h"
#include "WidthIterator.h"

namespace WebCore {

SVGTextMetricsBuilder::SVGTextMetricsBuilder()
    : m_text(0)
    , m_characters(0)
    , m_textLength(0)
    , m_textPosition(0)
    , m_finished(false)
    , m_isComplexText(false)
    , m_preserveWhiteSpace(false)
    , m_totalWidth(0)
{
}

void SVGTextMetricsBuilder::advance()
{
    m_textPosition += m_currentMetrics.length();
    if (m_textPosition >= m_textLength) {
        m_finished = true;
        return;
    }

    bool startsSurrogatePair = U16_IS_LEAD(m_characters[m_textPosition]) && (m_textPosition + 1) < m_textLength && U16_IS_TRAIL(m_characters[m_textPosition + 1]);
#if PLATFORM(QT)
    advanceComplexText(startsSurrogatePair);
#else
    if (m_isComplexText)
        advanceComplexText(startsSurrogatePair);
    else
        advanceSimpleText(startsSurrogatePair);
#endif

    if (!m_currentMetrics.length())
        m_finished = true;
}

void SVGTextMetricsBuilder::advanceSimpleText(bool startsSurrogatePair)
{
#if PLATFORM(QT)
    UNUSED_PARAM(startsSurrogatePair);
    ASSERT_NOT_REACHED();
#else
    unsigned metricsLength = m_simpleWidthIterator->advance(m_textPosition + 1);
    if (!metricsLength) {
        m_currentMetrics = SVGTextMetrics();
        return;
    }

    if (startsSurrogatePair)
        ASSERT(metricsLength == 2);

    float currentWidth = m_simpleWidthIterator->runWidthSoFar() - m_totalWidth;
    m_totalWidth = m_simpleWidthIterator->runWidthSoFar();

#if ENABLE(SVG_FONTS)
    m_currentMetrics = SVGTextMetrics(m_text, m_textPosition, metricsLength, currentWidth, m_simpleWidthIterator->lastGlyphName());
#else
    m_currentMetrics = SVGTextMetrics(m_text, m_textPosition, metricsLength, currentWidth, emptyString());
#endif
#endif
}

void SVGTextMetricsBuilder::advanceComplexText(bool startsSurrogatePair)
{
    unsigned metricsLength = startsSurrogatePair ? 2 : 1;
    m_currentMetrics = SVGTextMetrics::measureCharacterRange(m_text, m_textPosition, metricsLength);
    m_complexStartToCurrentMetrics = SVGTextMetrics::measureCharacterRange(m_text, 0, m_textPosition + metricsLength);
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

void SVGTextMetricsBuilder::measureTextRenderer(RenderSVGInlineText*, const UChar*& lastCharacter, Vector<SVGTextMetrics>* textMetricsValues)
{
    ASSERT(textMetricsValues ? textMetricsValues->isEmpty() : true);
    while (!m_finished) {
        advance();

        const UChar* currentCharacter = m_characters + m_textPosition;
        if (*currentCharacter == ' ' && !m_preserveWhiteSpace && (!lastCharacter || *lastCharacter == ' ')) {
            if (textMetricsValues)
                textMetricsValues->append(SVGTextMetrics(SVGTextMetrics::SkippedSpaceMetrics));
            continue;
        }

        if (textMetricsValues)
            textMetricsValues->append(m_currentMetrics);
        lastCharacter = currentCharacter;
    }
}

void SVGTextMetricsBuilder::walkTreeUntilSpecificLeafIsReached(RenderObject* start, RenderSVGInlineText* stopAtElement, const UChar*& lastCharacter, Vector<SVGTextMetrics>& textMetricsValues)
{
    for (RenderObject* child = start->firstChild(); child; child = child->nextSibling()) {
        if (child->isSVGInlineText()) {
            RenderSVGInlineText* text = toRenderSVGInlineText(child);
            if (stopAtElement && stopAtElement != text) {
                measureTextRenderer(text, lastCharacter, 0);
                continue;
            }

            measureTextRenderer(text, lastCharacter, &textMetricsValues);
            if (stopAtElement) {
                ASSERT(stopAtElement == text);
                return;
            }

            continue;
        }

        if (!child->isSVGInline())
            continue;

        walkTreeUntilSpecificLeafIsReached(child, stopAtElement, lastCharacter, textMetricsValues);
    }
}

void SVGTextMetricsBuilder::measureAllCharactersOfRenderer(RenderSVGInlineText* text, Vector<SVGTextMetrics>& textMetricsValues)
{
    ASSERT(!m_finished);
    ASSERT(text);
    ASSERT(textMetricsValues.isEmpty());

    RenderSVGText* textRoot = RenderSVGText::locateRenderSVGTextAncestor(text);
    if (!textRoot)
        return;

    m_text = text;
    m_characters = text->characters();
    m_textLength = text->textLength();
    m_preserveWhiteSpace = text->style()->whiteSpace() == PRE;

    const Font& scaledFont = text->scaledFont();
    TextRun run = SVGTextMetrics::constructTextRun(text, text->characters(), 0, text->textLength());
    m_isComplexText = scaledFont.codePath(run) == Font::Complex;
#if !PLATFORM(QT)
    if (!m_isComplexText)
        m_simpleWidthIterator = adoptPtr(new WidthIterator(&text->style()->font(), run));
#endif

    const UChar* lastCharacter = 0;
    walkTreeUntilSpecificLeafIsReached(textRoot, text, lastCharacter, textMetricsValues);
}

}

#endif // ENABLE(SVG)
