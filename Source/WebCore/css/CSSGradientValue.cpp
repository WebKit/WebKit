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

#include "ColorInterpolation.h"
#include "StyleBuilderState.h"
#include "StyleGradientImage.h"
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

template<typename CSSType> struct StyleImageIsUncacheable<std::optional<CSSType>> {
    bool operator()(const std::optional<CSSType>& value) { return value && styleImageIsUncacheable(*value); }
};

template<typename CSSType, size_t inlineCapacity> struct StyleImageIsUncacheable<SpaceSeparatedVector<CSSType, inlineCapacity>> {
    bool operator()(const SpaceSeparatedVector<CSSType, inlineCapacity>& value) { return std::ranges::any_of(value, [](auto& element) { return styleImageIsUncacheable(element); }); }
};

template<typename CSSType, size_t inlineCapacity> struct StyleImageIsUncacheable<CommaSeparatedVector<CSSType, inlineCapacity>> {
    bool operator()(const CommaSeparatedVector<CSSType, inlineCapacity>& value) { return std::ranges::any_of(value, [](auto& element) { return styleImageIsUncacheable(element); }); }
};

template<typename... CSSTypes> struct StyleImageIsUncacheable<SpaceSeparatedTuple<CSSTypes...>> {
    bool operator()(const SpaceSeparatedTuple<CSSTypes...>& value) { return styleImageIsUncacheableOnTupleLike(value); }
};

template<typename... CSSTypes> struct StyleImageIsUncacheable<CommaSeparatedTuple<CSSTypes...>> {
    bool operator()(const CommaSeparatedTuple<CSSTypes...>& value) { return styleImageIsUncacheableOnTupleLike(value); }
};

template<typename... CSSTypes> struct StyleImageIsUncacheable<std::variant<CSSTypes...>> {
    bool operator()(const std::variant<CSSTypes...>& value) { return WTF::switchOn(value, [](const auto& alternative) { return styleImageIsUncacheable(alternative); }); }
};

template<> struct StyleImageIsUncacheable<CSSUnitType> {
    bool operator()(const CSSUnitType& value) { return conversionToCanonicalUnitRequiresConversionData(value); }
};

template<RawNumeric CSSType> struct StyleImageIsUncacheable<CSSType> {
    constexpr bool operator()(const CSSType& value) { return styleImageIsUncacheable(value.type); }
};

template<RawNumeric CSSType> struct StyleImageIsUncacheable<UnevaluatedCalc<CSSType>> {
    constexpr bool operator()(const UnevaluatedCalc<CSSType>& value) { return value.calc->requiresConversionData(); }
};

template<RawNumeric CSSType> struct StyleImageIsUncacheable<PrimitiveNumeric<CSSType>> {
    constexpr bool operator()(const PrimitiveNumeric<CSSType>& value) { return styleImageIsUncacheable(value.value); }
};

template<CSSValueID C> struct StyleImageIsUncacheable<Constant<C>> {
    constexpr bool operator()(const Constant<C>&) { return false; }
};

template<> struct StyleImageIsUncacheable<GradientColorInterpolationMethod> {
    constexpr bool operator()(const GradientColorInterpolationMethod&) { return false; }
};

template<> struct StyleImageIsUncacheable<GradientRepeat> {
    constexpr bool operator()(const GradientRepeat&) { return false; }
};

template<> struct StyleImageIsUncacheable<Position> {
    bool operator()(const Position& value) { return styleImageIsUncacheable(value.value); }
};

template<typename CSSType> struct StyleImageIsUncacheable<GradientColorStop<CSSType>> {
    bool operator()(const GradientColorStop<CSSType>& value)
    {
        if (styleImageIsUncacheable(value.position))
            return true;
        if (value.color && Style::BuilderState::isColorFromPrimitiveValueDerivedFromElement(*value.color))
            return true;
        return false;
    }
};

template<typename CSSType> requires (TreatAsTupleLike<CSSType>) struct StyleImageIsUncacheable<CSSType> {
    bool operator()(const CSSType& value) { return styleImageIsUncacheableOnTupleLike(value); }
};

} // namespace (anonymous)
} // namespace CSS

// MARK: -

template<typename T> RefPtr<StyleImage> getOrCreateStyleImage(const T& gradient, RefPtr<StyleImage>& cachedStyleImage, Style::BuilderState& state)
{
    if (cachedStyleImage)
        return cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        Style::toStyle(gradient, state)
    );
    if (!CSS::styleImageIsUncacheable(gradient))
        cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return getOrCreateStyleImage(m_gradient, m_cachedStyleImage, state);
}

RefPtr<StyleImage> CSSPrefixedLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return getOrCreateStyleImage(m_gradient, m_cachedStyleImage, state);
}

RefPtr<StyleImage> CSSDeprecatedLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return getOrCreateStyleImage(m_gradient, m_cachedStyleImage, state);
}

RefPtr<StyleImage> CSSRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return getOrCreateStyleImage(m_gradient, m_cachedStyleImage, state);
}

RefPtr<StyleImage> CSSPrefixedRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return getOrCreateStyleImage(m_gradient, m_cachedStyleImage, state);
}

RefPtr<StyleImage> CSSDeprecatedRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return getOrCreateStyleImage(m_gradient, m_cachedStyleImage, state);
}

RefPtr<StyleImage> CSSConicGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return getOrCreateStyleImage(m_gradient, m_cachedStyleImage, state);
}

// MARK: - Linear.

String CSSLinearGradientValue::customCSSText() const
{
    StringBuilder builder;
    CSS::serializationForCSS(builder, m_gradient);
    return builder.toString();
}

bool CSSLinearGradientValue::equals(const CSSLinearGradientValue& other) const
{
    return m_gradient == other.m_gradient;
}

// MARK: - Prefixed Linear.

String CSSPrefixedLinearGradientValue::customCSSText() const
{
    StringBuilder builder;
    CSS::serializationForCSS(builder, m_gradient);
    return builder.toString();
}

bool CSSPrefixedLinearGradientValue::equals(const CSSPrefixedLinearGradientValue& other) const
{
    return m_gradient == other.m_gradient;
}

// MARK: - Deprecated Linear.

String CSSDeprecatedLinearGradientValue::customCSSText() const
{
    StringBuilder builder;
    CSS::serializationForCSS(builder, m_gradient);
    return builder.toString();
}

bool CSSDeprecatedLinearGradientValue::equals(const CSSDeprecatedLinearGradientValue& other) const
{
    return m_gradient == other.m_gradient;
}

// MARK: - Radial.

String CSSRadialGradientValue::customCSSText() const
{
    StringBuilder builder;
    CSS::serializationForCSS(builder, m_gradient);
    return builder.toString();
}

bool CSSRadialGradientValue::equals(const CSSRadialGradientValue& other) const
{
    return m_gradient == other.m_gradient;
}

// MARK: Prefixed Radial.

String CSSPrefixedRadialGradientValue::customCSSText() const
{
    StringBuilder builder;
    CSS::serializationForCSS(builder, m_gradient);
    return builder.toString();
}

bool CSSPrefixedRadialGradientValue::equals(const CSSPrefixedRadialGradientValue& other) const
{
    return m_gradient == other.m_gradient;
}

// MARK: - Deprecated Radial.

String CSSDeprecatedRadialGradientValue::customCSSText() const
{
    StringBuilder builder;
    CSS::serializationForCSS(builder, m_gradient);
    return builder.toString();
}

bool CSSDeprecatedRadialGradientValue::equals(const CSSDeprecatedRadialGradientValue& other) const
{
    return m_gradient == other.m_gradient;
}

// MARK: - Conic

String CSSConicGradientValue::customCSSText() const
{
    StringBuilder builder;
    CSS::serializationForCSS(builder, m_gradient);
    return builder.toString();
}

bool CSSConicGradientValue::equals(const CSSConicGradientValue& other) const
{
    return m_gradient == other.m_gradient;
}

} // namespace WebCore
