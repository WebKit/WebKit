/*
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
 */

#pragma once

#include "CSSMeshGradient.h"
#include "StyleColor.h"
#include "StyleCoordinatePair.h"
#include "StyleGradient.h"
#include "StylePosition.h"

namespace WebCore {

class MeshGradient;

namespace Style {

// https://github.com/w3c/csswg-drafts/pull/14655

// <mesh-relative-control-point> = <coordinate-pair> [from origin]?
struct MeshGradientRelativeControlPoint {
    CoordinatePair offset;
    std::optional<CSS::Keyword::Origin> anchor;

    bool operator==(const MeshGradientRelativeControlPoint&) const = default;
};

template<size_t I> const auto& get(const MeshGradientRelativeControlPoint& value)
{
    if constexpr (!I)
        return value.offset;
    else if constexpr (I == 1)
        return value.anchor;
}

DEFINE_TYPE_MAPPING(CSS::MeshGradientRelativeControlPoint, MeshGradientRelativeControlPoint)

// <mesh-absolute-position> = <position> [ with <position> [ / <position> ]{0,3} ]?
struct MeshGradientAbsolutePosition {
    using ControlPoints = std::optional<SlashSeparatedVector<Position, 4>>;

    Position position;
    ControlPoints controlPoints;

    bool operator==(const MeshGradientAbsolutePosition&) const = default;
};

template<size_t I> const auto& get(const MeshGradientAbsolutePosition& value)
{
    if constexpr (!I)
        return value.position;
    else if constexpr (I == 1)
        return value.controlPoints;
}

DEFINE_TYPE_MAPPING(CSS::MeshGradientAbsolutePosition, MeshGradientAbsolutePosition)

// <mesh-relative-position> = by <coordinate-pair> [ with <mesh-relative-control-point> [ / <mesh-relative-control-point> ]{0,3} ]?
struct MeshGradientRelativePosition {
    using ControlPoints = std::optional<SlashSeparatedVector<MeshGradientRelativeControlPoint, 4>>;

    CoordinatePair offset;
    ControlPoints controlPoints;

    bool operator==(const MeshGradientRelativePosition&) const = default;
};

template<size_t I> const auto& get(const MeshGradientRelativePosition& value)
{
    if constexpr (!I)
        return value.offset;
    else if constexpr (I == 1)
        return value.controlPoints;
}

DEFINE_TYPE_MAPPING(CSS::MeshGradientRelativePosition, MeshGradientRelativePosition)

// <mesh-point> = <color> [ <mesh-absolute-position> | <mesh-relative-position> ]?
struct MeshGradientPoint {
    using Location = Variant<MeshGradientAbsolutePosition, MeshGradientRelativePosition>;

    Color color;
    std::optional<Location> location;

    bool operator==(const MeshGradientPoint&) const = default;
};

template<size_t I> const auto& get(const MeshGradientPoint& value)
{
    if constexpr (!I)
        return value.color;
    else if constexpr (I == 1)
        return value.location;
}

DEFINE_TYPE_MAPPING(CSS::MeshGradientPoint, MeshGradientPoint)

// <mesh-point>#{2,} [ ';' <mesh-point>#{2,} ]+
using MeshGradientRow = CommaSeparatedVector<MeshGradientPoint>;
using MeshGradientRows = SemicolonSeparatedVector<MeshGradientRow>;

// mesh-gradient( [ <color-interpolation-method> || <color> ]?, <mesh-point>#{2,} [ ';' <mesh-point>#{2,} ]+ )
struct MeshGradient {
    GradientColorInterpolationMethod colorInterpolationMethod;
    Markable<Color> backgroundColor;
    MeshGradientRows rows;

    bool operator==(const MeshGradient&) const = default;
};

template<size_t I> const auto& get(const MeshGradient& value)
{
    if constexpr (!I)
        return value.colorInterpolationMethod;
    else if constexpr (I == 1)
        return value.backgroundColor;
    else if constexpr (I == 2)
        return value.rows;
}

DEFINE_TYPE_MAPPING(CSS::MeshGradient, MeshGradient)

bool containsCurrentColor(const MeshGradient&);

Ref<WebCore::MeshGradient> createPlatformMeshGradient(const MeshGradient&, FloatSize, const ComputedStyle&);

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::MeshGradientRelativeControlPoint, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::MeshGradientAbsolutePosition, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::MeshGradientRelativePosition, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::MeshGradientPoint, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::MeshGradient, 3)
