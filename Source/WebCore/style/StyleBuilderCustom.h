/*
 * Copyright (C) 2013-2014 Google Inc. All rights reserved.
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSBoxShadowPropertyValue.h"
#include "CSSCalcSymbolTable.h"
#include "CSSColorValue.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSCounterStyleRule.h"
#include "CSSCounterValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFontValue.h"
#include "CSSGradientValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "CSSRatioValue.h"
#include "CSSRectValue.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSTextShadowPropertyValue.h"
#include "CSSURLValue.h"
#include "CounterContent.h"
#include "CursorList.h"
#include "ElementAncestorIteratorInlines.h"
#include "FontVariantBuilder.h"
#include "HTMLElement.h"
#include "LocalFrame.h"
#include "SVGElement.h"
#include "StyleBoxShadow.h"
#include "StyleBuilderConverter.h"
#include "StyleBuilderStateInlines.h"
#include "StyleCachedImage.h"
#include "StyleCursorImage.h"
#include "StyleCustomPropertyData.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StyleImageSet.h"
#include "StyleRatio.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleTextShadow.h"
#include "TextSizeAdjustment.h"

namespace WebCore {
namespace Style {

#define DECLARE_PROPERTY_CUSTOM_HANDLERS(property) \
    static void applyInherit##property(BuilderState&); \
    static void applyInitial##property(BuilderState&); \
    static void applyValue##property(BuilderState&, CSSValue&)

template<typename T> inline T forwardInheritedValue(T&& value) { return std::forward<T>(value); }
inline WebCore::Color forwardInheritedValue(const WebCore::Color& value) { auto copy = value; return copy; }
inline Color forwardInheritedValue(const Color& value) { auto copy = value; return copy; }
inline WebCore::Length forwardInheritedValue(const WebCore::Length& value) { auto copy = value; return copy; }
inline LengthSize forwardInheritedValue(const LengthSize& value) { auto copy = value; return copy; }
inline LengthBox forwardInheritedValue(const LengthBox& value) { auto copy = value; return copy; }
inline GapLength forwardInheritedValue(const GapLength& value) { auto copy = value; return copy; }
inline FilterOperations forwardInheritedValue(const FilterOperations& value) { auto copy = value; return copy; }
inline TransformOperations forwardInheritedValue(const TransformOperations& value) { auto copy = value; return copy; }
inline ScrollMarginEdge forwardInheritedValue(const ScrollMarginEdge& value) { auto copy = value; return copy; }
inline ScrollPaddingEdge forwardInheritedValue(const ScrollPaddingEdge& value) { auto copy = value; return copy; }
inline MarginEdge forwardInheritedValue(const MarginEdge& value) { auto copy = value; return copy; }
inline PaddingEdge forwardInheritedValue(const PaddingEdge& value) { auto copy = value; return copy; }
inline InsetEdge forwardInheritedValue(const InsetEdge& value) { auto copy = value; return copy; }
inline PreferredSize forwardInheritedValue(const PreferredSize& value) { auto copy = value; return copy; }
inline MinimumSize forwardInheritedValue(const MinimumSize& value) { auto copy = value; return copy; }
inline MaximumSize forwardInheritedValue(const MaximumSize& value) { auto copy = value; return copy; }
inline FlexBasis forwardInheritedValue(const FlexBasis& value) { auto copy = value; return copy; }
inline DynamicRangeLimit forwardInheritedValue(const DynamicRangeLimit& value) { auto copy = value; return copy; }
inline CornerShapeValue forwardInheritedValue(const CornerShapeValue& value) { auto copy = value; return copy; }
inline URL forwardInheritedValue(const URL& value) { auto copy = value; return copy; }
inline ViewTransitionName forwardInheritedValue(const ViewTransitionName& value) { auto copy = value; return copy; }
inline FixedVector<WebCore::Length> forwardInheritedValue(const FixedVector<WebCore::Length>& value) { auto copy = value; return copy; }
inline FixedVector<BoxShadow> forwardInheritedValue(const FixedVector<BoxShadow>& value) { auto copy = value; return copy; }
inline FixedVector<TextShadow> forwardInheritedValue(const FixedVector<TextShadow>& value) { auto copy = value; return copy; }
inline FixedVector<ScopedName> forwardInheritedValue(const FixedVector<ScopedName>& value) { auto copy = value; return copy; }
inline FixedVector<PositionTryFallback> forwardInheritedValue(const FixedVector<PositionTryFallback>& value) { auto copy = value; return copy; }
inline FixedVector<Ref<ScrollTimeline>> forwardInheritedValue(const FixedVector<Ref<ScrollTimeline>>& value) { auto copy = value; return copy; }
inline FixedVector<Ref<ViewTimeline>> forwardInheritedValue(const FixedVector<Ref<ViewTimeline>>& value) { auto copy = value; return copy; }
inline FixedVector<AtomString> forwardInheritedValue(const FixedVector<AtomString>& value) { auto copy = value; return copy; }
inline FixedVector<ScrollAxis> forwardInheritedValue(const FixedVector<ScrollAxis>& value) { auto copy = value; return copy; }
inline FixedVector<ViewTimelineInsets> forwardInheritedValue(const FixedVector<ViewTimelineInsets>& value) { auto copy = value; return copy; }
inline Vector<GridTrackSize> forwardInheritedValue(const Vector<GridTrackSize>& value) { auto copy = value; return copy; }

// Note that we assume the CSS parser only allows valid CSSValue types.
class BuilderCustom {
public:
    // Custom handling of inherit, initial and value setting.
    DECLARE_PROPERTY_CUSTOM_HANDLERS(AspectRatio);
    // FIXME: <https://webkit.org/b/212506> Teach makeprop.pl to generate setters for hasExplicitlySet* flags
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderBottomLeftRadius);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderBottomRightRadius);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderTopLeftRadius);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderTopRightRadius);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageOutset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageRepeat);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageSlice);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageWidth);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BoxShadow);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CaretColor);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Clip);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Color);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(ContainIntrinsicWidth);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(ContainIntrinsicHeight);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Content);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterIncrement);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterReset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterSet);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Cursor);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Fill);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontFamily);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontSize);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontStyle);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontVariantAlternates);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontVariantLigatures);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontVariantNumeric);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontVariantEastAsian);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(GridTemplateAreas);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(LetterSpacing);
#if ENABLE(TEXT_AUTOSIZING)
    DECLARE_PROPERTY_CUSTOM_HANDLERS(LineHeight);
#endif
    DECLARE_PROPERTY_CUSTOM_HANDLERS(MaskBorderOutset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(MaskBorderRepeat);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(MaskBorderSlice);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(MaskBorderWidth);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(PaddingBottom);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(PaddingLeft);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(PaddingRight);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(PaddingTop);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(OutlineStyle);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Stroke);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(TextEmphasisStyle);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(TextIndent);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(TextShadow);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitBoxShadow);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Zoom);

    // Custom handling of inherit + value setting only.
    static void applyInheritVerticalAlign(BuilderState&);
    static void applyValueVerticalAlign(BuilderState&, CSSValue&);
    static void applyInheritBaselineShift(BuilderState&);
    static void applyValueBaselineShift(BuilderState&, CSSValue&);

    // Custom handling of inherit setting only.
    static void applyInheritWordSpacing(BuilderState&);

    // Custom handling of value setting only.
    static void applyValueDirection(BuilderState&, CSSValue&);
    static void applyValueWebkitLocale(BuilderState&, CSSValue&);
    static void applyValueTextOrientation(BuilderState&, CSSValue&);
#if ENABLE(TEXT_AUTOSIZING)
    static void applyValueWebkitTextSizeAdjust(BuilderState&, CSSValue&);
#endif
    static void applyValueWebkitTextZoom(BuilderState&, CSSValue&);
    static void applyValueWritingMode(BuilderState&, CSSValue&);
    static void applyValueFontSizeAdjust(BuilderState&, CSSValue&);

#if ENABLE(DARK_MODE_CSS)
    static void applyValueColorScheme(BuilderState&, CSSValue&);
#endif

    static void applyValueStrokeWidth(BuilderState&, CSSValue&);
    static void applyValueStrokeColor(BuilderState&, CSSValue&);

private:
    static void resetUsedZoom(BuilderState&);

    enum CounterBehavior { Increment, Reset, Set };
    template <CounterBehavior counterBehavior>
    static void applyInheritCounter(BuilderState&);
    template <CounterBehavior counterBehavior>
    static void applyValueCounter(BuilderState&, CSSValue&);

    static float largerFontSize(float size);
    static float smallerFontSize(float size);
    static float determineRubyTextSizeMultiplier(BuilderState&);
};

inline void BuilderCustom::applyValueDirection(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setDirection(fromCSSValue<TextDirection>(value));
    builderState.style().setHasExplicitlySetDirection();
}

inline void BuilderCustom::resetUsedZoom(BuilderState& builderState)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    builderState.setUsedZoom(builderState.parentStyle().usedZoom());
}

inline void BuilderCustom::applyInitialZoom(BuilderState& builderState)
{
    resetUsedZoom(builderState);
    builderState.setZoom(RenderStyle::initialZoom());
}

inline void BuilderCustom::applyInheritZoom(BuilderState& builderState)
{
    resetUsedZoom(builderState);
    builderState.setZoom(forwardInheritedValue(builderState.parentStyle().zoom()));
}

inline void BuilderCustom::applyValueZoom(BuilderState& builderState, CSSValue& value)
{
    auto primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueNormal) {
        resetUsedZoom(builderState);
        builderState.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue->isPercentage()) {
        resetUsedZoom(builderState);
        if (float percent = primitiveValue->resolveAsPercentage<float>(builderState.cssToLengthConversionData()))
            builderState.setZoom(percent / 100.0f);
    } else if (primitiveValue->isNumber()) {
        resetUsedZoom(builderState);
        if (float number = primitiveValue->resolveAsNumber<float>(builderState.cssToLengthConversionData()))
            builderState.setZoom(number);
    }
}

inline void BuilderCustom::applyInheritVerticalAlign(BuilderState& builderState)
{
    builderState.style().setVerticalAlignLength(forwardInheritedValue(builderState.parentStyle().verticalAlignLength()));
    builderState.style().setVerticalAlign(forwardInheritedValue(builderState.parentStyle().verticalAlign()));
}

inline void BuilderCustom::applyValueVerticalAlign(BuilderState& builderState, CSSValue& value)
{
    auto primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() != CSSValueInvalid)
        builderState.style().setVerticalAlign(fromCSSValueID<VerticalAlign>(primitiveValue->valueID()));
    else
        builderState.style().setVerticalAlignLength(primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData()));
}

inline void BuilderCustom::applyInheritTextIndent(BuilderState& builderState)
{
    builderState.style().setTextIndent(forwardInheritedValue(builderState.parentStyle().textIndent()));
    builderState.style().setTextIndentLine(forwardInheritedValue(builderState.parentStyle().textIndentLine()));
    builderState.style().setTextIndentType(forwardInheritedValue(builderState.parentStyle().textIndentType()));
}

inline void BuilderCustom::applyInitialTextIndent(BuilderState& builderState)
{
    builderState.style().setTextIndent(RenderStyle::initialTextIndent());
    builderState.style().setTextIndentLine(RenderStyle::initialTextIndentLine());
    builderState.style().setTextIndentType(RenderStyle::initialTextIndentType());
}

inline void BuilderCustom::applyValueTextIndent(BuilderState& builderState, CSSValue& value)
{
    WebCore::Length lengthPercentageValue;
    TextIndentLine textIndentLineValue = RenderStyle::initialTextIndentLine();
    TextIndentType textIndentTypeValue = RenderStyle::initialTextIndentType();

    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        // Values coming from CSSTypedOM didn't go through the parser and may not have been converted to a CSSValueList.
        lengthPercentageValue = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData());
    } else {
        auto list = BuilderConverter::requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
        if (!list)
            return;

        for (auto& primitiveValue : *list) {
            if (!primitiveValue.valueID())
                lengthPercentageValue = primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(builderState.cssToLengthConversionData());
            else if (primitiveValue.valueID() == CSSValueEachLine)
                textIndentLineValue = TextIndentLine::EachLine;
            else if (primitiveValue.valueID() == CSSValueHanging)
                textIndentTypeValue = TextIndentType::Hanging;
        }
    }

    if (lengthPercentageValue.isUndefined())
        return;

    builderState.style().setTextIndent(WTFMove(lengthPercentageValue));
    builderState.style().setTextIndentLine(textIndentLineValue);
    builderState.style().setTextIndentType(textIndentTypeValue);
}

enum BorderImageType { BorderImage, MaskBorder };
enum BorderImageModifierType { Outset, Repeat, Slice, Width };
template<BorderImageType type, BorderImageModifierType modifier>
class ApplyPropertyBorderImageModifier {
public:
    static void applyInheritValue(BuilderState& builderState)
    {
        NinePieceImage image(getValue(builderState.style()));
        switch (modifier) {
        case Outset:
            image.copyOutsetFrom(getValue(builderState.parentStyle()));
            break;
        case Repeat:
            image.copyRepeatFrom(getValue(builderState.parentStyle()));
            break;
        case Slice:
            image.copyImageSlicesFrom(getValue(builderState.parentStyle()));
            break;
        case Width:
            image.copyBorderSlicesFrom(getValue(builderState.parentStyle()));
            break;
        }
        setValue(builderState.style(), image);
    }

    static void applyInitialValue(BuilderState& builderState)
    {
        NinePieceImage image(getValue(builderState.style()));
        switch (modifier) {
        case Outset:
            image.setOutset(LengthBox(LengthType::Relative));
            break;
        case Repeat:
            image.setHorizontalRule(NinePieceImageRule::Stretch);
            image.setVerticalRule(NinePieceImageRule::Stretch);
            break;
        case Slice:
            // Masks have a different initial value for slices. Preserve the value of "0 fill" for backwards compatibility.
            image.setImageSlices(type == BorderImage ? LengthBox(WebCore::Length(100, LengthType::Percent), WebCore::Length(100, LengthType::Percent), WebCore::Length(100, LengthType::Percent), WebCore::Length(100, LengthType::Percent)) : LengthBox(LengthType::Fixed));
            image.setFill(false);
            break;
        case Width:
            // FIXME: This is a local variable to work around a bug in the GCC 8.1 Address Sanitizer.
            // Might be slightly less efficient when the type is not BorderImage since this is unused in that case.
            // Should be switched back to a temporary when possible. See https://webkit.org/b/186980
            LengthBox lengthBox(WebCore::Length(1, LengthType::Relative), WebCore::Length(1, LengthType::Relative), WebCore::Length(1, LengthType::Relative), WebCore::Length(1, LengthType::Relative));
            // Masks have a different initial value for widths. They use an 'auto' value rather than trying to fit to the border.
            image.setBorderSlices(type == BorderImage ? lengthBox : LengthBox());
            image.setOverridesBorderWidths(false);
            break;
        }
        setValue(builderState.style(), image);
    }

    static void applyValue(BuilderState& builderState, CSSValue& value)
    {
        NinePieceImage image(getValue(builderState.style()));
        switch (modifier) {
        case Outset:
            image.setOutset(builderState.styleMap().mapNinePieceImageQuad(value));
            break;
        case Repeat:
            builderState.styleMap().mapNinePieceImageRepeat(value, image);
            break;
        case Slice:
            builderState.styleMap().mapNinePieceImageSlice(value, image);
            break;
        case Width:
            builderState.styleMap().mapNinePieceImageWidth(value, image);
            break;
        }
        setValue(builderState.style(), image);
    }

private:
    static const NinePieceImage& getValue(const RenderStyle& style)
    {
        return type == BorderImage ? style.borderImage() : style.maskBorder();
    }

    static void setValue(RenderStyle& style, const NinePieceImage& value)
    {
        return type == BorderImage ? style.setBorderImage(value) : style.setMaskBorder(value);
    }
};

#define DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(type, modifier) \
inline void BuilderCustom::applyInherit##type##modifier(BuilderState& builderState) \
{ \
    ApplyPropertyBorderImageModifier<type, modifier>::applyInheritValue(builderState); \
} \
inline void BuilderCustom::applyInitial##type##modifier(BuilderState& builderState) \
{ \
    ApplyPropertyBorderImageModifier<type, modifier>::applyInitialValue(builderState); \
} \
inline void BuilderCustom::applyValue##type##modifier(BuilderState& builderState, CSSValue& value) \
{ \
    ApplyPropertyBorderImageModifier<type, modifier>::applyValue(builderState, value); \
}

DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Outset)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Repeat)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Slice)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Width)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(MaskBorder, Outset)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(MaskBorder, Repeat)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(MaskBorder, Slice)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(MaskBorder, Width)

inline void BuilderCustom::applyInheritWordSpacing(BuilderState& builderState)
{
    builderState.style().setWordSpacing(forwardInheritedValue(builderState.parentStyle().computedWordSpacing()));
}

inline void BuilderCustom::applyInheritLetterSpacing(BuilderState& builderState)
{
    builderState.style().setLetterSpacing(forwardInheritedValue(builderState.parentStyle().computedLetterSpacing()));
}

inline void BuilderCustom::applyInitialLetterSpacing(BuilderState& builderState)
{
    builderState.style().setLetterSpacing(RenderStyle::initialLetterSpacing());
}

void maybeUpdateFontForLetterSpacing(BuilderState& builderState, CSSValue& value)
{
    // This is unfortunate. It's related to https://github.com/w3c/csswg-drafts/issues/5498.
    //
    // From StyleBuilder's point of view, there's a dependency cycle:
    // letter-spacing accepts an arbitrary <length>, which must be resolved against a font, which must
    // be selected after all the properties that affect font selection are processed, but letter-spacing
    // itself affects font selection because it can disable font features. StyleBuilder has some (valid)
    // ASSERT()s which would fire because of this cycle.
    //
    // There isn't *actually* a dependency cycle, though, as none of the font-relative units are
    // actually sensitive to font features (luckily). The problem is that our StyleBuilder is only
    // smart enough to consider fonts as one indivisible thing, rather than having the deeper
    // understanding that different parts of fonts may or may not depend on each other.
    //
    // So, we update the font early here, so that if there is a font-relative unit inside the CSSValue,
    // its font is updated and ready to go. In the worst case there might be a second call to
    // updateFont() later, but that isn't bad for perf because 1. It only happens twice if there is
    // actually a font-relative unit passed to letter-spacing, and 2. updateFont() internally has logic
    // to only do work if the font is actually dirty.

    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->isFontRelativeLength() || primitiveValue->isCalculated())
            builderState.updateFont();
    }
}

inline void BuilderCustom::applyValueLetterSpacing(BuilderState& builderState, CSSValue& value)
{
    maybeUpdateFontForLetterSpacing(builderState, value);
    builderState.style().setLetterSpacing(BuilderConverter::convertTextLengthOrNormal(builderState, value));
}

#if ENABLE(TEXT_AUTOSIZING)

inline void BuilderCustom::applyInheritLineHeight(BuilderState& builderState)
{
    builderState.style().setLineHeight(forwardInheritedValue(builderState.parentStyle().lineHeight()));
    builderState.style().setSpecifiedLineHeight(forwardInheritedValue(builderState.parentStyle().specifiedLineHeight()));
}

inline void BuilderCustom::applyInitialLineHeight(BuilderState& builderState)
{
    builderState.style().setLineHeight(RenderStyle::initialLineHeight());
    builderState.style().setSpecifiedLineHeight(RenderStyle::initialSpecifiedLineHeight());
}

static inline float computeBaseSpecifiedFontSize(const Document& document, const RenderStyle& style, bool percentageAutosizingEnabled)
{
    float result = style.specifiedFontSize();
    auto* frame = document.frame();
    if (frame && style.textZoom() != TextZoom::Reset)
        result *= frame->textZoomFactor();
    result *= style.usedZoom();
    if (percentageAutosizingEnabled
        && (!document.settings().textAutosizingUsesIdempotentMode() || document.settings().idempotentModeAutosizingOnlyHonorsPercentages()))
        result *= style.textSizeAdjust().multiplier();
    return result;
}

static inline float computeLineHeightMultiplierDueToFontSize(const Document& document, const RenderStyle& style, const CSSPrimitiveValue& value)
{
    bool percentageAutosizingEnabled = document.settings().textAutosizingEnabled() && style.textSizeAdjust().isPercentage();

    if (value.isLength()) {
        auto minimumFontSize = document.settings().minimumFontSize();
        if (minimumFontSize > 0) {
            auto specifiedFontSize = computeBaseSpecifiedFontSize(document, style, percentageAutosizingEnabled);
            // Small font sizes cause a preposterously large (near infinity) line-height. Add a fuzz-factor of 1px which opts out of
            // boosted line-height.
            if (specifiedFontSize < minimumFontSize && specifiedFontSize >= 1) {
                // FIXME: There are two settings which are relevant here: minimum font size, and minimum logical font size (as
                // well as things like the zoom property, text zoom on the page, and text autosizing). The minimum logical font
                // size is nonzero by default, and already incorporated into the computed font size, so if we just use the ratio
                // of the computed : specified font size, it will be > 1 in the cases where the minimum logical font size kicks
                // in. In general, this is the right thing to do, however, this kind of blanket change is too risky to perform
                // right now. https://bugs.webkit.org/show_bug.cgi?id=174570 tracks turning this on. For now, we can just pretend
                // that the minimum font size is the only thing affecting the computed font size.

                // This calculation matches the line-height computed size calculation in
                // TextAutoSizing::Value::adjustTextNodeSizes().
                auto scaleChange = minimumFontSize / specifiedFontSize;
                return scaleChange;
            }
        }
    }

    if (percentageAutosizingEnabled && !document.settings().textAutosizingUsesIdempotentMode())
        return style.textSizeAdjust().multiplier();
    return 1;
}

inline void BuilderCustom::applyValueLineHeight(BuilderState& builderState, CSSValue& value)
{
    if (CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID())) {
        applyInitialLineHeight(builderState);
        return;
    }

    auto lineHeight = BuilderConverter::convertLineHeight(builderState, value, 1);

    WebCore::Length computedLineHeight;
    if (lineHeight.isNormal())
        computedLineHeight = lineHeight;
    else {
        auto primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
        if (!primitiveValue)
            return;
        auto multiplier = computeLineHeightMultiplierDueToFontSize(builderState.document(), builderState.style(), *primitiveValue);
        if (multiplier == 1)
            computedLineHeight = lineHeight;
        else
            computedLineHeight = BuilderConverter::convertLineHeight(builderState, value, multiplier);
    }

    builderState.style().setLineHeight(WTFMove(computedLineHeight));
    builderState.style().setSpecifiedLineHeight(WTFMove(lineHeight));
}

#endif

inline void BuilderCustom::applyInheritOutlineStyle(BuilderState& builderState)
{
    if (builderState.parentStyle().hasAutoOutlineStyle())
        builderState.style().setHasAutoOutlineStyle();
    else
        builderState.style().setOutlineStyle(forwardInheritedValue(builderState.parentStyle().outlineStyle()));
}

inline void BuilderCustom::applyInitialOutlineStyle(BuilderState& builderState)
{
    builderState.style().setOutlineStyle(RenderStyle::initialBorderStyle());
}

inline void BuilderCustom::applyValueOutlineStyle(BuilderState& builderState, CSSValue& value)
{
    if (value.valueID() == CSSValueAuto)
        builderState.style().setHasAutoOutlineStyle();
    else
        builderState.style().setOutlineStyle(fromCSSValue<BorderStyle>(value));
}

inline void BuilderCustom::applyInitialCaretColor(BuilderState& builderState)
{
    if (builderState.applyPropertyToRegularStyle())
        builderState.style().setHasAutoCaretColor();
    if (builderState.applyPropertyToVisitedLinkStyle())
        builderState.style().setHasVisitedLinkAutoCaretColor();
}

inline void BuilderCustom::applyInheritCaretColor(BuilderState& builderState)
{
    auto& color = builderState.parentStyle().caretColor();
    if (builderState.applyPropertyToRegularStyle()) {
        if (builderState.parentStyle().hasAutoCaretColor())
            builderState.style().setHasAutoCaretColor();
        else
            builderState.style().setCaretColor(forwardInheritedValue(color));
    }
    if (builderState.applyPropertyToVisitedLinkStyle()) {
        if (builderState.parentStyle().hasVisitedLinkAutoCaretColor())
            builderState.style().setHasVisitedLinkAutoCaretColor();
        else
            builderState.style().setVisitedLinkCaretColor(forwardInheritedValue(color));
    }
}

inline void BuilderCustom::applyValueCaretColor(BuilderState& builderState, CSSValue& value)
{
    if (builderState.applyPropertyToRegularStyle()) {
        if (value.valueID() == CSSValueAuto)
            builderState.style().setHasAutoCaretColor();
        else
            builderState.style().setCaretColor(builderState.createStyleColor(value, ForVisitedLink::No));
    }
    if (builderState.applyPropertyToVisitedLinkStyle()) {
        if (value.valueID() == CSSValueAuto)
            builderState.style().setHasVisitedLinkAutoCaretColor();
        else
            builderState.style().setVisitedLinkCaretColor(builderState.createStyleColor(value, ForVisitedLink::Yes));
    }
}

inline void BuilderCustom::applyInitialClip(BuilderState& builderState)
{
    builderState.style().setClip(WebCore::Length(), WebCore::Length(), WebCore::Length(), WebCore::Length());
    builderState.style().setHasClip(false);
}

inline void BuilderCustom::applyInheritClip(BuilderState& builderState)
{
    auto& parentStyle = builderState.parentStyle();
    if (!parentStyle.hasClip())
        return applyInitialClip(builderState);
    builderState.style().setClip(WebCore::Length { parentStyle.clipTop() }, WebCore::Length { parentStyle.clipRight() },
        WebCore::Length { parentStyle.clipBottom() }, WebCore::Length { parentStyle.clipLeft() });
    builderState.style().setHasClip(true);
}

inline void BuilderCustom::applyValueClip(BuilderState& builderState, CSSValue& value)
{
    if (value.isRect()) {
        auto& conversionData = builderState.cssToLengthConversionData();
        auto primitiveValueTop = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value.rect().top());
        if (!primitiveValueTop)
            return;
        auto primitiveValueRight = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value.rect().right());
        if (!primitiveValueRight)
            return;
        auto primitiveValueBottom = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value.rect().bottom());
        if (!primitiveValueBottom)
            return;
        auto primitiveValueLeft = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value.rect().left());
        if (!primitiveValueLeft)
            return;

        auto top = primitiveValueTop->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        auto right = primitiveValueRight->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        auto bottom = primitiveValueBottom->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        auto left = primitiveValueLeft->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);

        builderState.style().setClip(WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left));
        builderState.style().setHasClip(true);
    } else {
        ASSERT(value.valueID() == CSSValueAuto);
        applyInitialClip(builderState);
    }
}

inline void BuilderCustom::applyValueWebkitLocale(BuilderState& builderState, CSSValue& value)
{
    auto primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueAuto)
        builderState.setFontDescriptionSpecifiedLocale(nullAtom());
    else
        builderState.setFontDescriptionSpecifiedLocale(AtomString { primitiveValue->stringValue() });
}

inline void BuilderCustom::applyValueWritingMode(BuilderState& builderState, CSSValue& value)
{
    builderState.setWritingMode(fromCSSValue<StyleWritingMode>(value));
    builderState.style().setHasExplicitlySetWritingMode();
}

inline void BuilderCustom::applyValueTextOrientation(BuilderState& builderState, CSSValue& value)
{
    builderState.setTextOrientation(fromCSSValue<TextOrientation>(value));
}

#if ENABLE(TEXT_AUTOSIZING)
inline void BuilderCustom::applyValueWebkitTextSizeAdjust(BuilderState& builderState, CSSValue& value)
{
    auto primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueAuto)
        builderState.style().setTextSizeAdjust(TextSizeAdjustment::autoAdjust());
    else if (primitiveValue->valueID() == CSSValueNone)
        builderState.style().setTextSizeAdjust(TextSizeAdjustment::none());
    else
        builderState.style().setTextSizeAdjust(TextSizeAdjustment(primitiveValue->resolveAsPercentage<float>(builderState.cssToLengthConversionData())));

    builderState.setFontDirty();
}
#endif

inline void BuilderCustom::applyValueWebkitTextZoom(BuilderState& builderState, CSSValue& value)
{
    auto primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueNormal)
        builderState.style().setTextZoom(TextZoom::Normal);
    else if (primitiveValue->valueID() == CSSValueReset)
        builderState.style().setTextZoom(TextZoom::Reset);
    builderState.setFontDirty();
}

#if ENABLE(DARK_MODE_CSS)
inline void BuilderCustom::applyValueColorScheme(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setColorScheme(BuilderConverter::convertColorScheme(builderState, value));
    builderState.style().setHasExplicitlySetColorScheme();
}
#endif

inline void BuilderCustom::applyInitialTextShadow(BuilderState& builderState)
{
    builderState.style().setTextShadow({ });
}

inline void BuilderCustom::applyInheritTextShadow(BuilderState& builderState)
{
    builderState.style().setTextShadow(forwardInheritedValue(builderState.parentStyle().textShadow()));
}

inline void BuilderCustom::applyValueTextShadow(BuilderState& builderState, CSSValue& value)
{
    if (RefPtr primitive = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT(primitive->valueID() == CSSValueNone);
        builderState.style().setTextShadow({ });
        return;
    }

    RefPtr shadow = BuilderConverter::requiredDowncast<CSSTextShadowPropertyValue>(builderState, value);
    if (!shadow)
        return;

    WTF::switchOn(shadow->shadow(),
        [&](CSS::Keyword::None) {
            builderState.style().setTextShadow({ });
        },
        [&](const auto& list) {
            builderState.style().setTextShadow(
                FixedVector<TextShadow>::createWithSizeFromGenerator(list.size(), [&](auto index) {
                    return Style::toStyle(list[list.size() - index - 1], builderState);
                })
            );
        }
    );
}

inline void BuilderCustom::applyInitialBoxShadow(BuilderState& builderState)
{
    builderState.style().setBoxShadow({ });
}

inline void BuilderCustom::applyInheritBoxShadow(BuilderState& builderState)
{
    builderState.style().setBoxShadow(forwardInheritedValue(builderState.parentStyle().boxShadow()));
}

inline void BuilderCustom::applyValueBoxShadow(BuilderState& builderState, CSSValue& value)
{
    if (RefPtr primitive = dynamicDowncast<CSSPrimitiveValue>(value)) {
        ASSERT(primitive->valueID() == CSSValueNone);
        builderState.style().setBoxShadow({ });
        return;
    }

    RefPtr shadow = BuilderConverter::requiredDowncast<CSSBoxShadowPropertyValue>(builderState, value);
    if (!shadow)
        return;

    WTF::switchOn(shadow->shadow(),
        [&](CSS::Keyword::None) {
            builderState.style().setBoxShadow({ });
        },
        [&](const auto& list) {
            builderState.style().setBoxShadow(
                FixedVector<BoxShadow>::createWithSizeFromGenerator(list.size(), [&](auto index) {
                    return Style::toStyle(list[list.size() - index - 1], builderState);
                })
            );
        }
    );
}

inline void BuilderCustom::applyInitialFontFamily(BuilderState& builderState)
{
    auto& fontDescription = builderState.fontDescription();
    auto initialDesc = FontCascadeDescription();

    // We need to adjust the size to account for the generic family change from monospace to non-monospace.
    if (fontDescription.useFixedDefaultSize()) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier())
            builderState.setFontDescriptionFontSize(Style::fontSizeForKeyword(sizeIdentifier, false, builderState.document()));
    }
    if (!initialDesc.firstFamily().isEmpty())
        builderState.setFontDescriptionFamilies(initialDesc.families());
}

inline void BuilderCustom::applyInheritFontFamily(BuilderState& builderState)
{
    auto parentFontDescription = builderState.parentStyle().fontDescription();

    builderState.setFontDescriptionFamilies(parentFontDescription.families());
    builderState.setFontDescriptionIsSpecifiedFont(parentFontDescription.isSpecifiedFont());
}

inline void BuilderCustom::applyValueFontFamily(BuilderState& builderState, CSSValue& value)
{
    auto& fontDescription = builderState.fontDescription();
    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();

    Vector<AtomString> families;

    if (is<CSSPrimitiveValue>(value)) {
        auto valueID = value.valueID();
        if (!CSSPropertyParserHelpers::isSystemFontShorthand(valueID)) {
            // Early return if the invalid CSSValueID is set while using CSSOM API.
            return;
        }
        AtomString family = SystemFontDatabase::singleton().systemFontShorthandFamily(CSSPropertyParserHelpers::lowerFontShorthand(valueID));
        ASSERT(!family.isEmpty());
        builderState.setFontDescriptionIsSpecifiedFont(false);
        families = Vector<AtomString>::from(WTFMove(family));
    } else {
        auto valueList = BuilderConverter::requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, value);
        if (!valueList)
            return;

        bool isFirstFont = true;
        families = WTF::compactMap(*valueList, [&](auto& contentValue) -> std::optional<AtomString> {
            AtomString family;
            bool isGenericFamily = false;
            if (contentValue.isFontFamily())
                family = AtomString { contentValue.stringValue() };
            else if (contentValue.valueID() == CSSValueWebkitBody)
                family = AtomString { builderState.document().settings().standardFontFamily() };
            else {
                isGenericFamily = true;
                family = CSSPropertyParserHelpers::genericFontFamily(contentValue.valueID());
                ASSERT(!family.isEmpty());
            }
            if (family.isNull())
                return std::nullopt;
            if (isFirstFont) {
                builderState.setFontDescriptionIsSpecifiedFont(!isGenericFamily);
                isFirstFont = false;
            }
            return family;
        });
        if (families.isEmpty())
            return;
    }

    builderState.setFontDescriptionFamilies(families);

    if (fontDescription.useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier())
            builderState.setFontDescriptionFontSize(Style::fontSizeForKeyword(sizeIdentifier, !oldFamilyUsedFixedDefaultSize, builderState.document()));
    }
}

inline void BuilderCustom::applyInitialBorderBottomLeftRadius(BuilderState& builderState)
{
    builderState.style().setBorderBottomLeftRadius(RenderStyle::initialBorderRadius());
    builderState.style().setHasExplicitlySetBorderBottomLeftRadius(false);
}

inline void BuilderCustom::applyInheritBorderBottomLeftRadius(BuilderState& builderState)
{
    builderState.style().setBorderBottomLeftRadius(forwardInheritedValue(builderState.parentStyle().borderBottomLeftRadius()));
    builderState.style().setHasExplicitlySetBorderBottomLeftRadius(builderState.parentStyle().hasExplicitlySetBorderBottomLeftRadius());
}

inline void BuilderCustom::applyValueBorderBottomLeftRadius(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setBorderBottomLeftRadius(BuilderConverter::convertRadius(builderState, value));
    builderState.style().setHasExplicitlySetBorderBottomLeftRadius(true);
}

inline void BuilderCustom::applyInitialBorderBottomRightRadius(BuilderState& builderState)
{
    builderState.style().setBorderBottomRightRadius(RenderStyle::initialBorderRadius());
    builderState.style().setHasExplicitlySetBorderBottomRightRadius(false);
}

inline void BuilderCustom::applyInheritBorderBottomRightRadius(BuilderState& builderState)
{
    builderState.style().setBorderBottomRightRadius(forwardInheritedValue(builderState.parentStyle().borderBottomRightRadius()));
    builderState.style().setHasExplicitlySetBorderBottomRightRadius(builderState.parentStyle().hasExplicitlySetBorderBottomRightRadius());
}

inline void BuilderCustom::applyValueBorderBottomRightRadius(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setBorderBottomRightRadius(BuilderConverter::convertRadius(builderState, value));
    builderState.style().setHasExplicitlySetBorderBottomRightRadius(true);
}

inline void BuilderCustom::applyInitialBorderTopLeftRadius(BuilderState& builderState)
{
    builderState.style().setBorderTopLeftRadius(RenderStyle::initialBorderRadius());
    builderState.style().setHasExplicitlySetBorderTopLeftRadius(false);
}

inline void BuilderCustom::applyInheritBorderTopLeftRadius(BuilderState& builderState)
{
    builderState.style().setBorderTopLeftRadius(forwardInheritedValue(builderState.parentStyle().borderTopLeftRadius()));
    builderState.style().setHasExplicitlySetBorderTopLeftRadius(builderState.parentStyle().hasExplicitlySetBorderTopLeftRadius());
}

inline void BuilderCustom::applyValueBorderTopLeftRadius(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setBorderTopLeftRadius(BuilderConverter::convertRadius(builderState, value));
    builderState.style().setHasExplicitlySetBorderTopLeftRadius(true);
}

inline void BuilderCustom::applyInitialBorderTopRightRadius(BuilderState& builderState)
{
    builderState.style().setBorderTopRightRadius(RenderStyle::initialBorderRadius());
    builderState.style().setHasExplicitlySetBorderTopRightRadius(false);
}

inline void BuilderCustom::applyInheritBorderTopRightRadius(BuilderState& builderState)
{
    builderState.style().setBorderTopRightRadius(forwardInheritedValue(builderState.parentStyle().borderTopRightRadius()));
    builderState.style().setHasExplicitlySetBorderTopRightRadius(builderState.parentStyle().hasExplicitlySetBorderTopRightRadius());
}

inline void BuilderCustom::applyValueBorderTopRightRadius(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setBorderTopRightRadius(BuilderConverter::convertRadius(builderState, value));
    builderState.style().setHasExplicitlySetBorderTopRightRadius(true);
}

inline void BuilderCustom::applyInheritBaselineShift(BuilderState& builderState)
{
    auto& svgStyle = builderState.style().accessSVGStyle();
    auto& svgParentStyle = builderState.parentStyle().svgStyle();
    svgStyle.setBaselineShift(forwardInheritedValue(svgParentStyle.baselineShift()));
    svgStyle.setBaselineShiftValue(forwardInheritedValue(svgParentStyle.baselineShiftValue()));
}

inline void BuilderCustom::applyValueBaselineShift(BuilderState& builderState, CSSValue& value)
{
    auto& svgStyle = builderState.style().accessSVGStyle();
    auto primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->isValueID()) {
        switch (primitiveValue->valueID()) {
        case CSSValueBaseline:
            svgStyle.setBaselineShift(BaselineShift::Baseline);
            break;
        case CSSValueSub:
            svgStyle.setBaselineShift(BaselineShift::Sub);
            break;
        case CSSValueSuper:
            svgStyle.setBaselineShift(BaselineShift::Super);
            break;
        default:
            break;
        }
    } else {
        svgStyle.setBaselineShift(BaselineShift::Length);
        svgStyle.setBaselineShiftValue(BuilderConverter::convertLength(builderState, *primitiveValue));
    }
}

inline void BuilderCustom::applyInitialTextEmphasisStyle(BuilderState& builderState)
{
    builderState.style().setTextEmphasisFill(RenderStyle::initialTextEmphasisFill());
    builderState.style().setTextEmphasisMark(RenderStyle::initialTextEmphasisMark());
    builderState.style().setTextEmphasisCustomMark(RenderStyle::initialTextEmphasisCustomMark());
}

inline void BuilderCustom::applyInheritTextEmphasisStyle(BuilderState& builderState)
{
    builderState.style().setTextEmphasisFill(forwardInheritedValue(builderState.parentStyle().textEmphasisFill()));
    builderState.style().setTextEmphasisMark(forwardInheritedValue(builderState.parentStyle().textEmphasisMark()));
    builderState.style().setTextEmphasisCustomMark(forwardInheritedValue(builderState.parentStyle().textEmphasisCustomMark()));
}

inline void BuilderCustom::applyInitialAspectRatio(BuilderState& builderState)
{
    builderState.style().setAspectRatioType(RenderStyle::initialAspectRatioType());
    builderState.style().setAspectRatio(RenderStyle::initialAspectRatioWidth(), RenderStyle::initialAspectRatioHeight());
}

inline void BuilderCustom::applyInheritAspectRatio(BuilderState&)
{
}

inline void BuilderCustom::applyValueAspectRatio(BuilderState& builderState, CSSValue& value)
{
    auto resolveRatio = [&](const CSSRatioValue& ratioValue) -> std::pair<double, double> {
        auto styleRatio = Style::toStyle(ratioValue.ratio(), builderState);
        return { styleRatio.numerator.value, styleRatio.denominator.value };
    };

    if (value.valueID() == CSSValueAuto) {
        builderState.style().setAspectRatioType(AspectRatioType::Auto);
        return;
    }
    if (RefPtr ratio = dynamicDowncast<CSSRatioValue>(value)) {
        auto [width, height] = resolveRatio(*ratio);
        if (!width || !height)
            builderState.style().setAspectRatioType(AspectRatioType::AutoZero);
        else
            builderState.style().setAspectRatioType(AspectRatioType::Ratio);
        builderState.style().setAspectRatio(width, height);
        return;
    }

    auto list = BuilderConverter::requiredListDowncast<CSSValueList, CSSValue, 2>(builderState, value);
    if (!list)
        return;

    auto ratio = BuilderConverter::requiredDowncast<CSSRatioValue>(builderState, list->item(1));
    if (!ratio)
        return;
    auto [width, height] = resolveRatio(*ratio);
    builderState.style().setAspectRatioType(AspectRatioType::AutoAndRatio);
    builderState.style().setAspectRatio(width, height);
}

inline void BuilderCustom::applyValueTextEmphasisStyle(BuilderState& builderState, CSSValue& value)
{
    if (auto* list = dynamicDowncast<CSSValueList>(value)) {
        ASSERT(list->length() == 2);

        for (auto& item : *list) {
            auto valueID = item.valueID();
            if (valueID == CSSValueFilled || valueID == CSSValueOpen)
                builderState.style().setTextEmphasisFill(fromCSSValueID<TextEmphasisFill>(valueID));
            else
                builderState.style().setTextEmphasisMark(fromCSSValueID<TextEmphasisMark>(valueID));
        }
        builderState.style().setTextEmphasisCustomMark(nullAtom());
        return;
    }

    auto primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    if (primitiveValue->isString()) {
        builderState.style().setTextEmphasisFill(TextEmphasisFill::Filled);
        builderState.style().setTextEmphasisMark(TextEmphasisMark::Custom);
        builderState.style().setTextEmphasisCustomMark(AtomString { primitiveValue->stringValue() });
        return;
    }

    builderState.style().setTextEmphasisCustomMark(nullAtom());

    if (primitiveValue->valueID() == CSSValueFilled || primitiveValue->valueID() == CSSValueOpen) {
        builderState.style().setTextEmphasisFill(fromCSSValue<TextEmphasisFill>(value));
        builderState.style().setTextEmphasisMark(TextEmphasisMark::Auto);
    } else {
        builderState.style().setTextEmphasisFill(TextEmphasisFill::Filled);
        builderState.style().setTextEmphasisMark(fromCSSValue<TextEmphasisMark>(value));
    }
}

template <BuilderCustom::CounterBehavior counterBehavior>
inline void BuilderCustom::applyInheritCounter(BuilderState& builderState)
{
    auto& map = builderState.style().accessCounterDirectives().map;
    for (auto& keyValue : builderState.parentStyle().counterDirectives().map) {
        auto& directives = map.add(keyValue.key, CounterDirectives { }).iterator->value;
        if (counterBehavior == Reset)
            directives.resetValue = keyValue.value.resetValue;
        else if (counterBehavior == Increment)
            directives.incrementValue = keyValue.value.incrementValue;
        else
            directives.setValue = keyValue.value.setValue;
    }
}

template <BuilderCustom::CounterBehavior counterBehavior>
inline void BuilderCustom::applyValueCounter(BuilderState& builderState, CSSValue& value)
{
    bool setCounterIncrementToNone = counterBehavior == Increment && value.valueID() == CSSValueNone;

    if (!is<CSSValueList>(value) && !setCounterIncrementToNone)
        return;

    auto& map = builderState.style().accessCounterDirectives().map;
    for (auto& keyValue : map) {
        if (counterBehavior == Reset)
            keyValue.value.resetValue = std::nullopt;
        else if (counterBehavior == Increment)
            keyValue.value.incrementValue = std::nullopt;
        else
            keyValue.value.setValue = std::nullopt;
    }

    if (setCounterIncrementToNone)
        return;

    auto& conversionData = builderState.cssToLengthConversionData();

    auto list = BuilderConverter::requiredListDowncast<CSSValueList, CSSValuePair>(builderState, value);
    if (!list)
        return;

    for (auto& pairValue : *list) {
        auto pair = BuilderConverter::requiredPairDowncast<CSSPrimitiveValue>(builderState, pairValue);
        if (!pair)
            return;
        AtomString identifier { pair->first.stringValue() };
        int value =  pair->second.resolveAsNumber<int>(conversionData);
        auto& directives = map.add(identifier, CounterDirectives { }).iterator->value;
        if (counterBehavior == Reset)
            directives.resetValue = value;
        else if (counterBehavior == Increment)
            directives.incrementValue = saturatedSum(directives.incrementValue.value_or(0), value);
        else
            directives.setValue = value;
    }
}

inline void BuilderCustom::applyInitialCounterIncrement(BuilderState&)
{
}

inline void BuilderCustom::applyInheritCounterIncrement(BuilderState& builderState)
{
    applyInheritCounter<Increment>(builderState);
}

inline void BuilderCustom::applyValueCounterIncrement(BuilderState& builderState, CSSValue& value)
{
    applyValueCounter<Increment>(builderState, value);
}

inline void BuilderCustom::applyInitialCounterReset(BuilderState&)
{
}

inline void BuilderCustom::applyInheritCounterReset(BuilderState& builderState)
{
    applyInheritCounter<Reset>(builderState);
}

inline void BuilderCustom::applyValueCounterReset(BuilderState& builderState, CSSValue& value)
{
    applyValueCounter<Reset>(builderState, value);
}

inline void BuilderCustom::applyInitialCounterSet(BuilderState&)
{
}

inline void BuilderCustom::applyInheritCounterSet(BuilderState& builderState)
{
    applyInheritCounter<Set>(builderState);
}

inline void BuilderCustom::applyValueCounterSet(BuilderState& builderState, CSSValue& value)
{
    applyValueCounter<Set>(builderState, value);
}

inline void BuilderCustom::applyInitialCursor(BuilderState& builderState)
{
    builderState.style().clearCursorList();
    builderState.style().setCursor(RenderStyle::initialCursor());
}

inline void BuilderCustom::applyInheritCursor(BuilderState& builderState)
{
    builderState.style().setCursor(forwardInheritedValue(builderState.parentStyle().cursor()));
    builderState.style().setCursorList(forwardInheritedValue(builderState.parentStyle().cursors()));
}

inline void BuilderCustom::applyValueCursor(BuilderState& builderState, CSSValue& value)
{
    builderState.style().clearCursorList();
    if (is<CSSPrimitiveValue>(value)) {
        auto cursor = fromCSSValue<CursorType>(value);
        if (builderState.style().cursor() != cursor)
            builderState.style().setCursor(cursor);
        return;
    }

    builderState.style().setCursor(CursorType::Auto);

    auto list = BuilderConverter::requiredListDowncast<CSSValueList, CSSValue>(builderState, value);
    if (!list)
        return;

    for (auto& item : *list) {
        if (auto* image = dynamicDowncast<CSSCursorImageValue>(item)) {
            auto styleImage = image->createStyleImage(builderState);
            auto hotSpot = styleImage->hotSpot();
            builderState.style().addCursor(WTFMove(styleImage), hotSpot);
            continue;
        }

        builderState.style().setCursor(fromCSSValue<CursorType>(item));
        ASSERT_WITH_MESSAGE(&item == &list->item(list->size() - 1), "Cursor ID fallback should always be last in the list");
        return;
    }
}

inline std::pair<Color, SVGPaintType> colorAndSVGPaintType(BuilderState& builderState, const CSSValue& localValue, Style::URL& url)
{
    if (RefPtr localURLValue = dynamicDowncast<CSSURLValue>(localValue)) {
        url = Style::toStyle(localURLValue->url(), builderState);
        return { Color::currentColor(), SVGPaintType::URI };
    }
    if (RefPtr localPrimitiveValue = dynamicDowncast<CSSPrimitiveValue>(localValue)) {
        auto valueID = localPrimitiveValue->valueID();
        if (valueID == CSSValueNone)
            return { Color::currentColor(), url.isNone() ? SVGPaintType::None : SVGPaintType::URINone };
        if (valueID == CSSValueCurrentcolor) {
            builderState.style().setDisallowsFastPathInheritance();
            return { Color::currentColor(), url.isNone() ? SVGPaintType::CurrentColor : SVGPaintType::URICurrentColor };
        }
    }

    return { builderState.createStyleColor(localValue), url.isNone() ? SVGPaintType::RGBColor : SVGPaintType::URIRGBColor };
}

inline void BuilderCustom::applyInitialFill(BuilderState& builderState)
{
    auto& svgStyle = builderState.style().accessSVGStyle();
    svgStyle.setFillPaint(SVGRenderStyle::initialFillPaintType(), SVGRenderStyle::initialFillPaintColor(), SVGRenderStyle::initialFillPaintUri(), builderState.applyPropertyToRegularStyle(), builderState.applyPropertyToVisitedLinkStyle());
}

inline void BuilderCustom::applyInheritFill(BuilderState& builderState)
{
    auto& svgStyle = builderState.style().accessSVGStyle();
    auto& svgParentStyle = builderState.parentStyle().svgStyle();
    svgStyle.setFillPaint(forwardInheritedValue(svgParentStyle.fillPaintType()), forwardInheritedValue(svgParentStyle.fillPaintColor()), forwardInheritedValue(svgParentStyle.fillPaintUri()), builderState.applyPropertyToRegularStyle(), builderState.applyPropertyToVisitedLinkStyle());
}

inline void BuilderCustom::applyValueFill(BuilderState& builderState, CSSValue& value)
{
    auto& svgStyle = builderState.style().accessSVGStyle();
    RefPtr<const CSSValue> localValue;
    auto url = Style::URL::none();

    if (RefPtr list = dynamicDowncast<CSSValueList>(value)) {
        auto urlValue = BuilderConverter::requiredDowncast<CSSURLValue>(builderState, *list->item(0));
        if (!urlValue)
            return;
        url = Style::toStyle(urlValue->url(), builderState);
        localValue = list->protectedItem(1);
        if (!localValue)
            return;
    }
    auto [color, paintType] = colorAndSVGPaintType(builderState, localValue ? *localValue : value, url);
    svgStyle.setFillPaint(paintType, WTFMove(color), WTFMove(url), builderState.applyPropertyToRegularStyle(), builderState.applyPropertyToVisitedLinkStyle());
}

inline void BuilderCustom::applyInitialStroke(BuilderState& builderState)
{
    SVGRenderStyle& svgStyle = builderState.style().accessSVGStyle();
    svgStyle.setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), builderState.applyPropertyToRegularStyle(), builderState.applyPropertyToVisitedLinkStyle());
}

inline void BuilderCustom::applyInheritStroke(BuilderState& builderState)
{
    auto& svgStyle = builderState.style().accessSVGStyle();
    auto& svgParentStyle = builderState.parentStyle().svgStyle();
    svgStyle.setStrokePaint(forwardInheritedValue(svgParentStyle.strokePaintType()), forwardInheritedValue(svgParentStyle.strokePaintColor()), forwardInheritedValue(svgParentStyle.strokePaintUri()), builderState.applyPropertyToRegularStyle(), builderState.applyPropertyToVisitedLinkStyle());
}

inline void BuilderCustom::applyValueStroke(BuilderState& builderState, CSSValue& value)
{
    auto& svgStyle = builderState.style().accessSVGStyle();
    RefPtr<const CSSValue> localValue;
    auto url = Style::URL::none();

    if (RefPtr list = dynamicDowncast<CSSValueList>(value)) {
        auto urlValue = BuilderConverter::requiredDowncast<CSSURLValue>(builderState, *list->item(0));
        if (!urlValue)
            return;
        url = Style::toStyle(urlValue->url(), builderState);
        localValue = list->protectedItem(1);
        if (!localValue)
            return;
    }

    auto [color, paintType] = colorAndSVGPaintType(builderState, localValue ? *localValue : value, url);
    svgStyle.setStrokePaint(paintType, WTFMove(color), WTFMove(url), builderState.applyPropertyToRegularStyle(), builderState.applyPropertyToVisitedLinkStyle());
}

inline void BuilderCustom::applyInitialContent(BuilderState& builderState)
{
    builderState.style().clearContent();
    builderState.style().setHasContentNone(false);
}

inline void BuilderCustom::applyInheritContent(BuilderState&)
{
}

inline void BuilderCustom::applyValueContent(BuilderState& builderState, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(value.valueID() == CSSValueNormal || value.valueID() == CSSValueNone);
        builderState.style().clearContent();
        builderState.style().setHasContentNone(value.valueID() == CSSValueNone);
        return;
    }

    auto* altTextPair = dynamicDowncast<CSSValuePair>(value);
    auto visibleContentList = BuilderConverter::requiredDowncast<CSSValueList>(builderState, altTextPair ? altTextPair->first() : value);
    if (!visibleContentList)
        return;

    auto processAttrContent = [&](const CSSPrimitiveValue& primitiveValue) -> AtomString {
        // FIXME: Can a namespace be specified for an attr(foo)?
        if (builderState.style().pseudoElementType() == PseudoId::None)
            builderState.style().setHasAttrContent();
        else
            const_cast<RenderStyle&>(builderState.parentStyle()).setHasAttrContent();

        auto value = primitiveValue.cssAttrValue();
        QualifiedName attr(nullAtom(), value->attributeName().impl(), nullAtom());
        const AtomString& attributeValue = builderState.element() ? builderState.element()->getAttribute(attr) : nullAtom();

        // Register the fact that the attribute value affects the style.
        builderState.registerContentAttribute(attr.localName());

        if (attributeValue.isNull()) {
            auto fallback = dynamicDowncast<CSSPrimitiveValue>(value->fallback());
            return fallback && fallback->isString() ? fallback->stringValue().impl() : emptyAtom();
        }
        return attributeValue.impl();
    };


    bool didSet = false;
    for (auto& item : *visibleContentList) {
        if (item.isImage()) {
            builderState.style().setContent(builderState.createStyleImage(item), didSet);
            didSet = true;
            continue;
        }

        auto* primitive = dynamicDowncast<CSSPrimitiveValue>(item);
        if (primitive && primitive->isString()) {
            builderState.style().setContent(primitive->stringValue().impl(), didSet);
            didSet = true;
        } else if (primitive && primitive->isAttr()) {
            builderState.style().setContent(processAttrContent(*primitive), didSet);
            didSet = true;
        } else if (auto* counter = dynamicDowncast<CSSCounterValue>(item)) {
            ListStyleType listStyleType;
            if (counter->counterStyle())
                listStyleType = BuilderConverter::convertListStyleType(builderState, *counter->counterStyle());
            builderState.style().setContent(makeUnique<CounterContent>(counter->identifier(), listStyleType, counter->separator()), didSet);
            didSet = true;
        } else {
            switch (item.valueID()) {
            case CSSValueOpenQuote:
                builderState.style().setContent(QuoteType::OpenQuote, didSet);
                didSet = true;
                break;
            case CSSValueCloseQuote:
                builderState.style().setContent(QuoteType::CloseQuote, didSet);
                didSet = true;
                break;
            case CSSValueNoOpenQuote:
                builderState.style().setContent(QuoteType::NoOpenQuote, didSet);
                didSet = true;
                break;
            case CSSValueNoCloseQuote:
                builderState.style().setContent(QuoteType::NoCloseQuote, didSet);
                didSet = true;
                break;
            default:
                // normal and none do not have any effect.
                break;
            }
        }
    }

    if (!didSet) {
        builderState.style().clearContent();
        return;
    }

    if (!altTextPair) {
        builderState.style().setContentAltText({ });
        return;
    }

    auto altTextContentList = BuilderConverter::requiredListDowncast<CSSValueList, CSSPrimitiveValue>(builderState, altTextPair->second());
    if (!altTextContentList)
        return;

    StringBuilder altText;
    for (auto& item : *altTextContentList) {
        if (item.isString())
            altText.append(item.stringValue());
        else if (item.isAttr())
            altText.append(processAttrContent(item));
    }
    builderState.style().setContentAltText(altText.toString());
}

inline void BuilderCustom::applyInheritFontVariantLigatures(BuilderState& builderState)
{
    builderState.setFontDescriptionVariantCommonLigatures(builderState.parentFontDescription().variantCommonLigatures());
    builderState.setFontDescriptionVariantDiscretionaryLigatures(builderState.parentFontDescription().variantDiscretionaryLigatures());
    builderState.setFontDescriptionVariantHistoricalLigatures(builderState.parentFontDescription().variantHistoricalLigatures());
    builderState.setFontDescriptionVariantContextualAlternates(builderState.parentFontDescription().variantContextualAlternates());
}

inline void BuilderCustom::applyInitialFontVariantLigatures(BuilderState& builderState)
{
    builderState.setFontDescriptionVariantCommonLigatures(FontVariantLigatures::Normal);
    builderState.setFontDescriptionVariantDiscretionaryLigatures(FontVariantLigatures::Normal);
    builderState.setFontDescriptionVariantHistoricalLigatures(FontVariantLigatures::Normal);
    builderState.setFontDescriptionVariantContextualAlternates(FontVariantLigatures::Normal);
}

inline void BuilderCustom::applyValueFontVariantLigatures(BuilderState& builderState, CSSValue& value)
{
    if (CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID())) {
        applyInitialFontVariantLigatures(builderState);
        return;
    }
    auto variantLigatures = extractFontVariantLigatures(value);
    builderState.setFontDescriptionVariantCommonLigatures(variantLigatures.commonLigatures);
    builderState.setFontDescriptionVariantDiscretionaryLigatures(variantLigatures.discretionaryLigatures);
    builderState.setFontDescriptionVariantHistoricalLigatures(variantLigatures.historicalLigatures);
    builderState.setFontDescriptionVariantContextualAlternates(variantLigatures.contextualAlternates);
}

inline void BuilderCustom::applyInheritFontVariantNumeric(BuilderState& builderState)
{
    builderState.setFontDescriptionVariantNumericFigure(builderState.parentFontDescription().variantNumericFigure());
    builderState.setFontDescriptionVariantNumericSpacing(builderState.parentFontDescription().variantNumericSpacing());
    builderState.setFontDescriptionVariantNumericFraction(builderState.parentFontDescription().variantNumericFraction());
    builderState.setFontDescriptionVariantNumericOrdinal(builderState.parentFontDescription().variantNumericOrdinal());
    builderState.setFontDescriptionVariantNumericSlashedZero(builderState.parentFontDescription().variantNumericSlashedZero());
}

inline void BuilderCustom::applyInitialFontVariantNumeric(BuilderState& builderState)
{
    builderState.setFontDescriptionVariantNumericFigure(FontVariantNumericFigure::Normal);
    builderState.setFontDescriptionVariantNumericSpacing(FontVariantNumericSpacing::Normal);
    builderState.setFontDescriptionVariantNumericFraction(FontVariantNumericFraction::Normal);
    builderState.setFontDescriptionVariantNumericOrdinal(FontVariantNumericOrdinal::Normal);
    builderState.setFontDescriptionVariantNumericSlashedZero(FontVariantNumericSlashedZero::Normal);
}

inline void BuilderCustom::applyValueFontVariantNumeric(BuilderState& builderState, CSSValue& value)
{
    if (CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID())) {
        applyInitialFontVariantNumeric(builderState);
        return;
    }
    auto variantNumeric = extractFontVariantNumeric(value);
    builderState.setFontDescriptionVariantNumericFigure(variantNumeric.figure);
    builderState.setFontDescriptionVariantNumericSpacing(variantNumeric.spacing);
    builderState.setFontDescriptionVariantNumericFraction(variantNumeric.fraction);
    builderState.setFontDescriptionVariantNumericOrdinal(variantNumeric.ordinal);
    builderState.setFontDescriptionVariantNumericSlashedZero(variantNumeric.slashedZero);
}

inline void BuilderCustom::applyInheritFontVariantEastAsian(BuilderState& builderState)
{
    builderState.setFontDescriptionVariantEastAsianVariant(builderState.parentFontDescription().variantEastAsianVariant());
    builderState.setFontDescriptionVariantEastAsianWidth(builderState.parentFontDescription().variantEastAsianWidth());
    builderState.setFontDescriptionVariantEastAsianRuby(builderState.parentFontDescription().variantEastAsianRuby());
}

inline void BuilderCustom::applyInitialFontVariantEastAsian(BuilderState& builderState)
{
    builderState.setFontDescriptionVariantEastAsianVariant(FontVariantEastAsianVariant::Normal);
    builderState.setFontDescriptionVariantEastAsianWidth(FontVariantEastAsianWidth::Normal);
    builderState.setFontDescriptionVariantEastAsianRuby(FontVariantEastAsianRuby::Normal);
}

inline void BuilderCustom::applyValueFontVariantEastAsian(BuilderState& builderState, CSSValue& value)
{
    if (CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID())) {
        applyInitialFontVariantEastAsian(builderState);
        return;
    }
    auto variantEastAsian = extractFontVariantEastAsian(value);
    builderState.setFontDescriptionVariantEastAsianVariant(variantEastAsian.variant);
    builderState.setFontDescriptionVariantEastAsianWidth(variantEastAsian.width);
    builderState.setFontDescriptionVariantEastAsianRuby(variantEastAsian.ruby);
}

inline void BuilderCustom::applyInheritFontVariantAlternates(BuilderState& builderState)
{
    builderState.setFontDescriptionVariantAlternates(builderState.parentFontDescription().variantAlternates());
}

inline void BuilderCustom::applyInitialFontVariantAlternates(BuilderState& builderState)
{
    builderState.setFontDescriptionVariantAlternates(FontVariantAlternates::Normal());
}

inline void BuilderCustom::applyValueFontVariantAlternates(BuilderState& builderState, CSSValue& value)
{
    if (CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID())) {
        applyInitialFontVariantAlternates(builderState);
        return;
    }
    auto fontDescription = builderState.fontDescription();
    fontDescription.setVariantAlternates(extractFontVariantAlternates(value, builderState));
    builderState.setFontDescription(WTFMove(fontDescription));
}

inline void BuilderCustom::applyInitialFontSize(BuilderState& builderState)
{
    auto fontDescription = builderState.fontDescription();
    float size = Style::fontSizeForKeyword(CSSValueMedium, fontDescription.useFixedDefaultSize(), builderState.document());

    if (size < 0)
        return;

    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);
    builderState.setFontSize(fontDescription, size);
    builderState.setFontDescription(WTFMove(fontDescription));
}

inline void BuilderCustom::applyInheritFontSize(BuilderState& builderState)
{
    const auto& parentFontDescription = builderState.parentStyle().fontDescription();
    float size = parentFontDescription.specifiedSize();

    if (size < 0)
        return;

    builderState.setFontDescriptionKeywordSize(parentFontDescription.keywordSize());
    builderState.setFontDescriptionFontSize(size);
}

// When the CSS keyword "larger" is used, this function will attempt to match within the keyword
// table, and failing that, will simply multiply by 1.2.
inline float BuilderCustom::largerFontSize(float size)
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
    // the next size level.
    return size * 1.2f;
}

// Like the previous function, but for the keyword "smaller".
inline float BuilderCustom::smallerFontSize(float size)
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
    // the next size level.
    return size / 1.2f;
}

inline float BuilderCustom::determineRubyTextSizeMultiplier(BuilderState& builderState)
{
    if (!builderState.style().isInterCharacterRubyPosition())
        return 0.5f;

    auto rubyPosition = builderState.style().rubyPosition();
    if (rubyPosition == RubyPosition::InterCharacter) {
        // If the writing mode of the enclosing ruby container is vertical, 'inter-character' value has the same effect as over.
        return !builderState.parentStyle().writingMode().isVerticalTypographic() ? 0.3f : 0.5f;
    }

    // Legacy inter-character behavior.
    // FIXME: This hack is to ensure tone marks are the same size as
    // the bopomofo. This code will go away if we make a special renderer
    // for the tone marks eventually.
    if (auto* element = builderState.element()) {
        for (auto& ancestor : ancestorsOfType<HTMLElement>(*element)) {
            if (ancestor.hasTagName(HTMLNames::rtTag))
                return 1.0f;
        }
    }
    return 0.25f;
}

static inline void applyFontStyle(BuilderState& state, std::optional<FontSelectionValue> slope, FontStyleAxis axis)
{
    auto& description = state.fontDescription();
    if (description.italic() == slope && description.fontStyleAxis() == axis)
        return;

    auto copy = description;
    copy.setItalic(slope);
    copy.setFontStyleAxis(axis);
    state.setFontDescription(WTFMove(copy));
}

inline void BuilderCustom::applyInitialFontStyle(BuilderState& state)
{
    applyFontStyle(state, FontCascadeDescription::initialItalic(), FontCascadeDescription::initialFontStyleAxis());
}

inline void BuilderCustom::applyInheritFontStyle(BuilderState& state)
{
    applyFontStyle(state, state.parentFontDescription().italic(), state.parentFontDescription().fontStyleAxis());
}

inline void BuilderCustom::applyValueFontStyle(BuilderState& state, CSSValue& value)
{
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    auto keyword = primitiveValue ? primitiveValue->valueID() : CSSValueOblique;

    std::optional<FontSelectionValue> slope;
    if (!CSSPropertyParserHelpers::isSystemFontShorthand(keyword))
        slope = BuilderConverter::convertFontStyleFromValue(state, value);

    applyFontStyle(state, slope, keyword == CSSValueItalic ? FontStyleAxis::ital : FontStyleAxis::slnt);
}

inline void BuilderCustom::applyValueFontSize(BuilderState& builderState, CSSValue& value)
{
    auto& fontDescription = builderState.fontDescription();
    builderState.setFontDescriptionKeywordSizeFromIdentifier(CSSValueInvalid);

    float parentSize = builderState.parentStyle().fontDescription().specifiedSize();
    bool parentIsAbsoluteSize = builderState.parentStyle().fontDescription().isAbsoluteSize();

    auto primitiveValue = BuilderConverter::requiredDowncast<CSSPrimitiveValue>(builderState, value);
    if (!primitiveValue)
        return;

    float size = 0;
    if (CSSValueID ident = primitiveValue->valueID()) {
        builderState.setFontDescriptionIsAbsoluteSize((parentIsAbsoluteSize && (ident == CSSValueLarger || ident == CSSValueSmaller || ident == CSSValueWebkitRubyText)) || CSSPropertyParserHelpers::isSystemFontShorthand(ident));

        if (CSSPropertyParserHelpers::isSystemFontShorthand(ident))
            size = SystemFontDatabase::singleton().systemFontShorthandSize(CSSPropertyParserHelpers::lowerFontShorthand(ident));

        switch (ident) {
        case CSSValueXxSmall:
        case CSSValueXSmall:
        case CSSValueSmall:
        case CSSValueMedium:
        case CSSValueLarge:
        case CSSValueXLarge:
        case CSSValueXxLarge:
        case CSSValueXxxLarge:
            size = Style::fontSizeForKeyword(ident, fontDescription.useFixedDefaultSize(), builderState.document());
            builderState.setFontDescriptionKeywordSizeFromIdentifier(ident);
            break;
        case CSSValueLarger:
            size = largerFontSize(parentSize);
            break;
        case CSSValueSmaller:
            size = smallerFontSize(parentSize);
            break;
        case CSSValueWebkitRubyText:
            size = determineRubyTextSizeMultiplier(builderState) * parentSize;
            break;
        default:
            break;
        }
    } else {
        builderState.setFontDescriptionIsAbsoluteSize(parentIsAbsoluteSize || !primitiveValue->isParentFontRelativeLength());
        auto conversionData = builderState.cssToLengthConversionData().copyForFontSize();
        if (primitiveValue->isLength())
            size = primitiveValue->resolveAsLength<float>(conversionData);
        else if (primitiveValue->isPercentage())
            size = (primitiveValue->resolveAsPercentage<float>(conversionData) * parentSize) / 100.0f;
        else if (primitiveValue->isCalculatedPercentageWithLength())
            size = primitiveValue->cssCalcValue()->createCalculationValue(conversionData, CSSCalcSymbolTable { })->evaluate(parentSize);
        else
            return;
    }

    if (size < 0)
        return;

    builderState.setFontDescriptionFontSize(std::min(maximumAllowedFontSize, size));
}

inline void BuilderCustom::applyValueFontSizeAdjust(BuilderState& builderState, CSSValue& value)
{
    builderState.setFontDescriptionFontSizeAdjust(BuilderConverter::convertFontSizeAdjust(builderState, value));
}

inline void BuilderCustom::applyInitialGridTemplateAreas(BuilderState& builderState)
{
    builderState.style().setImplicitNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines());
    builderState.style().setImplicitNamedGridRowLines(RenderStyle::initialNamedGridRowLines());

    builderState.style().setNamedGridArea(RenderStyle::initialNamedGridArea());
    builderState.style().setNamedGridAreaRowCount(RenderStyle::initialNamedGridAreaCount());
    builderState.style().setNamedGridAreaColumnCount(RenderStyle::initialNamedGridAreaCount());
}

inline void BuilderCustom::applyInheritGridTemplateAreas(BuilderState& builderState)
{
    builderState.style().setImplicitNamedGridColumnLines(builderState.parentStyle().implicitNamedGridColumnLines());
    builderState.style().setImplicitNamedGridRowLines(builderState.parentStyle().implicitNamedGridRowLines());

    builderState.style().setNamedGridArea(builderState.parentStyle().namedGridArea());
    builderState.style().setNamedGridAreaRowCount(builderState.parentStyle().namedGridAreaRowCount());
    builderState.style().setNamedGridAreaColumnCount(builderState.parentStyle().namedGridAreaColumnCount());
}

inline void BuilderCustom::applyValueGridTemplateAreas(BuilderState& builderState, CSSValue& value)
{
    if (value.valueID() == CSSValueNone) {
        applyInitialGridTemplateAreas(builderState);
        return;
    }

    auto gridTemplateAreasValue = BuilderConverter::requiredDowncast<CSSGridTemplateAreasValue>(builderState, value);
    if (!gridTemplateAreasValue)
        return;

    const NamedGridAreaMap& newNamedGridAreas = gridTemplateAreasValue->gridAreaMap();

    builderState.style().setImplicitNamedGridColumnLines(BuilderConverter::createImplicitNamedGridLinesFromGridArea(builderState, newNamedGridAreas, GridTrackSizingDirection::ForColumns));
    builderState.style().setImplicitNamedGridRowLines(BuilderConverter::createImplicitNamedGridLinesFromGridArea(builderState, newNamedGridAreas, GridTrackSizingDirection::ForRows));

    builderState.style().setNamedGridArea(gridTemplateAreasValue->gridAreaMap());
    builderState.style().setNamedGridAreaRowCount(gridTemplateAreasValue->rowCount());
    builderState.style().setNamedGridAreaColumnCount(gridTemplateAreasValue->columnCount());
}

inline void BuilderCustom::applyValueStrokeWidth(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setStrokeWidth(BuilderConverter::convertLengthAllowingNumber(builderState, value));
    builderState.style().setHasExplicitlySetStrokeWidth(true);
}

inline void BuilderCustom::applyValueStrokeColor(BuilderState& builderState, CSSValue& value)
{
    if (builderState.applyPropertyToRegularStyle())
        builderState.style().setStrokeColor(builderState.createStyleColor(value, ForVisitedLink::No));
    if (builderState.applyPropertyToVisitedLinkStyle())
        builderState.style().setVisitedLinkStrokeColor(builderState.createStyleColor(value, ForVisitedLink::Yes));
    builderState.style().setHasExplicitlySetStrokeColor(true);
}

// For the color property, "currentcolor" is actually the inherited computed color.
inline void BuilderCustom::applyValueColor(BuilderState& builderState, CSSValue& value)
{
    if (builderState.applyPropertyToRegularStyle()) {
        auto color = builderState.createStyleColor(value, ForVisitedLink::No);
        builderState.style().setColor(color.resolveColor(builderState.parentStyle().color()));
    }
    if (builderState.applyPropertyToVisitedLinkStyle()) {
        auto color = builderState.createStyleColor(value, ForVisitedLink::Yes);
        builderState.style().setVisitedLinkColor(color.resolveColor(builderState.parentStyle().visitedLinkColor()));
    }

    builderState.style().setDisallowsFastPathInheritance();
    builderState.style().setHasExplicitlySetColor(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialColor(BuilderState& builderState)
{
    if (builderState.applyPropertyToRegularStyle())
        builderState.style().setColor(RenderStyle::initialColor());
    if (builderState.applyPropertyToVisitedLinkStyle())
        builderState.style().setVisitedLinkColor(RenderStyle::initialColor());

    builderState.style().setDisallowsFastPathInheritance();
    builderState.style().setHasExplicitlySetColor(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritColor(BuilderState& builderState)
{
    if (builderState.applyPropertyToRegularStyle())
        builderState.style().setColor(forwardInheritedValue(builderState.parentStyle().color()));
    if (builderState.applyPropertyToVisitedLinkStyle())
        builderState.style().setVisitedLinkColor(forwardInheritedValue(builderState.parentStyle().color()));

    builderState.style().setDisallowsFastPathInheritance();
    builderState.style().setHasExplicitlySetColor(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialContainIntrinsicWidth(BuilderState& builderState)
{
    builderState.style().setContainIntrinsicWidthType(RenderStyle::initialContainIntrinsicWidthType());
    builderState.style().setContainIntrinsicWidth(RenderStyle::initialContainIntrinsicWidth());
}

inline void BuilderCustom::applyInheritContainIntrinsicWidth(BuilderState& builderState)
{
    builderState.style().setContainIntrinsicWidthType(forwardInheritedValue(builderState.parentStyle().containIntrinsicWidthType()));
    builderState.style().setContainIntrinsicWidth(forwardInheritedValue(builderState.parentStyle().containIntrinsicWidth()));
}

inline void BuilderCustom::applyValueContainIntrinsicWidth(BuilderState& builderState, CSSValue& value)
{
    auto& style = builderState.style();
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone) {
            style.setContainIntrinsicWidth(RenderStyle::initialContainIntrinsicWidth());
            return style.setContainIntrinsicWidthType(ContainIntrinsicSizeType::None);
        }

        if (primitiveValue->isLength()) {
            style.setContainIntrinsicWidthType(ContainIntrinsicSizeType::Length);
            auto width = primitiveValue->resolveAsLength<WebCore::Length>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
            style.setContainIntrinsicWidth(width);
        }
        return;
    }

    auto pair = BuilderConverter::requiredPairDowncast<CSSPrimitiveValue>(builderState, value);
    if (!pair)
        return;

    ASSERT(pair->first.valueID() == CSSValueAuto);
    if (pair->second.valueID() == CSSValueNone)
        style.setContainIntrinsicWidthType(ContainIntrinsicSizeType::AutoAndNone);
    else {
        ASSERT(pair->second.isLength());
        style.setContainIntrinsicWidthType(ContainIntrinsicSizeType::AutoAndLength);
        auto lengthValue = pair->second.resolveAsLength<WebCore::Length>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        style.setContainIntrinsicWidth(lengthValue);
    }
}

inline void BuilderCustom::applyInitialContainIntrinsicHeight(BuilderState& builderState)
{
    builderState.style().setContainIntrinsicHeightType(RenderStyle::initialContainIntrinsicHeightType());
    builderState.style().setContainIntrinsicHeight(RenderStyle::initialContainIntrinsicHeight());
}

inline void BuilderCustom::applyInheritContainIntrinsicHeight(BuilderState& builderState)
{
    builderState.style().setContainIntrinsicHeightType(forwardInheritedValue(builderState.parentStyle().containIntrinsicHeightType()));
    builderState.style().setContainIntrinsicHeight(forwardInheritedValue(builderState.parentStyle().containIntrinsicHeight()));
}

inline void BuilderCustom::applyValueContainIntrinsicHeight(BuilderState& builderState, CSSValue& value)
{
    auto& style = builderState.style();
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->valueID() == CSSValueNone) {
            style.setContainIntrinsicHeight(RenderStyle::initialContainIntrinsicHeight());
            return style.setContainIntrinsicHeightType(ContainIntrinsicSizeType::None);
        }

        if (primitiveValue->isLength()) {
            style.setContainIntrinsicHeightType(ContainIntrinsicSizeType::Length);
            auto height = primitiveValue->resolveAsLength<WebCore::Length>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
            style.setContainIntrinsicHeight(height);
        }
        return;
    }


    auto pair = BuilderConverter::requiredPairDowncast<CSSPrimitiveValue>(builderState, value);
    if (!pair)
        return;

    ASSERT(pair->first.valueID() == CSSValueAuto);
    if (pair->second.valueID() == CSSValueNone)
        style.setContainIntrinsicHeightType(ContainIntrinsicSizeType::AutoAndNone);
    else {
        ASSERT(pair->second.isLength());
        style.setContainIntrinsicHeightType(ContainIntrinsicSizeType::AutoAndLength);
        auto lengthValue = pair->second.resolveAsLength<WebCore::Length>(builderState.cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        style.setContainIntrinsicHeight(lengthValue);
    }
}

inline void BuilderCustom::applyInitialPaddingBottom(BuilderState& builderState)
{
    builderState.style().setPaddingBottom(RenderStyle::initialPadding());
    builderState.style().setHasExplicitlySetPaddingBottom(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritPaddingBottom(BuilderState& builderState)
{
    builderState.style().setPaddingBottom(forwardInheritedValue(builderState.parentStyle().paddingBottom()));
    builderState.style().setHasExplicitlySetPaddingBottom(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyValuePaddingBottom(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setPaddingBottom(BuilderConverter::convertPaddingEdge(builderState, value));
    builderState.style().setHasExplicitlySetPaddingBottom(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialPaddingLeft(BuilderState& builderState)
{
    builderState.style().setPaddingLeft(RenderStyle::initialPadding());
    builderState.style().setHasExplicitlySetPaddingLeft(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritPaddingLeft(BuilderState& builderState)
{
    builderState.style().setPaddingLeft(forwardInheritedValue(builderState.parentStyle().paddingLeft()));
    builderState.style().setHasExplicitlySetPaddingLeft(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyValuePaddingLeft(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setPaddingLeft(BuilderConverter::convertPaddingEdge(builderState, value));
    builderState.style().setHasExplicitlySetPaddingLeft(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialPaddingRight(BuilderState& builderState)
{
    builderState.style().setPaddingRight(RenderStyle::initialPadding());
    builderState.style().setHasExplicitlySetPaddingRight(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritPaddingRight(BuilderState& builderState)
{
    builderState.style().setPaddingRight(forwardInheritedValue(builderState.parentStyle().paddingRight()));
    builderState.style().setHasExplicitlySetPaddingRight(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyValuePaddingRight(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setPaddingRight(BuilderConverter::convertPaddingEdge(builderState, value));
    builderState.style().setHasExplicitlySetPaddingRight(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInitialPaddingTop(BuilderState& builderState)
{
    builderState.style().setPaddingTop(RenderStyle::initialPadding());
    builderState.style().setHasExplicitlySetPaddingTop(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyInheritPaddingTop(BuilderState& builderState)
{
    builderState.style().setPaddingTop(forwardInheritedValue(builderState.parentStyle().paddingTop()));
    builderState.style().setHasExplicitlySetPaddingTop(builderState.isAuthorOrigin());
}

inline void BuilderCustom::applyValuePaddingTop(BuilderState& builderState, CSSValue& value)
{
    builderState.style().setPaddingTop(BuilderConverter::convertPaddingEdge(builderState, value));
    builderState.style().setHasExplicitlySetPaddingTop(builderState.isAuthorOrigin());
}

} // namespace Style
} // namespace WebCore
