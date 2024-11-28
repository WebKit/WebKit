/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "CSSGradientValue.h"

#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "ColorInterpolation.h"
#include "StyleBuilderState.h"
#include "StyleGradientImage.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {
namespace {

template<typename> struct StyleImageIsUncacheable;

template<typename CSSType> static bool styleImageIsUncacheable(const CSSType& value)
{
    return StyleImageIsUncacheable<CSSType>()(value);
}

template<typename TupleLike> static bool styleImageIsUncacheableOnTupleLike(const TupleLike& tupleLike)
{
    return WTF::apply([&](const auto& ...x) { return (styleImageIsUncacheable(x) || ...); }, tupleLike);
}

template<typename VariantLike> static bool styleImageIsUncacheableOnVariantLike(const VariantLike& variantLike)
{
    return WTF::switchOn(variantLike, [](const auto& alternative) { return styleImageIsUncacheable(alternative); });
}

template<typename CSSType> struct StyleImageIsUncacheable<std::optional<CSSType>> {
    bool operator()(const auto& value) { return value && styleImageIsUncacheable(*value); }
};

template<CSSValueID C, typename CSSType> struct StyleImageIsUncacheable<FunctionNotation<C, CSSType>> {
    bool operator()(const auto& value) { return styleImageIsUncacheable(value.parameters); }
};

template<typename CSSType, size_t inlineCapacity> struct StyleImageIsUncacheable<SpaceSeparatedVector<CSSType, inlineCapacity>> {
    bool operator()(const auto& value) { return std::ranges::any_of(value, [](auto& element) { return styleImageIsUncacheable(element); }); }
};

template<typename CSSType, size_t inlineCapacity> struct StyleImageIsUncacheable<CommaSeparatedVector<CSSType, inlineCapacity>> {
    bool operator()(const auto& value) { return std::ranges::any_of(value, [](auto& element) { return styleImageIsUncacheable(element); }); }
};

template<typename CSSType, size_t N> struct StyleImageIsUncacheable<SpaceSeparatedArray<CSSType, N>> {
    bool operator()(const auto& value) { return styleImageIsUncacheableOnTupleLike(value); }
};

template<typename CSSType, size_t N> struct StyleImageIsUncacheable<CommaSeparatedArray<CSSType, N>> {
    bool operator()(const auto& value) { return styleImageIsUncacheableOnTupleLike(value); }
};

template<typename... CSSTypes> struct StyleImageIsUncacheable<SpaceSeparatedTuple<CSSTypes...>> {
    bool operator()(const auto& value) { return styleImageIsUncacheableOnTupleLike(value); }
};

template<typename... CSSTypes> struct StyleImageIsUncacheable<CommaSeparatedTuple<CSSTypes...>> {
    bool operator()(const auto& value) { return styleImageIsUncacheableOnTupleLike(value); }
};

template<typename... CSSTypes> struct StyleImageIsUncacheable<std::variant<CSSTypes...>> {
    bool operator()(const auto& value) { return styleImageIsUncacheableOnVariantLike(value); }
};

template<> struct StyleImageIsUncacheable<CSSUnitType> {
    bool operator()(const auto& value) { return conversionToCanonicalUnitRequiresConversionData(value); }
};

template<RawNumeric CSSType> struct StyleImageIsUncacheable<CSSType> {
    constexpr bool operator()(const auto& value) { return styleImageIsUncacheable(value.type); }
};

template<RawNumeric CSSType> struct StyleImageIsUncacheable<UnevaluatedCalc<CSSType>> {
    constexpr bool operator()(const auto& value) { return value.protectedCalc()->requiresConversionData(); }
};

template<RawNumeric CSSType> struct StyleImageIsUncacheable<PrimitiveNumeric<CSSType>> {
    constexpr bool operator()(const auto& value) { return styleImageIsUncacheableOnVariantLike(value); }
};

template<auto R> struct StyleImageIsUncacheable<NumberOrPercentageResolvedToNumber<R>> {
    constexpr bool operator()(const auto& value) { return styleImageIsUncacheableOnVariantLike(value); }
};

template<CSSValueID C> struct StyleImageIsUncacheable<Constant<C>> {
    constexpr bool operator()(const auto&) { return false; }
};

template<> struct StyleImageIsUncacheable<GradientColorInterpolationMethod> {
    constexpr bool operator()(const auto&) { return false; }
};

template<typename CSSType> struct StyleImageIsUncacheable<GradientColorStop<CSSType>> {
    bool operator()(const auto& value)
    {
        if (styleImageIsUncacheable(value.position))
            return true;
        if (value.color && Style::BuilderState::isColorFromPrimitiveValueDerivedFromElement(*value.color))
            return true;
        return false;
    }
};

template<typename CSSType> requires (TreatAsTupleLike<CSSType>) struct StyleImageIsUncacheable<CSSType> {
    bool operator()(const auto& value) { return styleImageIsUncacheableOnTupleLike(value); }
};

template<typename CSSType> requires (TreatAsTypeWrapper<CSSType>) struct StyleImageIsUncacheable<CSSType> {
    bool operator()(const auto& value) { return styleImageIsUncacheable(get<0>(value)); }
};

} // namespace (anonymous)
} // namespace CSS

// MARK: -

RefPtr<StyleImage> CSSGradientValue::createStyleImage(const Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        Style::toStyle(m_gradient, state)
    );
    if (!CSS::styleImageIsUncacheable(m_gradient))
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

String CSSGradientValue::customCSSText() const
{
    return CSS::serializationForCSS(m_gradient);
}

bool CSSGradientValue::equals(const CSSGradientValue& other) const
{
    return m_gradient == other.m_gradient;
}

IterationStatus CSSGradientValue::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_gradient);
}

} // namespace WebCore
