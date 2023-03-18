/**
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
#include "CSSAttrValue.h"
#include "CSSBackgroundRepeatValue.h"
#include "CSSBasicShapes.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSCalcValue.h"
#include "CSSCanvasValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCounterNameValue.h"
#include "CSSCounterValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomIdentValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFamilyValue.h"
#include "CSSFontFeatureValue.h"
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
#include "CSSIdentValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSLineBoxContainValue.h"
#include "CSSNamedImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPaintImageValue.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSQuadValue.h"
#include "CSSRayValue.h"
#include "CSSRectValue.h"
#include "CSSReflectValue.h"
#include "CSSResolvedColorValue.h"
#include "CSSShadowValue.h"
#include "CSSStringValue.h"
#include "CSSSubgridValue.h"
#include "CSSTimingFunctionValue.h"
#include "CSSTransformListValue.h"
#include "CSSURLValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSVariableReferenceValue.h"
#include "EventTarget.h"

namespace WebCore {

struct SameSizeAsCSSValue {
    uint32_t refCount;
    uint32_t bitfields;
};

static_assert(sizeof(CSSValue) == sizeof(SameSizeAsCSSValue), "CSS value should stay small");

// For performance and memory size reasons, we do not use virtual functions in this class.
static_assert(!std::is_polymorphic_v<CSSValue>);

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSValue);

template<typename Visitor> constexpr decltype(auto) CSSValue::visitDerived(Visitor&& visitor)
{
    // Check that we are storing CSSValue::type in the correct number of bits.
    static_assert(static_cast<uintptr_t>(LastType) <= TypeMask);
    static_assert(static_cast<uintptr_t>(LastType) > (TypeMask >> 1));

    // Check that we are storing CSSValueID values in the correct number of bits.
    static_assert(lastCSSValueKeyword <= IdentMask);
    static_assert(lastCSSValueKeyword > (IdentMask >> 1));

    // Check that CSSPropertyID values fit in the bits we use for CSSValueID.
    constexpr auto lastCSSProperty = firstCSSProperty + numCSSProperties - 1;
    static_assert(lastCSSProperty <= IdentMask);

    // Check that we have the correct number of bits for units.
    static_assert(static_cast<uintptr_t>(LastUnit) <= UnitMask);
    static_assert(static_cast<uintptr_t>(LastUnit) > (UnitMask >> 1));

    switch (type()) {
    case Type::AspectRatio:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSAspectRatioValue>(*this));
    case Type::Attr:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSAttrValue>(*this));
    case Type::BackgroundRepeat:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSBackgroundRepeatValue>(*this));
    case Type::BorderImageSlice:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSBorderImageSliceValue>(*this));
    case Type::BorderImageWidth:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSBorderImageWidthValue>(*this));
    case Type::Calculation:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCalcValue>(*this));
    case Type::Canvas:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCanvasValue>(*this));
    case Type::Circle:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCircleValue>(*this));
    case Type::Color:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSResolvedColorValue>(*this));
    case Type::ConicGradient:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSConicGradientValue>(*this));
    case Type::ContentDistribution:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSContentDistributionValue>(*this));
    case Type::Counter:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCounterValue>(*this));
    case Type::CounterName:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCounterNameValue>(*this));
    case Type::Crossfade:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCrossfadeValue>(*this));
    case Type::CubicBezierTimingFunction:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCubicBezierTimingFunctionValue>(*this));
    case Type::CursorImage:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCursorImageValue>(*this));
    case Type::CustomIdent:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCustomIdentValue>(*this));
    case Type::CustomProperty:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSCustomPropertyValue>(*this));
    case Type::DeprecatedLinearGradient:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSDeprecatedLinearGradientValue>(*this));
    case Type::DeprecatedRadialGradient:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSDeprecatedRadialGradientValue>(*this));
    case Type::Ellipse:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSEllipseValue>(*this));
    case Type::FilterImage:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFilterImageValue>(*this));
    case Type::Font:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontValue>(*this));
    case Type::FontFaceSrcLocal:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontFaceSrcLocalValue>(*this));
    case Type::FontFaceSrcResource:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontFaceSrcResourceValue>(*this));
    case Type::FontFamily:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontFamilyValue>(*this));
    case Type::FontFeature:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontFeatureValue>(*this));
    case Type::FontStyleWithAngle:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontStyleWithAngleValue>(*this));
    case Type::FontStyleRange:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontStyleRangeValue>(*this));
    case Type::FontVariantAlternates:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontVariantAlternatesValue>(*this));
    case Type::FontVariation:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFontVariationValue>(*this));
    case Type::Function:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSFunctionValue>(*this));
    case Type::GridAutoRepeat:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSGridAutoRepeatValue>(*this));
    case Type::GridIntegerRepeat:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSGridIntegerRepeatValue>(*this));
    case Type::GridLineNames:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSGridLineNamesValue>(*this));
    case Type::GridTemplateAreas:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSGridTemplateAreasValue>(*this));
    case Type::Ident:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSIdentValue>(*this));
    case Type::Image:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSImageValue>(*this));
    case Type::ImageSet:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSImageSetValue>(*this));
    case Type::InsetShape:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSInsetShapeValue>(*this));
    case Type::LineBoxContain:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSLineBoxContainValue>(*this));
    case Type::LinearGradient:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSLinearGradientValue>(*this));
    case Type::NamedImage:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSNamedImageValue>(*this));
    case Type::PrefixedLinearGradient:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPrefixedLinearGradientValue>(*this));
    case Type::PrefixedRadialGradient:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPrefixedRadialGradientValue>(*this));
    case Type::RadialGradient:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSRadialGradientValue>(*this));
    case Type::OffsetRotate:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSOffsetRotateValue>(*this));
#if ENABLE(CSS_PAINTING_API)
    case Type::PaintImage:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPaintImageValue>(*this));
#endif
    case Type::Path:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPathValue>(*this));
    case Type::PendingSubstitutionValue:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPendingSubstitutionValue>(*this));
    case Type::Polygon:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPolygonValue>(*this));
    case Type::Primitive:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSPrimitiveValue>(*this));
    case Type::Quad:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSQuadValue>(*this));
    case Type::Ray:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSRayValue>(*this));
    case Type::Rect:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSRectValue>(*this));
    case Type::Reflect:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSReflectValue>(*this));
    case Type::Shadow:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSShadowValue>(*this));
    case Type::Subgrid:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSSubgridValue>(*this));
    case Type::StepsTimingFunction:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSStepsTimingFunctionValue>(*this));
    case Type::String:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSStringValue>(*this));
    case Type::SpringTimingFunction:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSSpringTimingFunctionValue>(*this));
    case Type::TransformList:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSTransformListValue>(*this));
    case Type::URL:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSURLValue>(*this));
    case Type::UnicodeRange:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSUnicodeRangeValue>(*this));
    case Type::UnresolvedColor:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSUnresolvedColorValue>(*this));
    case Type::ValueList:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSValueList>(*this));
    case Type::ValuePair:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSValuePair>(*this));
    case Type::VariableReference:
        return std::invoke(std::forward<Visitor>(visitor), downcast<CSSVariableReferenceValue>(*this));
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
    if (type() == other.type()) {
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

Ref<CSSValue> CSSValue::create(const Length& length)
{
    switch (length.type()) {
    case LengthType::Auto:
        return CSSIdentValue::create(CSSValueAuto);
    case LengthType::Content:
        return CSSIdentValue::create(CSSValueContent);
    case LengthType::FillAvailable:
        return CSSIdentValue::create(CSSValueWebkitFillAvailable);
    case LengthType::FitContent:
        return CSSIdentValue::create(CSSValueFitContent);
    case LengthType::Intrinsic:
        return CSSIdentValue::create(CSSValueIntrinsic);
    case LengthType::MaxContent:
        return CSSIdentValue::create(CSSValueMaxContent);
    case LengthType::MinContent:
        return CSSIdentValue::create(CSSValueMinContent);
    case LengthType::MinIntrinsic:
        return CSSIdentValue::create(CSSValueMinIntrinsic);
    case LengthType::Calculated:
    case LengthType::Fixed:
    case LengthType::Percent:
    case LengthType::Relative:
    case LengthType::Undefined:
        return CSSPrimitiveValue::create(length);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Ref<CSSValue> CSSValue::create(const Length& length, const RenderStyle& style)
{
    switch (length.type()) {
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::FillAvailable:
    case LengthType::FitContent:
    case LengthType::Intrinsic:
    case LengthType::MaxContent:
    case LengthType::MinContent:
    case LengthType::MinIntrinsic:
        return create(length);
    case LengthType::Calculated:
    case LengthType::Fixed:
    case LengthType::Percent:
    case LengthType::Relative:
    case LengthType::Undefined:
        return CSSPrimitiveValue::create(length, style);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

}
