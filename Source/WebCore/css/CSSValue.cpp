/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSAppleColorFilterValue.h"
#include "CSSAttrValue.h"
#include "CSSBackgroundRepeatValue.h"
#include "CSSBasicShapeValue.h"
#include "CSSBorderImageOutsetValue.h"
#include "CSSBorderImageRepeatValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageSourceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSBoxShadowPropertyValue.h"
#include "CSSCanvasValue.h"
#include "CSSClipValue.h"
#include "CSSColorImageValue.h"
#include "CSSColorSchemeValue.h"
#include "CSSColorValue.h"
#include "CSSContentValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomIdentValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSDynamicRangeLimitValue.h"
#include "CSSEasingFunctionValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFilterValue.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFamilyNameValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontStyleRangeValue.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSFontValue.h"
#include "CSSFontVariationValue.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSGridAutoFlowValue.h"
#include "CSSGridLineValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSGridTemplateListValue.h"
#include "CSSGridTrackSizesValue.h"
#include "CSSImageSetOptionValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSKeywordValue.h"
#include "CSSLightDarkImageValue.h"
#include "CSSMaskBorderOutsetValue.h"
#include "CSSMaskBorderRepeatValue.h"
#include "CSSMaskBorderSliceValue.h"
#include "CSSMaskBorderSourceValue.h"
#include "CSSMaskBorderWidthValue.h"
#include "CSSNamedImageValue.h"
#include "CSSOffsetRotateValue.h"
#include "CSSPaintImageValue.h"
#include "CSSPathValue.h"
#include "CSSPositionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSQuotesValue.h"
#include "CSSRatioValue.h"
#include "CSSRayValue.h"
#include "CSSScrollValue.h"
#include "CSSSerializationContext.h"
#include "CSSShorthandSubstitutionValue.h"
#include "CSSStringValue.h"
#include "CSSSubstitutionValue.h"
#include "CSSTextShadowPropertyValue.h"
#include "CSSToLengthConversionData.h"
#include "CSSTransformListValue.h"
#include "CSSURLValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSViewValue.h"
#include "CSSWebkitBoxReflectValue.h"
#include "ComputedStyleDependencies.h"
#include "DeprecatedCSSOMCustomValue.h"
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
    using enum CSSValue::ClassType;
    switch (m_classType) {
    case AppleColorFilter:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSAppleColorFilterValue>(*this));
    case Attr:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSAttrValue>(*this));
    case BackgroundRepeat:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBackgroundRepeatValue>(*this));
    case BasicShape:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBasicShapeValue>(*this));
    case BorderImageOutset:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBorderImageOutsetValue>(*this));
    case BorderImageRepeat:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBorderImageRepeatValue>(*this));
    case BorderImageSlice:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBorderImageSliceValue>(*this));
    case BorderImageSource:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBorderImageSourceValue>(*this));
    case BorderImageWidth:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBorderImageWidthValue>(*this));
    case BoxShadowProperty:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSBoxShadowPropertyValue>(*this));
    case Canvas:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCanvasValue>(*this));
    case Clip:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSClipValue>(*this));
    case Color:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSColorValue>(*this));
#if ENABLE(DARK_MODE_CSS)
    case ColorScheme:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSColorSchemeValue>(*this));
#endif
    case Content:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSContentValue>(*this));
    case Crossfade:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCrossfadeValue>(*this));
    case CursorImage:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCursorImageValue>(*this));
    case CustomIdent:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCustomIdentValue>(*this));
    case CustomProperty:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSCustomPropertyValue>(*this));
    case DynamicRangeLimit:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSDynamicRangeLimitValue>(*this));
    case EasingFunction:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSEasingFunctionValue>(*this));
    case FilterImage:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFilterImageValue>(*this));
    case Filter:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFilterValue>(*this));
    case Font:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontValue>(*this));
    case FontFaceSrcLocal:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontFaceSrcLocalValue>(*this));
    case FontFaceSrcResource:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontFaceSrcResourceValue>(*this));
    case FontFamilyName:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontFamilyNameValue>(*this));
    case FontFeature:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontFeatureValue>(*this));
    case FontStyleWithAngle:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontStyleWithAngleValue>(*this));
    case FontStyleRange:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontStyleRangeValue>(*this));
    case FontVariation:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFontVariationValue>(*this));
    case Function:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSFunctionValue>(*this));
    case Gradient:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGradientValue>(*this));
    case GridAutoFlow:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGridAutoFlowValue>(*this));
    case GridLineValue:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGridLineValue>(*this));
    case GridTemplateAreas:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGridTemplateAreasValue>(*this));
    case GridTemplateList:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGridTemplateListValue>(*this));
    case GridTrackSizes:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSGridTrackSizesValue>(*this));
    case Keyword:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSKeywordValue>(*this));
    case Image:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSImageValue>(*this));
    case ImageSetOption:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSImageSetOptionValue>(*this));
    case ImageSet:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSImageSetValue>(*this));
    case MaskBorderOutset:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSMaskBorderOutsetValue>(*this));
    case MaskBorderRepeat:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSMaskBorderRepeatValue>(*this));
    case MaskBorderSlice:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSMaskBorderSliceValue>(*this));
    case MaskBorderSource:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSMaskBorderSourceValue>(*this));
    case MaskBorderWidth:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSMaskBorderWidthValue>(*this));
    case ColorImage:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSColorImageValue>(*this));
    case LightDarkImage:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSLightDarkImageValue>(*this));
    case NamedImage:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSNamedImageValue>(*this));
    case OffsetRotate:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSOffsetRotateValue>(*this));
    case PaintImage:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPaintImageValue>(*this));
    case Path:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPathValue>(*this));
    case ShorthandSubstitution:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSShorthandSubstitutionValue>(*this));
    case Position:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPositionValue>(*this));
    case PositionX:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPositionXValue>(*this));
    case PositionY:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPositionYValue>(*this));
    case Primitive:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSPrimitiveValue>(*this));
    case Quotes:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSQuotesValue>(*this));
    case Ratio:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSRatioValue>(*this));
    case Ray:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSRayValue>(*this));
    case Scroll:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSScrollValue>(*this));
    case String:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSStringValue>(*this));
    case TextShadowProperty:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSTextShadowPropertyValue>(*this));
    case TransformList:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSTransformListValue>(*this));
    case URL:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSURLValue>(*this));
    case UnicodeRange:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSUnicodeRangeValue>(*this));
    case ValueList:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSValueList>(*this));
    case ValuePair:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSValuePair>(*this));
    case Substitution:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSSubstitutionValue>(*this));
    case View:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSViewValue>(*this));
    case WebkitBoxReflect:
        return std::invoke(std::forward<Visitor>(visitor), uncheckedDowncast<CSSWebkitBoxReflectValue>(*this));
    }

    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Visitor> constexpr decltype(auto) CSSValue::visitDerived(Visitor&& visitor) const
{
    return const_cast<CSSValue&>(*this).visitDerived([&](auto& value) {
        return std::invoke(std::forward<Visitor>(visitor), std::as_const(value));
    });
}

inline bool CSSValue::customTraverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>&)
{
    return false;
}

bool CSSValue::traverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>& handler) const
{
    return visitDerived([&](auto& value) {
        return value.customTraverseSubresources(handler);
    });
}

IterationStatus CSSValue::visitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return visitDerived([&](auto& value) {
        return value.customVisitChildren(func);
    });
}

bool CSSValue::mayDependOnBaseURL() const
{
    return visitDerived([&](auto& value) {
        return value.customMayDependOnBaseURL();
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
    // FIXME: Unclear why it's OK that we do not cover CSSValuePair, CSSBorderImageSliceValue, CSSBorderImageWidthValue, and others here. Probably should use visitDerived unless they don't allow the primitive values that can have dependencies. May want to base this on a traverseValues or forEachValue function instead.
    // FIXME: Consider a non-recursive algorithm for walking this tree of dependencies.
    if (auto* asList = dynamicDowncast<CSSValueContainingVector>(*this)) {
        for (auto& listValue : *asList)
            listValue.collectComputedStyleDependencies(dependencies);
        return;
    }
    if (auto* asPrimitiveValue = dynamicDowncast<CSSPrimitiveValue>(*this))
        asPrimitiveValue->collectComputedStyleDependencies(dependencies);
}

bool CSSValue::canResolveDependenciesWithConversionData(const CSSToLengthConversionData& conversionData) const
{
    return computedStyleDependencies().canResolveDependenciesWithConversionData(conversionData);
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (classType() == other.classType()) {
        return visitDerived([&]<typename ValueType> (ValueType& typedThis) {
            static_assert(!std::is_same_v<decltype(&ValueType::equals), decltype(&CSSValue::equals)>);
            return typedThis.equals(uncheckedDowncast<ValueType>(other));
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

String CSSValue::cssText(const CSS::SerializationContext& context) const
{
    return visitDerived([&](auto& value) {
        return value.customCSSText(context);
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
    value->visitDerived([]<typename ValueType>(ValueType& value) {
        std::destroy_at(&value);
        ValueType::freeAfterDestruction(&value);
    });
}

Ref<DeprecatedCSSOMValue> CSSValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& owner) const
{
    return visitDerived([&](auto& value) -> Ref<DeprecatedCSSOMValue> {
        return value.customCreateDeprecatedCSSOMWrapper(owner);
    });
}

Ref<DeprecatedCSSOMValue> CSSValue::customCreateDeprecatedCSSOMWrapper(CSSStyleDeclaration& owner) const
{
    return DeprecatedCSSOMCustomValue::create([copy = Ref { *this }](const CSS::SerializationContext& context) {
        return copy->cssText(context);
    }, owner);
}

void add(Hasher& hasher, const CSSValue& value)
{
    value.addHash(hasher);
}

} // namespace WebCore
