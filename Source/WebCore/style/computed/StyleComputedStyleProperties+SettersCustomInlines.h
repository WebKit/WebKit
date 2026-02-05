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

#include "StyleComputedStyleBase+SettersInlines.h"

#define SET_STYLE_PROPERTY_BASE(read, value, write) do { if (!compareEqual(read, value)) write; } while (0)
#define SET_STYLE_PROPERTY(read, write, value) SET_STYLE_PROPERTY_BASE(read, value, write = value)
#define SET(group, variable, value) SET_STYLE_PROPERTY(group->variable, group.access().variable, value)
#define SET_NESTED(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent->variable, group.access().parent.access().variable, value)
#define SET_DOUBLY_NESTED(group, grandparent, parent, variable, value) SET_STYLE_PROPERTY(group->grandparent->parent->variable, group.access().grandparent.access().parent.access().variable, value)
#define SET_NESTED_STRUCT(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent.variable, group.access().parent.variable, value)
#define SET_STYLE_PROPERTY_PAIR(read, write, variable1, value1, variable2, value2) do { Ref readable = Ref { *read }; if (!compareEqual(readable->variable1, value1) || !compareEqual(readable->variable2, value2)) { Ref writable = write; writable->variable1 = value1; writable->variable2 = value2; } } while (0)
#define SET_PAIR(group, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group, group.access(), variable1, value1, variable2, value2)
#define SET_NESTED_PAIR(group, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->parent, group.access().parent.access(), variable1, value1, variable2, value2)
#define SET_DOUBLY_NESTED_PAIR(group, grandparent, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->grandparent->parent, group.access().grandparent.access().parent.access(), variable1, value1, variable2, value2)

namespace WebCore {
namespace Style {

// FIXME: - Below are property setters that are not yet generated

// FIXME: Support setters that need to return a `bool` value to indicate if the property changed.
inline bool ComputedStyleProperties::setDirection(TextDirection bidiDirection)
{
    if (writingMode().computedTextDirection() == bidiDirection)
        return false;
    m_inheritedFlags.writingMode.setTextDirection(bidiDirection);
    return true;
}

inline bool ComputedStyleProperties::setTextOrientation(TextOrientation textOrientation)
{
    if (writingMode().computedTextOrientation() == textOrientation)
        return false;
    m_inheritedFlags.writingMode.setTextOrientation(textOrientation);
    return true;
}

inline bool ComputedStyleProperties::setWritingMode(StyleWritingMode mode)
{
    if (writingMode().computedWritingMode() == mode)
        return false;
    m_inheritedFlags.writingMode.setWritingMode(mode);
    return true;
}

inline bool ComputedStyleProperties::setZoom(Zoom zoom)
{
    // Clamp the effective zoom value to avoid overflow in derived computations.
    // This matches other engines values for compatibility.
    constexpr float minEffectiveZoom = 1e-6f;
    constexpr float maxEffectiveZoom = 1e6f;
    setUsedZoom(clampTo<float>(usedZoom() * evaluate<float>(zoom), minEffectiveZoom, maxEffectiveZoom));

    if (compareEqual(m_nonInheritedData->rareData->zoom, zoom))
        return false;
    m_nonInheritedData.access().rareData.access().zoom = zoom;
    return true;
}

// FIXME: Support properties that set more than one value when set.

inline void ComputedStyleProperties::setAppearance(StyleAppearance appearance)
{
    SET_NESTED_PAIR(m_nonInheritedData, miscData, appearance, static_cast<unsigned>(appearance), usedAppearance, static_cast<unsigned>(appearance));
}

inline void ComputedStyleProperties::setBlendMode(BlendMode mode)
{
    SET_NESTED(m_nonInheritedData, rareData, effectiveBlendMode, static_cast<unsigned>(mode));
    SET(m_inheritedRareData, isInSubtreeWithBlendMode, mode != BlendMode::Normal);
}

inline void ComputedStyleProperties::setDisplay(DisplayType value)
{
    m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(value);
    m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(value);
}

// FIXME: Support generating properties that have their storage spread out

inline void ComputedStyleProperties::setSpecifiedZIndex(ZIndex index)
{
    SET_NESTED_PAIR(m_nonInheritedData, boxData, hasAutoSpecifiedZIndex, static_cast<uint8_t>(index.m_isAuto), specifiedZIndexValue, index.m_value);
}

inline void ComputedStyleProperties::setCursor(Cursor cursor)
{
    m_inheritedFlags.cursorType = static_cast<unsigned>(cursor.predefined);
    SET(m_inheritedRareData, cursorImages, WTF::move(cursor.images));
}

// MARK: Fonts

inline void ComputedStyleProperties::setTextSpacingTrim(TextSpacingTrim value)
{
    auto description = fontDescription();
    description.setTextSpacingTrim(value.platform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setTextAutospace(TextAutospace value)
{
    auto description = fontDescription();
    description.setTextAutospace(toPlatform(value));
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontSize(float size)
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
    setFontDescription(WTF::move(description));

    // Whenever the font size changes, letter-spacing and word-spacing, which are dependent on font-size, must be re-synchronized.
    synchronizeLetterSpacingWithFontCascade();
    synchronizeWordSpacingWithFontCascade();
}

inline void ComputedStyleProperties::setFontSizeAdjust(FontSizeAdjust sizeAdjust)
{
    auto description = fontDescription();
    description.setFontSizeAdjust(sizeAdjust.platform());
    setFontDescription(WTF::move(description));
}

#if ENABLE(VARIATION_FONTS)

inline void ComputedStyleProperties::setFontOpticalSizing(FontOpticalSizing opticalSizing)
{
    auto description = fontDescription();
    description.setOpticalSizing(opticalSizing);
    setFontDescription(WTF::move(description));
}

#endif

inline void ComputedStyleProperties::setFontFamily(FontFamilies families)
{
    auto description = fontDescription();
    description.setFamilies(families.takePlatform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontFeatureSettings(FontFeatureSettings settings)
{
    auto description = fontDescription();
    description.setFeatureSettings(settings.takePlatform());
    setFontDescription(WTF::move(description));
}

#if ENABLE(VARIATION_FONTS)

inline void ComputedStyleProperties::setFontVariationSettings(FontVariationSettings settings)
{
    auto description = fontDescription();
    description.setVariationSettings(settings.takePlatform());
    setFontDescription(WTF::move(description));
}

#endif

inline void ComputedStyleProperties::setFontWeight(FontWeight value)
{
    auto description = fontDescription();
    description.setWeight(value.platform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontWidth(FontWidth value)
{
    auto description = fontDescription();
    description.setWidth(value.platform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontStyle(FontStyle style)
{
    auto description = fontDescription();
    description.setFontStyleSlope(style.platformSlope());
    description.setFontStyleAxis(style.platformAxis());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontPalette(FontPalette value)
{
    auto description = fontDescription();
    description.setFontPalette(value.platform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontKerning(Kerning value)
{
    auto description = fontDescription();
    description.setKerning(value);
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontSmoothing(FontSmoothingMode value)
{
    auto description = fontDescription();
    description.setFontSmoothing(value);
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontSynthesisSmallCaps(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisSmallCaps(value);
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontSynthesisStyle(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisStyle(value);
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontSynthesisWeight(FontSynthesisLonghandValue value)
{
    auto description = fontDescription();
    description.setFontSynthesisWeight(value);
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontVariantAlternates(FontVariantAlternates value)
{
    auto description = fontDescription();
    description.setVariantAlternates(value.takePlatform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontVariantCaps(FontVariantCaps value)
{
    auto description = fontDescription();
    description.setVariantCaps(value);
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontVariantEastAsian(FontVariantEastAsian value)
{
    auto description = fontDescription();
    description.setVariantEastAsian(value.platform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontVariantEmoji(FontVariantEmoji value)
{
    auto description = fontDescription();
    description.setVariantEmoji(value);
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontVariantLigatures(FontVariantLigatures value)
{
    auto description = fontDescription();
    description.setVariantLigatures(value.platform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontVariantNumeric(FontVariantNumeric value)
{
    auto description = fontDescription();
    description.setVariantNumeric(value.platform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setFontVariantPosition(FontVariantPosition value)
{
    auto description = fontDescription();
    description.setVariantPosition(value);
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setLocale(WebkitLocale value)
{
    auto description = fontDescription();
    description.setSpecifiedLocale(value.takePlatform());
    setFontDescription(WTF::move(description));
}

inline void ComputedStyleProperties::setTextRendering(TextRenderingMode value)
{
    auto description = fontDescription();
    description.setTextRenderingMode(value);
    setFontDescription(WTF::move(description));
}

// MARK: Counter Directives

inline void ComputedStyleProperties::didSetCounterIncrement()
{
    updateUsedCounterIncrementDirectives();
}

inline void ComputedStyleProperties::didSetCounterReset()
{
    updateUsedCounterResetDirectives();
}

inline void ComputedStyleProperties::didSetCounterSet()
{
    updateUsedCounterSetDirectives();
}

} // namespace Style
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
