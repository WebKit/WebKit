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

#pragma once

#include "RenderStyleBaseSetters.h"

#define SET_STYLE_PROPERTY_BASE(read, value, write) do { if (!compareEqual(read, value)) write; } while (0)
#define SET_STYLE_PROPERTY(read, write, value) SET_STYLE_PROPERTY_BASE(read, value, write = value)
#define SET(group, variable, value) SET_STYLE_PROPERTY(group->variable, group.access().variable, value)
#define SET_NESTED(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent->variable, group.access().parent.access().variable, value)
#define SET_DOUBLY_NESTED(group, grandparent, parent, variable, value) SET_STYLE_PROPERTY(group->grandparent->parent->variable, group.access().grandparent.access().parent.access().variable, value)
#define SET_NESTED_STRUCT(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent.variable, group.access().parent.variable, value)
#define SET_STYLE_PROPERTY_PAIR(read, write, variable1, value1, variable2, value2) do { auto& readable = *read; if (!compareEqual(readable.variable1, value1) || !compareEqual(readable.variable2, value2)) { auto& writable = write; writable.variable1 = value1; writable.variable2 = value2; } } while (0)
#define SET_PAIR(group, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group, group.access(), variable1, value1, variable2, value2)
#define SET_NESTED_PAIR(group, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->parent, group.access().parent.access(), variable1, value1, variable2, value2)
#define SET_DOUBLY_NESTED_PAIR(group, grandparent, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->grandparent->parent, group.access().grandparent.access().parent.access(), variable1, value1, variable2, value2)

namespace WebCore {

// FIXME: - Below are property setters that are not yet generated

// FIXME: Support setters that need to return a `bool` value to indicate if the property changed.
inline bool RenderStyleProperties::setDirection(TextDirection bidiDirection)
{
    if (writingMode().computedTextDirection() == bidiDirection)
        return false;
    m_inheritedFlags.writingMode.setTextDirection(bidiDirection);
    return true;
}

inline bool RenderStyleProperties::setTextOrientation(TextOrientation textOrientation)
{
    if (writingMode().computedTextOrientation() == textOrientation)
        return false;
    m_inheritedFlags.writingMode.setTextOrientation(textOrientation);
    return true;
}

inline bool RenderStyleProperties::setWritingMode(StyleWritingMode mode)
{
    if (writingMode().computedWritingMode() == mode)
        return false;
    m_inheritedFlags.writingMode.setWritingMode(mode);
    return true;
}

inline bool RenderStyleProperties::setZoom(Style::Zoom zoom)
{
    // Clamp the effective zoom value to avoid overflow in derived computations.
    // This matches other engines values for compatibility.
    constexpr float minEffectiveZoom = 1e-6f;
    constexpr float maxEffectiveZoom = 1e6f;
    setUsedZoom(clampTo<float>(usedZoom() * Style::evaluate<float>(zoom), minEffectiveZoom, maxEffectiveZoom));

    if (compareEqual(m_nonInheritedData->rareData->zoom, zoom))
        return false;
    m_nonInheritedData.access().rareData.access().zoom = zoom;
    return true;
}

// FIXME: Support properties that set more than one value when set.

inline void RenderStyleProperties::setAppearance(StyleAppearance appearance)
{
    SET_NESTED_PAIR(m_nonInheritedData, miscData, appearance, static_cast<unsigned>(appearance), usedAppearance, static_cast<unsigned>(appearance));
}

inline void RenderStyleProperties::setBlendMode(BlendMode mode)
{
    SET_NESTED(m_nonInheritedData, rareData, effectiveBlendMode, static_cast<unsigned>(mode));
    SET(m_rareInheritedData, isInSubtreeWithBlendMode, mode != BlendMode::Normal);
}

inline void RenderStyleProperties::setDisplay(DisplayType value)
{
    m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(value);
    m_nonInheritedFlags.effectiveDisplay = m_nonInheritedFlags.originalDisplay;
}

// FIXME: Support generating properties that have their storage spread out

inline void RenderStyleProperties::setSpecifiedZIndex(Style::ZIndex index)
{
    SET_NESTED_PAIR(m_nonInheritedData, boxData, hasAutoSpecifiedZIndex, static_cast<uint8_t>(index.m_isAuto), specifiedZIndexValue, index.m_value);
}

inline void RenderStyleProperties::setCursor(Style::Cursor cursor)
{
    m_inheritedFlags.cursorType = static_cast<unsigned>(cursor.predefined);
    SET(m_rareInheritedData, cursorImages, WTFMove(cursor.images));
}

// MARK: Support Font properties

inline void RenderStyleProperties::setTextSpacingTrim(Style::TextSpacingTrim value)
{
    auto description = fontDescription();
    description.setTextSpacingTrim(value.platform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setTextAutospace(Style::TextAutospace value)
{
    auto description = fontDescription();
    description.setTextAutospace(Style::toPlatform(value));
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontSize(Style::FontSize size)
{
    // size must be specifiedSize if Text Autosizing is enabled, but computedSize if text
    // zoom is enabled (if neither is enabled it's irrelevant as they're probably the same).

    auto platformSize = size.value.unresolvedValue();

    ASSERT(std::isfinite(platformSize));
    if (!std::isfinite(platformSize) || platformSize < 0)
        platformSize = 0;
    else
        platformSize = std::min(maximumAllowedFontSize, platformSize);

    auto description = fontDescription();
    description.setSpecifiedSize(platformSize);
    description.setComputedSize(platformSize);
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontSizeAdjust(Style::FontSizeAdjust sizeAdjust)
{
    auto description = fontDescription();
    description.setFontSizeAdjust(sizeAdjust.platform());
    setFontDescription(WTFMove(description));
}

#if ENABLE(VARIATION_FONTS)

inline void RenderStyleProperties::setFontOpticalSizing(FontOpticalSizing opticalSizing)
{
    auto description = fontDescription();
    description.setOpticalSizing(opticalSizing);
    setFontDescription(WTFMove(description));
}

#endif

inline void RenderStyleProperties::setFontFamily(Style::FontFamilies families)
{
    auto description = fontDescription();
    description.setFamilies(families.takePlatform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontFeatureSettings(Style::FontFeatureSettings settings)
{
    auto description = fontDescription();
    description.setFeatureSettings(settings.takePlatform());
    setFontDescription(WTFMove(description));
}

#if ENABLE(VARIATION_FONTS)

inline void RenderStyleProperties::setFontVariationSettings(Style::FontVariationSettings settings)
{
    auto description = fontDescription();
    description.setVariationSettings(settings.takePlatform());
    setFontDescription(WTFMove(description));
}

#endif

inline void RenderStyleProperties::setFontWeight(Style::FontWeight value)
{
    auto description = fontDescription();
    description.setWeight(value.platform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontWidth(Style::FontWidth value)
{
    auto description = fontDescription();
    description.setWidth(value.platform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontStyle(Style::FontStyle style)
{
    auto description = fontDescription();
    description.setFontStyleSlope(style.platformSlope());
    description.setFontStyleAxis(style.platformAxis());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontPalette(Style::FontPalette value)
{
    auto description = fontDescription();
    description.setFontPalette(value.platform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontKerning(Kerning value)
{
    auto description = fontDescription();
    description.setKerning(value);
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontSmoothing(FontSmoothingMode value)
{
    auto description = fontDescription();
    description.setFontSmoothing(value);
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontSynthesisSmallCaps(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisSmallCaps(value);
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontSynthesisStyle(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisStyle(value);
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontSynthesisWeight(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisWeight(value);
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontVariantAlternates(Style::FontVariantAlternates value)
{
    auto description = fontDescription();
    description.setVariantAlternates(value.takePlatform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontVariantCaps(FontVariantCaps value)
{
    auto description = fontDescription();
    description.setVariantCaps(value);
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontVariantEastAsian(Style::FontVariantEastAsian value)
{
    auto description = fontDescription();
    description.setVariantEastAsian(value.platform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontVariantEmoji(FontVariantEmoji value)
{
    auto description = fontDescription();
    description.setVariantEmoji(value);
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontVariantLigatures(Style::FontVariantLigatures value)
{
    auto description = fontDescription();
    description.setVariantLigatures(value.platform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontVariantNumeric(Style::FontVariantNumeric value)
{
    auto description = fontDescription();
    description.setVariantNumeric(value.platform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setFontVariantPosition(FontVariantPosition value)
{
    auto description = fontDescription();
    description.setVariantPosition(value);
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setLocale(Style::WebkitLocale value)
{
    auto description = fontDescription();
    description.setSpecifiedLocale(value.takePlatform());
    setFontDescription(WTFMove(description));
}

inline void RenderStyleProperties::setTextRendering(TextRenderingMode value)
{
    auto description = fontDescription();
    description.setTextRenderingMode(value);
    setFontDescription(WTFMove(description));
}

} // namespace WebCore

#undef SET
#undef SET_DOUBLY_NESTED
#undef SET_DOUBLY_NESTED_PAIR
#undef SET_NESTED
#undef SET_NESTED_PAIR
#undef SET_NESTED_STRUCT
#undef SET_PAIR
#undef SET_STYLE_PROPERTY
#undef SET_STYLE_PROPERTY_BASE
#undef SET_STYLE_PROPERTY_PAIR
