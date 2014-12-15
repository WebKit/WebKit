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
#include "CSSShadowValue.h"
#include "Frame.h"
#include "LocaleToScriptMapping.h"
#include "Rect.h"
#include "SVGElement.h"
#include "StyleFontSizeFunctions.h"
#include "StyleResolver.h"

namespace WebCore {

// Note that we assume the CSS parser only allows valid CSSValue types.
class StyleBuilderCustom {
public:
    static void applyValueWebkitMarqueeIncrement(StyleResolver&, CSSValue&);

    static void applyValueDirection(StyleResolver&, CSSValue&);

    static void applyInitialZoom(StyleResolver&);
    static void applyInheritZoom(StyleResolver&);
    static void applyValueZoom(StyleResolver&, CSSValue&);

    static void applyValueVerticalAlign(StyleResolver&, CSSValue&);

#if ENABLE(CSS_IMAGE_RESOLUTION)
    static void applyInheritImageResolution(StyleResolver&);
    static void applyInitialImageResolution(StyleResolver&);
    static void applyValueImageResolution(StyleResolver&, CSSValue&);
#endif // ENABLE(CSS_IMAGE_RESOLUTION)

    static void applyInheritSize(StyleResolver&);
    static void applyInitialSize(StyleResolver&);
    static void applyValueSize(StyleResolver&, CSSValue&);

    static void applyInheritTextIndent(StyleResolver&);
    static void applyInitialTextIndent(StyleResolver&);
    static void applyValueTextIndent(StyleResolver&, CSSValue&);

#define DECLARE_BORDER_IMAGE_MODIFIER_HANDLER(type, modifier) \
    static void applyInherit##type##modifier(StyleResolver&); \
    static void applyInitial##type##modifier(StyleResolver&); \
    static void applyValue##type##modifier(StyleResolver&, CSSValue&)

    DECLARE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Outset);
    DECLARE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Repeat);
    DECLARE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Slice);
    DECLARE_BORDER_IMAGE_MODIFIER_HANDLER(BorderImage, Width);
    DECLARE_BORDER_IMAGE_MODIFIER_HANDLER(WebkitMaskBoxImage, Outset);
    DECLARE_BORDER_IMAGE_MODIFIER_HANDLER(WebkitMaskBoxImage, Repeat);
    DECLARE_BORDER_IMAGE_MODIFIER_HANDLER(WebkitMaskBoxImage, Slice);
    DECLARE_BORDER_IMAGE_MODIFIER_HANDLER(WebkitMaskBoxImage, Width);

    static void applyValueWordSpacing(StyleResolver&, CSSValue&);

#if ENABLE(IOS_TEXT_AUTOSIZING)
    static void applyInheritLineHeight(StyleResolver&);
    static void applyInitialLineHeight(StyleResolver&);
#endif // ENABLE(IOS_TEXT_AUTOSIZING)
    static void applyValueLineHeight(StyleResolver&, CSSValue&);

    static void applyInheritOutlineStyle(StyleResolver&);
    static void applyInitialOutlineStyle(StyleResolver&);
    static void applyValueOutlineStyle(StyleResolver&, CSSValue&);

    static void applyInitialClip(StyleResolver&);
    static void applyInheritClip(StyleResolver&);
    static void applyValueClip(StyleResolver&, CSSValue&);

    static void applyValueWebkitLocale(StyleResolver&, CSSValue&);
    static void applyValueWebkitWritingMode(StyleResolver&, CSSValue&);
    static void applyValueWebkitTextOrientation(StyleResolver&, CSSValue&);
    static void applyValueWebkitJustifySelf(StyleResolver&, CSSValue&);
    static void applyValueWebkitPerspective(StyleResolver&, CSSValue&);

    static void applyInitialTextShadow(StyleResolver&);
    static void applyInheritTextShadow(StyleResolver&);
    static void applyValueTextShadow(StyleResolver&, CSSValue&);
    static void applyInitialBoxShadow(StyleResolver&);
    static void applyInheritBoxShadow(StyleResolver&);
    static void applyValueBoxShadow(StyleResolver&, CSSValue&);
    static void applyInitialWebkitBoxShadow(StyleResolver&);
    static void applyInheritWebkitBoxShadow(StyleResolver&);
    static void applyValueWebkitBoxShadow(StyleResolver&, CSSValue&);

    static void applyInitialFontFamily(StyleResolver&);
    static void applyInheritFontFamily(StyleResolver&);
    static void applyValueFontFamily(StyleResolver&, CSSValue&);

    static void applyInheritDisplay(StyleResolver&);
    static void applyValueDisplay(StyleResolver&, CSSValue&);

    static void applyInitialWebkitAspectRatio(StyleResolver&);
    static void applyInheritWebkitAspectRatio(StyleResolver&);
    static void applyValueWebkitAspectRatio(StyleResolver&, CSSValue&);

    static void applyInitialWebkitTextEmphasisStyle(StyleResolver&);
    static void applyInheritWebkitTextEmphasisStyle(StyleResolver&);
    static void applyValueWebkitTextEmphasisStyle(StyleResolver&, CSSValue&);

    static void applyInitialCounterIncrement(StyleResolver&) { }
    static void applyInheritCounterIncrement(StyleResolver&);
    static void applyValueCounterIncrement(StyleResolver&, CSSValue&);
    static void applyInitialCounterReset(StyleResolver&) { }
    static void applyInheritCounterReset(StyleResolver&);
    static void applyValueCounterReset(StyleResolver&, CSSValue&);

private:
    static void resetEffectiveZoom(StyleResolver&);
    static CSSToLengthConversionData csstoLengthConversionDataWithTextZoomFactor(StyleResolver&);
    static bool convertLineHeight(StyleResolver&, const CSSValue&, Length&, float multiplier = 1.f);

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
};

inline void StyleBuilderCustom::applyValueWebkitMarqueeIncrement(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    Length marqueeLength(Undefined);
    switch (primitiveValue.getValueID()) {
    case CSSValueSmall:
        marqueeLength = Length(1, Fixed); // 1px.
        break;
    case CSSValueNormal:
        marqueeLength = Length(6, Fixed); // 6px. The WinIE default.
        break;
    case CSSValueLarge:
        marqueeLength = Length(36, Fixed); // 36px.
        break;
    case CSSValueInvalid:
        marqueeLength = primitiveValue.convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(1.0f));
        break;
    default:
        break;
    }
    if (!marqueeLength.isUndefined())
        styleResolver.style()->setMarqueeIncrement(marqueeLength);
}

inline void StyleBuilderCustom::applyValueDirection(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.style()->setDirection(downcast<CSSPrimitiveValue>(value));

    Element* element = styleResolver.element();
    if (element && styleResolver.element() == element->document().documentElement())
        element->document().setDirectionSetOnDocumentElement(true);
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

    ASSERT(!lengthOrPercentageValue.isUndefined());
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

inline CSSToLengthConversionData StyleBuilderCustom::csstoLengthConversionDataWithTextZoomFactor(StyleResolver& styleResolver)
{
    if (Frame* frame = styleResolver.document().frame())
        return styleResolver.state().cssToLengthConversionData().copyWithAdjustedZoom(styleResolver.style()->effectiveZoom() * frame->textZoomFactor());

    return styleResolver.state().cssToLengthConversionData();
}

inline bool StyleBuilderCustom::convertLineHeight(StyleResolver& styleResolver, const CSSValue& value, Length& length, float multiplier)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (primitiveValue.getValueID() == CSSValueNormal) {
        length = RenderStyle::initialLineHeight();
        return true;
    }
    if (primitiveValue.isLength()) {
        length = primitiveValue.computeLength<Length>(csstoLengthConversionDataWithTextZoomFactor(styleResolver));
        if (multiplier != 1.f)
            length = Length(length.value() * multiplier, Fixed);
        return true;
    }
    if (primitiveValue.isPercentage()) {
        // FIXME: percentage should not be restricted to an integer here.
        length = Length((styleResolver.style()->computedFontSize() * primitiveValue.getIntValue()) / 100, Fixed);
        return true;
    }
    if (primitiveValue.isNumber()) {
        // FIXME: number and percentage values should produce the same type of Length (ie. Fixed or Percent).
        length = Length(primitiveValue.getDoubleValue() * multiplier * 100.0, Percent);
        return true;
    }
    return false;
}

inline void StyleBuilderCustom::applyValueWordSpacing(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    Length wordSpacing;
    if (primitiveValue.getValueID() == CSSValueNormal)
        wordSpacing = RenderStyle::initialWordSpacing();
    else if (primitiveValue.isLength())
        wordSpacing = primitiveValue.computeLength<Length>(csstoLengthConversionDataWithTextZoomFactor(styleResolver));
    else if (primitiveValue.isPercentage())
        wordSpacing = Length(clampTo<float>(primitiveValue.getDoubleValue(), minValueForCssLength, maxValueForCssLength), Percent);
    else if (primitiveValue.isNumber())
        wordSpacing = Length(primitiveValue.getDoubleValue(), Fixed);
    else
        return;
    styleResolver.style()->setWordSpacing(wordSpacing);
}

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
    Length lineHeight;
    float multiplier = styleResolver.style()->textSizeAdjust().isPercentage() ? styleResolver.style()->textSizeAdjust().multiplier() : 1.f;
    if (!convertLineHeight(styleResolver, value, lineHeight, multiplier))
        return;

    styleResolver.style()->setLineHeight(lineHeight);
    styleResolver.style()->setSpecifiedLineHeight(lineHeight);
}

#else

inline void StyleBuilderCustom::applyValueLineHeight(StyleResolver& styleResolver, CSSValue& value)
{
    Length lineHeight;
    if (!convertLineHeight(styleResolver, value, lineHeight))
        return;

    styleResolver.style()->setLineHeight(lineHeight);
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

    if (primitiveValue.getValueID() == CSSValueAuto)
        styleResolver.style()->setLocale(nullAtom);
    else
        styleResolver.style()->setLocale(primitiveValue.getStringValue());
    
    FontDescription fontDescription = styleResolver.style()->fontDescription();
    fontDescription.setScript(localeToScriptCodeForFontSelection(styleResolver.style()->locale()));
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyValueWebkitWritingMode(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.setWritingMode(downcast<CSSPrimitiveValue>(value));

    // FIXME: It is not ok to modify document state while applying style.
    auto& state = styleResolver.state();
    if (state.element() && state.element() == state.document().documentElement())
        state.document().setWritingModeSetOnDocumentElement(true);
}

inline void StyleBuilderCustom::applyValueWebkitTextOrientation(StyleResolver& styleResolver, CSSValue& value)
{
    styleResolver.setTextOrientation(downcast<CSSPrimitiveValue>(value));
}

inline void StyleBuilderCustom::applyValueWebkitJustifySelf(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (Pair* pairValue = primitiveValue.getPairValue()) {
        styleResolver.style()->setJustifySelf(*pairValue->first());
        styleResolver.style()->setJustifySelfOverflowAlignment(*pairValue->second());
    } else
        styleResolver.style()->setJustifySelf(primitiveValue);
}

inline void StyleBuilderCustom::applyValueWebkitPerspective(StyleResolver& styleResolver, CSSValue& value)
{
    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);

    if (primitiveValue.getValueID() == CSSValueNone) {
        styleResolver.style()->setPerspective(0);
        return;
    }

    float perspectiveValue;
    if (primitiveValue.isLength())
        perspectiveValue = primitiveValue.computeLength<float>(styleResolver.state().cssToLengthConversionData());
    else if (primitiveValue.isNumber()) {
        // For backward compatibility, treat valueless numbers as px.
        Ref<CSSPrimitiveValue> value(CSSPrimitiveValue::create(primitiveValue.getDoubleValue(), CSSPrimitiveValue::CSS_PX));
        perspectiveValue = value.get().computeLength<float>(styleResolver.state().cssToLengthConversionData());
    } else {
        ASSERT_NOT_REACHED();
        return;
    }

    if (perspectiveValue >= 0.0f)
        styleResolver.style()->setPerspective(perspectiveValue);
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
            color = styleResolver.colorFromPrimitiveValue(shadowValue.color.get());
        else
            color = styleResolver.style()->color();
        auto shadowData = std::make_unique<ShadowData>(IntPoint(x, y), blur, spread, shadowStyle, id == CSSPropertyWebkitBoxShadow, color.isValid() ? color : Color::transparent);
        if (id == CSSPropertyTextShadow)
            styleResolver.style()->setTextShadow(WTF::move(shadowData), !isFirstEntry); // add to the list if this is not the first entry
        else
            styleResolver.style()->setBoxShadow(WTF::move(shadowData), !isFirstEntry); // add to the list if this is not the first entry
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
    FontDescription fontDescription = styleResolver.style()->fontDescription();
    FontDescription initialDesc = FontDescription();

    // We need to adjust the size to account for the generic family change from monospace to non-monospace.
    if (fontDescription.keywordSize() && fontDescription.useFixedDefaultSize())
        styleResolver.setFontSize(fontDescription, Style::fontSizeForKeyword(CSSValueXxSmall + fontDescription.keywordSize() - 1, false, styleResolver.document()));
    if (!initialDesc.firstFamily().isEmpty())
        fontDescription.setFamilies(initialDesc.families());

    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyInheritFontFamily(StyleResolver& styleResolver)
{
    FontDescription fontDescription = styleResolver.style()->fontDescription();
    FontDescription parentFontDescription = styleResolver.parentStyle()->fontDescription();

    fontDescription.setFamilies(parentFontDescription.families());
    fontDescription.setIsSpecifiedFont(parentFontDescription.isSpecifiedFont());
    styleResolver.setFontDescription(fontDescription);
}

inline void StyleBuilderCustom::applyValueFontFamily(StyleResolver& styleResolver, CSSValue& value)
{
    auto& valueList = downcast<CSSValueList>(value);

    FontDescription fontDescription = styleResolver.style()->fontDescription();
    // Before mapping in a new font-family property, we should reset the generic family.
    bool oldFamilyUsedFixedDefaultSize = fontDescription.useFixedDefaultSize();

    Vector<AtomicString> families;
    families.reserveInitialCapacity(valueList.length());

    for (auto& item : valueList) {
        auto& contentValue = downcast<CSSPrimitiveValue>(item.get());
        AtomicString family;
        bool isGenericFamily = false;
        if (contentValue.isString())
            family = contentValue.getStringValue();
        else {
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

    if (fontDescription.keywordSize() && fontDescription.useFixedDefaultSize() != oldFamilyUsedFixedDefaultSize)
        styleResolver.setFontSize(fontDescription, Style::fontSizeForKeyword(CSSValueXxSmall + fontDescription.keywordSize() - 1, !oldFamilyUsedFixedDefaultSize, styleResolver.document()));

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

inline void StyleBuilderCustom::applyInheritCounterIncrement(StyleResolver& styleResolver)
{
    applyInheritCounter<Increment>(styleResolver);
}

inline void StyleBuilderCustom::applyValueCounterIncrement(StyleResolver& styleResolver, CSSValue& value)
{
    applyValueCounter<Increment>(styleResolver, value);
}

inline void StyleBuilderCustom::applyInheritCounterReset(StyleResolver& styleResolver)
{
    applyInheritCounter<Reset>(styleResolver);
}

inline void StyleBuilderCustom::applyValueCounterReset(StyleResolver& styleResolver, CSSValue& value)
{
    applyValueCounter<Reset>(styleResolver, value);
}

} // namespace WebCore

#endif // StyleBuilderCustom_h
