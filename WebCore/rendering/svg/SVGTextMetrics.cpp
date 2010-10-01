/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "SVGTextMetrics.h"

#include "RenderSVGInlineText.h"

namespace WebCore {

SVGTextMetrics::SVGTextMetrics()
    : m_width(0)
    , m_height(0)
    , m_length(0)
{
}

SVGTextMetrics::SVGTextMetrics(const Font& font, const TextRun& run, unsigned position, unsigned textLength)
    : m_width(0)
    , m_height(0)
    , m_length(0)
{
    int extraCharsAvailable = textLength - (position + run.length());
    int length = 0;

    m_width = font.floatWidth(run, extraCharsAvailable, length, m_glyph.name);
    m_height = font.height();
    m_glyph.unicodeString = String(run.characters(), length);
    m_glyph.isValid = true;

    ASSERT(length >= 0);
    m_length = static_cast<unsigned>(length);
}

bool SVGTextMetrics::operator==(const SVGTextMetrics& other)
{
    return m_width == other.m_width
        && m_height == other.m_height
        && m_length == other.m_length
        && m_glyph == other.m_glyph;
}

SVGTextMetrics SVGTextMetrics::emptyMetrics()
{
    DEFINE_STATIC_LOCAL(SVGTextMetrics, s_emptyMetrics, ());
    s_emptyMetrics.m_length = 1;
    return s_emptyMetrics;
}

SVGTextMetrics SVGTextMetrics::measureCharacterRange(RenderSVGInlineText* text, unsigned position, unsigned length)
{
    ASSERT(text);
    ASSERT(text->style());

    const Font& font = text->style()->font();
    const UChar* characters = text->characters();

    TextRun run(characters + position, length);
    run.disableSpacing();
    run.disableRoundingHacks();

    return SVGTextMetrics(font, run, position, text->textLength());
}

void SVGTextMetrics::measureAllCharactersIndividually(RenderSVGInlineText* text, Vector<SVGTextMetrics>& allMetrics)
{
    ASSERT(text);
    ASSERT(text->style());

    const Font& font = text->style()->font();
    const UChar* characters = text->characters();
    unsigned length = text->textLength();

    TextRun run(0, 0);
    run.disableSpacing();
    run.disableRoundingHacks();

    for (unsigned position = 0; position < length; ) {
        run.setText(characters + position, 1);

        SVGTextMetrics metrics(font, run, position, text->textLength());
        allMetrics.append(metrics);
        position += metrics.length();
    }
}

}

#endif // ENABLE(SVG)
