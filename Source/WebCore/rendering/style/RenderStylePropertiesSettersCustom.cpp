/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderStylePropertiesSettersCustom.h"

namespace WebCore {

void RenderStyleProperties::setTextSpacingTrim(Style::TextSpacingTrim value)
{
    auto description = fontDescription();
    description.setTextSpacingTrim(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setTextAutospace(Style::TextAutospace value)
{
    auto description = fontDescription();
    description.setTextAutospace(Style::toPlatform(value));
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontSize(float size)
{
    // size must be specifiedSize if Text Autosizing is enabled, but computedSize if text
    // zoom is enabled (if neither is enabled it's irrelevant as they're probably the same).

    ASSERT(std::isfinite(size));
    if (!std::isfinite(size) || size < 0)
        size = 0;
    else
        size = std::min(maximumAllowedFontSize, size);

    auto description = fontDescription();
    description.setSpecifiedSize(size);
    description.setComputedSize(size);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontSizeAdjust(Style::FontSizeAdjust sizeAdjust)
{
    auto description = fontDescription();
    description.setFontSizeAdjust(sizeAdjust.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontOpticalSizing(FontOpticalSizing opticalSizing)
{
    auto description = fontDescription();
    description.setOpticalSizing(opticalSizing);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontFamily(Style::FontFamilies families)
{
    auto description = fontDescription();
    description.setFamilies(families.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontFeatureSettings(Style::FontFeatureSettings settings)
{
    auto description = fontDescription();
    description.setFeatureSettings(settings.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontVariationSettings(Style::FontVariationSettings settings)
{
    auto description = fontDescription();
    description.setVariationSettings(settings.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontWeight(Style::FontWeight value)
{
    auto description = fontDescription();
    description.setWeight(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontWidth(Style::FontWidth value)
{
    auto description = fontDescription();
    description.setWidth(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontStyle(Style::FontStyle style)
{
    auto description = fontDescription();
    description.setFontStyleSlope(style.platformSlope());
    description.setFontStyleAxis(style.platformAxis());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontPalette(Style::FontPalette value)
{
    auto description = fontDescription();
    description.setFontPalette(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontKerning(Kerning value)
{
    auto description = fontDescription();
    description.setKerning(value);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontSmoothing(FontSmoothingMode value)
{
    auto description = fontDescription();
    description.setFontSmoothing(value);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontSynthesisSmallCaps(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisSmallCaps(value);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontSynthesisStyle(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisStyle(value);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontSynthesisWeight(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisWeight(value);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontVariantAlternates(Style::FontVariantAlternates value)
{
    auto description = fontDescription();
    description.setVariantAlternates(value.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontVariantCaps(FontVariantCaps value)
{
    auto description = fontDescription();
    description.setVariantCaps(value);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontVariantEastAsian(Style::FontVariantEastAsian value)
{
    auto description = fontDescription();
    description.setVariantEastAsian(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontVariantEmoji(FontVariantEmoji value)
{
    auto description = fontDescription();
    description.setVariantEmoji(value);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontVariantLigatures(Style::FontVariantLigatures value)
{
    auto description = fontDescription();
    description.setVariantLigatures(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontVariantNumeric(Style::FontVariantNumeric value)
{
    auto description = fontDescription();
    description.setVariantNumeric(value.platform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setFontVariantPosition(FontVariantPosition value)
{
    auto description = fontDescription();
    description.setVariantPosition(value);
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setLocale(Style::WebkitLocale value)
{
    auto description = fontDescription();
    description.setSpecifiedLocale(value.takePlatform());
    setFontDescription(WTFMove(description));
}

void RenderStyleProperties::setTextRendering(TextRenderingMode value)
{
    auto description = fontDescription();
    description.setTextRenderingMode(value);
    setFontDescription(WTFMove(description));
}

} // namespace WebCore
