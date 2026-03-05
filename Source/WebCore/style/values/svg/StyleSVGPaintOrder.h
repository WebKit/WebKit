/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StyleValueTypes.h>
#include <wtf/EnumTraits.h>

namespace WebCore {
namespace Style {

enum class PaintType : uint8_t { Fill, Stroke, Markers };

// <'paint-order'> = normal | [ fill || stroke || markers ]
// https://svgwg.org/svg2-draft/painting.html#PaintOrderProperty
// NOTE: A `SpaceSeparatedEnumSet` cannot be used here as the order of the
// values is relevant to the interpretation and serialization.
struct SVGPaintOrder {
    using value_type = std::span<const PaintType, 3>::value_type;
    using iterator = std::span<const PaintType, 3>::iterator;

    constexpr SVGPaintOrder(CSS::Keyword::Normal) { }
    constexpr SVGPaintOrder(CSS::Keyword::Fill) : m_type { Type::FillStrokeMarkers } { }
    constexpr SVGPaintOrder(CSS::Keyword::Fill, CSS::Keyword::Markers) : m_type { Type::FillMarkersStroke } { }
    constexpr SVGPaintOrder(CSS::Keyword::Stroke) : m_type { Type::StrokeFillMarkers } { }
    constexpr SVGPaintOrder(CSS::Keyword::Stroke, CSS::Keyword::Markers) : m_type { Type::StrokeMarkersFill } { }
    constexpr SVGPaintOrder(CSS::Keyword::Markers) : m_type { Type::MarkersFillStroke } { }
    constexpr SVGPaintOrder(CSS::Keyword::Markers, CSS::Keyword::Stroke) : m_type { Type::MarkersStrokeFill } { }

    constexpr bool isNormal() const { return m_type == Type::Normal; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_type) {
        case Type::Normal:
            return visitor(CSS::Keyword::Normal { });
        case Type::FillStrokeMarkers:
            return visitor(CSS::Keyword::Fill { });
        case Type::FillMarkersStroke:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Fill { }, CSS::Keyword::Markers { } });
        case Type::StrokeFillMarkers:
            return visitor(CSS::Keyword::Stroke { });
        case Type::StrokeMarkersFill:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Stroke { }, CSS::Keyword::Markers { } });
        case Type::MarkersFillStroke:
            return visitor(CSS::Keyword::Markers { });
        case Type::MarkersStrokeFill:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Markers { }, CSS::Keyword::Stroke { } });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    std::span<const PaintType, 3> paintTypes() const;

    iterator begin() const { return paintTypes().begin(); }
    iterator end() const { return paintTypes().end(); }

    constexpr bool operator==(const SVGPaintOrder&) const = default;

    // NOTE: The Type is exposed only to allow efficient storage using a bitfield.
    enum class Type : uint8_t {
        Normal,
        FillStrokeMarkers,
        FillMarkersStroke,
        StrokeFillMarkers,
        StrokeMarkersFill,
        MarkersFillStroke,
        MarkersStrokeFill,
    };
    constexpr SVGPaintOrder(Type type) : m_type { type } { }
    constexpr Type type() const { return m_type; }

    static constexpr SVGPaintOrder fromRaw(std::underlying_type_t<Type> rawValue)
    {
        return SVGPaintOrder { static_cast<Type>(rawValue) } ;
    }
    constexpr uint8_t toRaw() const { return std::to_underlying(m_type); }

private:
    Type m_type { Type::Normal };
};

// MARK: - Conversion

template<> struct CSSValueConversion<SVGPaintOrder> { auto operator()(BuilderState&, const CSSValue&) -> SVGPaintOrder; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SVGPaintOrder)
