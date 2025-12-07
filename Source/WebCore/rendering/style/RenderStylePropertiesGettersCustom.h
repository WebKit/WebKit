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

#include <WebCore/AnchorPositionEvaluator.h>
#include <WebCore/Element.h>
#include <WebCore/FontCascadeDescription.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/HitTestRequest.h>
#include <WebCore/PositionTryOrder.h>
#include <WebCore/RenderStyleBaseInlines.h>
#include <WebCore/RenderStyleProperties.h>
#include <WebCore/SVGRenderStyle.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/StyleAppearance.h>
#include <WebCore/StyleAppleColorFilterData.h>
#include <WebCore/StyleBackdropFilterData.h>
#include <WebCore/StyleBackgroundData.h>
#include <WebCore/StyleBoxData.h>
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
#include <WebCore/StyleMarqueeData.h>
#include <WebCore/StyleMiscNonInheritedData.h>
#include <WebCore/StyleMultiColData.h>
#include <WebCore/StyleNonInheritedData.h>
#include <WebCore/StyleRareInheritedData.h>
#include <WebCore/StyleRareNonInheritedData.h>
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

// FIXME: Support generating properties that have their storage spread out

inline Style::Cursor RenderStyleProperties::cursor() const
{
    return { m_rareInheritedData->cursorImages, static_cast<CursorType>(m_inheritedFlags.cursorType) };
}

inline Style::ZIndex RenderStyleProperties::specifiedZIndex() const
{
    return m_nonInheritedData->boxData->specifiedZIndex();
}

// FIXME: Support writing mode properties.

inline TextDirection RenderStyleProperties::computedDirection() const
{
    return writingMode().computedTextDirection();
}

inline StyleWritingMode RenderStyleProperties::computedWritingMode() const
{
    return writingMode().computedWritingMode();
}

inline TextOrientation RenderStyleProperties::computedTextOrientation() const
{
    return writingMode().computedTextOrientation();
}

// FIXME: Support properties where the getter returns a different value than the setter checks for equality or rename these to be used*() and generate the real getters.

inline Style::LineWidth RenderStyleProperties::borderBottomWidth() const
{
    return border().borderBottomWidth();
}

inline Style::LineWidth RenderStyleProperties::borderLeftWidth() const
{
    return border().borderLeftWidth();
}

inline Style::LineWidth RenderStyleProperties::borderRightWidth() const
{
    return border().borderRightWidth();
}

inline Style::LineWidth RenderStyleProperties::borderTopWidth() const
{
    return border().borderTopWidth();
}

inline Style::LineWidth RenderStyleProperties::columnRuleWidth() const
{
    return m_nonInheritedData->miscData->multiCol->columnRuleWidth();
}

// FIXME: Support font properties.

Style::FontSize RenderStyleProperties::computedFontSize() const
{
    return fontDescription().computedSize();
}

inline Style::FontFamilies RenderStyleProperties::fontFamily() const
{
    return { fontDescription().families(), fontDescription().isSpecifiedFont() };
}

inline Style::FontPalette RenderStyleProperties::fontPalette() const
{
    return fontDescription().fontPalette();
}

inline Style::FontSizeAdjust RenderStyleProperties::fontSizeAdjust() const
{
    return fontDescription().fontSizeAdjust();
}

inline Style::FontStyle RenderStyleProperties::fontStyle() const
{
    return { fontDescription().fontStyleSlope(), fontDescription().fontStyleAxis() };
}

#if ENABLE(VARIATION_FONTS)

inline FontOpticalSizing RenderStyleProperties::fontOpticalSizing() const
{
    return fontDescription().opticalSizing();
}

#endif

inline Style::FontFeatureSettings RenderStyleProperties::fontFeatureSettings() const
{
    return fontDescription().featureSettings();
}

#if ENABLE(VARIATION_FONTS)

inline Style::FontVariationSettings RenderStyleProperties::fontVariationSettings() const
{
    return fontDescription().variationSettings();
}

#endif

inline Style::FontWeight RenderStyleProperties::fontWeight() const
{
    return fontDescription().weight();
}

inline Style::FontWidth RenderStyleProperties::fontWidth() const
{
    return fontDescription().width();
}

inline Kerning RenderStyleProperties::fontKerning() const
{
    return fontDescription().kerning();
}

inline FontSmoothingMode RenderStyleProperties::fontSmoothing() const
{
    return fontDescription().fontSmoothing();
}

inline FontSynthesisLonghandValue RenderStyleProperties::fontSynthesisSmallCaps() const
{
    return fontDescription().fontSynthesisSmallCaps();
}

inline FontSynthesisLonghandValue RenderStyleProperties::fontSynthesisStyle() const
{
    return fontDescription().fontSynthesisStyle();
}

inline FontSynthesisLonghandValue RenderStyleProperties::fontSynthesisWeight() const
{
    return fontDescription().fontSynthesisWeight();
}

inline Style::FontVariantAlternates RenderStyleProperties::fontVariantAlternates() const
{
    return fontDescription().variantAlternates();
}

inline FontVariantCaps RenderStyleProperties::fontVariantCaps() const
{
    return fontDescription().variantCaps();
}

inline Style::FontVariantEastAsian RenderStyleProperties::fontVariantEastAsian() const
{
    return fontDescription().variantEastAsian();
}

inline FontVariantEmoji RenderStyleProperties::fontVariantEmoji() const
{
    return fontDescription().variantEmoji();
}

inline Style::FontVariantLigatures RenderStyleProperties::fontVariantLigatures() const
{
    return fontDescription().variantLigatures();
}

inline Style::FontVariantNumeric RenderStyleProperties::fontVariantNumeric() const
{
    return fontDescription().variantNumeric();
}

inline FontVariantPosition RenderStyleProperties::fontVariantPosition() const
{
    return fontDescription().variantPosition();
}

inline TextRenderingMode RenderStyleProperties::textRendering() const
{
    return fontDescription().textRenderingMode();
}

inline Style::TextAutospace RenderStyleProperties::textAutospace() const
{
    return fontDescription().textAutospace();
}

inline Style::TextSpacingTrim RenderStyleProperties::textSpacingTrim() const
{
    return fontDescription().textSpacingTrim();
}

inline Style::WebkitLocale RenderStyleProperties::locale() const
{
    return fontDescription().specifiedLocale();
}

} // namespace WebCore
