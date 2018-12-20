/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "CSSAspectRatioValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFontFamily.h"
#include "CSSFontValue.h"
#include "CSSGradientValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSShadowValue.h"
#include "Counter.h"
#include "CounterContent.h"
#include "CursorList.h"
#include "DashboardRegion.h"
#include "ElementAncestorIterator.h"
#include "FontVariantBuilder.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "Rect.h"
#include "SVGElement.h"
#include "SVGRenderStyle.h"
#include "StyleBuilderConverter.h"
#include "StyleCachedImage.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StyleResolver.h"
#include "WillChangeData.h"

namespace WebCore {

#define DECLARE_PROPERTY_CUSTOM_HANDLERS(property) \
    static void applyInherit##property(StyleResolver&); \
    static void applyInitial##property(StyleResolver&); \
    static void applyValue##property(StyleResolver&, CSSValue&)

template<typename T> inline T forwardInheritedValue(T&& value) { return std::forward<T>(value); }
inline Length forwardInheritedValue(const Length& value) { auto copy = value; return copy; }
inline LengthSize forwardInheritedValue(const LengthSize& value) { auto copy = value; return copy; }
inline LengthBox forwardInheritedValue(const LengthBox& value) { auto copy = value; return copy; }
inline GapLength forwardInheritedValue(const GapLength& value) { auto copy = value; return copy; }

// Note that we assume the CSS parser only allows valid CSSValue types.
class StyleBuilderCustom {
public:
    // Custom handling of inherit, initial and value setting.
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageOutset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageRepeat);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageSlice);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BorderImageWidth);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(BoxShadow);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Clip);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Content);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterIncrement);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterReset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Cursor);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Fill);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontFamily);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontSize);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontStyle);
#if ENABLE(CSS_IMAGE_RESOLUTION)
    DECLARE_PROPERTY_CUSTOM_HANDLERS(ImageResolution);
#endif
#if ENABLE(TEXT_AUTOSIZING)
    DECLARE_PROPERTY_CUSTOM_HANDLERS(LineHeight);
#endif
    DECLARE_PROPERTY_CUSTOM_HANDLERS(OutlineStyle);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Size);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Stroke);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(TextIndent);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(TextShadow);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitAspectRatio);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitBoxShadow);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontVariantLigatures);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontVariantNumeric);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontVariantEastAsian);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(GridTemplateAreas);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(GridTemplateColumns);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(GridTemplateRows);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitMaskBoxImageOutset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitMaskBoxImageRepeat);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitMaskBoxImageSlice);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitMaskBoxImageWidth);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitTextEmphasisStyle);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Zoom);

    // Custom handling of initial + inherit value setting only.
    static void applyInitialWebkitMaskImage(StyleResolver&) { }
    static void applyInheritWebkitMaskImage(StyleResolver&) { }
    static void applyInitialFontFeatureSettings(StyleResolver&) { }
    static void applyInheritFontFeatureSettings(StyleResolver&) { }
#if ENABLE(VARIATION_FONTS)
    static void applyInitialFontVariationSettings(StyleResolver&) { }
    static void applyInheritFontVariationSettings(StyleResolver&) { }
#endif

    // Custom handling of inherit + value setting only.
    static void applyInheritDisplay(StyleResolver&);
    static void applyValueDisplay(StyleResolver&, CSSValue&);

    // Custom handling of value setting only.
    static void applyValueBaselineShift(StyleResolver&, CSSValue&);
    static void applyValueDirection(StyleResolver&, CSSValue&);
    static void applyValueVerticalAlign(StyleResolver&, CSSValue&);
    static void applyInitialTextAlign(StyleResolver&);
    static void applyValueTextAlign(StyleResolver&, CSSValue&);
#if ENABLE(DASHBOARD_SUPPORT)
    static void applyValueWebkitDashboardRegion(StyleResolver&, CSSValue&);
#endif
    static void applyValueWebkitLocale(StyleResolver&, CSSValue&);
    static void applyValueWebkitTextOrientation(StyleResolver&, CSSValue&);
#if ENABLE(TEXT_AUTOSIZING)
    static void applyValueWebkitTextSizeAdjust(StyleResolver&, CSSValue&);
#endif
    static void applyValueWebkitTextZoom(StyleResolver&, CSSValue&);
    static void applyValueWritingMode(StyleResolver&, CSSValue&);
    static void applyValueAlt(StyleResolver&, CSSValue&);
    static void applyValueWillChange(StyleResolver&, CSSValue&);

#if ENABLE(DARK_MODE_CSS)
    static void applyValueSupportedColorSchemes(StyleResolver&, CSSValue&);
#endif

    static void applyValueStrokeWidth(StyleResolver&, CSSValue&);
    static void applyValueStrokeColor(StyleResolver&, CSSValue&);

    static void applyInitialCustomProperty(StyleResolver&, const CSSRegisteredCustomProperty*, const AtomicString& name);
    static void applyInheritCustomProperty(StyleResolver&, const CSSRegisteredCustomProperty*, const AtomicString& name);
    static void applyValueCustomProperty(StyleResolver&, const CSSRegisteredCustomProperty*, CSSCustomPropertyValue&);

private:
    static void resetEffectiveZoom(StyleResolver&);

    static Length mmLength(double mm);
    static Length inchLength(double inch);
    static bool getPageSizeFromName(CSSPrimitiveValue* pageSizeName, CSSPrimitiveValue* pageOrientation, Length& width, Length& height);

    template <CSSPropertyID id>
    static void applyTextOrBoxShadowValue(StyleResolver&, CSSValue&);
    static bool isValidDisplayValue(StyleResolver&, DisplayType);

    enum CounterBehavior {Increment = 0, Reset};
    template <CounterBehavior counterBehavior>
    static void applyInheritCounter(StyleResolver&);
    template <CounterBehavior counterBehavior>
    static void applyValueCounter(StyleResolver&, CSSValue&);

    static float largerFontSize(float size);
    static float smallerFontSize(float size);
    static float determineRubyTextSizeMultiplier(StyleResolver&);
};

inline void StyleBuilderCustom::applyValueDirection(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.style()->setDirection(downcast<CSSPrimitiveValue>(value));
    styleResolver.style()->setHasExplicitlySetDirection(true);
}

inline void StyleBuilderCustom::applyInitialTextAlign(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextAlign(RenderStyle::initialTextAlign());
    styleResolver.style()->setHasExplicitlySetTextAlign(true);
}

inline void StyleBuilderCustom::applyValueTextAlign(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.style()->setTextAlign(StyleBuilderConverter::convertTextAlign(styleResolver, value));
    styleResolver.style()->setHasExplicitlySetTextAlign(true);
}

inline void StyleBuilderCustom::resetEffectiveZoom(StyleResolver& styleResolver)
{
    // Reset the zoom in effect. This allows the setZoom method to accurately compute a new zoom in effect.
    styleResolver.setEffectiveZoom(styleResolver.parentStyle() ? styleResolver.parentStyle()->effectiveZoom() : RenderStyle::initialZoom());
}

inline void StyleBuilderCustom::applyInitialZoom(StyleResolver& styleResolver)
{
    resetEffectiveZoom(styleResolver);
    styleResolver.setZoom(RenderStyle::initialZoom());
}

inline void StyleBuilderCustom::applyInheritZoom(StyleResolver& styleResolver)
{
    resetEffectiveZoom(styleResolver);
    styleResolver.setZoom(styleResolver.parentStyle()->zoom());
}

inline void StyleBuilderCustom::applyValueZoom(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.valueID() == CSSValueNormal) {
        resetEffectiveZoom(styleResolver);
        styleResolver.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue.valueID() == CSSValueReset) {
        styleResolver.setEffectiveZoom(RenderStyle::initialZoom());
        styleResolver.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue.valueID() == CSSValueDocument) {
        float docZoom = styleResolver.rootElementStyle() ? styleResolver.rootElementStyle()->zoom() : RenderStyle::initialZoom();
        styleResolver.setEffectiveZoom(docZoom);
        styleResolver.setZoom(docZoom);
    } else if (primitiveValue.isPercentage()) {
        resetEffectiveZoom(styleResolver);
        if (float percent = primitiveValue.floatValue())
            styleResolver.setZoom(percent / 100.0f);
    } else if (primitiveValue.isNumber()) {
        resetEffectiveZoom(styleResolver);
        if (float number = primitiveValue.floatValue())
            styleResolver.setZoom(number);
    }
}
inline Length StyleBuilderCustom::mmLength(double mm)
{
    Ref<CSSPrimitiveValue> value(CSSPrimitiveValue::create(mm, CSSPrimitiveValue::CSS_MM));
    return value.get().computeLength<Length>(CSSToLengthConversionData());
}
inline Length StyleBuilderCustom::inchLength(double inch)
{
    Ref<CSSPrimitiveValue> value(CSSPrimitiveValue::create(inch, CSSPrimitiveValue::CSS_IN));
    return value.get().computeLength<Length>(CSSToLengthConversionData());
}
bool StyleBuilderCustom::getPageSizeFromName(CSSPrimitiveValue* pageSizeName, CSSPrimitiveValue* pageOrientation, Length& width, Length& height)
{
    static NeverDestroyed<Length> a5Width(mmLength(148));
    static NeverDestroyed<Length> a5Height(mmLength(210));
    static NeverDestroyed<Length> a4Width(mmLength(210));
    static NeverDestroyed<Length> a4Height(mmLength(297));
    static NeverDestroyed<Length> a3Width(mmLength(297));
    static NeverDestroyed<Length> a3Height(mmLength(420));
    static NeverDestroyed<Length> b5Width(mmLength(176));
    static NeverDestroyed<Length> b5Height(mmLength(250));
    static NeverDestroyed<Length> b4Width(mmLength(250));
    static NeverDestroyed<Length> b4Height(mmLength(353));
    static NeverDestroyed<Length> letterWidth(inchLength(8.5));
    static NeverDestroyed<Length> letterHeight(inchLength(11));
    static NeverDestroyed<Length> legalWidth(inchLength(8.5));
    static NeverDestroyed<Length> legalHeight(inchLength(14));
    static NeverDestroyed<Length> ledgerWidth(inchLength(11));
    static NeverDestroyed<Length> ledgerHeight(inchLength(17));

    if (!pageSizeName)
        return false;

    switch (pageSizeName->valueID()) {
    case CSSValueA5:
        width = a5Width;
        height = a5Height;
        break;
    case CSSValueA4:
        width = a4Width;
        height = a4Height;
        break;
    case CSSValueA3:
        width = a3Width;
        height = a3Height;
        break;
    case CSSValueB5:
        width = b5Width;
        height = b5Height;
        break;
    case CSSValueB4:
        width = b4Width;
        height = b4Height;
        break;
    case CSSValueLetter:
        width = letterWidth;
        height = letterHeight;
        break;
    case CSSValueLegal:
        width = legalWidth;
        height = legalHeight;
        break;
    case CSSValueLedger:
        width = ledgerWidth;
        height = ledgerHeight;
        break;
    default:
        return false;
    }

    if (pageOrientation) {
        switch (pageOrientation->valueID()) {
        case CSSValueLandscape:
            std::swap(width, height);
            break;
        case CSSValuePortrait:
            // Nothing to do.
            break;
        default:
            return false;
        }
    }
    return true;
}

inline void StyleBuilderCustom::applyValueVerticalAlign(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID())
        styleResolver.style()->setVerticalAlign(primitiveValue);
    else
        styleResolver.style()->setVerticalAlignLength(primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(styleResolver.state().cssToLengthConversionData()));
}

#if ENABLE(DASHBOARD_SUPPORT)

static Length convertToIntLength(const CSSPrimitiveValue* primitiveValue, const CSSToLengthConversionData& conversionData)
{
    return primitiveValue ? primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(conversionData) : Length(Undefined);
}

inline void StyleBuilderCustom::applyValueWebkitDashboardRegion(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNone) {
        styleResolver.style()->setDashboardRegions(RenderStyle::noneDashboardRegions());
        return;
    }

    auto* region = primitiveValue.dashboardRegionValue();
    if (!region)
        return;

    auto* first = region;
    while (region) {
        Length top = convertToIntLength(region->top(), styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        Length right = convertToIntLength(region->right(), styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        Length bottom = convertToIntLength(region->bottom(), styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        Length left = convertToIntLength(region->left(), styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));

        if (top.isUndefined())
            top = Length();
        if (right.isUndefined())
            right = Length();
        if (bottom.isUndefined())
            bottom = Length();
        if (left.isUndefined())
            left = Length();

        if (region->m_isCircle)
            styleResolver.style()->setDashboardRegion(StyleDashboardRegion::Circle, region->m_label, WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left), region != first);
        else if (region->m_isRectangle)
            styleResolver.style()->setDashboardRegion(StyleDashboardRegion::Rectangle, region->m_label, WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left), region != first);

        region = region->m_next.get();
    }

    styleResolver.document().setHasAnnotatedRegions(true);
}

#endif // ENABLE(DASHBOARD_SUPPORT)

#if ENABLE(CSS_IMAGE_RESOLUTION)

inline void StyleBuilderCustom::applyInheritImageResolution(StyleResolver& styleResolver)
{
    styleResolver.style()->setImageResolutionSource(styleResolver.parentStyle()->imageResolutionSource());
    styleResolver.style()->setImageResolutionSnap(styleResolver.parentStyle()->imageResolutionSnap());
    styleResolver.style()->setImageResolution(styleResolver.parentStyle()->imageResolution());
}

inline void StyleBuilderCustom::applyInitialImageResolution(StyleResolver& styleResolver)
{
    styleResolver.style()->setImageResolutionSource(RenderStyle::initialImageResolutionSource());
    styleResolver.style()->setImageResolutionSnap(RenderStyle::initialImageResolutionSnap());
    styleResolver.style()->setImageResolution(RenderStyle::initialImageResolution());
}

inline void StyleBuilderCustom::applyValueImageResolution(StyleResolver& styleResolver, CSSValue& value)
{
    ImageResolutionSource source = RenderStyle::initialImageResolutionSource();
    ImageResolutionSnap snap = RenderStyle::initialImageResolutionSnap();
    double resolution = RenderStyle::initialImageResolution();
    for (auto& item : downcast<CSSValueList>(value)) {
        CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(item.get());
        if (primitiveValue.valueID() == CSSValueFromImage)
            source = ImageResolutionSource::FromImage;
        else if (primitiveValue.valueID() == CSSValueSnap)
            snap = ImageResolutionSnap::Pixels;
        else
            resolution = primitiveValue.doubleValue(CSSPrimitiveValue::CSS_DPPX);
    }
    styleResolver.style()->setImageResolutionSource(source);
    styleResolver.style()->setImageResolutionSnap(snap);
    styleResolver.style()->setImageResolution(resolution);
}

#endif // ENABLE(CSS_IMAGE_RESOLUTION)

inline void StyleBuilderCustom::applyInheritSize(StyleResolver&) { }

inline void StyleBuilderCustom::applyInitialSize(StyleResolver&) { }

inline void StyleBuilderCustom::applyValueSize(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.style()->resetPageSizeType();

    if (!is<CSSValueList>(value))
        return;

    Length width;
    Length height;
    PageSizeType pageSizeType = PAGE_SIZE_AUTO;

    auto& valueList = downcast<CSSValueList>(value);
    switch (valueList.length()) {
    case 2: {
        auto firstValue = valueList.itemWithoutBoundsCheck(0);
        auto secondValue = valueList.itemWithoutBoundsCheck(1);
        // <length>{2} | <page-size> <orientation>
        if (!is<CSSPrimitiveValue>(*firstValue) || !is<CSSPrimitiveValue>(*secondValue))
            return;
        auto& firstPrimitiveValue = downcast<CSSPrimitiveValue>(*firstValue);
        auto& secondPrimitiveValue = downcast<CSSPrimitiveValue>(*secondValue);
        if (firstPrimitiveValue.isLength()) {
            // <length>{2}
            if (!secondPrimitiveValue.isLength())
                return;
            CSSToLengthConversionData conversionData = styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f);
            width = firstPrimitiveValue.computeLength<Length>(conversionData);
            height = secondPrimitiveValue.computeLength<Length>(conversionData);
        } else {
            // <page-size> <orientation>
            // The value order is guaranteed. See CSSParser::parseSizeParameter.
            if (!getPageSizeFromName(&firstPrimitiveValue, &secondPrimitiveValue, width, height))
                return;
        }
        pageSizeType = PAGE_SIZE_RESOLVED;
        break;
    }
    case 1: {
        auto value = valueList.itemWithoutBoundsCheck(0);
        // <length> | auto | <page-size> | [ portrait | landscape]
        if (!is<CSSPrimitiveValue>(*value))
            return;
        auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
        if (primitiveValue.isLength()) {
            // <length>
            pageSizeType = PAGE_SIZE_RESOLVED;
            width = height = primitiveValue.computeLength<Length>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        } else {
            switch (primitiveValue.valueID()) {
            case 0:
                return;
            case CSSValueAuto:
                pageSizeType = PAGE_SIZE_AUTO;
                break;
            case CSSValuePortrait:
                pageSizeType = PAGE_SIZE_AUTO_PORTRAIT;
                break;
            case CSSValueLandscape:
                pageSizeType = PAGE_SIZE_AUTO_LANDSCAPE;
                break;
            default:
                // <page-size>
                pageSizeType = PAGE_SIZE_RESOLVED;
                if (!getPageSizeFromName(&primitiveValue, nullptr, width, height))
                    return;
            }
        }
        break;
    }
    default:
        return;
    }
    styleResolver.style()->setPageSizeType(pageSizeType);
    styleResolver.style()->setPageSize({ WTFMove(width), WTFMove(height) });
}

inline void StyleBuilderCustom::applyInheritTextIndent(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextIndent(Length { styleResolver.parentStyle()->textIndent() });
#if ENABLE(CSS3_TEXT)
    styleResolver.style()->setTextIndentLine(styleResolver.parentStyle()->textIndentLine());
    styleResolver.style()->setTextIndentType(styleResolver.parentStyle()->textIndentType());
#endif
}

inline void StyleBuilderCustom::applyInitialTextIndent(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextIndent(RenderStyle::initialTextIndent());
#if ENABLE(CSS3_TEXT)
    styleResolver.style()->setTextIndentLine(RenderStyle::initialTextIndentLine());
    styleResolver.style()->setTextIndentType(RenderStyle::initialTextIndentType());
#endif
}

inline void StyleBuilderCustom::applyValueTextIndent(StyleResolver& styleResolver, CSSValue& value)
{
    Length lengthOrPercentageValue;
#if ENABLE(CSS3_TEXT)
    TextIndentLine textIndentLineValue = RenderStyle::initialTextIndentLine();
    TextIndentType textIndentTypeValue = RenderStyle::initialTextIndentType();
#endif
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(item.get());
        if (!primitiveValue.valueID())
            lengthOrPercentageValue = primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(styleResolver.state().cssToLengthConversionData());
#if ENABLE(CSS3_TEXT)
        else if (primitiveValue.valueID() == CSSValueWebkitEachLine)
            textIndentLineValue = TextIndentLine::EachLine;
        else if (primitiveValue.valueID() == CSSValueWebkitHanging)
            textIndentTypeValue = TextIndentType::Hanging;
#endif
    }

    if (lengthOrPercentageValue.isUndefined())
        return;

    styleResolver.style()->setTextIndent(WTFMove(lengthOrPercentageValue));
#if ENABLE(CSS3_TEXT)
    styleResolver.style()->setTextIndentLine(textIndentLineValue);
    styleResolver.style()->setTextIndentType(textIndentTypeValue);
#endif
}

enum BorderImageType { BorderImage, WebkitMaskBoxImage };
enum BorderImageModifierType { Outset, Repeat, Slice, Width };
template <BorderImageType type, BorderImageModifierType modifier>
class ApplyPropertyBorderImageModifier {
public:
    static void applyInheritValue(StyleResolver& styleResolver)
    {
        NinePieceImage image(getValue(styleResolver.style()));
        switch (modifier) {
        case Outset:
            image.copyOutsetFrom(getValue(styleResolver.parentStyle()));
            break;
        case Repeat:
            image.copyRepeatFrom(getValue(styleResolver.parentStyle()));
            break;
        case Slice:
            image.copyImageSlicesFrom(getValue(styleResolver.parentStyle()));
            break;
        case Width:
            image.copyBorderSlicesFrom(getValue(styleResolver.parentStyle()));
            break;
        }
        setValue(styleResolver.style(), image);
    }

    static void applyInitialValue(StyleResolver& styleResolver)
    {
        NinePieceImage image(getValue(styleResolver.style()));
        switch (modifier) {
        case Outset:
            image.setOutset(LengthBox(0));
            break;
        case Repeat:
            image.setHorizontalRule(StretchImageRule);
            image.setVerticalRule(StretchImageRule);
            break;
        case Slice:
            // Masks have a different initial value for slices. Preserve the value of 0 for backwards compatibility.
            image.setImageSlices(type == BorderImage ? LengthBox(Length(100, Percent), Length(100, Percent), Length(100, Percent), Length(100, Percent)) : LengthBox());
            image.setFill(false);
            break;
        case Width:
            // FIXME: This is a local variable to work around a bug in the GCC 8.1 Address Sanitizer.
            // Might be slightly less efficient when the type is not BorderImage since this is unused in that case.
            // Should be switched back to a temporary when possible. See https://webkit.org/b/186980
            LengthBox lengthBox(Length(1, Relative), Length(1, Relative), Length(1, Relative), Length(1, Relative));
            // Masks have a different initial value for widths. They use an 'auto' value rather than trying to fit to the border.
            image.setBorderSlices(type == BorderImage ? lengthBox : LengthBox());
            break;
        }
        setValue(styleResolver.style(), image);
    }

    static void applyValue(StyleResolver& styleResolver, CSSValue& value)
    {
        NinePieceImage image(getValue(styleResolver.style()));
        switch (modifier) {
        case Outset:
            image.setOutset(styleResolver.styleMap()->mapNinePieceImageQuad(value));
            break;
        case Repeat:
            styleResolver.styleMap()->mapNinePieceImageRepeat(value, image);
            break;
        case Slice:
            styleResolver.styleMap()->mapNinePieceImageSlice(value, image);
            break;
        case Width:
            image.setBorderSlices(styleResolver.styleMap()->mapNinePieceImageQuad(value));
            break;
        }
        setValue(styleResolver.style(), image);
    }

private:
    static const NinePieceImage& getValue(const RenderStyle* style)
    {
        return type == BorderImage ? style->borderImage() : style->maskBoxImage();
    }

    static void setValue(RenderStyle* style, const NinePieceImage& value)
    {
        return type == BorderImage ? style->setBorderImage(value) : style->setMaskBoxImage(value);
    }
};

#define DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(type, modifier) \
inline void StyleBuilderCustom::applyInherit##type##modifier(StyleResolver& styleResolver) \
{ \
    ApplyPropertyBorderImageModifier<type, modifier>::applyInheritValue(styleResolver); \
} \
inline void StyleBuilderCustom::applyInitial##type##modifier(StyleResolver& styleResolver) \
{ \
    ApplyPropertyBorderImageModifier<type, modifier>::applyInitialValue(styleResolver); \
} \
inline void StyleBuilderCustom::applyValue##type##modifier(StyleResolver& styleResolver, CSSValue& value) \
{ \
    ApplyPropertyBorderImageModifier<type, modifier>::applyValue(styleResolver, value); \
}

DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Outset)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Repeat)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Slice)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Width)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(WebkitMaskBoxImage, Outset)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(WebkitMaskBoxImage, Repeat)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(WebkitMaskBoxImage, Slice)
DEFINE_BORDER_IMAGE_MODIFIER_HANDLER(WebkitMaskBoxImage, Width)

#if ENABLE(TEXT_AUTOSIZING)

inline void StyleBuilderCustom::applyInheritLineHeight(StyleResolver& styleResolver)
{
    styleResolver.style()->setLineHeight(Length { styleResolver.parentStyle()->lineHeight() });
    styleResolver.style()->setSpecifiedLineHeight(Length { styleResolver.parentStyle()->specifiedLineHeight() });
}

inline void StyleBuilderCustom::applyInitialLineHeight(StyleResolver& styleResolver)
{
    styleResolver.style()->setLineHeight(RenderStyle::initialLineHeight());
    styleResolver.style()->setSpecifiedLineHeight(RenderStyle::initialSpecifiedLineHeight());
}

static inline float computeBaseSpecifiedFontSize(const Document& document, const RenderStyle& style, bool percentageAutosizingEnabled)
{
    float result = style.specifiedFontSize();
    auto* frame = document.frame();
    if (frame && style.textZoom() != TextZoom::Reset)
        result *= frame->textZoomFactor();
    result *= style.effectiveZoom();
    if (percentageAutosizingEnabled)
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

    if (percentageAutosizingEnabled)
        return style.textSizeAdjust().multiplier();
    return 1;
}

inline void StyleBuilderCustom::applyValueLineHeight(StyleResolver& styleResolver, CSSValue& value)
{
    Optional<Length> lineHeight = StyleBuilderConverter::convertLineHeight(styleResolver, value, 1);
    if (!lineHeight)
        return;

    Length computedLineHeight;
    if (lineHeight.value().isNegative())
        computedLineHeight = lineHeight.value();
    else {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        auto multiplier = computeLineHeightMultiplierDueToFontSize(styleResolver.document(), *styleResolver.style(), primitiveValue);
        if (multiplier == 1)
            computedLineHeight = lineHeight.value();
        else
            computedLineHeight = StyleBuilderConverter::convertLineHeight(styleResolver, value, multiplier).value();
    }

    styleResolver.style()->setLineHeight(WTFMove(computedLineHeight));
    styleResolver.style()->setSpecifiedLineHeight(WTFMove(lineHeight.value()));
}

#endif

inline void StyleBuilderCustom::applyInheritOutlineStyle(StyleResolver& styleResolver)
{
    styleResolver.style()->setOutlineStyleIsAuto(styleResolver.parentStyle()->outlineStyleIsAuto());
    styleResolver.style()->setOutlineStyle(styleResolver.parentStyle()->outlineStyle());
}

inline void StyleBuilderCustom::applyInitialOutlineStyle(StyleResolver& styleResolver)
{
    styleResolver.style()->setOutlineStyleIsAuto(RenderStyle::initialOutlineStyleIsAuto());
    styleResolver.style()->setOutlineStyle(RenderStyle::initialBorderStyle());
}

inline void StyleBuilderCustom::applyValueOutlineStyle(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    styleResolver.style()->setOutlineStyleIsAuto(primitiveValue);
    styleResolver.style()->setOutlineStyle(primitiveValue);
}

inline void StyleBuilderCustom::applyInitialClip(StyleResolver& styleResolver)
{
    styleResolver.style()->setClip(Length(), Length(), Length(), Length());
    styleResolver.style()->setHasClip(false);
}

inline void StyleBuilderCustom::applyInheritClip(StyleResolver& styleResolver)
{
    auto* parentStyle = styleResolver.parentStyle();
    if (!parentStyle->hasClip())
        return applyInitialClip(styleResolver);
    styleResolver.style()->setClip(Length { parentStyle->clipTop() }, Length { parentStyle->clipRight() },
        Length { parentStyle->clipBottom() }, Length { parentStyle->clipLeft() });
    styleResolver.style()->setHasClip(true);
}

inline void StyleBuilderCustom::applyValueClip(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (auto* rect = primitiveValue.rectValue()) {
        auto conversionData = styleResolver.state().cssToLengthConversionData();
        auto top = rect->top()->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        auto right = rect->right()->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        auto bottom = rect->bottom()->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        auto left = rect->left()->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        styleResolver.style()->setClip(WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left));
        styleResolver.style()->setHasClip(true);
    } else {
        ASSERT(primitiveValue.valueID() == CSSValueAuto);
        applyInitialClip(styleResolver);
    }
}

inline void StyleBuilderCustom::applyValueWebkitLocale(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    FontCascadeDescription fontDescription = styleResolver.style()->fontDescription();
    if (primitiveValue.valueID() == CSSValueAuto)
        fontDescription.setLocale(nullAtom());
    else
        fontDescription.setLocale(primitiveValue.stringValue());
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyValueWritingMode(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.setWritingMode(downcast<CSSPrimitiveValue>(value));
    styleResolver.style()->setHasExplicitlySetWritingMode(true);
}

inline void StyleBuilderCustom::applyValueWebkitTextOrientation(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.setTextOrientation(downcast<CSSPrimitiveValue>(value));
}

#if ENABLE(TEXT_AUTOSIZING)
inline void StyleBuilderCustom::applyValueWebkitTextSizeAdjust(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueAuto)
        styleResolver.style()->setTextSizeAdjust(TextSizeAdjustment(AutoTextSizeAdjustment));
    else if (primitiveValue.valueID() == CSSValueNone)
        styleResolver.style()->setTextSizeAdjust(TextSizeAdjustment(NoTextSizeAdjustment));
    else
        styleResolver.style()->setTextSizeAdjust(TextSizeAdjustment(primitiveValue.floatValue()));

    styleResolver.state().setFontDirty(true);
}
#endif

inline void StyleBuilderCustom::applyValueWebkitTextZoom(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.valueID() == CSSValueNormal)
        styleResolver.style()->setTextZoom(TextZoom::Normal);
    else if (primitiveValue.valueID() == CSSValueReset)
        styleResolver.style()->setTextZoom(TextZoom::Reset);
    styleResolver.state().setFontDirty(true);
}

#if ENABLE(DARK_MODE_CSS)
inline void StyleBuilderCustom::applyValueSupportedColorSchemes(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.style()->setSupportedColorSchemes(StyleBuilderConverter::convertSupportedColorSchemes(styleResolver, value));
    styleResolver.style()->setHasExplicitlySetSupportedColorSchemes(true);
}
#endif

template<CSSPropertyID property>
inline void StyleBuilderCustom::applyTextOrBoxShadowValue(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        if (property == CSSPropertyTextShadow)
            styleResolver.style()->setTextShadow(nullptr);
        else
            styleResolver.style()->setBoxShadow(nullptr);
        return;
    }

    bool isFirstEntry = true;
    for (auto& item : downcast<CSSValueList>(value)) {
        auto& shadowValue = downcast<CSSShadowValue>(item.get());
        auto conversionData = styleResolver.state().cssToLengthConversionData();
        int x = shadowValue.x->computeLength<int>(conversionData);
        int y = shadowValue.y->computeLength<int>(conversionData);
        int blur = shadowValue.blur ? shadowValue.blur->computeLength<int>(conversionData) : 0;
        int spread = shadowValue.spread ? shadowValue.spread->computeLength<int>(conversionData) : 0;
        ShadowStyle shadowStyle = shadowValue.style && shadowValue.style->valueID() == CSSValueInset ? Inset : Normal;
        Color color;
        if (shadowValue.color)
            color = styleResolver.colorFromPrimitiveValue(*shadowValue.color);
        else
            color = styleResolver.style()->color();
        auto shadowData = std::make_unique<ShadowData>(IntPoint(x, y), blur, spread, shadowStyle, property == CSSPropertyWebkitBoxShadow, color.isValid() ? color : Color::transparent);
        if (property == CSSPropertyTextShadow)
            styleResolver.style()->setTextShadow(WTFMove(shadowData), !isFirstEntry); // add to the list if this is not the first entry
        else
            styleResolver.style()->setBoxShadow(WTFMove(shadowData), !isFirstEntry); // add to the list if this is not the first entry
        isFirstEntry = false;
    }
}

inline void StyleBuilderCustom::applyInitialTextShadow(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextShadow(nullptr);
}

inline void StyleBuilderCustom::applyInheritTextShadow(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextShadow(styleResolver.parentStyle()->textShadow() ? std::make_unique<ShadowData>(*styleResolver.parentStyle()->textShadow()) : nullptr);
}

inline void StyleBuilderCustom::applyValueTextShadow(StyleResolver& styleResolver, CSSValue& value)
{
    applyTextOrBoxShadowValue<CSSPropertyTextShadow>(styleResolver, value);
}

inline void StyleBuilderCustom::applyInitialBoxShadow(StyleResolver& styleResolver)
{
    styleResolver.style()->setBoxShadow(nullptr);
}

inline void StyleBuilderCustom::applyInheritBoxShadow(StyleResolver& styleResolver)
{
    styleResolver.style()->setBoxShadow(styleResolver.parentStyle()->boxShadow() ? std::make_unique<ShadowData>(*styleResolver.parentStyle()->boxShadow()) : nullptr);
}

inline void StyleBuilderCustom::applyValueBoxShadow(StyleResolver& styleResolver, CSSValue& value)
{
    applyTextOrBoxShadowValue<CSSPropertyBoxShadow>(styleResolver, value);
}

inline void StyleBuilderCustom::applyInitialWebkitBoxShadow(StyleResolver& styleResolver)
{
    applyInitialBoxShadow(styleResolver);
}

inline void StyleBuilderCustom::applyInheritWebkitBoxShadow(StyleResolver& styleResolver)
{
    applyInheritBoxShadow(styleResolver);
}

inline void StyleBuilderCustom::applyValueWebkitBoxShadow(StyleResolver& styleResolver, CSSValue& value)
{
    applyTextOrBoxShadowValue<CSSPropertyWebkitBoxShadow>(styleResolver, value);
}

inline void StyleBuilderCustom::applyInitialFontFamily(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.style()->fontDescription();
    auto initialDesc = FontCascadeDescription();

    // We need to adjust the size to account for the generic family change from monospace to non-monospace.
    if (fontDescription.useFixedDefaultSize()) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier())
            styleResolver.setFontSize(fontDescription, Style::fontSizeForKeyword(sizeIdentifier, false, styleResolver.document()));
    }
    if (!initialDesc.firstFamily().isEmpty())
        fontDescription.setFamilies(initialDesc.families());

    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInheritFontFamily(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.style()->fontDescription();
    auto parentFontDescription = styleResolver.parentStyle()->fontDescription();

    fontDescription.setFamilies(parentFontDescription.families());
    fontDescription.setIsSpecifiedFont(parentFontDescription.isSpecifiedFont());
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyValueFontFamily(StyleResolver& styleResolver, CSSValue& value)
{
    auto& valueList = downcast<CSSValueList>(value);

    auto fontDescription = styleResolver.style()->fontDescription();
    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();

    Vector<AtomicString> families;
    families.reserveInitialCapacity(valueList.length());

    for (auto& item : valueList) {
        auto& contentValue = downcast<CSSPrimitiveValue>(item.get());
        AtomicString family;
        bool isGenericFamily = false;
        if (contentValue.isFontFamily()) {
            const CSSFontFamily& fontFamily = contentValue.fontFamily();
            family = fontFamily.familyName;
            // If the family name was resolved by the CSS parser from a system font ID, then it is generic.
            isGenericFamily = fontFamily.fromSystemFontID;
        } else {
            switch (contentValue.valueID()) {
            case CSSValueWebkitBody:
                family = styleResolver.settings().standardFontFamily();
                break;
            case CSSValueSerif:
                family = serifFamily;
                isGenericFamily = true;
                break;
            case CSSValueSansSerif:
                family = sansSerifFamily;
                isGenericFamily = true;
                break;
            case CSSValueCursive:
                family = cursiveFamily;
                isGenericFamily = true;
                break;
            case CSSValueFantasy:
                family = fantasyFamily;
                isGenericFamily = true;
                break;
            case CSSValueMonospace:
                family = monospaceFamily;
                isGenericFamily = true;
                break;
            case CSSValueWebkitPictograph:
                family = pictographFamily;
                isGenericFamily = true;
                break;
            case CSSValueSystemUi:
                family = systemUiFamily;
                isGenericFamily = true;
                break;
            default:
                break;
            }
        }

        if (family.isEmpty())
            continue;
        if (families.isEmpty())
            fontDescription.setIsSpecifiedFont(!isGenericFamily);
        families.uncheckedAppend(family);
    }

    if (families.isEmpty())
        return;
    fontDescription.setFamilies(families);

    if (fontDescription.useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize) {
        if (CSSValueID sizeIdentifier = fontDescription.keywordSizeAsIdentifier())
            styleResolver.setFontSize(fontDescription, Style::fontSizeForKeyword(sizeIdentifier, !oldFamilyUsedFixedDefaultSize, styleResolver.document()));
    }

    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline bool StyleBuilderCustom::isValidDisplayValue(StyleResolver& styleResolver, DisplayType display)
{
    if (is<SVGElement>(styleResolver.element()) && styleResolver.style()->styleType() == PseudoId::None)
        return display == DisplayType::Inline || display == DisplayType::Block || display == DisplayType::None;
    return true;
}

inline void StyleBuilderCustom::applyInheritDisplay(StyleResolver& styleResolver)
{
    DisplayType display = styleResolver.parentStyle()->display();
    if (isValidDisplayValue(styleResolver, display))
        styleResolver.style()->setDisplay(display);
}

inline void StyleBuilderCustom::applyValueDisplay(StyleResolver& styleResolver, CSSValue& value)
{
    DisplayType display = downcast<CSSPrimitiveValue>(value);
    if (isValidDisplayValue(styleResolver, display))
        styleResolver.style()->setDisplay(display);
}

inline void StyleBuilderCustom::applyValueBaselineShift(StyleResolver& styleResolver, CSSValue& value)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isValueID()) {
        switch (primitiveValue.valueID()) {
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
        svgStyle.setBaselineShiftValue(SVGLengthValue::fromCSSPrimitiveValue(primitiveValue));
    }
}

inline void StyleBuilderCustom::applyInitialWebkitAspectRatio(StyleResolver& styleResolver)
{
    styleResolver.style()->setAspectRatioType(RenderStyle::initialAspectRatioType());
    styleResolver.style()->setAspectRatioDenominator(RenderStyle::initialAspectRatioDenominator());
    styleResolver.style()->setAspectRatioNumerator(RenderStyle::initialAspectRatioNumerator());
}

inline void StyleBuilderCustom::applyInheritWebkitAspectRatio(StyleResolver& styleResolver)
{
    if (styleResolver.parentStyle()->aspectRatioType() == AspectRatioType::Auto)
        return;
    styleResolver.style()->setAspectRatioType(styleResolver.parentStyle()->aspectRatioType());
    styleResolver.style()->setAspectRatioDenominator(styleResolver.parentStyle()->aspectRatioDenominator());
    styleResolver.style()->setAspectRatioNumerator(styleResolver.parentStyle()->aspectRatioNumerator());
}

inline void StyleBuilderCustom::applyValueWebkitAspectRatio(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

        if (primitiveValue.valueID() == CSSValueFromDimensions)
            return styleResolver.style()->setAspectRatioType(AspectRatioType::FromDimensions);
        if (primitiveValue.valueID() == CSSValueFromIntrinsic)
            return styleResolver.style()->setAspectRatioType(AspectRatioType::FromIntrinsic);

        ASSERT(primitiveValue.valueID() == CSSValueAuto);
        return styleResolver.style()->setAspectRatioType(AspectRatioType::Auto);
    }

    auto& aspectRatioValue = downcast<CSSAspectRatioValue>(value);
    styleResolver.style()->setAspectRatioType(AspectRatioType::Specified);
    styleResolver.style()->setAspectRatioDenominator(aspectRatioValue.denominatorValue());
    styleResolver.style()->setAspectRatioNumerator(aspectRatioValue.numeratorValue());
}

inline void StyleBuilderCustom::applyInitialWebkitTextEmphasisStyle(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextEmphasisFill(RenderStyle::initialTextEmphasisFill());
    styleResolver.style()->setTextEmphasisMark(RenderStyle::initialTextEmphasisMark());
    styleResolver.style()->setTextEmphasisCustomMark(RenderStyle::initialTextEmphasisCustomMark());
}

inline void StyleBuilderCustom::applyInheritWebkitTextEmphasisStyle(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextEmphasisFill(styleResolver.parentStyle()->textEmphasisFill());
    styleResolver.style()->setTextEmphasisMark(styleResolver.parentStyle()->textEmphasisMark());
    styleResolver.style()->setTextEmphasisCustomMark(styleResolver.parentStyle()->textEmphasisCustomMark());
}

inline void StyleBuilderCustom::applyValueWebkitTextEmphasisStyle(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSValueList>(value)) {
        auto& list = downcast<CSSValueList>(value);
        ASSERT(list.length() == 2);

        for (auto& item : list) {
            CSSPrimitiveValue& value = downcast<CSSPrimitiveValue>(item.get());
            if (value.valueID() == CSSValueFilled || value.valueID() == CSSValueOpen)
                styleResolver.style()->setTextEmphasisFill(value);
            else
                styleResolver.style()->setTextEmphasisMark(value);
        }
        styleResolver.style()->setTextEmphasisCustomMark(nullAtom());
        return;
    }

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isString()) {
        styleResolver.style()->setTextEmphasisFill(TextEmphasisFill::Filled);
        styleResolver.style()->setTextEmphasisMark(TextEmphasisMark::Custom);
        styleResolver.style()->setTextEmphasisCustomMark(primitiveValue.stringValue());
        return;
    }

    styleResolver.style()->setTextEmphasisCustomMark(nullAtom());

    if (primitiveValue.valueID() == CSSValueFilled || primitiveValue.valueID() == CSSValueOpen) {
        styleResolver.style()->setTextEmphasisFill(primitiveValue);
        styleResolver.style()->setTextEmphasisMark(TextEmphasisMark::Auto);
    } else {
        styleResolver.style()->setTextEmphasisFill(TextEmphasisFill::Filled);
        styleResolver.style()->setTextEmphasisMark(primitiveValue);
    }
}

template <StyleBuilderCustom::CounterBehavior counterBehavior>
inline void StyleBuilderCustom::applyInheritCounter(StyleResolver& styleResolver)
{
    auto& map = styleResolver.style()->accessCounterDirectives();
    for (auto& keyValue : const_cast<RenderStyle*>(styleResolver.parentStyle())->accessCounterDirectives()) {
        auto& directives = map.add(keyValue.key, CounterDirectives { }).iterator->value;
        if (counterBehavior == Reset)
            directives.resetValue = keyValue.value.resetValue;
        else
            directives.incrementValue = keyValue.value.incrementValue;
    }
}

template <StyleBuilderCustom::CounterBehavior counterBehavior>
inline void StyleBuilderCustom::applyValueCounter(StyleResolver& styleResolver, CSSValue& value)
{
    bool setCounterIncrementToNone = counterBehavior == Increment && is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone;

    if (!is<CSSValueList>(value) && !setCounterIncrementToNone)
        return;

    CounterDirectiveMap& map = styleResolver.style()->accessCounterDirectives();
    for (auto& keyValue : map) {
        if (counterBehavior == Reset)
            keyValue.value.resetValue = WTF::nullopt;
        else
            keyValue.value.incrementValue = WTF::nullopt;
    }

    if (setCounterIncrementToNone)
        return;

    for (auto& item : downcast<CSSValueList>(value)) {
        Pair* pair = downcast<CSSPrimitiveValue>(item.get()).pairValue();
        AtomicString identifier = pair->first()->stringValue();
        int value = pair->second()->intValue();
        auto& directives = map.add(identifier, CounterDirectives { }).iterator->value;
        if (counterBehavior == Reset)
            directives.resetValue = value;
        else
            directives.incrementValue = saturatedAddition(directives.incrementValue.valueOr(0), value);
    }
}

inline void StyleBuilderCustom::applyInitialCounterIncrement(StyleResolver&) { }

inline void StyleBuilderCustom::applyInheritCounterIncrement(StyleResolver& styleResolver)
{
    applyInheritCounter<Increment>(styleResolver);
}

inline void StyleBuilderCustom::applyValueCounterIncrement(StyleResolver& styleResolver, CSSValue& value)
{
    applyValueCounter<Increment>(styleResolver, value);
}

inline void StyleBuilderCustom::applyInitialCounterReset(StyleResolver&) { }

inline void StyleBuilderCustom::applyInheritCounterReset(StyleResolver& styleResolver)
{
    applyInheritCounter<Reset>(styleResolver);
}

inline void StyleBuilderCustom::applyValueCounterReset(StyleResolver& styleResolver, CSSValue& value)
{
    applyValueCounter<Reset>(styleResolver, value);
}

inline void StyleBuilderCustom::applyInitialCursor(StyleResolver& styleResolver)
{
    styleResolver.style()->clearCursorList();
    styleResolver.style()->setCursor(RenderStyle::initialCursor());
}

inline void StyleBuilderCustom::applyInheritCursor(StyleResolver& styleResolver)
{
    styleResolver.style()->setCursor(styleResolver.parentStyle()->cursor());
    styleResolver.style()->setCursorList(styleResolver.parentStyle()->cursors());
}

inline void StyleBuilderCustom::applyValueCursor(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.style()->clearCursorList();
    if (is<CSSPrimitiveValue>(value)) {
        CursorType cursor = downcast<CSSPrimitiveValue>(value);
        if (styleResolver.style()->cursor() != cursor)
            styleResolver.style()->setCursor(cursor);
        return;
    }

    styleResolver.style()->setCursor(CursorType::Auto);
    auto& list = downcast<CSSValueList>(value);
    for (auto& item : list) {
        if (is<CSSCursorImageValue>(item)) {
            auto& image = downcast<CSSCursorImageValue>(item.get());
            styleResolver.style()->addCursor(styleResolver.styleImage(image), image.hotSpot());
            continue;
        }

        styleResolver.style()->setCursor(downcast<CSSPrimitiveValue>(item.get()));
        ASSERT_WITH_MESSAGE(item.ptr() == list.item(list.length() - 1), "Cursor ID fallback should always be last in the list");
        return;
    }
}

inline void StyleBuilderCustom::applyInitialFill(StyleResolver& styleResolver)
{
    auto& svgStyle = styleResolver.style()->accessSVGStyle();
    svgStyle.setFillPaint(SVGRenderStyle::initialFillPaintType(), SVGRenderStyle::initialFillPaintColor(), SVGRenderStyle::initialFillPaintUri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyInheritFill(StyleResolver& styleResolver)
{
    auto& svgStyle = styleResolver.style()->accessSVGStyle();
    auto& svgParentStyle = styleResolver.parentStyle()->svgStyle();
    svgStyle.setFillPaint(svgParentStyle.fillPaintType(), svgParentStyle.fillPaintColor(), svgParentStyle.fillPaintUri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());

}

inline void StyleBuilderCustom::applyValueFill(StyleResolver& styleResolver, CSSValue& value)
{
    auto& svgStyle = styleResolver.style()->accessSVGStyle();
    const auto* localValue = value.isPrimitiveValue() ? &downcast<CSSPrimitiveValue>(value) : nullptr;
    String url;
    if (value.isValueList()) {
        const CSSValueList& list = downcast<CSSValueList>(value);
        url = downcast<CSSPrimitiveValue>(list.item(0))->stringValue();
        localValue = downcast<CSSPrimitiveValue>(list.item(1));
    }

    if (!localValue)
        return;

    Color color;
    auto paintType = SVGPaintType::RGBColor;
    if (localValue->isURI()) {
        paintType = SVGPaintType::URI;
        url = localValue->stringValue();
    } else if (localValue->isValueID() && localValue->valueID() == CSSValueNone)
        paintType = url.isEmpty() ? SVGPaintType::None : SVGPaintType::URINone;
    else if (localValue->isValueID() && localValue->valueID() == CSSValueCurrentcolor) {
        color = styleResolver.style()->color();
        paintType = url.isEmpty() ? SVGPaintType::CurrentColor : SVGPaintType::URICurrentColor;
    } else {
        color = styleResolver.colorFromPrimitiveValue(*localValue);
        paintType = url.isEmpty() ? SVGPaintType::RGBColor : SVGPaintType::URIRGBColor;
    }
    svgStyle.setFillPaint(paintType, color, url, styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyInitialStroke(StyleResolver& styleResolver)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    svgStyle.setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyInheritStroke(StyleResolver& styleResolver)
{
    auto& svgStyle = styleResolver.style()->accessSVGStyle();
    auto& svgParentStyle = styleResolver.parentStyle()->svgStyle();
    svgStyle.setStrokePaint(svgParentStyle.strokePaintType(), svgParentStyle.strokePaintColor(), svgParentStyle.strokePaintUri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyValueStroke(StyleResolver& styleResolver, CSSValue& value)
{
    auto& svgStyle = styleResolver.style()->accessSVGStyle();
    const auto* localValue = value.isPrimitiveValue() ? &downcast<CSSPrimitiveValue>(value) : nullptr;
    String url;
    if (value.isValueList()) {
        const CSSValueList& list = downcast<CSSValueList>(value);
        url = downcast<CSSPrimitiveValue>(list.item(0))->stringValue();
        localValue = downcast<CSSPrimitiveValue>(list.item(1));
    }

    if (!localValue)
        return;

    Color color;
    auto paintType = SVGPaintType::RGBColor;
    if (localValue->isURI()) {
        paintType = SVGPaintType::URI;
        url = downcast<CSSPrimitiveValue>(localValue)->stringValue();
    } else if (localValue->isValueID() && localValue->valueID() == CSSValueNone)
        paintType = url.isEmpty() ? SVGPaintType::None : SVGPaintType::URINone;
    else if (localValue->isValueID() && localValue->valueID() == CSSValueCurrentcolor) {
        color = styleResolver.style()->color();
        paintType = url.isEmpty() ? SVGPaintType::CurrentColor : SVGPaintType::URICurrentColor;
    } else {
        color = styleResolver.colorFromPrimitiveValue(*localValue);
        paintType = url.isEmpty() ? SVGPaintType::RGBColor : SVGPaintType::URIRGBColor;
    }
    svgStyle.setStrokePaint(paintType, color, url, styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyInitialContent(StyleResolver& styleResolver)
{
    styleResolver.style()->clearContent();
}

inline void StyleBuilderCustom::applyInheritContent(StyleResolver&)
{
    // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not. This
    // note is a reminder that eventually "inherit" needs to be supported.
}

inline void StyleBuilderCustom::applyValueContent(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        const auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
        ASSERT_UNUSED(primitiveValue, primitiveValue.valueID() == CSSValueNormal || primitiveValue.valueID() == CSSValueNone);
        styleResolver.style()->clearContent();
        return;
    }

    bool didSet = false;
    for (auto& item : downcast<CSSValueList>(value)) {
        if (is<CSSImageGeneratorValue>(item)) {
            if (is<CSSGradientValue>(item))
                styleResolver.style()->setContent(StyleGeneratedImage::create(downcast<CSSGradientValue>(item.get()).gradientWithStylesResolved(styleResolver)), didSet);
            else
                styleResolver.style()->setContent(StyleGeneratedImage::create(downcast<CSSImageGeneratorValue>(item.get())), didSet);
            didSet = true;
        } else if (is<CSSImageSetValue>(item)) {
            styleResolver.style()->setContent(StyleCachedImage::create(item), didSet);
            didSet = true;
        }

        if (is<CSSImageValue>(item)) {
            styleResolver.style()->setContent(StyleCachedImage::create(item), didSet);
            didSet = true;
            continue;
        }

        if (!is<CSSPrimitiveValue>(item))
            continue;

        auto& contentValue = downcast<CSSPrimitiveValue>(item.get());
        if (contentValue.isString()) {
            styleResolver.style()->setContent(contentValue.stringValue().impl(), didSet);
            didSet = true;
        } else if (contentValue.isAttr()) {
            // FIXME: Can a namespace be specified for an attr(foo)?
            if (styleResolver.style()->styleType() == PseudoId::None)
                styleResolver.style()->setHasAttrContent();
            else
                const_cast<RenderStyle*>(styleResolver.parentStyle())->setHasAttrContent();
            QualifiedName attr(nullAtom(), contentValue.stringValue().impl(), nullAtom());
            const AtomicString& value = styleResolver.element()->getAttribute(attr);
            styleResolver.style()->setContent(value.isNull() ? emptyAtom() : value.impl(), didSet);
            didSet = true;
            // Register the fact that the attribute value affects the style.
            styleResolver.ruleSets().mutableFeatures().registerContentAttribute(attr.localName());
        } else if (contentValue.isCounter()) {
            auto* counterValue = contentValue.counterValue();
            ListStyleType listStyleType = ListStyleType::None;
            CSSValueID listStyleIdent = counterValue->listStyleIdent();
            if (listStyleIdent != CSSValueNone)
                listStyleType = static_cast<ListStyleType>(listStyleIdent - CSSValueDisc);
            auto counter = std::make_unique<CounterContent>(counterValue->identifier(), listStyleType, counterValue->separator());
            styleResolver.style()->setContent(WTFMove(counter), didSet);
            didSet = true;
        } else {
            switch (contentValue.valueID()) {
            case CSSValueOpenQuote:
                styleResolver.style()->setContent(QuoteType::OpenQuote, didSet);
                didSet = true;
                break;
            case CSSValueCloseQuote:
                styleResolver.style()->setContent(QuoteType::CloseQuote, didSet);
                didSet = true;
                break;
            case CSSValueNoOpenQuote:
                styleResolver.style()->setContent(QuoteType::NoOpenQuote, didSet);
                didSet = true;
                break;
            case CSSValueNoCloseQuote:
                styleResolver.style()->setContent(QuoteType::NoCloseQuote, didSet);
                didSet = true;
                break;
            default:
                // normal and none do not have any effect.
                break;
            }
        }
    }
    if (!didSet)
        styleResolver.style()->clearContent();
}

inline void StyleBuilderCustom::applyInheritFontVariantLigatures(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantCommonLigatures(styleResolver.parentFontDescription().variantCommonLigatures());
    fontDescription.setVariantDiscretionaryLigatures(styleResolver.parentFontDescription().variantDiscretionaryLigatures());
    fontDescription.setVariantHistoricalLigatures(styleResolver.parentFontDescription().variantHistoricalLigatures());
    fontDescription.setVariantContextualAlternates(styleResolver.parentFontDescription().variantContextualAlternates());
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInitialFontVariantLigatures(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantCommonLigatures(FontVariantLigatures::Normal);
    fontDescription.setVariantDiscretionaryLigatures(FontVariantLigatures::Normal);
    fontDescription.setVariantHistoricalLigatures(FontVariantLigatures::Normal);
    fontDescription.setVariantContextualAlternates(FontVariantLigatures::Normal);
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyValueFontVariantLigatures(StyleResolver& styleResolver, CSSValue& value)
{
    auto fontDescription = styleResolver.fontDescription();
    auto variantLigatures = extractFontVariantLigatures(value);
    fontDescription.setVariantCommonLigatures(variantLigatures.commonLigatures);
    fontDescription.setVariantDiscretionaryLigatures(variantLigatures.discretionaryLigatures);
    fontDescription.setVariantHistoricalLigatures(variantLigatures.historicalLigatures);
    fontDescription.setVariantContextualAlternates(variantLigatures.contextualAlternates);
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInheritFontVariantNumeric(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantNumericFigure(styleResolver.parentFontDescription().variantNumericFigure());
    fontDescription.setVariantNumericSpacing(styleResolver.parentFontDescription().variantNumericSpacing());
    fontDescription.setVariantNumericFraction(styleResolver.parentFontDescription().variantNumericFraction());
    fontDescription.setVariantNumericOrdinal(styleResolver.parentFontDescription().variantNumericOrdinal());
    fontDescription.setVariantNumericSlashedZero(styleResolver.parentFontDescription().variantNumericSlashedZero());
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInitialFontVariantNumeric(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantNumericFigure(FontVariantNumericFigure::Normal);
    fontDescription.setVariantNumericSpacing(FontVariantNumericSpacing::Normal);
    fontDescription.setVariantNumericFraction(FontVariantNumericFraction::Normal);
    fontDescription.setVariantNumericOrdinal(FontVariantNumericOrdinal::Normal);
    fontDescription.setVariantNumericSlashedZero(FontVariantNumericSlashedZero::Normal);
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyValueFontVariantNumeric(StyleResolver& styleResolver, CSSValue& value)
{
    auto fontDescription = styleResolver.fontDescription();
    auto variantNumeric = extractFontVariantNumeric(value);
    fontDescription.setVariantNumericFigure(variantNumeric.figure);
    fontDescription.setVariantNumericSpacing(variantNumeric.spacing);
    fontDescription.setVariantNumericFraction(variantNumeric.fraction);
    fontDescription.setVariantNumericOrdinal(variantNumeric.ordinal);
    fontDescription.setVariantNumericSlashedZero(variantNumeric.slashedZero);
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInheritFontVariantEastAsian(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantEastAsianVariant(styleResolver.parentFontDescription().variantEastAsianVariant());
    fontDescription.setVariantEastAsianWidth(styleResolver.parentFontDescription().variantEastAsianWidth());
    fontDescription.setVariantEastAsianRuby(styleResolver.parentFontDescription().variantEastAsianRuby());
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInitialFontVariantEastAsian(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantEastAsianVariant(FontVariantEastAsianVariant::Normal);
    fontDescription.setVariantEastAsianWidth(FontVariantEastAsianWidth::Normal);
    fontDescription.setVariantEastAsianRuby(FontVariantEastAsianRuby::Normal);
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyValueFontVariantEastAsian(StyleResolver& styleResolver, CSSValue& value)
{
    auto fontDescription = styleResolver.fontDescription();
    auto variantEastAsian = extractFontVariantEastAsian(value);
    fontDescription.setVariantEastAsianVariant(variantEastAsian.variant);
    fontDescription.setVariantEastAsianWidth(variantEastAsian.width);
    fontDescription.setVariantEastAsianRuby(variantEastAsian.ruby);
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInitialFontSize(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.style()->fontDescription();
    float size = Style::fontSizeForKeyword(CSSValueMedium, fontDescription.useFixedDefaultSize(), styleResolver.document());

    if (size < 0)
        return;

    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);
    styleResolver.setFontSize(fontDescription, size);
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInheritFontSize(StyleResolver& styleResolver)
{
    const auto& parentFontDescription = styleResolver.parentStyle()->fontDescription();
    float size = parentFontDescription.specifiedSize();

    if (size < 0)
        return;

    auto fontDescription = styleResolver.style()->fontDescription();
    fontDescription.setKeywordSize(parentFontDescription.keywordSize());
    styleResolver.setFontSize(fontDescription, size);
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

// When the CSS keyword "larger" is used, this function will attempt to match within the keyword
// table, and failing that, will simply multiply by 1.2.
inline float StyleBuilderCustom::largerFontSize(float size)
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
    // the next size level.
    return size * 1.2f;
}

// Like the previous function, but for the keyword "smaller".
inline float StyleBuilderCustom::smallerFontSize(float size)
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
    // the next size level.
    return size / 1.2f;
}

inline float StyleBuilderCustom::determineRubyTextSizeMultiplier(StyleResolver& styleResolver)
{
    if (styleResolver.style()->rubyPosition() != RubyPosition::InterCharacter)
        return 0.5f;

    // FIXME: This hack is to ensure tone marks are the same size as
    // the bopomofo. This code will go away if we make a special renderer
    // for the tone marks eventually.
    if (auto* element = styleResolver.state().element()) {
        for (auto& ancestor : ancestorsOfType<HTMLElement>(*element)) {
            if (ancestor.hasTagName(HTMLNames::rtTag))
                return 1.0f;
        }
    }
    return 0.25f;
}

inline void StyleBuilderCustom::applyInitialFontStyle(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setItalic(FontCascadeDescription::initialItalic());
    fontDescription.setFontStyleAxis(FontCascadeDescription::initialFontStyleAxis());
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInheritFontStyle(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setItalic(styleResolver.parentFontDescription().italic());
    fontDescription.setFontStyleAxis(styleResolver.parentFontDescription().fontStyleAxis());
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyValueFontStyle(StyleResolver& styleResolver, CSSValue& value)
{
    auto& fontStyleValue = downcast<CSSFontStyleValue>(value);
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setItalic(StyleBuilderConverter::convertFontStyleFromValue(fontStyleValue));
    fontDescription.setFontStyleAxis(fontStyleValue.fontStyleValue->valueID() == CSSValueItalic ? FontStyleAxis::ital : FontStyleAxis::slnt);
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyValueFontSize(StyleResolver& styleResolver, CSSValue& value)
{
    auto fontDescription = styleResolver.style()->fontDescription();
    fontDescription.setKeywordSizeFromIdentifier(CSSValueInvalid);

    float parentSize = 0;
    bool parentIsAbsoluteSize = false;
    if (auto* parentStyle = styleResolver.parentStyle()) {
        parentSize = parentStyle->fontDescription().specifiedSize();
        parentIsAbsoluteSize = parentStyle->fontDescription().isAbsoluteSize();
    }

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    float size;
    if (CSSValueID ident = primitiveValue.valueID()) {
        fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize && (ident == CSSValueLarger || ident == CSSValueSmaller || ident == CSSValueWebkitRubyText));

        // Keywords are being used.
        switch (ident) {
        case CSSValueXxSmall:
        case CSSValueXSmall:
        case CSSValueSmall:
        case CSSValueMedium:
        case CSSValueLarge:
        case CSSValueXLarge:
        case CSSValueXxLarge:
        case CSSValueWebkitXxxLarge:
            size = Style::fontSizeForKeyword(ident, fontDescription.useFixedDefaultSize(), styleResolver.document());
            fontDescription.setKeywordSizeFromIdentifier(ident);
            break;
        case CSSValueLarger:
            size = largerFontSize(parentSize);
            break;
        case CSSValueSmaller:
            size = smallerFontSize(parentSize);
            break;
        case CSSValueWebkitRubyText:
            size = determineRubyTextSizeMultiplier(styleResolver) * parentSize;
            break;
        default:
            return;
        }
    } else {
        fontDescription.setIsAbsoluteSize(parentIsAbsoluteSize || !(primitiveValue.isPercentage() || primitiveValue.isFontRelativeLength()));
        if (primitiveValue.isLength()) {
            size = primitiveValue.computeLength<float>(CSSToLengthConversionData(styleResolver.parentStyle(), styleResolver.rootElementStyle(), styleResolver.document().renderView(), 1.0f, true));
            styleResolver.state().setFontSizeHasViewportUnits(primitiveValue.isViewportPercentageLength());
        } else if (primitiveValue.isPercentage())
            size = (primitiveValue.floatValue() * parentSize) / 100.0f;
        else if (primitiveValue.isCalculatedPercentageWithLength()) {
            const auto& conversionData = styleResolver.state().cssToLengthConversionData();
            CSSToLengthConversionData parentConversionData { styleResolver.parentStyle(), conversionData.rootStyle(), styleResolver.document().renderView(), 1.0f, true };
            size = primitiveValue.cssCalcValue()->createCalculationValue(parentConversionData)->evaluate(parentSize);
        } else
            return;
    }

    if (size < 0)
        return;

    styleResolver.setFontSize(fontDescription, std::min(maximumAllowedFontSize, size));
    styleResolver.setFontDescription(WTFMove(fontDescription));
}

inline void StyleBuilderCustom::applyInitialGridTemplateAreas(StyleResolver& styleResolver)
{
    styleResolver.style()->setNamedGridArea(RenderStyle::initialNamedGridArea());
    styleResolver.style()->setNamedGridAreaRowCount(RenderStyle::initialNamedGridAreaCount());
    styleResolver.style()->setNamedGridAreaColumnCount(RenderStyle::initialNamedGridAreaCount());
}

inline void StyleBuilderCustom::applyInheritGridTemplateAreas(StyleResolver& styleResolver)
{
    styleResolver.style()->setNamedGridArea(styleResolver.parentStyle()->namedGridArea());
    styleResolver.style()->setNamedGridAreaRowCount(styleResolver.parentStyle()->namedGridAreaRowCount());
    styleResolver.style()->setNamedGridAreaColumnCount(styleResolver.parentStyle()->namedGridAreaColumnCount());
}

inline void StyleBuilderCustom::applyValueGridTemplateAreas(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueNone);
        return;
    }

    auto& gridTemplateAreasValue = downcast<CSSGridTemplateAreasValue>(value);
    const NamedGridAreaMap& newNamedGridAreas = gridTemplateAreasValue.gridAreaMap();

    NamedGridLinesMap namedGridColumnLines = styleResolver.style()->namedGridColumnLines();
    NamedGridLinesMap namedGridRowLines = styleResolver.style()->namedGridRowLines();
    StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(newNamedGridAreas, namedGridColumnLines, ForColumns);
    StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(newNamedGridAreas, namedGridRowLines, ForRows);
    styleResolver.style()->setNamedGridColumnLines(namedGridColumnLines);
    styleResolver.style()->setNamedGridRowLines(namedGridRowLines);

    styleResolver.style()->setNamedGridArea(gridTemplateAreasValue.gridAreaMap());
    styleResolver.style()->setNamedGridAreaRowCount(gridTemplateAreasValue.rowCount());
    styleResolver.style()->setNamedGridAreaColumnCount(gridTemplateAreasValue.columnCount());
}

inline void StyleBuilderCustom::applyInitialGridTemplateColumns(StyleResolver& styleResolver)
{
    styleResolver.style()->setGridColumns(RenderStyle::initialGridColumns());
    styleResolver.style()->setNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines());
    styleResolver.style()->setOrderedNamedGridColumnLines(RenderStyle::initialOrderedNamedGridColumnLines());
}

inline void StyleBuilderCustom::applyInheritGridTemplateColumns(StyleResolver& styleResolver)
{
    styleResolver.style()->setGridColumns(styleResolver.parentStyle()->gridColumns());
    styleResolver.style()->setNamedGridColumnLines(styleResolver.parentStyle()->namedGridColumnLines());
    styleResolver.style()->setOrderedNamedGridColumnLines(styleResolver.parentStyle()->orderedNamedGridColumnLines());
}

#define SET_TRACKS_DATA(tracksData, style, TrackType) \
    style->setGrid##TrackType##s(tracksData.m_trackSizes); \
    style->setNamedGrid##TrackType##Lines(tracksData.m_namedGridLines); \
    style->setOrderedNamedGrid##TrackType##Lines(tracksData.m_orderedNamedGridLines); \
    style->setGridAutoRepeat##TrackType##s(tracksData.m_autoRepeatTrackSizes); \
    style->setGridAutoRepeat##TrackType##sInsertionPoint(tracksData.m_autoRepeatInsertionPoint); \
    style->setAutoRepeatNamedGrid##TrackType##Lines(tracksData.m_autoRepeatNamedGridLines); \
    style->setAutoRepeatOrderedNamedGrid##TrackType##Lines(tracksData.m_autoRepeatOrderedNamedGridLines); \
    style->setGridAutoRepeat##TrackType##sType(tracksData.m_autoRepeatType); \
    style->setGridAutoRepeat##TrackType##sInsertionPoint(tracksData.m_autoRepeatInsertionPoint);

inline void StyleBuilderCustom::applyValueGridTemplateColumns(StyleResolver& styleResolver, CSSValue& value)
{
    StyleBuilderConverter::TracksData tracksData;
    if (!StyleBuilderConverter::createGridTrackList(value, tracksData, styleResolver))
        return;
    const NamedGridAreaMap& namedGridAreas = styleResolver.style()->namedGridArea();
    if (!namedGridAreas.isEmpty())
        StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(namedGridAreas, tracksData.m_namedGridLines, ForColumns);

    SET_TRACKS_DATA(tracksData, styleResolver.style(), Column);
}

inline void StyleBuilderCustom::applyInitialGridTemplateRows(StyleResolver& styleResolver)
{
    styleResolver.style()->setGridRows(RenderStyle::initialGridRows());
    styleResolver.style()->setNamedGridRowLines(RenderStyle::initialNamedGridRowLines());
    styleResolver.style()->setOrderedNamedGridRowLines(RenderStyle::initialOrderedNamedGridRowLines());
}

inline void StyleBuilderCustom::applyInheritGridTemplateRows(StyleResolver& styleResolver)
{
    styleResolver.style()->setGridRows(styleResolver.parentStyle()->gridRows());
    styleResolver.style()->setNamedGridRowLines(styleResolver.parentStyle()->namedGridRowLines());
    styleResolver.style()->setOrderedNamedGridRowLines(styleResolver.parentStyle()->orderedNamedGridRowLines());
}

inline void StyleBuilderCustom::applyValueGridTemplateRows(StyleResolver& styleResolver, CSSValue& value)
{
    StyleBuilderConverter::TracksData tracksData;
    if (!StyleBuilderConverter::createGridTrackList(value, tracksData, styleResolver))
        return;
    const NamedGridAreaMap& namedGridAreas = styleResolver.style()->namedGridArea();
    if (!namedGridAreas.isEmpty())
        StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(namedGridAreas, tracksData.m_namedGridLines, ForRows);

    SET_TRACKS_DATA(tracksData, styleResolver.style(), Row);
}

void StyleBuilderCustom::applyValueAlt(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isString())
        styleResolver.style()->setContentAltText(primitiveValue.stringValue());
    else if (primitiveValue.isAttr()) {
        // FIXME: Can a namespace be specified for an attr(foo)?
        if (styleResolver.style()->styleType() == PseudoId::None)
            styleResolver.style()->setUnique();
        else
            const_cast<RenderStyle*>(styleResolver.parentStyle())->setUnique();

        QualifiedName attr(nullAtom(), primitiveValue.stringValue(), nullAtom());
        const AtomicString& value = styleResolver.element()->getAttribute(attr);
        styleResolver.style()->setContentAltText(value.isNull() ? emptyAtom() : value);

        // Register the fact that the attribute value affects the style.
        styleResolver.ruleSets().mutableFeatures().registerContentAttribute(attr.localName());
    } else
        styleResolver.style()->setContentAltText(emptyAtom());
}

inline void StyleBuilderCustom::applyValueWillChange(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto);
        styleResolver.style()->setWillChange(nullptr);
        return;
    }

    auto willChange = WillChangeData::create();
    for (auto& item : downcast<CSSValueList>(value)) {
        if (!is<CSSPrimitiveValue>(item))
            continue;
        auto& primitiveValue = downcast<CSSPrimitiveValue>(item.get());
        switch (primitiveValue.valueID()) {
        case CSSValueScrollPosition:
            willChange->addFeature(WillChangeData::Feature::ScrollPosition);
            break;
        case CSSValueContents:
            willChange->addFeature(WillChangeData::Feature::Contents);
            break;
        default:
            if (primitiveValue.isPropertyID())
                willChange->addFeature(WillChangeData::Feature::Property, primitiveValue.propertyID());
            break;
        }
    }
    styleResolver.style()->setWillChange(WTFMove(willChange));
}

inline void StyleBuilderCustom::applyValueStrokeWidth(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.style()->setStrokeWidth(StyleBuilderConverter::convertLength(styleResolver, value));
    styleResolver.style()->setHasExplicitlySetStrokeWidth(true);
}

inline void StyleBuilderCustom::applyValueStrokeColor(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (styleResolver.applyPropertyToRegularStyle())
        styleResolver.style()->setStrokeColor(styleResolver.colorFromPrimitiveValue(primitiveValue, /* forVisitedLink */ false));
    if (styleResolver.applyPropertyToVisitedLinkStyle())
        styleResolver.style()->setVisitedLinkStrokeColor(styleResolver.colorFromPrimitiveValue(primitiveValue, /* forVisitedLink */ true));
    styleResolver.style()->setHasExplicitlySetStrokeColor(true);
}

inline void StyleBuilderCustom::applyInitialCustomProperty(StyleResolver& styleResolver, const CSSRegisteredCustomProperty* registered, const AtomicString& name)
{
    if (registered && registered->initialValue()) {
        auto initialValue = registered->initialValueCopy();
        applyValueCustomProperty(styleResolver, registered, *initialValue);
        return;
    }

    auto invalid = CSSCustomPropertyValue::createUnresolved(name, CSSValueInvalid);
    applyValueCustomProperty(styleResolver, registered, invalid.get());
}

inline void StyleBuilderCustom::applyInheritCustomProperty(StyleResolver& styleResolver, const CSSRegisteredCustomProperty* registered, const AtomicString& name)
{
    auto* parentValue = styleResolver.parentStyle() ? styleResolver.parentStyle()->inheritedCustomProperties().get(name) : nullptr;
    if (parentValue && !(registered && !registered->inherits))
        applyValueCustomProperty(styleResolver, registered, *parentValue);
    else
        applyInitialCustomProperty(styleResolver, registered, name);
}

inline void StyleBuilderCustom::applyValueCustomProperty(StyleResolver& styleResolver, const CSSRegisteredCustomProperty* registered, CSSCustomPropertyValue& value)
{
    ASSERT(value.isResolved());
    const auto& name = value.name();

    if (!registered || registered->inherits)
        styleResolver.style()->setInheritedCustomPropertyValue(name, makeRef(value));
    else
        styleResolver.style()->setNonInheritedCustomPropertyValue(name, makeRef(value));
}

} // namespace WebCore
