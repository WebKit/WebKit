/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StyleComputedStyleProperties.h>
#include <WebCore/StyleComputedStyleBase+GettersInlines.h>

#include <WebCore/AnchorPositionEvaluator.h>
#include <WebCore/AutosizeStatus.h>
#include <WebCore/Element.h>
#include <WebCore/FontCascadeDescription.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/HitTestRequest.h>
#include <WebCore/PositionTryOrder.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/StyleAppearance.h>
#include <WebCore/StyleAppleColorFilterData.h>
#include <WebCore/StyleBackdropFilterData.h>
#include <WebCore/StyleBackgroundData.h>
#include <WebCore/StyleBoxData.h>
#include <WebCore/StyleColorResolver.h>
#include <WebCore/StyleCustomPropertyData.h>
#include <WebCore/StyleDeprecatedFlexibleBoxData.h>
#include <WebCore/StyleFillLayers.h>
#include <WebCore/StyleFilterData.h>
#include <WebCore/StyleFlexibleBoxData.h>
#include <WebCore/StyleFontData.h>
#include <WebCore/StyleFontFamily.h>
#include <WebCore/StyleFontFeatureSettings.h>
#include <WebCore/StyleFontPalette.h>
#include <WebCore/StyleFontSizeAdjust.h>
#include <WebCore/StyleFontStyle.h>
#include <WebCore/StyleFontVariantAlternates.h>
#include <WebCore/StyleFontVariantEastAsian.h>
#include <WebCore/StyleFontVariantLigatures.h>
#include <WebCore/StyleFontVariantNumeric.h>
#include <WebCore/StyleFontVariationSettings.h>
#include <WebCore/StyleFontWeight.h>
#include <WebCore/StyleFontWidth.h>
#include <WebCore/StyleGridData.h>
#include <WebCore/StyleGridItemData.h>
#include <WebCore/StyleGridTrackSizingDirection.h>
#include <WebCore/StyleInheritedData.h>
#include <WebCore/StyleInheritedRareData.h>
#include <WebCore/StyleMarqueeData.h>
#include <WebCore/StyleMultiColumnData.h>
#include <WebCore/StyleNonInheritedData.h>
#include <WebCore/StyleNonInheritedMiscData.h>
#include <WebCore/StyleNonInheritedRareData.h>
#include <WebCore/StyleSVGData.h>
#include <WebCore/StyleSVGFillData.h>
#include <WebCore/StyleSVGLayoutData.h>
#include <WebCore/StyleSVGMarkerResourceData.h>
#include <WebCore/StyleSVGNonInheritedMiscData.h>
#include <WebCore/StyleSVGStopData.h>
#include <WebCore/StyleSVGStrokeData.h>
#include <WebCore/StyleSurroundData.h>
#include <WebCore/StyleTextAlign.h>
#include <WebCore/StyleTextAutospace.h>
#include <WebCore/StyleTextDecorationLine.h>
#include <WebCore/StyleTextSpacingTrim.h>
#include <WebCore/StyleTextTransform.h>
#include <WebCore/StyleTransformData.h>
#include <WebCore/StyleVisitedLinkColorData.h>
#include <WebCore/StyleWebKitLocale.h>
#include <WebCore/UnicodeBidi.h>
#include <WebCore/ViewTimeline.h>
#include <WebCore/WebAnimationTypes.h>

#if ENABLE(APPLE_PAY)
#include <WebCore/ApplePayButtonPart.h>
#endif

#if HAVE(CORE_MATERIAL)
#include <WebCore/AppleVisualEffect.h>
#endif

namespace WebCore {
namespace Style {

// FIXME: Support generating properties that have their storage spread out

inline Cursor ComputedStyleProperties::cursor() const
{
    return { m_inheritedRareData->cursorImages, static_cast<CursorType>(m_inheritedFlags.cursorType) };
}

inline ZIndex ComputedStyleProperties::specifiedZIndex() const
{
    return m_nonInheritedData->boxData->specifiedZIndex();
}

// FIXME: Support writing mode properties.

inline TextDirection ComputedStyleProperties::computedDirection() const
{
    return writingMode().computedTextDirection();
}

inline StyleWritingMode ComputedStyleProperties::computedWritingMode() const
{
    return writingMode().computedWritingMode();
}

inline TextOrientation ComputedStyleProperties::computedTextOrientation() const
{
    return writingMode().computedTextOrientation();
}

// FIXME: Support font properties.

float ComputedStyleProperties::specifiedFontSize() const
{
    return fontDescription().specifiedSize();
}

inline FontFamilies ComputedStyleProperties::fontFamily() const
{
    return { fontDescription().families(), fontDescription().isSpecifiedFont() };
}

inline FontPalette ComputedStyleProperties::fontPalette() const
{
    return fontDescription().fontPalette();
}

inline FontSizeAdjust ComputedStyleProperties::fontSizeAdjust() const
{
    return fontDescription().fontSizeAdjust();
}

inline FontStyle ComputedStyleProperties::fontStyle() const
{
    return { fontDescription().fontStyleSlope(), fontDescription().fontStyleAxis() };
}

#if ENABLE(VARIATION_FONTS)

inline FontOpticalSizing ComputedStyleProperties::fontOpticalSizing() const
{
    return fontDescription().opticalSizing();
}

#endif

inline FontFeatureSettings ComputedStyleProperties::fontFeatureSettings() const
{
    return fontDescription().featureSettings();
}

#if ENABLE(VARIATION_FONTS)

inline FontVariationSettings ComputedStyleProperties::fontVariationSettings() const
{
    return fontDescription().variationSettings();
}

#endif

inline FontWeight ComputedStyleProperties::fontWeight() const
{
    return fontDescription().weight();
}

inline FontWidth ComputedStyleProperties::fontWidth() const
{
    return fontDescription().width();
}

inline Kerning ComputedStyleProperties::fontKerning() const
{
    return fontDescription().kerning();
}

inline FontSmoothingMode ComputedStyleProperties::fontSmoothing() const
{
    return fontDescription().fontSmoothing();
}

inline FontSynthesisLonghandValue ComputedStyleProperties::fontSynthesisSmallCaps() const
{
    return fontDescription().fontSynthesisSmallCaps();
}

inline FontSynthesisLonghandValue ComputedStyleProperties::fontSynthesisStyle() const
{
    return fontDescription().fontSynthesisStyle();
}

inline FontSynthesisLonghandValue ComputedStyleProperties::fontSynthesisWeight() const
{
    return fontDescription().fontSynthesisWeight();
}

inline FontVariantAlternates ComputedStyleProperties::fontVariantAlternates() const
{
    return fontDescription().variantAlternates();
}

inline FontVariantCaps ComputedStyleProperties::fontVariantCaps() const
{
    return fontDescription().variantCaps();
}

inline FontVariantEastAsian ComputedStyleProperties::fontVariantEastAsian() const
{
    return fontDescription().variantEastAsian();
}

inline FontVariantEmoji ComputedStyleProperties::fontVariantEmoji() const
{
    return fontDescription().variantEmoji();
}

inline FontVariantLigatures ComputedStyleProperties::fontVariantLigatures() const
{
    return fontDescription().variantLigatures();
}

inline FontVariantNumeric ComputedStyleProperties::fontVariantNumeric() const
{
    return fontDescription().variantNumeric();
}

inline FontVariantPosition ComputedStyleProperties::fontVariantPosition() const
{
    return fontDescription().variantPosition();
}

inline TextRenderingMode ComputedStyleProperties::textRendering() const
{
    return fontDescription().textRenderingMode();
}

inline TextAutospace ComputedStyleProperties::textAutospace() const
{
    return fontDescription().textAutospace();
}

inline TextSpacingTrim ComputedStyleProperties::textSpacingTrim() const
{
    return fontDescription().textSpacingTrim();
}

inline WebkitLocale ComputedStyleProperties::locale() const
{
    return fontDescription().specifiedLocale();
}

// MARK: - Custom ColorPropertyTrait function definitions

inline const Color& ColorPropertyTraits<PropertyNameConstant<CSSPropertyColor>>::color(const ComputedStyleProperties&)
{
    // FIXME: This works because because `currentColor` will be resolved to `color()`. It would be slightly nicer if we could return an actual `Style::Color`, but `color()` is currently stored as a `WebCore::Color` and therefore we cannot return it as a reference.
    return Color::currentColor();
}

inline const Color& ColorPropertyTraits<PropertyNameConstant<CSSPropertyColor>>::visitedLinkColor(const ComputedStyleProperties&)
{
    // FIXME: This works because because `currentColor` will be resolved to `visitedLinkColor()`. It would be slightly nicer if we could return an actual `Style::Color`, but `visitedLinkColor()` is currently stored as a `WebCore::Color` and therefore we cannot return it as a reference.
    return Color::currentColor();
}

inline const Color& ColorPropertyTraits<PropertyNameConstant<CSSPropertyAccentColor>>::color(const ComputedStyleProperties& style)
{
    return style.accentColor().colorOrCurrentColor();
}

inline const Color& ColorPropertyTraits<PropertyNameConstant<CSSPropertyCaretColor>>::color(const ComputedStyleProperties& style)
{
    return style.caretColor().colorOrCurrentColor();
}

inline const Color& ColorPropertyTraits<PropertyNameConstant<CSSPropertyCaretColor>>::visitedLinkColor(const ComputedStyleProperties& style)
{
    return style.visitedLinkCaretColor().colorOrCurrentColor();
}

inline const Color& ColorPropertyTraits<PropertyNameConstant<CSSPropertyFill>>::color(const ComputedStyleProperties& style)
{
    return style.fill().colorDisregardingType();
}

inline const Color& ColorPropertyTraits<PropertyNameConstant<CSSPropertyFill>>::visitedLinkColor(const ComputedStyleProperties& style)
{
    return style.visitedLinkFill().colorDisregardingType();
}

inline const Color& ColorPropertyTraits<PropertyNameConstant<CSSPropertyStroke>>::color(const ComputedStyleProperties& style)
{
    return style.stroke().colorDisregardingType();
}

inline const Color& ColorPropertyTraits<PropertyNameConstant<CSSPropertyStroke>>::visitedLinkColor(const ComputedStyleProperties& style)
{
    return style.visitedLinkStroke().colorDisregardingType();
}

inline WebCore::Color ColorPropertyTraits<PropertyNameConstant<CSSPropertyTextDecorationColor>>::colorResolvingCurrentColor(const ComputedStyleProperties& style)
{
    auto& result = style.textDecorationColor();
    if (result.isCurrentColor()) {
        if ((style.hasExplicitlySetStrokeWidth() && style.strokeWidth().isPossiblyPositive()) || style.textStrokeWidth().isPositive()) {
            // Prefer stroke color if possible but not if it's fully transparent.
            if (style.hasExplicitlySetStrokeColor()) {
                auto strokeColor = style.strokeColor().resolveColor(style.color());
                if (strokeColor.isVisible())
                    return strokeColor;
            } else {
                auto strokeColor = style.textStrokeColor().resolveColor(style.color());
                if (strokeColor.isVisible())
                    return strokeColor;
            }
        }
        return style.textFillColor().resolveColor(style.color());
    }
    return result.resolveColor(style.color());
}

inline WebCore::Color ColorPropertyTraits<PropertyNameConstant<CSSPropertyTextDecorationColor>>::visitedLinkColorResolvingCurrentColor(const ComputedStyleProperties& style)
{
    auto& result = style.visitedLinkTextDecorationColor();
    if (result.isCurrentColor()) {
        if ((style.hasExplicitlySetStrokeWidth() && style.strokeWidth().isPossiblyPositive()) || style.textStrokeWidth().isPositive()) {
            // Prefer stroke color if possible but not if it's fully transparent.
            if (style.hasExplicitlySetStrokeColor()) {
                auto strokeColor = style.visitedLinkStrokeColor().resolveColor(style.visitedLinkColor());
                if (strokeColor.isVisible())
                    return strokeColor;
            } else {
                auto strokeColor = style.visitedLinkTextStrokeColor().resolveColor(style.visitedLinkColor());
                if (strokeColor.isVisible())
                    return strokeColor;
            }
        }
        return style.visitedLinkTextFillColor().resolveColor(style.visitedLinkColor());
    }
    return result.resolveColor(style.visitedLinkColor());
}

inline bool ColorPropertyTraits<PropertyNameConstant<CSSPropertyBackgroundColor>>::excludesVisitedLinkColor(const WebCore::Color& visitedLinkColor)
{
    // FIXME: Technically someone could explicitly specify the color transparent, but for now we'll just
    // assume that if the background color is transparent that it wasn't set. Note that it's weird that
    // we're returning unvisited info for a visited link, but given our restriction that the alpha values
    // have to match, it makes more sense to return the unvisited background color if specified than it
    // does to return black. This behavior matches what Firefox 4 does as well.
    return visitedLinkColor == WebCore::Color::transparentBlack;
}

} // namespace Style
} // namespace WebCore
