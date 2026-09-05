/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleTransformList.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/StyleZoomPrimitives.h>

namespace WebCore {

class FloatSize;
class TransformationMatrix;

namespace Style {

// <'portal-transform'> = none | auto | auto? <transform-list> | <transform-list> auto <transform-list>?
// https://webkit.github.io/explainers/css-spatial/Overview.html#stage-transform

struct PortalTransform {
    // `auto? <transform-list>`
    struct AfterAuto {
        std::optional<CSS::Keyword::Auto> autoKeyword;
        TransformList after;

        bool operator==(const AfterAuto&) const = default;
    };

    // `<transform-list> auto <transform-list>?`
    struct BeforeAndAfterAuto {
        TransformList before;
        CSS::Keyword::Auto autoKeyword;
        ListOrNullopt<TransformList> after;

        bool operator==(const BeforeAndAfterAuto&) const = default;
    };

    PortalTransform(CSS::Keyword::None)
        : m_value { CSS::Keyword::None { } }
    {
    }

    PortalTransform(CSS::Keyword::Auto)
        : m_value { CSS::Keyword::Auto { } }
    {
    }

    static PortalTransform withAutoFit(TransformList&& beforeAuto, TransformList&& afterAuto);
    static PortalTransform withoutAutoFit(TransformList&& transforms);

    bool hasAuto() const;
    bool isNone() const { return std::holds_alternative<CSS::Keyword::None>(m_value); }

    void applyBeforeAuto(TransformationMatrix&, const FloatSize&, ZoomFactor) const;
    void applyAfterAuto(TransformationMatrix&, const FloatSize&, ZoomFactor) const;

    template<typename... F> decltype(auto) switchOn(F&&... f) const { return WTF::switchOn(m_value, std::forward<F>(f)...); }

    bool operator==(const PortalTransform&) const = default;

private:
    using Value = Variant<CSS::Keyword::None, CSS::Keyword::Auto, AfterAuto, BeforeAndAfterAuto>;

    PortalTransform(Value&& value)
        : m_value { WTF::move(value) }
    {
    }

    friend struct Blending<PortalTransform>;

    const TransformList& beforeAutoList() const LIFETIME_BOUND;
    const TransformList& afterAutoList() const LIFETIME_BOUND;

    Value m_value;
};

template<size_t I> const auto& get(const PortalTransform::AfterAuto& value)
{
    if constexpr (!I)
        return value.autoKeyword;
    else if constexpr (I == 1)
        return value.after;
}

template<size_t I> const auto& get(const PortalTransform::BeforeAndAfterAuto& value)
{
    if constexpr (!I)
        return value.before;
    else if constexpr (I == 1)
        return value.autoKeyword;
    else if constexpr (I == 2)
        return value.after;
}

// MARK: - Conversion

template<> struct CSSValueConversion<PortalTransform> { auto operator()(BuilderState&, const CSSValue&) -> PortalTransform; };

// MARK: - Blending

template<> struct Blending<PortalTransform> {
    auto canBlend(const PortalTransform&, const PortalTransform&, CompositeOperation) -> bool;
    constexpr auto requiresInterpolationForAccumulativeIteration(const PortalTransform&, const PortalTransform&) -> bool { return true; }
    auto blend(const PortalTransform&, const PortalTransform&, const Interpolation::Context&) -> PortalTransform;
};

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::PortalTransform::AfterAuto, 2)
DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::PortalTransform::BeforeAndAfterAuto, 3)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::PortalTransform)
