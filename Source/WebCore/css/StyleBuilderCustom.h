/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef StyleBuilderCustom_h
#define StyleBuilderCustom_h

#include "CSSAspectRatioValue.h"
#include "CSSCursorImageValue.h"
#include "CSSFontFamily.h"
#include "CSSFontValue.h"
#include "CSSGradientValue.h"
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
#include "RenderTheme.h"
#include "SVGElement.h"
#include "SVGRenderStyle.h"
#include "StyleBuilderConverter.h"
#include "StyleFontSizeFunctions.h"
#include "StyleGeneratedImage.h"
#include "StyleResolver.h"
#include "WillChangeData.h"

namespace WebCore {

#define DECLARE_PROPERTY_CUSTOM_HANDLERS(property) \
    static void applyInherit##property(StyleResolver&); \
    static void applyInitial##property(StyleResolver&); \
    static void applyValue##property(StyleResolver&, CSSValue&)

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
    DECLARE_PROPERTY_CUSTOM_HANDLERS(ColumnGap);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Content);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterIncrement);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(CounterReset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Cursor);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Fill);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontFamily);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontSize);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(FontWeight);
#if ENABLE(CSS_IMAGE_RESOLUTION)
    DECLARE_PROPERTY_CUSTOM_HANDLERS(ImageResolution);
#endif
#if ENABLE(IOS_TEXT_AUTOSIZING)
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
#if ENABLE(CSS_GRID_LAYOUT)
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitGridTemplateAreas);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitGridTemplateColumns);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitGridTemplateRows);
#endif // ENABLE(CSS_GRID_LAYOUT)
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitMaskBoxImageOutset);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitMaskBoxImageRepeat);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitMaskBoxImageSlice);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitMaskBoxImageWidth);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitSvgShadow);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(WebkitTextEmphasisStyle);
    DECLARE_PROPERTY_CUSTOM_HANDLERS(Zoom);

    // Custom handling of initial + inherit value setting only.
    static void applyInitialWebkitMaskImage(StyleResolver&) { }
    static void applyInheritWebkitMaskImage(StyleResolver&) { }
    static void applyInitialFontFeatureSettings(StyleResolver&) { }
    static void applyInheritFontFeatureSettings(StyleResolver&) { }

    // Custom handling of inherit + value setting only.
    static void applyInheritDisplay(StyleResolver&);
    static void applyValueDisplay(StyleResolver&, CSSValue&);

    // Custom handling of value setting only.
    static void applyValueBaselineShift(StyleResolver&, CSSValue&);
    static void applyValueDirection(StyleResolver&, CSSValue&);
    static void applyValueVerticalAlign(StyleResolver&, CSSValue&);
#if ENABLE(DASHBOARD_SUPPORT)
    static void applyValueWebkitDashboardRegion(StyleResolver&, CSSValue&);
#endif
    static void applyValueWebkitLocale(StyleResolver&, CSSValue&);
    static void applyValueWebkitTextOrientation(StyleResolver&, CSSValue&);
#if ENABLE(IOS_TEXT_AUTOSIZING)
    static void applyValueWebkitTextSizeAdjust(StyleResolver&, CSSValue&);
#endif
    static void applyValueWebkitTextZoom(StyleResolver&, CSSValue&);
    static void applyValueWebkitWritingMode(StyleResolver&, CSSValue&);
    static void applyValueAlt(StyleResolver&, CSSValue&);
#if ENABLE(CSS_SCROLL_SNAP)
    static void applyInitialWebkitScrollSnapPointsX(StyleResolver&);
    static void applyInheritWebkitScrollSnapPointsX(StyleResolver&);
    static void applyInitialWebkitScrollSnapPointsY(StyleResolver&);
    static void applyInheritWebkitScrollSnapPointsY(StyleResolver&);
#endif
    static void applyValueWillChange(StyleResolver&, CSSValue&);

private:
    static void resetEffectiveZoom(StyleResolver&);

    static Length mmLength(double mm);
    static Length inchLength(double inch);
    static bool getPageSizeFromName(CSSPrimitiveValue* pageSizeName, CSSPrimitiveValue* pageOrientation, Length& width, Length& height);

    template <CSSPropertyID id>
    static void applyTextOrBoxShadowValue(StyleResolver&, CSSValue&);
    static bool isValidDisplayValue(StyleResolver&, EDisplay);

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

    if (primitiveValue.getValueID() == CSSValueNormal) {
        resetEffectiveZoom(styleResolver);
        styleResolver.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue.getValueID() == CSSValueReset) {
        styleResolver.setEffectiveZoom(RenderStyle::initialZoom());
        styleResolver.setZoom(RenderStyle::initialZoom());
    } else if (primitiveValue.getValueID() == CSSValueDocument) {
        float docZoom = styleResolver.rootElementStyle() ? styleResolver.rootElementStyle()->zoom() : RenderStyle::initialZoom();
        styleResolver.setEffectiveZoom(docZoom);
        styleResolver.setZoom(docZoom);
    } else if (primitiveValue.isPercentage()) {
        resetEffectiveZoom(styleResolver);
        if (float percent = primitiveValue.getFloatValue())
            styleResolver.setZoom(percent / 100.0f);
    } else if (primitiveValue.isNumber()) {
        resetEffectiveZoom(styleResolver);
        if (float number = primitiveValue.getFloatValue())
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

    switch (pageSizeName->getValueID()) {
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
        switch (pageOrientation->getValueID()) {
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
    if (primitiveValue.getValueID())
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
    if (primitiveValue.getValueID() == CSSValueNone) {
        styleResolver.style()->setDashboardRegions(RenderStyle::noneDashboardRegions());
        return;
    }

    DashboardRegion* region = primitiveValue.getDashboardRegionValue();
    if (!region)
        return;

    DashboardRegion* first = region;
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
            styleResolver.style()->setDashboardRegion(StyleDashboardRegion::Circle, region->m_label, top, right, bottom, left, region == first ? false : true);
        else if (region->m_isRectangle)
            styleResolver.style()->setDashboardRegion(StyleDashboardRegion::Rectangle, region->m_label, top, right, bottom, left, region == first ? false : true);
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
        if (primitiveValue.getValueID() == CSSValueFromImage)
            source = ImageResolutionFromImage;
        else if (primitiveValue.getValueID() == CSSValueSnap)
            snap = ImageResolutionSnapPixels;
        else
            resolution = primitiveValue.getDoubleValue(CSSPrimitiveValue::CSS_DPPX);
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
    Length width;
    Length height;
    PageSizeType pageSizeType = PAGE_SIZE_AUTO;
    if (!is<CSSValueList>(value))
        return;

    auto& valueList = downcast<CSSValueList>(value);
    switch (valueList.length()) {
    case 2: {
        CSSValue* firstValue = valueList.itemWithoutBoundsCheck(0);
        CSSValue* secondValue = valueList.itemWithoutBoundsCheck(1);
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
        CSSValue* value = valueList.itemWithoutBoundsCheck(0);
        // <length> | auto | <page-size> | [ portrait | landscape]
        if (!is<CSSPrimitiveValue>(*value))
            return;
        auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
        if (primitiveValue.isLength()) {
            // <length>
            pageSizeType = PAGE_SIZE_RESOLVED;
            width = height = primitiveValue.computeLength<Length>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        } else {
            switch (primitiveValue.getValueID()) {
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
    styleResolver.style()->setPageSize(LengthSize(width, height));
}

inline void StyleBuilderCustom::applyInheritTextIndent(StyleResolver& styleResolver)
{
    styleResolver.style()->setTextIndent(styleResolver.parentStyle()->textIndent());
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
        if (!primitiveValue.getValueID())
            lengthOrPercentageValue = primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(styleResolver.state().cssToLengthConversionData());
#if ENABLE(CSS3_TEXT)
        else if (primitiveValue.getValueID() == CSSValueWebkitEachLine)
            textIndentLineValue = TextIndentEachLine;
        else if (primitiveValue.getValueID() == CSSValueWebkitHanging)
            textIndentTypeValue = TextIndentHanging;
#endif
    }

    if (lengthOrPercentageValue.isUndefined())
        return;

    styleResolver.style()->setTextIndent(lengthOrPercentageValue);
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
            // Masks have a different initial value for widths. They use an 'auto' value rather than trying to fit to the border.
            image.setBorderSlices(type == BorderImage ? LengthBox(Length(1, Relative), Length(1, Relative), Length(1, Relative), Length(1, Relative)) : LengthBox());
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
    static const NinePieceImage& getValue(RenderStyle* style)
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

#if ENABLE(IOS_TEXT_AUTOSIZING)

inline void StyleBuilderCustom::applyInheritLineHeight(StyleResolver& styleResolver)
{
    styleResolver.style()->setLineHeight(styleResolver.parentStyle()->lineHeight());
    styleResolver.style()->setSpecifiedLineHeight(styleResolver.parentStyle()->specifiedLineHeight());
}

inline void StyleBuilderCustom::applyInitialLineHeight(StyleResolver& styleResolver)
{
    styleResolver.style()->setLineHeight(RenderStyle::initialLineHeight());
    styleResolver.style()->setSpecifiedLineHeight(RenderStyle::initialSpecifiedLineHeight());
}

inline void StyleBuilderCustom::applyValueLineHeight(StyleResolver& styleResolver, CSSValue& value)
{
    float multiplier = styleResolver.style()->textSizeAdjust().isPercentage() ? styleResolver.style()->textSizeAdjust().multiplier() : 1.f;
    Optional<Length> lineHeight = StyleBuilderConverter::convertLineHeight(styleResolver, value, multiplier);
    if (!lineHeight)
        return;

    styleResolver.style()->setLineHeight(lineHeight.value());
    styleResolver.style()->setSpecifiedLineHeight(lineHeight.value());
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
    RenderStyle* parentStyle = styleResolver.parentStyle();
    if (!parentStyle->hasClip())
        return applyInitialClip(styleResolver);
    styleResolver.style()->setClip(parentStyle->clipTop(), parentStyle->clipRight(), parentStyle->clipBottom(), parentStyle->clipLeft());
    styleResolver.style()->setHasClip(true);
}

inline void StyleBuilderCustom::applyValueClip(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (Rect* rect = primitiveValue.getRectValue()) {
        auto conversionData = styleResolver.state().cssToLengthConversionData();
        Length top = rect->top()->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        Length right = rect->right()->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        Length bottom = rect->bottom()->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        Length left = rect->left()->convertToLength<FixedIntegerConversion | PercentConversion | AutoConversion>(conversionData);
        styleResolver.style()->setClip(top, right, bottom, left);
        styleResolver.style()->setHasClip(true);
    } else {
        ASSERT(primitiveValue.getValueID() == CSSValueAuto);
        applyInitialClip(styleResolver);
    }
}

inline void StyleBuilderCustom::applyValueWebkitLocale(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    FontCascadeDescription fontDescription = styleResolver.style()->fontDescription();
    if (primitiveValue.getValueID() == CSSValueAuto)
        fontDescription.setLocale(nullAtom);
    else
        fontDescription.setLocale(primitiveValue.getStringValue());
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyValueWebkitWritingMode(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.setWritingMode(downcast<CSSPrimitiveValue>(value));
    styleResolver.style()->setHasExplicitlySetWritingMode(true);
}

inline void StyleBuilderCustom::applyValueWebkitTextOrientation(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.setTextOrientation(downcast<CSSPrimitiveValue>(value));
}

#if ENABLE(IOS_TEXT_AUTOSIZING)
inline void StyleBuilderCustom::applyValueWebkitTextSizeAdjust(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueAuto)
        styleResolver.style()->setTextSizeAdjust(TextSizeAdjustment(AutoTextSizeAdjustment));
    else if (primitiveValue.getValueID() == CSSValueNone)
        styleResolver.style()->setTextSizeAdjust(TextSizeAdjustment(NoTextSizeAdjustment));
    else
        styleResolver.style()->setTextSizeAdjust(TextSizeAdjustment(primitiveValue.getFloatValue()));

    styleResolver.state().setFontDirty(true);
}
#endif

inline void StyleBuilderCustom::applyValueWebkitTextZoom(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueNormal)
        styleResolver.style()->setTextZoom(TextZoomNormal);
    else if (primitiveValue.getValueID() == CSSValueReset)
        styleResolver.style()->setTextZoom(TextZoomReset);
    styleResolver.state().setFontDirty(true);
}

template <CSSPropertyID id>
inline void StyleBuilderCustom::applyTextOrBoxShadowValue(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone);
        if (id == CSSPropertyTextShadow)
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
        ShadowStyle shadowStyle = shadowValue.style && shadowValue.style->getValueID() == CSSValueInset ? Inset : Normal;
        Color color;
        if (shadowValue.color)
            color = styleResolver.colorFromPrimitiveValue(*shadowValue.color);
        else
            color = styleResolver.style()->color();
        auto shadowData = std::make_unique<ShadowData>(IntPoint(x, y), blur, spread, shadowStyle, id == CSSPropertyWebkitBoxShadow, color.isValid() ? color : Color::transparent);
        if (id == CSSPropertyTextShadow)
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

    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInheritFontFamily(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.style()->fontDescription();
    auto parentFontDescription = styleResolver.parentStyle()->fontDescription();

    fontDescription.setFamilies(parentFontDescription.families());
    fontDescription.setIsSpecifiedFont(parentFontDescription.isSpecifiedFont());
    styleResolver.setFontDescription(fontDescription);
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
            switch (contentValue.getValueID()) {
            case CSSValueWebkitBody:
                if (Settings* settings = styleResolver.document().settings())
                    family = settings->standardFontFamily();
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

    styleResolver.setFontDescription(fontDescription);
}

inline bool StyleBuilderCustom::isValidDisplayValue(StyleResolver& styleResolver, EDisplay display)
{
    if (is<SVGElement>(styleResolver.element()) && styleResolver.style()->styleType() == NOPSEUDO)
        return display == INLINE || display == BLOCK || display == NONE;
    return true;
}

inline void StyleBuilderCustom::applyInheritDisplay(StyleResolver& styleResolver)
{
    EDisplay display = styleResolver.parentStyle()->display();
    if (isValidDisplayValue(styleResolver, display))
        styleResolver.style()->setDisplay(display);
}

inline void StyleBuilderCustom::applyValueDisplay(StyleResolver& styleResolver, CSSValue& value)
{
    EDisplay display = downcast<CSSPrimitiveValue>(value);
    if (isValidDisplayValue(styleResolver, display))
        styleResolver.style()->setDisplay(display);
}

inline void StyleBuilderCustom::applyValueBaselineShift(StyleResolver& styleResolver, CSSValue& value)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isValueID()) {
        switch (primitiveValue.getValueID()) {
        case CSSValueBaseline:
            svgStyle.setBaselineShift(BS_BASELINE);
            break;
        case CSSValueSub:
            svgStyle.setBaselineShift(BS_SUB);
            break;
        case CSSValueSuper:
            svgStyle.setBaselineShift(BS_SUPER);
            break;
        default:
            break;
        }
    } else {
        svgStyle.setBaselineShift(BS_LENGTH);
        svgStyle.setBaselineShiftValue(SVGLength::fromCSSPrimitiveValue(primitiveValue));
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
    if (styleResolver.parentStyle()->aspectRatioType() == AspectRatioAuto)
        return;
    styleResolver.style()->setAspectRatioType(styleResolver.parentStyle()->aspectRatioType());
    styleResolver.style()->setAspectRatioDenominator(styleResolver.parentStyle()->aspectRatioDenominator());
    styleResolver.style()->setAspectRatioNumerator(styleResolver.parentStyle()->aspectRatioNumerator());
}

inline void StyleBuilderCustom::applyValueWebkitAspectRatio(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

        if (primitiveValue.getValueID() == CSSValueFromDimensions)
            return styleResolver.style()->setAspectRatioType(AspectRatioFromDimensions);
        if (primitiveValue.getValueID() == CSSValueFromIntrinsic)
            return styleResolver.style()->setAspectRatioType(AspectRatioFromIntrinsic);

        ASSERT(primitiveValue.getValueID() == CSSValueAuto);
        return styleResolver.style()->setAspectRatioType(AspectRatioAuto);
    }

    auto& aspectRatioValue = downcast<CSSAspectRatioValue>(value);
    styleResolver.style()->setAspectRatioType(AspectRatioSpecified);
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
            if (value.getValueID() == CSSValueFilled || value.getValueID() == CSSValueOpen)
                styleResolver.style()->setTextEmphasisFill(value);
            else
                styleResolver.style()->setTextEmphasisMark(value);
        }
        styleResolver.style()->setTextEmphasisCustomMark(nullAtom);
        return;
    }

    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isString()) {
        styleResolver.style()->setTextEmphasisFill(TextEmphasisFillFilled);
        styleResolver.style()->setTextEmphasisMark(TextEmphasisMarkCustom);
        styleResolver.style()->setTextEmphasisCustomMark(primitiveValue.getStringValue());
        return;
    }

    styleResolver.style()->setTextEmphasisCustomMark(nullAtom);

    if (primitiveValue.getValueID() == CSSValueFilled || primitiveValue.getValueID() == CSSValueOpen) {
        styleResolver.style()->setTextEmphasisFill(primitiveValue);
        styleResolver.style()->setTextEmphasisMark(TextEmphasisMarkAuto);
    } else {
        styleResolver.style()->setTextEmphasisFill(TextEmphasisFillFilled);
        styleResolver.style()->setTextEmphasisMark(primitiveValue);
    }
}

template <StyleBuilderCustom::CounterBehavior counterBehavior>
inline void StyleBuilderCustom::applyInheritCounter(StyleResolver& styleResolver)
{
    CounterDirectiveMap& map = styleResolver.style()->accessCounterDirectives();
    for (auto& keyValue : styleResolver.parentStyle()->accessCounterDirectives()) {
        CounterDirectives& directives = map.add(keyValue.key, CounterDirectives()).iterator->value;
        if (counterBehavior == Reset)
            directives.inheritReset(keyValue.value);
        else
            directives.inheritIncrement(keyValue.value);
    }
}

template <StyleBuilderCustom::CounterBehavior counterBehavior>
inline void StyleBuilderCustom::applyValueCounter(StyleResolver& styleResolver, CSSValue& value)
{
    bool setCounterIncrementToNone = counterBehavior == Increment && is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone;

    if (!is<CSSValueList>(value) && !setCounterIncrementToNone)
        return;

    CounterDirectiveMap& map = styleResolver.style()->accessCounterDirectives();
    for (auto& keyValue : map) {
        if (counterBehavior == Reset)
            keyValue.value.clearReset();
        else
            keyValue.value.clearIncrement();
    }

    if (setCounterIncrementToNone)
        return;

    for (auto& item : downcast<CSSValueList>(value)) {
        Pair* pair = downcast<CSSPrimitiveValue>(item.get()).getPairValue();
        if (!pair || !pair->first() || !pair->second())
            continue;

        AtomicString identifier = pair->first()->getStringValue();
        int value = pair->second()->getIntValue();
        CounterDirectives& directives = map.add(identifier, CounterDirectives()).iterator->value;
        if (counterBehavior == Reset)
            directives.setResetValue(value);
        else
            directives.addIncrementValue(value);
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
        ECursor cursor = downcast<CSSPrimitiveValue>(value);
        if (styleResolver.style()->cursor() != cursor)
            styleResolver.style()->setCursor(cursor);
        return;
    }

    styleResolver.style()->setCursor(CursorAuto);
    auto& list = downcast<CSSValueList>(value);
    for (auto& item : list) {
        if (is<CSSCursorImageValue>(item.get())) {
            auto& image = downcast<CSSCursorImageValue>(item.get());
            if (image.updateIfSVGCursorIsUsed(styleResolver.element())) // Elements with SVG cursors are not allowed to share style.
                styleResolver.style()->setUnique();
            styleResolver.style()->addCursor(styleResolver.styleImage(CSSPropertyCursor, image), image.hotSpot());
            continue;
        }

        styleResolver.style()->setCursor(downcast<CSSPrimitiveValue>(item.get()));
        ASSERT_WITH_MESSAGE(item.ptr() == list.item(list.length() - 1), "Cursor ID fallback should always be last in the list");
        return;
    }
}

inline void StyleBuilderCustom::applyInitialFill(StyleResolver& styleResolver)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    svgStyle.setFillPaint(SVGRenderStyle::initialFillPaintType(), SVGRenderStyle::initialFillPaintColor(), SVGRenderStyle::initialFillPaintUri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyInheritFill(StyleResolver& styleResolver)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    const SVGRenderStyle& svgParentStyle = styleResolver.parentStyle()->svgStyle();
    svgStyle.setFillPaint(svgParentStyle.fillPaintType(), svgParentStyle.fillPaintColor(), svgParentStyle.fillPaintUri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());

}

inline void StyleBuilderCustom::applyValueFill(StyleResolver& styleResolver, CSSValue& value)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    SVGPaint& svgPaint = downcast<SVGPaint>(value);
    svgStyle.setFillPaint(svgPaint.paintType(), StyleBuilderConverter::convertSVGColor(styleResolver, svgPaint), svgPaint.uri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyInitialStroke(StyleResolver& styleResolver)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    svgStyle.setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyInheritStroke(StyleResolver& styleResolver)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    const SVGRenderStyle& svgParentStyle = styleResolver.parentStyle()->svgStyle();
    svgStyle.setStrokePaint(svgParentStyle.strokePaintType(), svgParentStyle.strokePaintColor(), svgParentStyle.strokePaintUri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyValueStroke(StyleResolver& styleResolver, CSSValue& value)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    SVGPaint& svgPaint = downcast<SVGPaint>(value);
    svgStyle.setStrokePaint(svgPaint.paintType(), StyleBuilderConverter::convertSVGColor(styleResolver, svgPaint), svgPaint.uri(), styleResolver.applyPropertyToRegularStyle(), styleResolver.applyPropertyToVisitedLinkStyle());
}

inline void StyleBuilderCustom::applyInitialWebkitSvgShadow(StyleResolver& styleResolver)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    svgStyle.setShadow(nullptr);
}

inline void StyleBuilderCustom::applyInheritWebkitSvgShadow(StyleResolver& styleResolver)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    const SVGRenderStyle& svgParentStyle = styleResolver.parentStyle()->svgStyle();
    svgStyle.setShadow(svgParentStyle.shadow() ? std::make_unique<ShadowData>(*svgParentStyle.shadow()) : nullptr);
}

inline void StyleBuilderCustom::applyValueWebkitSvgShadow(StyleResolver& styleResolver, CSSValue& value)
{
    SVGRenderStyle& svgStyle = styleResolver.style()->accessSVGStyle();
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone);
        svgStyle.setShadow(nullptr);
        return;
    }

    auto& shadowValue = downcast<CSSShadowValue>(*downcast<CSSValueList>(value).itemWithoutBoundsCheck(0));
    IntPoint location(shadowValue.x->computeLength<int>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f)),
        shadowValue.y->computeLength<int>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f)));
    int blur = shadowValue.blur ? shadowValue.blur->computeLength<int>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f)) : 0;
    Color color;
    if (shadowValue.color)
        color = styleResolver.colorFromPrimitiveValue(*shadowValue.color);

    // -webkit-svg-shadow does should not have a spread or style
    ASSERT(!shadowValue.spread);
    ASSERT(!shadowValue.style);

    svgStyle.setShadow(std::make_unique<ShadowData>(location, blur, 0, Normal, false, color.isValid() ? color : Color::transparent));
}

inline void StyleBuilderCustom::applyInitialFontWeight(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setWeight(FontWeightNormal);
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInheritFontWeight(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setWeight(styleResolver.parentFontDescription().weight());
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyValueFontWeight(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    auto fontDescription = styleResolver.fontDescription();
    switch (primitiveValue.getValueID()) {
    case CSSValueInvalid:
        ASSERT_NOT_REACHED();
        break;
    case CSSValueBolder:
        fontDescription.setWeight(styleResolver.parentStyle()->fontDescription().weight());
        fontDescription.setWeight(fontDescription.bolderWeight());
        break;
    case CSSValueLighter:
        fontDescription.setWeight(styleResolver.parentStyle()->fontDescription().weight());
        fontDescription.setWeight(fontDescription.lighterWeight());
        break;
    default:
        fontDescription.setWeight(primitiveValue);
    }
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInitialColumnGap(StyleResolver& styleResolver)
{
    styleResolver.style()->setHasNormalColumnGap();
}

inline void StyleBuilderCustom::applyInheritColumnGap(StyleResolver& styleResolver)
{
    if (styleResolver.parentStyle()->hasNormalColumnGap())
        styleResolver.style()->setHasNormalColumnGap();
    else
        styleResolver.style()->setColumnGap(styleResolver.parentStyle()->columnGap());
}

inline void StyleBuilderCustom::applyValueColumnGap(StyleResolver& styleResolver, CSSValue& value)
{
    if (downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNormal)
        styleResolver.style()->setHasNormalColumnGap();
    else
        styleResolver.style()->setColumnGap(StyleBuilderConverter::convertComputedLength<float>(styleResolver, value));
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
    bool didSet = false;
    for (auto& item : downcast<CSSValueList>(value)) {
        if (is<CSSImageGeneratorValue>(item.get())) {
            if (is<CSSGradientValue>(item.get()))
                styleResolver.style()->setContent(StyleGeneratedImage::create(*downcast<CSSGradientValue>(item.get()).gradientWithStylesResolved(&styleResolver)), didSet);
            else
                styleResolver.style()->setContent(StyleGeneratedImage::create(downcast<CSSImageGeneratorValue>(item.get())), didSet);
            didSet = true;
#if ENABLE(CSS_IMAGE_SET)
        } else if (is<CSSImageSetValue>(item.get())) {
            styleResolver.style()->setContent(styleResolver.setOrPendingFromValue(CSSPropertyContent, downcast<CSSImageSetValue>(item.get())), didSet);
            didSet = true;
#endif
        }

        if (is<CSSImageValue>(item.get())) {
            styleResolver.style()->setContent(styleResolver.cachedOrPendingFromValue(CSSPropertyContent, downcast<CSSImageValue>(item.get())), didSet);
            didSet = true;
            continue;
        }

        if (!is<CSSPrimitiveValue>(item.get()))
            continue;

        auto& contentValue = downcast<CSSPrimitiveValue>(item.get());
        if (contentValue.isString()) {
            styleResolver.style()->setContent(contentValue.getStringValue().impl(), didSet);
            didSet = true;
        } else if (contentValue.isAttr()) {
            // FIXME: Can a namespace be specified for an attr(foo)?
            if (styleResolver.style()->styleType() == NOPSEUDO)
                styleResolver.style()->setUnique();
            else
                styleResolver.parentStyle()->setUnique();
            QualifiedName attr(nullAtom, contentValue.getStringValue().impl(), nullAtom);
            const AtomicString& value = styleResolver.element()->getAttribute(attr);
            styleResolver.style()->setContent(value.isNull() ? emptyAtom : value.impl(), didSet);
            didSet = true;
            // Register the fact that the attribute value affects the style.
            styleResolver.ruleSets().features().attributeCanonicalLocalNamesInRules.add(attr.localName().impl());
            styleResolver.ruleSets().features().attributeLocalNamesInRules.add(attr.localName().impl());
        } else if (contentValue.isCounter()) {
            Counter* counterValue = contentValue.getCounterValue();
            EListStyleType listStyleType = NoneListStyle;
            CSSValueID listStyleIdent = counterValue->listStyleIdent();
            if (listStyleIdent != CSSValueNone)
                listStyleType = static_cast<EListStyleType>(listStyleIdent - CSSValueDisc);
            auto counter = std::make_unique<CounterContent>(counterValue->identifier(), listStyleType, counterValue->separator());
            styleResolver.style()->setContent(WTFMove(counter), didSet);
            didSet = true;
        } else {
            switch (contentValue.getValueID()) {
            case CSSValueOpenQuote:
                styleResolver.style()->setContent(OPEN_QUOTE, didSet);
                didSet = true;
                break;
            case CSSValueCloseQuote:
                styleResolver.style()->setContent(CLOSE_QUOTE, didSet);
                didSet = true;
                break;
            case CSSValueNoOpenQuote:
                styleResolver.style()->setContent(NO_OPEN_QUOTE, didSet);
                didSet = true;
                break;
            case CSSValueNoCloseQuote:
                styleResolver.style()->setContent(NO_CLOSE_QUOTE, didSet);
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
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInitialFontVariantLigatures(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantCommonLigatures(FontVariantLigatures::Normal);
    fontDescription.setVariantDiscretionaryLigatures(FontVariantLigatures::Normal);
    fontDescription.setVariantHistoricalLigatures(FontVariantLigatures::Normal);
    fontDescription.setVariantContextualAlternates(FontVariantLigatures::Normal);
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyValueFontVariantLigatures(StyleResolver& styleResolver, CSSValue& value)
{
    auto fontDescription = styleResolver.fontDescription();
    WebCore::applyValueFontVariantLigatures(fontDescription, value);
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInheritFontVariantNumeric(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantNumericFigure(styleResolver.parentFontDescription().variantNumericFigure());
    fontDescription.setVariantNumericSpacing(styleResolver.parentFontDescription().variantNumericSpacing());
    fontDescription.setVariantNumericFraction(styleResolver.parentFontDescription().variantNumericFraction());
    fontDescription.setVariantNumericOrdinal(styleResolver.parentFontDescription().variantNumericOrdinal());
    fontDescription.setVariantNumericSlashedZero(styleResolver.parentFontDescription().variantNumericSlashedZero());
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInitialFontVariantNumeric(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantNumericFigure(FontVariantNumericFigure::Normal);
    fontDescription.setVariantNumericSpacing(FontVariantNumericSpacing::Normal);
    fontDescription.setVariantNumericFraction(FontVariantNumericFraction::Normal);
    fontDescription.setVariantNumericOrdinal(FontVariantNumericOrdinal::Normal);
    fontDescription.setVariantNumericSlashedZero(FontVariantNumericSlashedZero::Normal);
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyValueFontVariantNumeric(StyleResolver& styleResolver, CSSValue& value)
{
    auto fontDescription = styleResolver.fontDescription();
    WebCore::applyValueFontVariantNumeric(fontDescription, value);
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInheritFontVariantEastAsian(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantEastAsianVariant(styleResolver.parentFontDescription().variantEastAsianVariant());
    fontDescription.setVariantEastAsianWidth(styleResolver.parentFontDescription().variantEastAsianWidth());
    fontDescription.setVariantEastAsianRuby(styleResolver.parentFontDescription().variantEastAsianRuby());
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInitialFontVariantEastAsian(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.fontDescription();
    fontDescription.setVariantEastAsianVariant(FontVariantEastAsianVariant::Normal);
    fontDescription.setVariantEastAsianWidth(FontVariantEastAsianWidth::Normal);
    fontDescription.setVariantEastAsianRuby(FontVariantEastAsianRuby::Normal);
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyValueFontVariantEastAsian(StyleResolver& styleResolver, CSSValue& value)
{
    auto fontDescription = styleResolver.fontDescription();
    WebCore::applyValueFontVariantEastAsian(fontDescription, value);
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInitialFontSize(StyleResolver& styleResolver)
{
    auto fontDescription = styleResolver.style()->fontDescription();
    float size = Style::fontSizeForKeyword(CSSValueMedium, fontDescription.useFixedDefaultSize(), styleResolver.document());

    if (size < 0)
        return;

    fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);
    styleResolver.setFontSize(fontDescription, size);
    styleResolver.setFontDescription(fontDescription);
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
    styleResolver.setFontDescription(fontDescription);
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
    if (styleResolver.style()->rubyPosition() != RubyPositionInterCharacter)
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
    if (CSSValueID ident = primitiveValue.getValueID()) {
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
            size = (primitiveValue.getFloatValue() * parentSize) / 100.0f;
        else if (primitiveValue.isCalculatedPercentageWithLength())
            size = primitiveValue.cssCalcValue()->createCalculationValue(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f))->evaluate(parentSize);
        else
            return;
    }

    if (size < 0)
        return;

    styleResolver.setFontSize(fontDescription, std::min(maximumAllowedFontSize, size));
    styleResolver.setFontDescription(fontDescription);
}

#if ENABLE(CSS_GRID_LAYOUT)
inline void StyleBuilderCustom::applyInitialWebkitGridTemplateAreas(StyleResolver& styleResolver)
{
    styleResolver.style()->setNamedGridArea(RenderStyle::initialNamedGridArea());
    styleResolver.style()->setNamedGridAreaRowCount(RenderStyle::initialNamedGridAreaCount());
    styleResolver.style()->setNamedGridAreaColumnCount(RenderStyle::initialNamedGridAreaCount());
}

inline void StyleBuilderCustom::applyInheritWebkitGridTemplateAreas(StyleResolver& styleResolver)
{
    styleResolver.style()->setNamedGridArea(styleResolver.parentStyle()->namedGridArea());
    styleResolver.style()->setNamedGridAreaRowCount(styleResolver.parentStyle()->namedGridAreaRowCount());
    styleResolver.style()->setNamedGridAreaColumnCount(styleResolver.parentStyle()->namedGridAreaColumnCount());
}

inline void StyleBuilderCustom::applyValueWebkitGridTemplateAreas(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueNone);
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

inline void StyleBuilderCustom::applyInitialWebkitGridTemplateColumns(StyleResolver& styleResolver)
{
    styleResolver.style()->setGridColumns(RenderStyle::initialGridColumns());
    styleResolver.style()->setNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines());
    styleResolver.style()->setOrderedNamedGridColumnLines(RenderStyle::initialOrderedNamedGridColumnLines());
}

inline void StyleBuilderCustom::applyInheritWebkitGridTemplateColumns(StyleResolver& styleResolver)
{
    styleResolver.style()->setGridColumns(styleResolver.parentStyle()->gridColumns());
    styleResolver.style()->setNamedGridColumnLines(styleResolver.parentStyle()->namedGridColumnLines());
    styleResolver.style()->setOrderedNamedGridColumnLines(styleResolver.parentStyle()->orderedNamedGridColumnLines());
}

inline void StyleBuilderCustom::applyValueWebkitGridTemplateColumns(StyleResolver& styleResolver, CSSValue& value)
{
    Vector<GridTrackSize> trackSizes;
    NamedGridLinesMap namedGridLines;
    OrderedNamedGridLinesMap orderedNamedGridLines;
    if (!StyleBuilderConverter::createGridTrackList(value, trackSizes, namedGridLines, orderedNamedGridLines, styleResolver))
        return;
    const NamedGridAreaMap& namedGridAreas = styleResolver.style()->namedGridArea();
    if (!namedGridAreas.isEmpty())
        StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(namedGridAreas, namedGridLines, ForColumns);

    styleResolver.style()->setGridColumns(trackSizes);
    styleResolver.style()->setNamedGridColumnLines(namedGridLines);
    styleResolver.style()->setOrderedNamedGridColumnLines(orderedNamedGridLines);
}

inline void StyleBuilderCustom::applyInitialWebkitGridTemplateRows(StyleResolver& styleResolver)
{
    styleResolver.style()->setGridRows(RenderStyle::initialGridRows());
    styleResolver.style()->setNamedGridRowLines(RenderStyle::initialNamedGridRowLines());
    styleResolver.style()->setOrderedNamedGridRowLines(RenderStyle::initialOrderedNamedGridRowLines());
}

inline void StyleBuilderCustom::applyInheritWebkitGridTemplateRows(StyleResolver& styleResolver)
{
    styleResolver.style()->setGridRows(styleResolver.parentStyle()->gridRows());
    styleResolver.style()->setNamedGridRowLines(styleResolver.parentStyle()->namedGridRowLines());
    styleResolver.style()->setOrderedNamedGridRowLines(styleResolver.parentStyle()->orderedNamedGridRowLines());
}

inline void StyleBuilderCustom::applyValueWebkitGridTemplateRows(StyleResolver& styleResolver, CSSValue& value)
{
    Vector<GridTrackSize> trackSizes;
    NamedGridLinesMap namedGridLines;
    OrderedNamedGridLinesMap orderedNamedGridLines;
    if (!StyleBuilderConverter::createGridTrackList(value, trackSizes, namedGridLines, orderedNamedGridLines, styleResolver))
        return;
    const NamedGridAreaMap& namedGridAreas = styleResolver.style()->namedGridArea();
    if (!namedGridAreas.isEmpty())
        StyleBuilderConverter::createImplicitNamedGridLinesFromGridArea(namedGridAreas, namedGridLines, ForRows);

    styleResolver.style()->setGridRows(trackSizes);
    styleResolver.style()->setNamedGridRowLines(namedGridLines);
    styleResolver.style()->setOrderedNamedGridRowLines(orderedNamedGridLines);
}
#endif // ENABLE(CSS_GRID_LAYOUT)

void StyleBuilderCustom::applyValueAlt(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.isString())
        styleResolver.style()->setContentAltText(primitiveValue.getStringValue());
    else if (primitiveValue.isAttr()) {
        // FIXME: Can a namespace be specified for an attr(foo)?
        if (styleResolver.style()->styleType() == NOPSEUDO)
            styleResolver.style()->setUnique();
        else
            styleResolver.parentStyle()->setUnique();

        QualifiedName attr(nullAtom, primitiveValue.getStringValue(), nullAtom);
        const AtomicString& value = styleResolver.element()->getAttribute(attr);
        styleResolver.style()->setContentAltText(value.isNull() ? emptyAtom : value);

        // Register the fact that the attribute value affects the style.
        styleResolver.ruleSets().features().attributeCanonicalLocalNamesInRules.add(attr.localName().impl());
        styleResolver.ruleSets().features().attributeLocalNamesInRules.add(attr.localName().impl());
    } else
        styleResolver.style()->setContentAltText(emptyAtom);
}

#if ENABLE(CSS_SCROLL_SNAP)
inline void StyleBuilderCustom::applyInitialWebkitScrollSnapPointsX(StyleResolver& styleResolver)
{
    styleResolver.style()->setScrollSnapPointsX(nullptr);
}

inline void StyleBuilderCustom::applyInheritWebkitScrollSnapPointsX(StyleResolver& styleResolver)
{
    styleResolver.style()->setScrollSnapPointsX(styleResolver.parentStyle()->scrollSnapPointsX() ? std::make_unique<ScrollSnapPoints>(*styleResolver.parentStyle()->scrollSnapPointsX()) : nullptr);
}

inline void StyleBuilderCustom::applyInitialWebkitScrollSnapPointsY(StyleResolver& styleResolver)
{
    styleResolver.style()->setScrollSnapPointsY(nullptr);
}

inline void StyleBuilderCustom::applyInheritWebkitScrollSnapPointsY(StyleResolver& styleResolver)
{
    styleResolver.style()->setScrollSnapPointsY(styleResolver.parentStyle()->scrollSnapPointsY() ? std::make_unique<ScrollSnapPoints>(*styleResolver.parentStyle()->scrollSnapPointsY()) : nullptr);
}
#endif

inline void StyleBuilderCustom::applyValueWillChange(StyleResolver& styleResolver, CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value)) {
        ASSERT(downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueAuto);
        styleResolver.style()->setWillChange(nullptr);
        return;
    }

    Ref<WillChangeData> willChange = WillChangeData::create();
    for (auto& item : downcast<CSSValueList>(value)) {
        if (!is<CSSPrimitiveValue>(item.get()))
            continue;

        const auto& primitiveValue = downcast<CSSPrimitiveValue>(item.get());
        switch (primitiveValue.getValueID()) {
        case CSSValueScrollPosition:
            willChange->addFeature(WillChangeData::Feature::ScrollPosition);
            break;
        case CSSValueContents:
            willChange->addFeature(WillChangeData::Feature::Contents);
            break;
        default:
            if (primitiveValue.isPropertyID())
                willChange->addFeature(WillChangeData::Feature::Property, primitiveValue.getPropertyID());
            break;
        }
    }

    styleResolver.style()->setWillChange(WTFMove(willChange));
}

} // namespace WebCore

#endif // StyleBuilderCustom_h
