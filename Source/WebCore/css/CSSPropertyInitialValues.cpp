/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
#include "CSSPropertyInitialValues.h"

#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSQuadValue.h"
#include "CSSUnits.h"
#include "CSSValueKeywords.h"
#include "CSSValuePair.h"
#include "RectBase.h"
#include <wtf/Variant.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

struct InitialNumericValue {
    double number;
    CSSUnitType type { CSSUnitType::CSS_NUMBER };
};
using InitialValue = Variant<CSSValueID, InitialNumericValue>;

static constexpr InitialValue initialValueForLonghand(CSSPropertyID longhand)
{
    // Currently, this tries to cover just longhands that can be omitted from shorthands when parsing or serializing.
    // Later, we likely want to cover all properties, and generate the table from CSSProperties.json.
    switch (longhand) {
    case CSSPropertyAccentColor:
    case CSSPropertyAlignSelf:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationTimeline:
    case CSSPropertyAspectRatio:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBlockSize:
    case CSSPropertyBlockStepAlign:
    case CSSPropertyBottom:
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
    case CSSPropertyBreakInside:
    case CSSPropertyCaretColor:
    case CSSPropertyClip:
    case CSSPropertyColumnCount:
    case CSSPropertyColumnWidth:
    case CSSPropertyCursor:
    case CSSPropertyDominantBaseline:
    case CSSPropertyFlexBasis:
    case CSSPropertyFontKerning:
    case CSSPropertyFontSynthesisSmallCaps:
    case CSSPropertyFontSynthesisStyle:
    case CSSPropertyFontSynthesisWeight:
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
    case CSSPropertyGridColumnEnd:
    case CSSPropertyGridColumnStart:
    case CSSPropertyGridRowEnd:
    case CSSPropertyGridRowStart:
    case CSSPropertyHeight:
    case CSSPropertyImageRendering:
    case CSSPropertyInlineSize:
    case CSSPropertyInputSecurity:
    case CSSPropertyInsetBlockEnd:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetInlineEnd:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyJustifySelf:
    case CSSPropertyLeft:
    case CSSPropertyLineBreak:
    case CSSPropertyMaskBorderWidth:
    case CSSPropertyMaskSize:
    case CSSPropertyOffsetAnchor:
    case CSSPropertyOffsetRotate:
    case CSSPropertyOverflowAnchor:
    case CSSPropertyOverscrollBehaviorBlock:
    case CSSPropertyOverscrollBehaviorInline:
    case CSSPropertyOverscrollBehaviorX:
    case CSSPropertyOverscrollBehaviorY:
    case CSSPropertyPage:
    case CSSPropertyPointerEvents:
    case CSSPropertyQuotes:
    case CSSPropertyRight:
    case CSSPropertyScrollBehavior:
    case CSSPropertyScrollPaddingBlockEnd:
    case CSSPropertyScrollPaddingBlockStart:
    case CSSPropertyScrollPaddingBottom:
    case CSSPropertyScrollPaddingInlineEnd:
    case CSSPropertyScrollPaddingInlineStart:
    case CSSPropertyScrollPaddingLeft:
    case CSSPropertyScrollPaddingRight:
    case CSSPropertyScrollPaddingTop:
    case CSSPropertyScrollbarColor:
    case CSSPropertyScrollbarGutter:
    case CSSPropertyScrollbarWidth:
    case CSSPropertySize:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlignLast:
    case CSSPropertyTextDecorationSkipInk:
    case CSSPropertyTextDecorationThickness:
    case CSSPropertyTextJustify:
    case CSSPropertyTextUnderlineOffset:
    case CSSPropertyTextUnderlinePosition:
    case CSSPropertyTop:
    case CSSPropertyWebkitMaskSourceType:
    case CSSPropertyWillChange:
    case CSSPropertyZIndex:
    case CSSPropertyZoom:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontOpticalSizing:
#endif
        return CSSValueAuto;
    case CSSPropertyAlignContent:
    case CSSPropertyAlignItems:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationRangeEnd:
    case CSSPropertyAnimationRangeStart:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyColumnGap:
    case CSSPropertyContainerType:
    case CSSPropertyContent:
    case CSSPropertyFontFeatureSettings:
    case CSSPropertyFontPalette:
    case CSSPropertyFontWidth:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariantAlternates:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantEastAsian:
    case CSSPropertyFontVariantEmoji:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontVariantPosition:
    case CSSPropertyFontWeight:
    case CSSPropertyJustifyContent:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyOffsetPosition:
    case CSSPropertyOverflowWrap:
    case CSSPropertyRowGap:
    case CSSPropertyScrollSnapStop:
    case CSSPropertySpeakAs:
    case CSSPropertyTextBoxTrim:
    case CSSPropertyTransitionBehavior:
    case CSSPropertyWordBreak:
    case CSSPropertyWordSpacing:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontVariationSettings:
#endif
        return CSSValueNormal;
    case CSSPropertyAlignmentBaseline:
    case CSSPropertyVerticalAlign:
        return CSSValueBaseline;
    case CSSPropertyAnimationDelay:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
        return InitialNumericValue { 0, CSSUnitType::CSS_S };
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationName:
    case CSSPropertyAppearance:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBlockEllipsis:
    case CSSPropertyBlockStepSize:
    case CSSPropertyBorderBlockEndStyle:
    case CSSPropertyBorderBlockStartStyle:
    case CSSPropertyBorderBlockStyle:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderInlineEndStyle:
    case CSSPropertyBorderInlineStartStyle:
    case CSSPropertyBorderInlineStyle:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBoxShadow:
    case CSSPropertyClear:
    case CSSPropertyClipPath:
    case CSSPropertyColumnRuleStyle:
    case CSSPropertyColumnSpan:
    case CSSPropertyContain:
    case CSSPropertyContainIntrinsicBlockSize:
    case CSSPropertyContainIntrinsicHeight:
    case CSSPropertyContainIntrinsicInlineSize:
    case CSSPropertyContainIntrinsicWidth:
    case CSSPropertyContainerName:
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
    case CSSPropertyFilter:
    case CSSPropertyFloat:
    case CSSPropertyFontSizeAdjust:
    case CSSPropertyGridTemplateAreas:
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
    case CSSPropertyHangingPunctuation:
    case CSSPropertyListStyleImage:
    case CSSPropertyMarginTrim:
    case CSSPropertyMarkerEnd:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerStart:
    case CSSPropertyMaskBorderSource:
    case CSSPropertyMaskImage:
    case CSSPropertyMaxBlockSize:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxInlineSize:
    case CSSPropertyMaxLines:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyOffsetPath:
    case CSSPropertyOutlineStyle:
    case CSSPropertyPerspective:
    case CSSPropertyResize:
    case CSSPropertyRotate:
    case CSSPropertyScale:
    case CSSPropertyScrollSnapAlign:
    case CSSPropertyScrollSnapType:
    case CSSPropertyShapeOutside:
    case CSSPropertyStrokeDasharray:
    case CSSPropertyTextCombineUpright:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextEmphasisStyle:
    case CSSPropertyTextGroupAlign:
    case CSSPropertyTextShadow:
    case CSSPropertyTextTransform:
    case CSSPropertyTransform:
    case CSSPropertyTranslate:
    case CSSPropertyWidth:
        return CSSValueNone;
    case CSSPropertyBlockStepInsert:
        return CSSValueMarginBox;
    case CSSPropertyBlockStepRound:
        return CSSValueUp;
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyFillOpacity:
    case CSSPropertyFlexShrink:
    case CSSPropertyFloodOpacity:
    case CSSPropertyStrokeOpacity:
    case CSSPropertyOpacity:
        return InitialNumericValue { 1, CSSUnitType::CSS_NUMBER };
    case CSSPropertyAnimationPlayState:
        return CSSValueRunning;
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyTransitionTimingFunction:
        return CSSValueEase;
    case CSSPropertyBackgroundAttachment:
        return CSSValueScroll;
    case CSSPropertyBackfaceVisibility:
    case CSSPropertyContentVisibility:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyVisibility:
        return CSSValueVisible;
    case CSSPropertyBackgroundClip:
    case CSSPropertyMaskClip:
    case CSSPropertyMaskOrigin:
    case CSSPropertyWebkitMaskClip:
        return CSSValueBorderBox;
    case CSSPropertyBackgroundColor:
        return CSSValueTransparent;
    case CSSPropertyBackgroundOrigin:
        return CSSValuePaddingBox;
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
        return InitialNumericValue { 0, CSSUnitType::CSS_PERCENTAGE };
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyMaskRepeat:
        return CSSValueRepeat;
    case CSSPropertyBorderBlockColor:
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderColor:
    case CSSPropertyBorderInlineColor:
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineStartColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextEmphasisColor:
    case CSSPropertyWebkitTextStrokeColor:
        return CSSValueCurrentcolor;
    case CSSPropertyBorderBlockEndWidth:
    case CSSPropertyBorderBlockStartWidth:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderInlineEndWidth:
    case CSSPropertyBorderInlineStartWidth:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyColumnRuleWidth:
    case CSSPropertyFontSize:
    case CSSPropertyOutlineWidth:
        return CSSValueMedium;
    case CSSPropertyBorderCollapse:
        return CSSValueSeparate;
    case CSSPropertyBorderImageOutset:
    case CSSPropertyMaskBorderOutset:
        return InitialNumericValue { 0, CSSUnitType::CSS_NUMBER };
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyMaskBorderRepeat:
        return CSSValueStretch;
    case CSSPropertyBorderImageSlice:
        return InitialNumericValue { 100, CSSUnitType::CSS_PERCENTAGE };
    case CSSPropertyBoxSizing:
        return CSSValueContentBox;
    case CSSPropertyCaptionSide:
        return CSSValueTop;
    case CSSPropertyClipRule:
    case CSSPropertyFillRule:
        return CSSValueNonzero;
    case CSSPropertyColor:
        return CSSValueCanvastext;
    case CSSPropertyColorInterpolationFilters:
        return CSSValueLinearRGB;
    case CSSPropertyColumnFill:
        return CSSValueBalance;
    case CSSPropertyDisplay:
        return CSSValueInline;
    case CSSPropertyEmptyCells:
        return CSSValueShow;
    case CSSPropertyFlexDirection:
    case CSSPropertyGridAutoFlow:
        return CSSValueRow;
    case CSSPropertyFlexWrap:
        return CSSValueNowrap;
    case CSSPropertyFloodColor:
        return CSSValueBlack;
    case CSSPropertyImageOrientation:
        return CSSValueFromImage;
    case CSSPropertyJustifyItems:
        return CSSValueLegacy;
    case CSSPropertyLightingColor:
        return CSSValueWhite;
    case CSSPropertyLineFitEdge:
        return CSSValueLeading;
    case CSSPropertyListStylePosition:
        return CSSValueOutside;
    case CSSPropertyListStyleType:
        return CSSValueDisc;
    case CSSPropertyMaskBorderSlice:
        return InitialNumericValue { 0, CSSUnitType::CSS_NUMBER };
    case CSSPropertyMaskComposite:
        return CSSValueAdd;
    case CSSPropertyMaskMode:
        return CSSValueMatchSource;
    case CSSPropertyMaskType:
        return CSSValueLuminance;
    case CSSPropertyObjectFit:
        return CSSValueFill;
    case CSSPropertyOffsetDistance:
    case CSSPropertyTransformOriginZ:
    case CSSPropertyWebkitTextStrokeWidth:
        return InitialNumericValue { 0, CSSUnitType::CSS_PX };
    case CSSPropertyOrphans:
    case CSSPropertyWidows:
        return InitialNumericValue { 2, CSSUnitType::CSS_NUMBER };
    case CSSPropertyPerspectiveOriginX:
    case CSSPropertyPerspectiveOriginY:
    case CSSPropertyTransformOriginX:
    case CSSPropertyTransformOriginY:
        return InitialNumericValue { 50, CSSUnitType::CSS_PERCENTAGE };
    case CSSPropertyPosition:
        return CSSValueStatic;
    case CSSPropertyPositionTryOrder:
        return CSSValueNormal;
    case CSSPropertyPositionTryFallbacks:
        return CSSValueNone;
    case CSSPropertyPrintColorAdjust:
        return CSSValueEconomy;
    case CSSPropertyScrollTimelineAxis:
    case CSSPropertyViewTimelineAxis:
        return CSSValueBlock;
    case CSSPropertyScrollTimelineName:
    case CSSPropertyViewTimelineName:
        return CSSValueNone;
    case CSSPropertyViewTimelineInset:
        return CSSValueAuto;
    case CSSPropertyStrokeColor:
        return CSSValueTransparent;
    case CSSPropertyStrokeLinecap:
        return CSSValueButt;
    case CSSPropertyStrokeLinejoin:
        return CSSValueMiter;
    case CSSPropertyStrokeMiterlimit:
        return InitialNumericValue { 4, CSSUnitType::CSS_NUMBER };
    case CSSPropertyStrokeWidth:
        return InitialNumericValue { 1, CSSUnitType::CSS_PX };
    case CSSPropertyTabSize:
        return InitialNumericValue { 8, CSSUnitType::CSS_NUMBER };
    case CSSPropertyTextAlign:
        return CSSValueStart;
    case CSSPropertyTextDecorationStyle:
        return CSSValueSolid;
    case CSSPropertyTextBoxEdge:
        return CSSValueAuto;
    case CSSPropertyTextOrientation:
        return CSSValueMixed;
    case CSSPropertyTextOverflow:
        return CSSValueClip;
    case CSSPropertyTextWrapMode:
        return CSSValueWrap;
    case CSSPropertyTextWrapStyle:
        return CSSValueAuto;
    case CSSPropertyTransformBox:
        return CSSValueViewBox;
    case CSSPropertyTransformStyle:
        return CSSValueFlat;
    case CSSPropertyTransitionProperty:
        return CSSValueAll;
    case CSSPropertyWritingMode:
        return CSSValueHorizontalTb;
    case CSSPropertyTextSpacingTrim:
        return CSSValueSpaceAll;
    case CSSPropertyTextAutospace:
        return CSSValueNoAutospace;
    case CSSPropertyWhiteSpaceCollapse:
        return CSSValueCollapse;
    case CSSPropertyFieldSizing:
        return CSSValueFixed;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static bool isValueIDPair(const CSSValue& value, CSSValueID valueID)
{
    return value.isPair() && isValueID(value.first(), valueID) && isValueID(value.second(), valueID);
}

static bool isNumber(const CSSPrimitiveValue& value, double number, CSSUnitType type)
{
    return value.primitiveType() == type && !value.isCalculated() && value.valueNoConversionDataRequired<double>() == number;
}

static bool isNumber(const CSSPrimitiveValue* value, double number, CSSUnitType type)
{
    return value && isNumber(*value, number, type);
}

static bool isNumber(const CSSValue& value, double number, CSSUnitType type)
{
    return isNumber(dynamicDowncast<CSSPrimitiveValue>(value), number, type);
}

static bool isNumber(const RectBase& quad, double number, CSSUnitType type)
{
    return isNumber(quad.top(), number, type)
        && isNumber(quad.right(), number, type)
        && isNumber(quad.bottom(), number, type)
        && isNumber(quad.left(), number, type);
}

static bool isValueID(const RectBase& quad, CSSValueID valueID)
{
    return isValueID(quad.top(), valueID)
        && isValueID(quad.right(), valueID)
        && isValueID(quad.bottom(), valueID)
        && isValueID(quad.left(), valueID);
}

static bool isNumericQuad(const CSSValue& value, double number, CSSUnitType type)
{
    return value.isQuad() && isNumber(value.quad(), number, type);
}

bool isInitialValueForLonghand(CSSPropertyID longhand, const CSSValue& value)
{
    if (value.isImplicitInitialValue())
        return true;
    switch (longhand) {
    case CSSPropertyBackgroundSize:
    case CSSPropertyMaskSize:
        if (isValueIDPair(value, CSSValueAuto))
            return true;
        break;
    case CSSPropertyBorderImageOutset:
    case CSSPropertyMaskBorderOutset:
        if (isNumericQuad(value, 0, CSSUnitType::CSS_NUMBER))
            return true;
        break;
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyMaskBorderRepeat:
        if (isValueIDPair(value, CSSValueStretch))
            return true;
        break;
    case CSSPropertyBorderImageSlice:
        if (auto sliceValue = dynamicDowncast<CSSBorderImageSliceValue>(value)) {
            if (!sliceValue->fill() && isNumber(sliceValue->slices(), 100, CSSUnitType::CSS_PERCENTAGE))
                return true;
        }
        break;
    case CSSPropertyBorderImageWidth:
        if (auto widthValue = dynamicDowncast<CSSBorderImageWidthValue>(value)) {
            if (!widthValue->overridesBorderWidths() && isNumber(widthValue->widths(), 1, CSSUnitType::CSS_NUMBER))
                return true;
        }
        break;
    case CSSPropertyOffsetRotate:
        if (auto rotateValue = dynamicDowncast<CSSOffsetRotateValue>(value)) {
            if (rotateValue->isInitialValue())
                return true;
        }
        break;
    case CSSPropertyMaskBorderSlice:
        if (auto sliceValue = dynamicDowncast<CSSBorderImageSliceValue>(value)) {
            if (!sliceValue->fill() && isNumber(sliceValue->slices(), 0, CSSUnitType::CSS_NUMBER))
                return true;
        }
        return false;
    case CSSPropertyMaskBorderWidth:
        if (auto widthValue = dynamicDowncast<CSSBorderImageWidthValue>(value)) {
            if (!widthValue->overridesBorderWidths() && isValueID(widthValue->widths(), CSSValueAuto))
                return true;
        }
        break;
    default:
        break;
    }
    return WTF::switchOn(initialValueForLonghand(longhand),
        [&](CSSValueID initialValue) {
            return isValueID(value, initialValue);
        },
        [&](InitialNumericValue initialValue) {
            return isNumber(value, initialValue.number, initialValue.type);
        }
    );
}

ASCIILiteral initialValueTextForLonghand(CSSPropertyID longhand)
{
    return WTF::switchOn(initialValueForLonghand(longhand),
        [](CSSValueID value) {
            return nameLiteral(value);
        },
        [](InitialNumericValue initialValue) {
            switch (initialValue.type) {
            case CSSUnitType::CSS_NUMBER:
                if (initialValue.number == 0.0)
                    return "0"_s;
                if (initialValue.number == 1.0)
                    return "1"_s;
                if (initialValue.number == 2.0)
                    return "2"_s;
                if (initialValue.number == 4.0)
                    return "4"_s;
                if (initialValue.number == 8.0)
                    return "8"_s;
                break;
            case CSSUnitType::CSS_PERCENTAGE:
                if (initialValue.number == 0.0)
                    return "0%"_s;
                if (initialValue.number == 50.0)
                    return "50%"_s;
                if (initialValue.number == 100.0)
                    return "100%"_s;
                break;
            case CSSUnitType::CSS_PX:
                if (initialValue.number == 0.0)
                    return "0px"_s;
                if (initialValue.number == 1.0)
                    return "1px"_s;
                break;
            case CSSUnitType::CSS_S:
                if (initialValue.number == 0.0)
                    return "0s"_s;
                break;
            default:
                break;
            }
            ASSERT_NOT_REACHED();
            return ""_s;
        }
    );
}

CSSValueID initialValueIDForLonghand(CSSPropertyID longhand)
{
    return WTF::switchOn(initialValueForLonghand(longhand),
        [](CSSValueID value) {
            return value;
        },
        [](InitialNumericValue) {
            return CSSValueInvalid;
        }
    );
}

} // namespace WebCore
