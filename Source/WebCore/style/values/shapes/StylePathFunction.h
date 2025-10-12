/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSPathFunction.h>
#include <WebCore/StyleFillRule.h>
#include <WebCore/StylePathComputation.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleWindRuleComputation.h>

namespace WebCore {
namespace Style {

enum class PathConversion : bool { None, ForceAbsolute };

struct Path {
    struct Data {
        SVGPathByteStream byteStream;

        bool operator==(const Data&) const = default;
    };

    std::optional<FillRule> fillRule;
    Data data;

    float zoom { 1 };

    bool operator==(const Path&) const = default;
};
using PathFunction = FunctionNotation<CSSValuePath, Path>;

template<size_t I> const auto& get(const Path& value)
{
    if constexpr (!I)
        return value.fillRule;
    else if constexpr (I == 1)
        return value.data;
    else if constexpr (I == 2)
        return value.zoom;
}

// MARK: - Conversion

// Non-standard parameters, `conversion` and `zoom`, are needed in some instances of Style <-> CSS conversions for Path.

template<> struct ToCSS<Path> { auto operator()(const Path&, const RenderStyle&, PathConversion = PathConversion::None) -> CSS::Path; };
template<> struct ToStyle<CSS::Path> { auto operator()(const CSS::Path&, const BuilderState&, std::optional<float> zoom = 1.0f) -> Path; };

template<> struct ToCSS<Path::Data> { auto operator()(const Path::Data&, const RenderStyle&, PathConversion = PathConversion::None) -> CSS::Path::Data; };
template<> struct ToStyle<CSS::Path::Data> { auto operator()(const CSS::Path::Data&, const BuilderState&) -> Path::Data; };

template<> struct CSSValueCreation<PathFunction> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const PathFunction&, PathConversion = PathConversion::None); };

// MARK: - Serialization

template<> struct Serialize<Path> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const Path&, PathConversion = PathConversion::None); };

// MARK: - Path

template<> struct PathComputation<Path> { WebCore::Path operator()(const Path&, const FloatRect&); };

// MARK: - Wind Rule

template<> struct WindRuleComputation<Path> { WebCore::WindRule operator()(const Path&); };

// MARK: - Blending

template<> struct Blending<Path> {
    auto canBlend(const Path&, const Path&) -> bool;
    auto blend(const Path&, const Path&, const BlendingContext&) -> Path;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const Path::Data&);

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Path, 2)
