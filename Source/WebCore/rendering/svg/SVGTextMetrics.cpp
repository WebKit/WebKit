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
#include "SVGTextMetrics.h"

#include "RenderSVGInlineText.h"
#include "UnicodeBidi.h"

namespace WebCore {

SVGTextMetrics::SVGTextMetrics(const RenderSVGInlineText& textRenderer, const TextRun& run)
{
    float scalingFactor = textRenderer.scalingFactor();
    ASSERT(scalingFactor);

    const FontCascade& scaledFont = textRenderer.scaledFont();

    // Calculate width/height using the scaled font, divide this result by the scalingFactor afterwards.
    m_width = scaledFont.width(run) / scalingFactor;
    m_glyph.name = emptyString();
    m_height = scaledFont.metricsOfPrimaryFont().height() / scalingFactor;

    m_glyph.unicodeString = run.text().toString();
    m_glyph.isValid = true;

    m_length = static_cast<unsigned>(run.length());
}

TextRun SVGTextMetrics::constructTextRun(const RenderSVGInlineText& text, unsigned position, unsigned length)
{
    const RenderStyle& style = text.style();

    TextRun run(StringView(text.text()).substring(position, length),
        0, /* xPos, only relevant with allowTabs=true */
        0, /* padding, only relevant for justified text, not relevant for SVG */
        ExpansionBehavior::allowRightOnly(),
        style.writingMode().bidiDirection(),
        isOverride(style.unicodeBidi()) /* directionalOverride */);

    // We handle letter & word spacing ourselves.
    run.disableSpacing();
    return run;
}

SVGTextMetrics SVGTextMetrics::measureCharacterRange(const RenderSVGInlineText& text, unsigned position, unsigned length)
{
    return SVGTextMetrics(text, constructTextRun(text, position, length));
}

SVGTextMetrics::SVGTextMetrics(const RenderSVGInlineText& text, unsigned length, float width)
{
    float scalingFactor = text.scalingFactor();
    ASSERT(scalingFactor);

    m_width = width / scalingFactor;
    m_height = text.scaledFont().metricsOfPrimaryFont().height() / scalingFactor;

    m_length = length;
}

}
