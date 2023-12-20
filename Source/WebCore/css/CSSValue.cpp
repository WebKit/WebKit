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
#include "CSSProperty.h"
#include "CSSQuadValue.h"
#include "CSSRayValue.h"
#include "CSSRectValue.h"
#include "CSSReflectValue.h"
#include "CSSScrollValue.h"
#include "CSSShadowValue.h"
#include "CSSSubgridValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSTransformListValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSVariableReferenceValue.h"
#include "CSSViewValue.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "DeprecatedCSSOMValueList.h"
#include "EventTarget.h"
#include <wtf/Hasher.h>

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
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSAspectRatioValue>(*this));
    case BackgroundRepeatClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBackgroundRepeatValue>(*this));
    case BorderImageSliceClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBorderImageSliceValue>(*this));
    case BorderImageWidthClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBorderImageWidthValue>(*this));
    case CalculationClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCalcValue>(*this));
    case CanvasClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCanvasValue>(*this));
    case CircleClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCircleValue>(*this));
    case ConicGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSConicGradientValue>(*this));
    case ContentDistributionClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSContentDistributionValue>(*this));
    case CounterClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCounterValue>(*this));
    case CrossfadeClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCrossfadeValue>(*this));
    case CubicBezierTimingFunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCubicBezierTimingFunctionValue>(*this));
    case CursorImageClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCursorImageValue>(*this));
    case CustomPropertyClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCustomPropertyValue>(*this));
    case DeprecatedLinearGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSDeprecatedLinearGradientValue>(*this));
    case DeprecatedRadialGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSDeprecatedRadialGradientValue>(*this));
    case EllipseClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSEllipseValue>(*this));
    case FilterImageClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFilterImageValue>(*this));
    case FontClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontValue>(*this));
    case FontFaceSrcLocalClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontFaceSrcLocalValue>(*this));
    case FontFaceSrcResourceClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontFaceSrcResourceValue>(*this));
    case FontFeatureClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontFeatureValue>(*this));
    case FontPaletteValuesOverrideColorsClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontPaletteValuesOverrideColorsValue>(*this));
    case FontStyleWithAngleClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontStyleWithAngleValue>(*this));
    case FontStyleRangeClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontStyleRangeValue>(*this));
    case FontVariantAlternatesClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontVariantAlternatesValue>(*this));
    case FontVariationClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontVariationValue>(*this));
    case FunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFunctionValue>(*this));
    case GridAutoRepeatClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGridAutoRepeatValue>(*this));
    case GridIntegerRepeatClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGridIntegerRepeatValue>(*this));
    case GridLineNamesClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGridLineNamesValue>(*this));
    case GridTemplateAreasClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGridTemplateAreasValue>(*this));
    case ImageClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSImageValue>(*this));
    case ImageSetOptionClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSImageSetOptionValue>(*this));
    case ImageSetClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSImageSetValue>(*this));
    case InsetShapeClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSInsetShapeValue>(*this));
    case LineBoxContainClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSLineBoxContainValue>(*this));
    case LinearGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSLinearGradientValue>(*this));
    case LinearTimingFunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSLinearTimingFunctionValue>(*this));
    case NamedImageClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSNamedImageValue>(*this));
    case PrefixedLinearGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPrefixedLinearGradientValue>(*this));
    case PrefixedRadialGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPrefixedRadialGradientValue>(*this));
    case RadialGradientClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSRadialGradientValue>(*this));
    case OffsetRotateClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSOffsetRotateValue>(*this));
    case PathClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPathValue>(*this));
    case PendingSubstitutionValueClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPendingSubstitutionValue>(*this));
    case PolygonClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPolygonValue>(*this));
    case PrimitiveClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPrimitiveValue>(*this));
    case QuadClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSQuadValue>(*this));
    case RayClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSRayValue>(*this));
    case RectClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSRectValue>(*this));
    case RectShapeClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSRectShapeValue>(*this));
    case ReflectClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSReflectValue>(*this));
    case ScrollClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSScrollValue>(*this));
    case ShadowClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSShadowValue>(*this));
    case SubgridClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSSubgridValue>(*this));
    case StepsTimingFunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSStepsTimingFunctionValue>(*this));
    case SpringTimingFunctionClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSSpringTimingFunctionValue>(*this));
    case TransformListClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSTransformListValue>(*this));
    case UnicodeRangeClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSUnicodeRangeValue>(*this));
    case ValueListClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSValueList>(*this));
    case ValuePairClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSValuePair>(*this));
    case VariableReferenceClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSVariableReferenceValue>(*this));
    case ViewClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSViewValue>(*this));
    case XywhShapeClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSXywhValue>(*this));
#if ENABLE(CSS_PAINTING_API)
    case PaintImageClass:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPaintImageValue>(*this));
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

void CSSValue::setReplacementURLForSubresources(const HashMap<String, String>& replacementURLStrings)
{
    return visitDerived([&](auto& value) {
        return value.customSetReplacementURLForSubresources(replacementURLStrings);
    });
}

void CSSValue::clearReplacementURLForSubresources()
{
    return visitDerived([&](auto& value) {
        return value.customClearReplacementURLForSubresources();
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
    if (auto* thisList = dynamicDowncast<CSSValueList>(*this))
        return thisList->containsSingleEqualItem(other);
    if (auto* otherList = dynamicDowncast<CSSValueList>(other))
        return otherList->containsSingleEqualItem(*this);
    return false;
}

bool CSSValue::addHash(Hasher& hasher) const
{
    // To match equals() a single item list could have the same hash as the item.
    // FIXME: Some Style::Builder functions can only handle list values.

    add(hasher, classType());

    return visitDerived([&](auto& typedThis) {
        return typedThis.addDerivedHash(hasher);
    });
}

// FIXME: Add custom hash functions for all derived classes and remove this function.
bool CSSValue::addDerivedHash(Hasher& hasher) const
{
    add(hasher, this);
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
        return uncheckedDowncast<CSSImageValue>(*this).createDeprecatedCSSOMWrapper(styleDeclaration);
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

void add(Hasher& hasher, const CSSValue& value)
{
    value.addHash(hasher);
}

}
