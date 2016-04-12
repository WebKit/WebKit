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

#include "CSSAnimationTriggerScrollValue.h"
#include "CSSAspectRatioValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSCalculationValue.h"
#include "CSSCanvasValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontValue.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSNamedImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSReflectValue.h"
#include "CSSShadowValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSUnsetValue.h"
#include "CSSValueList.h"
#include "CSSVariableDependentValue.h"
#include "CSSVariableValue.h"
#include "SVGColor.h"
#include "SVGPaint.h"
#include "WebKitCSSFilterValue.h"
#include "WebKitCSSTransformValue.h"

#if ENABLE(CSS_GRID_LAYOUT)
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#endif

namespace WebCore {

struct SameSizeAsCSSValue : public RefCounted<SameSizeAsCSSValue> {
    uint32_t bitfields;
};

COMPILE_ASSERT(sizeof(CSSValue) == sizeof(SameSizeAsCSSValue), CSS_value_should_stay_small);

class TextCloneCSSValue : public CSSValue {
public:
    static Ref<TextCloneCSSValue> create(ClassType classType, const String& text)
    {
        return adoptRef(*new TextCloneCSSValue(classType, text));
    }

    String cssText() const { return m_cssText; }

private:
    TextCloneCSSValue(ClassType classType, const String& text) 
        : CSSValue(classType, /*isCSSOMSafe*/ true)
        , m_cssText(text)
    {
        m_isTextClone = true;
    }

    String m_cssText;
};

bool CSSValue::isImplicitInitialValue() const
{
    return m_classType == InitialClass && downcast<CSSInitialValue>(*this).isImplicit();
}

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

void CSSValue::addSubresourceStyleURLs(ListHashSet<URL>& urls, const StyleSheetContents* styleSheet) const
{
    // This should get called for internal instances only.
    ASSERT(!isCSSOMSafe());

    if (is<CSSPrimitiveValue>(*this))
        downcast<CSSPrimitiveValue>(*this).addSubresourceStyleURLs(urls, styleSheet);
    else if (is<CSSValueList>(*this))
        downcast<CSSValueList>(*this).addSubresourceStyleURLs(urls, styleSheet);
    else if (is<CSSFontFaceSrcValue>(*this))
        downcast<CSSFontFaceSrcValue>(*this).addSubresourceStyleURLs(urls, styleSheet);
    else if (is<CSSReflectValue>(*this))
        downcast<CSSReflectValue>(*this).addSubresourceStyleURLs(urls, styleSheet);
}

bool CSSValue::traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const
{
    // This should get called for internal instances only.
    ASSERT(!isCSSOMSafe());

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
#if ENABLE(CSS_IMAGE_SET)
    if (is<CSSImageSetValue>(*this))
        return downcast<CSSImageSetValue>(*this).traverseSubresources(handler);
#endif
    return false;
}

template<class ChildClassType>
inline static bool compareCSSValues(const CSSValue& first, const CSSValue& second)
{
    return static_cast<const ChildClassType&>(first).equals(static_cast<const ChildClassType&>(second));
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        return static_cast<const TextCloneCSSValue*>(this)->cssText() == other.cssText();
    }

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
        case FontClass:
            return compareCSSValues<CSSFontValue>(*this, other);
        case FontFaceSrcClass:
            return compareCSSValues<CSSFontFaceSrcValue>(*this, other);
        case FontFeatureClass:
            return compareCSSValues<CSSFontFeatureValue>(*this, other);
        case FunctionClass:
            return compareCSSValues<CSSFunctionValue>(*this, other);
        case LinearGradientClass:
            return compareCSSValues<CSSLinearGradientValue>(*this, other);
        case RadialGradientClass:
            return compareCSSValues<CSSRadialGradientValue>(*this, other);
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
#if ENABLE(CSS_GRID_LAYOUT)
        case GridAutoRepeatClass:
            return compareCSSValues<CSSGridAutoRepeatValue>(*this, other);
        case GridLineNamesClass:
            return compareCSSValues<CSSGridLineNamesValue>(*this, other);
        case GridTemplateAreasClass:
            return compareCSSValues<CSSGridTemplateAreasValue>(*this, other);
#endif
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
        case UnicodeRangeClass:
            return compareCSSValues<CSSUnicodeRangeValue>(*this, other);
        case ValueListClass:
            return compareCSSValues<CSSValueList>(*this, other);
        case WebKitCSSTransformClass:
            return compareCSSValues<WebKitCSSTransformValue>(*this, other);
        case LineBoxContainClass:
            return compareCSSValues<CSSLineBoxContainValue>(*this, other);
        case CalculationClass:
            return compareCSSValues<CSSCalcValue>(*this, other);
#if ENABLE(CSS_IMAGE_SET)
        case ImageSetClass:
            return compareCSSValues<CSSImageSetValue>(*this, other);
#endif
        case WebKitCSSFilterClass:
            return compareCSSValues<WebKitCSSFilterValue>(*this, other);
        case SVGColorClass:
            return compareCSSValues<SVGColor>(*this, other);
        case SVGPaintClass:
            return compareCSSValues<SVGPaint>(*this, other);
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
        case AnimationTriggerScrollClass:
            return compareCSSValues<CSSAnimationTriggerScrollValue>(*this, other);
#endif
        case CSSContentDistributionClass:
            return compareCSSValues<CSSContentDistributionValue>(*this, other);
        case CustomPropertyClass:
            return compareCSSValues<CSSCustomPropertyValue>(*this, other);
        case VariableDependentClass:
            return compareCSSValues<CSSVariableDependentValue>(*this, other);
        case VariableClass:
            return compareCSSValues<CSSVariableValue>(*this, other);
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
    if (m_isTextClone) {
         ASSERT(isCSSOMSafe());
        return static_cast<const TextCloneCSSValue*>(this)->cssText();
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

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
    case FontClass:
        return downcast<CSSFontValue>(*this).customCSSText();
    case FontFaceSrcClass:
        return downcast<CSSFontFaceSrcValue>(*this).customCSSText();
    case FontFeatureClass:
        return downcast<CSSFontFeatureValue>(*this).customCSSText();
    case FunctionClass:
        return downcast<CSSFunctionValue>(*this).customCSSText();
    case LinearGradientClass:
        return downcast<CSSLinearGradientValue>(*this).customCSSText();
    case RadialGradientClass:
        return downcast<CSSRadialGradientValue>(*this).customCSSText();
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
#if ENABLE(CSS_GRID_LAYOUT)
    case GridAutoRepeatClass:
        return downcast<CSSGridAutoRepeatValue>(*this).customCSSText();
    case GridLineNamesClass:
        return downcast<CSSGridLineNamesValue>(*this).customCSSText();
    case GridTemplateAreasClass:
        return downcast<CSSGridTemplateAreasValue>(*this).customCSSText();
#endif
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
    case UnicodeRangeClass:
        return downcast<CSSUnicodeRangeValue>(*this).customCSSText();
    case ValueListClass:
        return downcast<CSSValueList>(*this).customCSSText();
    case WebKitCSSTransformClass:
        return downcast<WebKitCSSTransformValue>(*this).customCSSText();
    case LineBoxContainClass:
        return downcast<CSSLineBoxContainValue>(*this).customCSSText();
    case CalculationClass:
        return downcast<CSSCalcValue>(*this).customCSSText();
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        return downcast<CSSImageSetValue>(*this).customCSSText();
#endif
    case WebKitCSSFilterClass:
        return downcast<WebKitCSSFilterValue>(*this).customCSSText();
    case SVGColorClass:
        return downcast<SVGColor>(*this).customCSSText();
    case SVGPaintClass:
        return downcast<SVGPaint>(*this).customCSSText();
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    case AnimationTriggerScrollClass:
        return downcast<CSSAnimationTriggerScrollValue>(*this).customCSSText();
#endif
    case CSSContentDistributionClass:
        return downcast<CSSContentDistributionValue>(*this).customCSSText();
    case CustomPropertyClass:
        return downcast<CSSCustomPropertyValue>(*this).customCSSText();
    case VariableDependentClass:
        return downcast<CSSVariableDependentValue>(*this).customCSSText();
    case VariableClass:
        return downcast<CSSVariableValue>(*this).customCSSText();
    }

    ASSERT_NOT_REACHED();
    return String();
}

void CSSValue::destroy()
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        delete static_cast<TextCloneCSSValue*>(this);
        return;
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

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
    case FunctionClass:
        delete downcast<CSSFunctionValue>(this);
        return;
    case LinearGradientClass:
        delete downcast<CSSLinearGradientValue>(this);
        return;
    case RadialGradientClass:
        delete downcast<CSSRadialGradientValue>(this);
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
#if ENABLE(CSS_GRID_LAYOUT)
    case GridAutoRepeatClass:
        delete downcast<CSSGridAutoRepeatValue>(this);
        return;
    case GridLineNamesClass:
        delete downcast<CSSGridLineNamesValue>(this);
        return;
    case GridTemplateAreasClass:
        delete downcast<CSSGridTemplateAreasValue>(this);
        return;
#endif
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
    case UnicodeRangeClass:
        delete downcast<CSSUnicodeRangeValue>(this);
        return;
    case ValueListClass:
        delete downcast<CSSValueList>(this);
        return;
    case WebKitCSSTransformClass:
        delete downcast<WebKitCSSTransformValue>(this);
        return;
    case LineBoxContainClass:
        delete downcast<CSSLineBoxContainValue>(this);
        return;
    case CalculationClass:
        delete downcast<CSSCalcValue>(this);
        return;
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        delete downcast<CSSImageSetValue>(this);
        return;
#endif
    case FilterImageClass:
        delete downcast<CSSFilterImageValue>(this);
        return;
    case WebKitCSSFilterClass:
        delete downcast<WebKitCSSFilterValue>(this);
        return;
    case SVGColorClass:
        delete downcast<SVGColor>(this);
        return;
    case SVGPaintClass:
        delete downcast<SVGPaint>(this);
        return;
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    case AnimationTriggerScrollClass:
        delete downcast<CSSAnimationTriggerScrollValue>(this);
        return;
#endif
    case CSSContentDistributionClass:
        delete downcast<CSSContentDistributionValue>(this);
        return;
    case CustomPropertyClass:
        delete downcast<CSSCustomPropertyValue>(this);
        return;
    case VariableDependentClass:
        delete downcast<CSSVariableDependentValue>(this);
        return;
    case VariableClass:
        delete downcast<CSSVariableValue>(this);
        return;
    }
    ASSERT_NOT_REACHED();
}

RefPtr<CSSValue> CSSValue::cloneForCSSOM() const
{
    switch (classType()) {
    case PrimitiveClass:
        return downcast<CSSPrimitiveValue>(*this).cloneForCSSOM();
    case ValueListClass:
        return downcast<CSSValueList>(*this).cloneForCSSOM();
    case ImageClass:
    case CursorImageClass:
        return downcast<CSSImageValue>(*this).cloneForCSSOM();
    case WebKitCSSFilterClass:
        return downcast<WebKitCSSFilterValue>(*this).cloneForCSSOM();
    case WebKitCSSTransformClass:
        return downcast<WebKitCSSTransformValue>(*this).cloneForCSSOM();
#if ENABLE(CSS_IMAGE_SET)
    case ImageSetClass:
        return downcast<CSSImageSetValue>(*this).cloneForCSSOM();
#endif
    case SVGColorClass:
        return downcast<SVGColor>(*this).cloneForCSSOM();
    case SVGPaintClass:
        return downcast<SVGPaint>(*this).cloneForCSSOM();
    default:
        ASSERT(!isSubtypeExposedToCSSOM());
        return TextCloneCSSValue::create(classType(), cssText());
    }
}

bool CSSValue::isInvalidCustomPropertyValue() const
{
    return isCustomPropertyValue() && downcast<CSSCustomPropertyValue>(*this).isInvalid();
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
