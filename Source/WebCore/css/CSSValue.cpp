/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "CSSBasicShapes.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSCalcValue.h"
#include "CSSCanvasValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCounterValue.h"
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
#include "CSSImageSetOptionValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSNamedImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPaintImageValue.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSQuadValue.h"
#include "CSSRayValue.h"
#include "CSSRectValue.h"
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
    case CalculationClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCalcValue>(*this));
    case CanvasClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCanvasValue>(*this));
    case CircleClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCircleValue>(*this));
    case ConicGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSConicGradientValue>(*this));
    case ContentDistributionClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSContentDistributionValue>(*this));
    case CounterClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCounterValue>(*this));
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
    case EllipseClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSEllipseValue>(*this));
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
    case ImageSetOptionClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSImageSetOptionValue>(*this));
    case ImageSetClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSImageSetValue>(*this));
    case InsetShapeClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSInsetShapeValue>(*this));
    case LineBoxContainClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSLineBoxContainValue>(*this));
    case LinearGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSLinearGradientValue>(*this));
    case LinearTimingFunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSLinearTimingFunctionValue>(*this));
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
    case PathClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPathValue>(*this));
    case PendingSubstitutionValueClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPendingSubstitutionValue>(*this));
    case PolygonClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPolygonValue>(*this));
    case PrimitiveClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPrimitiveValue>(*this));
    case QuadClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSQuadValue>(*this));
    case RayClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSRayValue>(*this));
    case RectClass:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSRectValue>(*this));
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

ComputedStyleDependencies CSSValue::computedStyleDependencies() const
{
    ComputedStyleDependencies dependencies;
    collectComputedStyleDependencies(dependencies);
    return dependencies;
}

void CSSValue::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    // FIXME: Unclear why it's OK that we do not cover CSSValuePair, CSSQuadValue, CSSRectValue, CSSBorderImageSliceValue, CSSBorderImageWidthValue, and others here. Probably should use visitDerived unless they don't allow the primitive values that can have dependencies. May want to base this on a traverseValues or forEachValue function instead.
    // FIXME: Consider a non-recursive algorithm for walking this tree of dependencies.
    if (auto* asList = dynamicDowncast<CSSValueContainingVector>(*this)) {
        for (auto& listValue : *asList)
            listValue.collectComputedStyleDependencies(dependencies);
        return;
    }
    if (auto* asPrimitiveValue = dynamicDowncast<CSSPrimitiveValue>(*this))
        asPrimitiveValue->collectComputedStyleDependencies(dependencies);
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (classType() == other.classType()) {
        return visitDerived([&](auto& typedThis) {
            using ValueType = std::remove_reference_t<decltype(typedThis)>;
            static_assert(!std::is_same_v<decltype(&ValueType::equals), decltype(&CSSValue::equals)>);
            return typedThis.equals(downcast<ValueType>(other));
        });
    }
    if (is<CSSValueList>(*this))
        return downcast<CSSValueList>(*this).containsSingleEqualItem(other);
    if (is<CSSValueList>(other))
        return downcast<CSSValueList>(other).containsSingleEqualItem(*this);
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

// FIXME: Consider renaming to DeprecatedCSSOMValue::create and moving it out of the CSSValue class.
Ref<DeprecatedCSSOMValue> CSSValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& styleDeclaration) const
{
    switch (classType()) {
    case ImageClass:
        return downcast<CSSImageValue>(*this).createDeprecatedCSSOMWrapper(styleDeclaration);
    case PrimitiveClass:
    case CounterClass:
    case QuadClass:
    case RectClass:
    case ValuePairClass:
        return DeprecatedCSSOMPrimitiveValue::create(*this, styleDeclaration);
    case ValueListClass:
    case GridAutoRepeatClass: // FIXME: Likely this class should not be exposed and serialized as a CSSValueList. Confirm and remove this case.
    case GridIntegerRepeatClass: // FIXME: Likely this class should not be exposed and serialized as a CSSValueList. Confirm and remove this case.
    case ImageSetClass: // FIXME: Likely this class should not be exposed and serialized as a CSSValueList. Confirm and remove this case.
    case SubgridClass: // FIXME: Likely this class should not be exposed and serialized as a CSSValueList. Confirm and remove this case.
    case TransformListClass:
        return DeprecatedCSSOMValueList::create(downcast<CSSValueContainingVector>(*this), styleDeclaration);
    default:
        return DeprecatedCSSOMComplexValue::create(*this, styleDeclaration);
    }
}

}
