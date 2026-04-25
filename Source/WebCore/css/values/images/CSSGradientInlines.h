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

#include "CSSGradient.h"

namespace WebCore {
namespace CSS {

// MARK: - GradientColorStop get<>

template<size_t I, typename C, typename P> const auto& get(const GradientColorStop<C, P>& stop)
{
    if constexpr (!I)
        return stop.color;
    else if constexpr (I == 1)
        return stop.position;
}

// MARK: - LinearGradient get<>

template<size_t I> const auto& get(const LinearGradient& gradient)
{
    if constexpr (!I)
        return gradient.colorInterpolationMethod;
    else if constexpr (I == 1)
        return gradient.gradientLine;
    else if constexpr (I == 2)
        return gradient.stops;
}

// MARK: - PrefixedLinearGradient get<>

template<size_t I> const auto& get(const PrefixedLinearGradient& gradient)
{
    if constexpr (!I)
        return gradient.colorInterpolationMethod;
    else if constexpr (I == 1)
        return gradient.gradientLine;
    else if constexpr (I == 2)
        return gradient.stops;
}

// MARK: - DeprecatedLinearGradient get<>

template<size_t I> const auto& get(const DeprecatedLinearGradient& gradient)
{
    if constexpr (!I)
        return gradient.colorInterpolationMethod;
    else if constexpr (I == 1)
        return gradient.gradientLine;
    else if constexpr (I == 2)
        return gradient.stops;
}

// MARK: - RadialGradient get<>

template<size_t I> const auto& get(const RadialGradient::Ellipse& ellipse)
{
    if constexpr (!I)
        return ellipse.size;
    else if constexpr (I == 1)
        return ellipse.position;
}

template<size_t I> const auto& get(const RadialGradient::Circle& circle)
{
    if constexpr (!I)
        return circle.size;
    else if constexpr (I == 1)
        return circle.position;
}

template<size_t I> const auto& get(const RadialGradient& gradient)
{
    if constexpr (!I)
        return gradient.colorInterpolationMethod;
    else if constexpr (I == 1)
        return gradient.gradientBox;
    else if constexpr (I == 2)
        return gradient.stops;
}

// MARK: - PrefixedRadialGradient get<>

template<size_t I> const auto& get(const PrefixedRadialGradient::Ellipse& ellipse)
{
    if constexpr (!I)
        return ellipse.size;
    else if constexpr (I == 1)
        return ellipse.position;
}

template<size_t I> const auto& get(const PrefixedRadialGradient::Circle& circle)
{
    if constexpr (!I)
        return circle.size;
    else if constexpr (I == 1)
        return circle.position;
}

template<size_t I> const auto& get(const PrefixedRadialGradient& gradient)
{
    if constexpr (!I)
        return gradient.colorInterpolationMethod;
    else if constexpr (I == 1)
        return gradient.gradientBox;
    else if constexpr (I == 2)
        return gradient.stops;
}

// MARK: - DeprecatedRadialGradient get<>

template<size_t I> const auto& get(const DeprecatedRadialGradient::GradientBox& gradientBox)
{
    if constexpr (!I)
        return gradientBox.first;
    else if constexpr (I == 1)
        return gradientBox.firstRadius;
    else if constexpr (I == 2)
        return gradientBox.second;
    else if constexpr (I == 3)
        return gradientBox.secondRadius;
}

template<size_t I> const auto& get(const DeprecatedRadialGradient& gradient)
{
    if constexpr (!I)
        return gradient.colorInterpolationMethod;
    else if constexpr (I == 1)
        return gradient.gradientBox;
    else if constexpr (I == 2)
        return gradient.stops;
}

// MARK: - ConicGradient get<>

template<size_t I> const auto& get(const ConicGradient::GradientBox& gradientBox)
{
    if constexpr (!I)
        return gradientBox.angle;
    else if constexpr (I == 1)
        return gradientBox.position;
}

template<size_t I> const auto& get(const ConicGradient& gradient)
{
    if constexpr (!I)
        return gradient.colorInterpolationMethod;
    else if constexpr (I == 1)
        return gradient.gradientBox;
    else if constexpr (I == 2)
        return gradient.stops;
}

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::LinearGradient, 3)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PrefixedLinearGradient, 3)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::DeprecatedLinearGradient, 3)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::RadialGradient::Ellipse, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::RadialGradient::Circle, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::RadialGradient, 3)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PrefixedRadialGradient::Ellipse, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PrefixedRadialGradient::Circle, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::PrefixedRadialGradient, 3)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::DeprecatedRadialGradient::GradientBox, 4)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::DeprecatedRadialGradient, 3)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ConicGradient::GradientBox, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ConicGradient, 3)
