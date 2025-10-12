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

#include <WebCore/StyleColor.h>
#include <WebCore/StyleURL.h>

namespace WebCore {
namespace Style {

enum class ForVisitedLink : bool;

// <paint> = none | <color> | <url> [none | <color>]? | context-fill | context-stroke
// NOTE: `context-fill` and `context-stroke` are not implemented.
// https://svgwg.org/svg2-draft/painting.html#SpecifyingPaint
struct SVGPaint {
    struct URLNone {
        URL url;
        CSS::Keyword::None none;

        bool operator==(const URLNone&) const = default;
    };
    struct URLColor {
        URL url;
        Color color;

        bool operator==(const URLColor&) const = default;
    };

    SVGPaint(CSS::Keyword::None)
        : m_type { Type::None }
    {
    }
    SVGPaint(Style::Color&& value)
        : m_type { Type::Color }
        , m_color { WTFMove(value) }
    {
    }
    SVGPaint(Style::URL&& value)
        : m_type { Type::URL }
        , m_url { WTFMove(value) }
    {
    }
    SVGPaint(URLNone&& value)
        : m_type { Type::URLNone }
        , m_url { WTFMove(value.url) }
    {
    }
    SVGPaint(URLColor&& value)
        : m_type { Type::URLColor }
        , m_url { WTFMove(value.url) }
        , m_color { WTFMove(value.color) }
    {
    }

    bool isNone() const { return m_type == Type::None; }
    bool isColor() const { return m_type == Type::Color; }
    bool isURL() const { return m_type == Type::URL; }
    bool isURLNone() const { return m_type == Type::URLNone; }
    bool isURLColor() const { return m_type == Type::URLColor; }

    bool hasColor() const { return isColor() || isURLColor(); }
    bool hasURL() const { return isURL() || isURLNone() || isURLColor(); }

    std::optional<Color> tryColor() const { return isColor() ? std::make_optional(m_color) : std::nullopt; }
    std::optional<URL> tryURL() const { return isURL() ? std::make_optional(m_url) : std::nullopt; }
    std::optional<URLNone> tryURLNone() const { return isURLNone() ? std::make_optional(URLNone { m_url, CSS::Keyword::None { } }) : std::nullopt; }
    std::optional<URLColor> tryURLColor() const { return isURLColor() ? std::make_optional(URLColor { m_url, m_color }) : std::nullopt; }

    std::optional<Color> tryAnyColor() const { return hasColor() ? std::make_optional(m_color) : std::nullopt; }
    std::optional<URL> tryAnyURL() const { return hasURL() ? std::make_optional(m_url) : std::nullopt; }

    const Color& colorDisregardingType() const { return m_color; }
    const URL& urlDisregardingType() const { return m_url; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_type) {
        case Type::None:
            return visitor(CSS::Keyword::None { });
        case Type::Color:
            return visitor(m_color);
        case Type::URL:
            return visitor(m_url);
        case Type::URLNone:
            return visitor(URLNone { m_url, CSS::Keyword::None { } });
        case Type::URLColor:
            return visitor(URLColor { m_url, m_color });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const SVGPaint&) const = default;

    bool hasSameType(const SVGPaint& other) const { return m_type == other.m_type; }

private:
    enum class Type : uint8_t {
        None,
        Color,
        URL,
        URLNone,
        URLColor
    };

    Type m_type;
    URL m_url { .resolved = { }, .modifiers = { } };
    Color m_color { WebCore::Color { } };
};

template<size_t I> const auto& get(const SVGPaint::URLNone& value)
{
    if constexpr (!I)
        return value.url;
    else if constexpr (I == 1)
        return value.none;
}

template<size_t I> const auto& get(const SVGPaint::URLColor& value)
{
    if constexpr (!I)
        return value.url;
    else if constexpr (I == 1)
        return value.color;
}

bool containsCurrentColor(const Style::SVGPaint&);

// MARK: - Conversion

template<> struct CSSValueConversion<SVGPaint> { auto operator()(BuilderState&, const CSSValue&, ForVisitedLink) -> SVGPaint; };

// MARK: - Blending

template<> struct Blending<SVGPaint> {
    auto equals(const SVGPaint&, const SVGPaint&, const RenderStyle&, const RenderStyle&) -> bool;
    auto canBlend(const SVGPaint&, const SVGPaint&) -> bool;
    auto blend(const SVGPaint&, const SVGPaint&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> SVGPaint;
};

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::SVGPaint::URLNone, 2)
DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::SVGPaint::URLColor, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SVGPaint)
