/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "CSSValue.h"

#include "CSSAspectRatioValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSCalculationValue.h"
#include "CSSCanvasValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomIdentValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleRangeValue.h"
#include "CSSFontStyleValue.h"
#include "CSSFontValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSNamedImageValue.h"
#include "CSSPaintImageValue.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSReflectValue.h"
#include "CSSShadowValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSUnsetValue.h"
#include "CSSValueList.h"
#include "CSSVariableReferenceValue.h"

#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"

#include "DeprecatedCSSOMPrimitiveValue.h"
#include "DeprecatedCSSOMValueList.h"

namespace WebCore {

struct SameSizeAsCSSValue {
    uint32_t refCount;
    uint32_t bitfields;
};

COMPILE_ASSERT(sizeof(CSSValue) == sizeof(SameSizeAsCSSValue), CSS_value_should_stay_small);

bool CSSValue::isImplicitInitialValue() const
{
    return m_classType == InitialClass && downcast<CSSInitialValue>(*this).isImplicit();
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSValue);

CSSValue::Type CSSValue::cssValueType() const
{
    if (isInheritedValue())
        return CSS_INHERIT;
    if (isPrimitiveValue())
        return CSS_PRIMITIVE_VALUE;
    if (isValueList())
        return CSS_VALUE_LIST;
    if (isInitialValue())
        return CSS_INITIAL;
    if (isUnsetValue())
        return CSS_UNSET;
    if (isRevertValue())
        return CSS_REVERT;
    return CSS_CUSTOM;
}

bool CSSValue::traverseSubresources(const WTF::Function<bool (const CachedResource&)>& handler) const
{
    if (is<CSSValueList>(*this))
        return downcast<CSSValueList>(*this).traverseSubresources(handler);
    if (is<CSSFontFaceSrcValue>(*this))
        return downcast<CSSFontFaceSrcValue>(*this).traverseSubresources(handler);
    if (is<CSSImageValue>(*this))
        return downcast<CSSImageValue>(*this).traverseSubresources(handler);
    if (is<CSSCrossfadeValue>(*this))
        return downcast<CSSCrossfadeValue>(*this).traverseSubresources(handler);
    if (is<CSSFilterImageValue>(*this))
        return downcast<CSSFilterImageValue>(*this).traverseSubresources(handler);
    if (is<CSSImageSetValue>(*this))
        return downcast<CSSImageSetValue>(*this).traverseSubresources(handler);
    return false;
}

void CSSValue::collectDirectComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    if (is<CSSPrimitiveValue>(*this))
        downcast<CSSPrimitiveValue>(*this).collectDirectComputationalDependencies(values);
}

void CSSValue::collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    if (is<CSSPrimitiveValue>(*this))
        downcast<CSSPrimitiveValue>(*this).collectDirectRootComputationalDependencies(values);
}

template<class ChildClassType>
inline static bool compareCSSValues(const CSSValue& first, const CSSValue& second)
{
    return static_cast<const ChildClassType&>(first).equals(static_cast<const ChildClassType&>(second));
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (m_classType == other.m_classType) {
        switch (m_classType) {
        case AspectRatioClass:
            return compareCSSValues<CSSAspectRatioValue>(*this, other);
        case BorderImageSliceClass:
            return compareCSSValues<CSSBorderImageSliceValue>(*this, other);
        case CanvasClass:
            return compareCSSValues<CSSCanvasValue>(*this, other);
        case NamedImageClass:
            return compareCSSValues<CSSNamedImageValue>(*this, other);
        case CursorImageClass:
            return compareCSSValues<CSSCursorImageValue>(*this, other);
        case FilterImageClass:
            return compareCSSValues<CSSFilterImageValue>(*this, other);
#if ENABLE(CSS_PAINTING_API)
        case PaintImageClass:
            return compareCSSValues<CSSPaintImageValue>(*this, other);
#endif
        case FontClass:
            return compareCSSValues<CSSFontValue>(*this, other);
        case FontFaceSrcClass:
            return compareCSSValues<CSSFontFaceSrcValue>(*this, other);
        case FontFeatureClass:
            return compareCSSValues<CSSFontFeatureValue>(*this, other);
#if ENABLE(VARIATION_FONTS)
        case FontVariationClass:
            return compareCSSValues<CSSFontVariationValue>(*this, other);
#endif
        case FunctionClass:
            return compareCSSValues<CSSFunctionValue>(*this, other);
        case LinearGradientClass:
            return compareCSSValues<CSSLinearGradientValue>(*this, other);
        case RadialGradientClass:
            return compareCSSValues<CSSRadialGradientValue>(*this, other);
        case ConicGradientClass:
            return compareCSSValues<CSSConicGradientValue>(*this, other);
        case CrossfadeClass:
            return compareCSSValues<CSSCrossfadeValue>(*this, other);
        case ImageClass:
            return compareCSSValues<CSSImageValue>(*this, other);
        case InheritedClass:
            return compareCSSValues<CSSInheritedValue>(*this, other);
        case InitialClass:
            return compareCSSValues<CSSInitialValue>(*this, other);
        case UnsetClass:
            return compareCSSValues<CSSUnsetValue>(*this, other);
        case RevertClass:
            return compareCSSValues<CSSRevertValue>(*this, other);
        case GridAutoRepeatClass:
            return compareCSSValues<CSSGridAutoRepeatValue>(*this, other);
        case GridIntegerRepeatClass:
            return compareCSSValues<CSSGridIntegerRepeatValue>(*this, other);
        case GridLineNamesClass:
            return compareCSSValues<CSSGridLineNamesValue>(*this, other);
        case GridTemplateAreasClass:
            return compareCSSValues<CSSGridTemplateAreasValue>(*this, other);
        case PrimitiveClass:
            return compareCSSValues<CSSPrimitiveValue>(*this, other);
        case ReflectClass:
            return compareCSSValues<CSSReflectValue>(*this, other);
        case ShadowClass:
            return compareCSSValues<CSSShadowValue>(*this, other);
        case CubicBezierTimingFunctionClass:
            return compareCSSValues<CSSCubicBezierTimingFunctionValue>(*this, other);
        case StepsTimingFunctionClass:
            return compareCSSValues<CSSStepsTimingFunctionValue>(*this, other);
        case SpringTimingFunctionClass:
            return compareCSSValues<CSSSpringTimingFunctionValue>(*this, other);
        case UnicodeRangeClass:
            return compareCSSValues<CSSUnicodeRangeValue>(*this, other);
        case ValueListClass:
            return compareCSSValues<CSSValueList>(*this, other);
        case LineBoxContainClass:
            return compareCSSValues<CSSLineBoxContainValue>(*this, other);
        case CalculationClass:
            return compareCSSValues<CSSCalcValue>(*this, other);
        case ImageSetClass:
            return compareCSSValues<CSSImageSetValue>(*this, other);
        case CSSContentDistributionClass:
            return compareCSSValues<CSSContentDistributionValue>(*this, other);
        case CustomPropertyClass:
            return compareCSSValues<CSSCustomPropertyValue>(*this, other);
        case VariableReferenceClass:
            return compareCSSValues<CSSVariableReferenceValue>(*this, other);
        case PendingSubstitutionValueClass:
            return compareCSSValues<CSSPendingSubstitutionValue>(*this, other);
        case FontStyleClass:
            return compareCSSValues<CSSFontStyleValue>(*this, other);
        case FontStyleRangeClass:
            return compareCSSValues<CSSFontStyleRangeValue>(*this, other);
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    } else if (is<CSSValueList>(*this) && !is<CSSValueList>(other))
        return downcast<CSSValueList>(*this).equals(other);
    else if (!is<CSSValueList>(*this) && is<CSSValueList>(other))
        return static_cast<const CSSValueList&>(other).equals(*this);
    return false;
}

String CSSValue::cssText() const
{
    switch (classType()) {
    case AspectRatioClass:
        return downcast<CSSAspectRatioValue>(*this).customCSSText();
    case BorderImageSliceClass:
        return downcast<CSSBorderImageSliceValue>(*this).customCSSText();
    case CanvasClass:
        return downcast<CSSCanvasValue>(*this).customCSSText();
    case NamedImageClass:
        return downcast<CSSNamedImageValue>(*this).customCSSText();
    case CursorImageClass:
        return downcast<CSSCursorImageValue>(*this).customCSSText();
    case FilterImageClass:
        return downcast<CSSFilterImageValue>(*this).customCSSText();
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        return downcast<CSSPaintImageValue>(*this).customCSSText();
#endif
    case FontClass:
        return downcast<CSSFontValue>(*this).customCSSText();
    case FontFaceSrcClass:
        return downcast<CSSFontFaceSrcValue>(*this).customCSSText();
    case FontFeatureClass:
        return downcast<CSSFontFeatureValue>(*this).customCSSText();
#if ENABLE(VARIATION_FONTS)
    case FontVariationClass:
        return downcast<CSSFontVariationValue>(*this).customCSSText();
#endif
    case FunctionClass:
        return downcast<CSSFunctionValue>(*this).customCSSText();
    case LinearGradientClass:
        return downcast<CSSLinearGradientValue>(*this).customCSSText();
    case RadialGradientClass:
        return downcast<CSSRadialGradientValue>(*this).customCSSText();
    case ConicGradientClass:
        return downcast<CSSConicGradientValue>(*this).customCSSText();
    case CrossfadeClass:
        return downcast<CSSCrossfadeValue>(*this).customCSSText();
    case ImageClass:
        return downcast<CSSImageValue>(*this).customCSSText();
    case InheritedClass:
        return downcast<CSSInheritedValue>(*this).customCSSText();
    case InitialClass:
        return downcast<CSSInitialValue>(*this).customCSSText();
    case UnsetClass:
        return downcast<CSSUnsetValue>(*this).customCSSText();
    case RevertClass:
        return downcast<CSSRevertValue>(*this).customCSSText();
    case GridAutoRepeatClass:
        return downcast<CSSGridAutoRepeatValue>(*this).customCSSText();
    case GridIntegerRepeatClass:
        return downcast<CSSGridIntegerRepeatValue>(*this).customCSSText();
    case GridLineNamesClass:
        return downcast<CSSGridLineNamesValue>(*this).customCSSText();
    case GridTemplateAreasClass:
        return downcast<CSSGridTemplateAreasValue>(*this).customCSSText();
    case PrimitiveClass:
        return downcast<CSSPrimitiveValue>(*this).customCSSText();
    case ReflectClass:
        return downcast<CSSReflectValue>(*this).customCSSText();
    case ShadowClass:
        return downcast<CSSShadowValue>(*this).customCSSText();
    case CubicBezierTimingFunctionClass:
        return downcast<CSSCubicBezierTimingFunctionValue>(*this).customCSSText();
    case StepsTimingFunctionClass:
        return downcast<CSSStepsTimingFunctionValue>(*this).customCSSText();
    case SpringTimingFunctionClass:
        return downcast<CSSSpringTimingFunctionValue>(*this).customCSSText();
    case UnicodeRangeClass:
        return downcast<CSSUnicodeRangeValue>(*this).customCSSText();
    case ValueListClass:
        return downcast<CSSValueList>(*this).customCSSText();
    case LineBoxContainClass:
        return downcast<CSSLineBoxContainValue>(*this).customCSSText();
    case CalculationClass:
        return downcast<CSSCalcValue>(*this).customCSSText();
    case ImageSetClass:
        return downcast<CSSImageSetValue>(*this).customCSSText();
    case CSSContentDistributionClass:
        return downcast<CSSContentDistributionValue>(*this).customCSSText();
    case CustomPropertyClass:
        return downcast<CSSCustomPropertyValue>(*this).customCSSText();
    case CustomIdentClass:
        return downcast<CSSCustomIdentValue>(*this).customCSSText();
    case VariableReferenceClass:
        return downcast<CSSVariableReferenceValue>(*this).customCSSText();
    case PendingSubstitutionValueClass:
        return downcast<CSSPendingSubstitutionValue>(*this).customCSSText();
    case FontStyleClass:
        return downcast<CSSFontStyleValue>(*this).customCSSText();
    case FontStyleRangeClass:
        return downcast<CSSFontStyleRangeValue>(*this).customCSSText();
    }

    ASSERT_NOT_REACHED();
    return String();
}

void CSSValue::destroy()
{
    switch (classType()) {
    case AspectRatioClass:
        delete downcast<CSSAspectRatioValue>(this);
        return;
    case BorderImageSliceClass:
        delete downcast<CSSBorderImageSliceValue>(this);
        return;
    case CanvasClass:
        delete downcast<CSSCanvasValue>(this);
        return;
    case NamedImageClass:
        delete downcast<CSSNamedImageValue>(this);
        return;
    case CursorImageClass:
        delete downcast<CSSCursorImageValue>(this);
        return;
    case FontClass:
        delete downcast<CSSFontValue>(this);
        return;
    case FontFaceSrcClass:
        delete downcast<CSSFontFaceSrcValue>(this);
        return;
    case FontFeatureClass:
        delete downcast<CSSFontFeatureValue>(this);
        return;
#if ENABLE(VARIATION_FONTS)
    case FontVariationClass:
        delete downcast<CSSFontVariationValue>(this);
        return;
#endif
    case FunctionClass:
        delete downcast<CSSFunctionValue>(this);
        return;
    case LinearGradientClass:
        delete downcast<CSSLinearGradientValue>(this);
        return;
    case RadialGradientClass:
        delete downcast<CSSRadialGradientValue>(this);
        return;
    case ConicGradientClass:
        delete downcast<CSSConicGradientValue>(this);
        return;
    case CrossfadeClass:
        delete downcast<CSSCrossfadeValue>(this);
        return;
    case ImageClass:
        delete downcast<CSSImageValue>(this);
        return;
    case InheritedClass:
        delete downcast<CSSInheritedValue>(this);
        return;
    case InitialClass:
        delete downcast<CSSInitialValue>(this);
        return;
    case UnsetClass:
        delete downcast<CSSUnsetValue>(this);
        return;
    case RevertClass:
        delete downcast<CSSRevertValue>(this);
        return;
    case GridAutoRepeatClass:
        delete downcast<CSSGridAutoRepeatValue>(this);
        return;
    case GridIntegerRepeatClass:
        delete downcast<CSSGridIntegerRepeatValue>(this);
        return;
    case GridLineNamesClass:
        delete downcast<CSSGridLineNamesValue>(this);
        return;
    case GridTemplateAreasClass:
        delete downcast<CSSGridTemplateAreasValue>(this);
        return;
    case PrimitiveClass:
        delete downcast<CSSPrimitiveValue>(this);
        return;
    case ReflectClass:
        delete downcast<CSSReflectValue>(this);
        return;
    case ShadowClass:
        delete downcast<CSSShadowValue>(this);
        return;
    case CubicBezierTimingFunctionClass:
        delete downcast<CSSCubicBezierTimingFunctionValue>(this);
        return;
    case StepsTimingFunctionClass:
        delete downcast<CSSStepsTimingFunctionValue>(this);
        return;
    case SpringTimingFunctionClass:
        delete downcast<CSSSpringTimingFunctionValue>(this);
        return;
    case UnicodeRangeClass:
        delete downcast<CSSUnicodeRangeValue>(this);
        return;
    case ValueListClass:
        delete downcast<CSSValueList>(this);
        return;
    case LineBoxContainClass:
        delete downcast<CSSLineBoxContainValue>(this);
        return;
    case CalculationClass:
        delete downcast<CSSCalcValue>(this);
        return;
    case ImageSetClass:
        delete downcast<CSSImageSetValue>(this);
        return;
    case FilterImageClass:
        delete downcast<CSSFilterImageValue>(this);
        return;
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        delete downcast<CSSPaintImageValue>(this);
        return;
#endif
    case CSSContentDistributionClass:
        delete downcast<CSSContentDistributionValue>(this);
        return;
    case CustomPropertyClass:
        delete downcast<CSSCustomPropertyValue>(this);
        return;
    case CustomIdentClass:
        delete downcast<CSSCustomIdentValue>(this);
        return;
    case VariableReferenceClass:
        delete downcast<CSSVariableReferenceValue>(this);
        return;
    case PendingSubstitutionValueClass:
        delete downcast<CSSPendingSubstitutionValue>(this);
        return;
    case FontStyleClass:
        delete downcast<CSSFontStyleValue>(this);
        return;
    case FontStyleRangeClass:
        delete downcast<CSSFontStyleRangeValue>(this);
        return;
    }
    ASSERT_NOT_REACHED();
}

Ref<DeprecatedCSSOMValue> CSSValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& styleDeclaration) const
{
    if (isImageValue() || isCursorImageValue())
        return downcast<CSSImageValue>(this)->createDeprecatedCSSOMWrapper(styleDeclaration);
    if (isPrimitiveValue())
        return DeprecatedCSSOMPrimitiveValue::create(downcast<CSSPrimitiveValue>(*this), styleDeclaration);
    if (isValueList())
        return DeprecatedCSSOMValueList::create(downcast<CSSValueList>(*this), styleDeclaration);
    return DeprecatedCSSOMComplexValue::create(*this, styleDeclaration);
}

bool CSSValue::treatAsInheritedValue(CSSPropertyID propertyID) const
{
    return classType() == InheritedClass || (classType() == UnsetClass && CSSProperty::isInheritedProperty(propertyID));
}

bool CSSValue::treatAsInitialValue(CSSPropertyID propertyID) const
{
    return classType() == InitialClass || (classType() == UnsetClass && !CSSProperty::isInheritedProperty(propertyID));
}

}
