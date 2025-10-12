/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StylePathFunction.h>

namespace WebCore {
namespace Style {

// <'d'> = none | <path()>
// https://svgwg.org/svg2-draft/paths.html#DProperty
// NOTE: The type is `SVGPathData` as `D` is just a bit too opaque.
struct SVGPathData {
    SVGPathData(CSS::Keyword::None)
    {
    }

    SVGPathData(PathFunction&& path)
        : m_path { WTFMove(path) }
    {
    }

    bool isNone() const { return !m_path; }
    bool isPath() const { return !!m_path; }
    const std::optional<PathFunction>& tryPath() const LIFETIME_BOUND { return m_path; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(*m_path);
    }

    bool operator==(const SVGPathData&) const = default;

private:
    std::optional<PathFunction> m_path { };
};

// MARK: - Conversion

template<> struct CSSValueConversion<SVGPathData> { auto operator()(BuilderState&, const CSSValue&) -> SVGPathData; };
template<> struct CSSValueCreation<SVGPathData> { auto operator()(CSSValuePool&, const RenderStyle&, const SVGPathData&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<SVGPathData> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const SVGPathData&); };

// MARK: - Blending

template<> struct Blending<SVGPathData> {
    auto canBlend(const SVGPathData&, const SVGPathData&) -> bool;
    auto blend(const SVGPathData&, const SVGPathData&, const BlendingContext&) -> SVGPathData;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SVGPathData)
