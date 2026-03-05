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

#include <WebCore/StyleComputedStyleProperties.h>
#include <WebCore/StyleComputedStyleBase+GettersInlines.h>

#include <WebCore/GraphicsTypes.h>
#include <WebCore/PositionTryOrder.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/StyleAppearance.h>
#include <WebCore/StyleAppleColorFilterData.h>
#include <WebCore/StyleBackdropFilterData.h>
#include <WebCore/StyleBackgroundData.h>
#include <WebCore/StyleBoxData.h>
#include <WebCore/StyleCustomPropertyData.h>
#include <WebCore/StyleDeprecatedFlexibleBoxData.h>
#include <WebCore/StyleDisplay.h>
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

constexpr SVGGlyphOrientationHorizontal ComputedStyleProperties::initialGlyphOrientationHorizontal()
{
    return SVGGlyphOrientationHorizontal::Degrees0;
}

inline WebCore::Color ComputedStyleProperties::initialColor()
{
    return WebCore::Color::black;
}

constexpr LineWidth ComputedStyleProperties::initialBorderBottomWidth()
{
    return LineWidth { 3.0f };
}

constexpr LineWidth ComputedStyleProperties::initialBorderLeftWidth()
{
    return LineWidth { 3.0f };
}

constexpr LineWidth ComputedStyleProperties::initialBorderRightWidth()
{
    return LineWidth { 3.0f };
}

constexpr LineWidth ComputedStyleProperties::initialBorderTopWidth()
{
    return LineWidth { 3.0f };
}

constexpr LineWidth ComputedStyleProperties::initialColumnRuleWidth()
{
    return LineWidth { 3.0f };
}

constexpr LineWidth ComputedStyleProperties::initialOutlineWidth()
{
    return LineWidth { 3.0f };
}

constexpr WebkitLineBoxContain ComputedStyleProperties::initialLineBoxContain()
{
    return { WebkitLineBoxContainValue::Block, WebkitLineBoxContainValue::Inline, WebkitLineBoxContainValue::Replaced };
}

constexpr TextEmphasisPosition ComputedStyleProperties::initialTextEmphasisPosition()
{
    return { TextEmphasisPositionValue::Over, TextEmphasisPositionValue::Right };
}

constexpr PositionVisibility ComputedStyleProperties::initialPositionVisibility()
{
    return PositionVisibilityValue::AnchorsVisible;
}

} // namespace Style
} // namespace WebCore
