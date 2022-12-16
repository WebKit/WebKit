/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "CSSBackgroundRepeatValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSCalcValue.h"
#include "CSSCanvasValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontPaletteValuesOverrideColorsValue.h"
#include "CSSFontStyleRangeValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontValue.h"
#include "CSSFontVariantAlternatesValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSNamedImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPaintImageValue.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSRayValue.h"
#include "CSSReflectValue.h"
#include "CSSShadowValue.h"
#include "CSSSubgridValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSTransformListValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSVariableReferenceValue.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "DeprecatedCSSOMValueList.h"
#include "EventTarget.h"
#include "Rect.h"

namespace WebCore {

struct SameSizeAsCSSValue {
    uint32_t refCount;
    uint32_t bitfields;
};

static_assert(sizeof(CSSValue) == sizeof(SameSizeAsCSSValue), "CSS value should stay small");

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSValue);

template<typename Visitor> constexpr decltype(auto) CSSValue::visitDerived(Visitor&& visitor)
{
    switch (classType()) {
    case AspectRatioClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSAspectRatioValue>(*this));
    case BackgroundRepeatClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSBackgroundRepeatValue>(*this));
    case BorderImageSliceClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSBorderImageSliceValue>(*this));
    case BorderImageWidthClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSBorderImageWidthValue>(*this));
    case CSSContentDistributionClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSContentDistributionValue>(*this));
    case CalculationClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCalcValue>(*this));
    case CanvasClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCanvasValue>(*this));
    case ConicGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSConicGradientValue>(*this));
    case CrossfadeClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCrossfadeValue>(*this));
    case CubicBezierTimingFunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCubicBezierTimingFunctionValue>(*this));
    case CursorImageClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCursorImageValue>(*this));
    case CustomPropertyClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCustomPropertyValue>(*this));
    case DeprecatedLinearGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSDeprecatedLinearGradientValue>(*this));
    case DeprecatedRadialGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSDeprecatedRadialGradientValue>(*this));
    case FilterImageClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFilterImageValue>(*this));
    case FontClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontValue>(*this));
    case FontFaceSrcLocalClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontFaceSrcLocalValue>(*this));
    case FontFaceSrcResourceClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontFaceSrcResourceValue>(*this));
    case FontFeatureClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontFeatureValue>(*this));
    case FontPaletteValuesOverrideColorsClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontPaletteValuesOverrideColorsValue>(*this));
    case FontStyleWithAngleClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontStyleWithAngleValue>(*this));
    case FontStyleRangeClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontStyleRangeValue>(*this));
    case FontVariantAlternatesClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontVariantAlternatesValue>(*this));
    case FontVariationClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontVariationValue>(*this));
    case FunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFunctionValue>(*this));
    case GridAutoRepeatClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSGridAutoRepeatValue>(*this));
    case GridIntegerRepeatClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSGridIntegerRepeatValue>(*this));
    case GridLineNamesClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSGridLineNamesValue>(*this));
    case GridTemplateAreasClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSGridTemplateAreasValue>(*this));
    case ImageClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSImageValue>(*this));
    case ImageSetClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSImageSetValue>(*this));
    case LineBoxContainClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSLineBoxContainValue>(*this));
    case LinearGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSLinearGradientValue>(*this));
    case NamedImageClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSNamedImageValue>(*this));
    case PrefixedLinearGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPrefixedLinearGradientValue>(*this));
    case PrefixedRadialGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPrefixedRadialGradientValue>(*this));
    case RadialGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSRadialGradientValue>(*this));
    case OffsetRotateClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSOffsetRotateValue>(*this));
    case PendingSubstitutionValueClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPendingSubstitutionValue>(*this));
    case PrimitiveClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPrimitiveValue>(*this));
    case RayClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSRayValue>(*this));
    case ReflectClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSReflectValue>(*this));
    case ShadowClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSShadowValue>(*this));
    case SubgridClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSSubgridValue>(*this));
    case StepsTimingFunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSStepsTimingFunctionValue>(*this));
    case SpringTimingFunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSSpringTimingFunctionValue>(*this));
    case TransformListClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSTransformListValue>(*this));
    case UnicodeRangeClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSUnicodeRangeValue>(*this));
    case ValueListClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSValueList>(*this));
    case ValuePairClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSValuePair>(*this));
    case VariableReferenceClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSVariableReferenceValue>(*this));
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPaintImageValue>(*this));
#endif
    }

    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Visitor> constexpr decltype(auto) CSSValue::visitDerived(Visitor&& visitor) const
{
    return const_cast<CSSValue&>(*this).visitDerived([&](auto& value) {
        return std::invoke(std::forward<Visitor>(visitor), std::as_const(value));
    });
}

inline bool CSSValue::customTraverseSubresources(const Function<bool(const CachedResource&)>&)
{
    return false;
}

bool CSSValue::traverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    return visitDerived([&](auto& value) {
        return value.customTraverseSubresources(handler);
    });
}

void CSSValue::collectDirectComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    if (auto* asList = dynamicDowncast<CSSValueList>(*this)) {
        for (auto& listValue : *asList)
            listValue->collectDirectComputationalDependencies(values);
        return;
    }

    if (is<CSSPrimitiveValue>(*this))
        downcast<CSSPrimitiveValue>(*this).collectDirectComputationalDependencies(values);
}

void CSSValue::collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    if (auto* asList = dynamicDowncast<CSSValueList>(*this)) {
        for (auto& listValue : *asList)
            listValue->collectDirectRootComputationalDependencies(values);
        return;
    }

    if (is<CSSPrimitiveValue>(*this))
        downcast<CSSPrimitiveValue>(*this).collectDirectRootComputationalDependencies(values);
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (classType() == other.classType()) {
        return visitDerived([&](auto& typedThis) {
            return typedThis.equals(downcast<std::remove_reference_t<decltype(typedThis)>>(other));
        });
    }
    if (is<CSSValueList>(*this) && !is<CSSValueList>(other))
        return downcast<CSSValueList>(*this).equals(other);
    if (!is<CSSValueList>(*this) && is<CSSValueList>(other))
        return downcast<CSSValueList>(other).equals(*this);
    return false;
}

bool CSSValue::isCSSLocalURL(StringView relativeURL)
{
    return relativeURL.isEmpty() || relativeURL.startsWith('#');
}

String CSSValue::cssText() const
{
    return visitDerived([](auto& value) {
        return value.customCSSText();
    });
}

ASCIILiteral CSSValue::separatorCSSText(ValueSeparator separator)
{
    switch (separator) {
    case SpaceSeparator:
        return " "_s;
    case CommaSeparator:
        return ", "_s;
    case SlashSeparator:
        return " / "_s;
    }
    ASSERT_NOT_REACHED();
    return " "_s;
}

void CSSValue::operator delete(CSSValue* value, std::destroying_delete_t)
{
    value->visitDerived([](auto& value) {
        std::destroy_at(&value);
        std::decay_t<decltype(value)>::freeAfterDestruction(&value);
    });
}

Ref<DeprecatedCSSOMValue> CSSValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& styleDeclaration) const
{
    if (isImageValue())
        return downcast<CSSImageValue>(this)->createDeprecatedCSSOMWrapper(styleDeclaration);
    if (isPrimitiveValue())
        return DeprecatedCSSOMPrimitiveValue::create(downcast<CSSPrimitiveValue>(*this), styleDeclaration);
    if (isValueList())
        return DeprecatedCSSOMValueList::create(downcast<CSSValueList>(*this), styleDeclaration);
    return DeprecatedCSSOMComplexValue::create(*this, styleDeclaration);
}

bool CSSValue::treatAsInheritedValue(CSSPropertyID propertyID) const
{
    return isInheritValue() || (isUnsetValue() && CSSProperty::isInheritedProperty(propertyID));
}

bool CSSValue::treatAsInitialValue(CSSPropertyID propertyID) const
{
    return isInitialValue() || (isUnsetValue() && !CSSProperty::isInheritedProperty(propertyID));
}

bool CSSValue::isInitialValue() const
{
    return is<CSSPrimitiveValue>(*this) && downcast<CSSPrimitiveValue>(*this).isInitialValue();
}

bool CSSValue::isImplicitInitialValue() const
{
    return is<CSSPrimitiveValue>(*this) && downcast<CSSPrimitiveValue>(*this).isImplicitInitialValue();
}

bool CSSValue::isInheritValue() const
{
    return is<CSSPrimitiveValue>(*this) && downcast<CSSPrimitiveValue>(*this).isInheritValue();
}

bool CSSValue::isUnsetValue() const
{
    return is<CSSPrimitiveValue>(*this) && downcast<CSSPrimitiveValue>(*this).isUnsetValue();
}

bool CSSValue::isRevertValue() const
{
    return is<CSSPrimitiveValue>(*this) && downcast<CSSPrimitiveValue>(*this).isRevertValue();
}

bool CSSValue::isRevertLayerValue() const
{
    return is<CSSPrimitiveValue>(*this) && downcast<CSSPrimitiveValue>(*this).isRevertLayerValue();
}

bool CSSValue::isCSSWideKeyword() const
{
    return is<CSSPrimitiveValue>(*this) && downcast<CSSPrimitiveValue>(*this).isCSSWideKeyword();
}


}
