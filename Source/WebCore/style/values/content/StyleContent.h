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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleCounterStyle.h>
#include <WebCore/StyleImageWrapper.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <leader()>            = leader( <leader-type> )
// <target>              = <target-counter()> | <target-counters()> | <target-text()>
// <counter>             = <counter()> | <counters()>
// <quote>               = open-quote | close-quote | no-open-quote | no-close-quote
// <content-replacement> = <image>
// <content-list>        = [ <string> | contents | <image> | <counter> | <quote> | <target> | <leader()> ]+
// <alt-content>         = [ <string> | <counter> ]+

// <'content'>           = normal | none | [ <content-replacement> | <content-list> ] [/ <alt-content> ]?
// https://www.w3.org/TR/css-content-3/#propdef-content

// MISSING from <content-list>:
//    contents
//    <leader()>
//    <target>

// MISSING from <alt-content>:
//   <counter>

struct Content {
    struct Text {
        String text;

        bool operator==(const Text&) const = default;
    };
    struct Image {
        ImageWrapper image;

        bool operator==(const Image&) const = default;
    };
    struct Counter {
        AtomString identifier;
        AtomString separator;
        CounterStyle style;

        template<typename... F> decltype(auto) switchOn(F&&...) const;

        bool operator==(const Counter&) const = default;
    };
    struct Quote {
        QuoteType quote;

        bool operator==(const Quote&) const = default;
    };
    using ListItem = Variant<Text, Image, Counter, Quote>;
    using List = SpaceSeparatedFixedVector<ListItem>;

    // FIXME: This struct could be optimized down to a pointer when unused by using TrailingArray.
    struct Data {
        List list;
        Markable<String> altText;

        bool operator==(const Data&) const = default;
    };

    Content(CSS::Keyword::None keyword)
        : m_value { keyword }
    {
    }

    Content(CSS::Keyword::Normal keyword)
        : m_value { keyword }
    {
    }

    Content(Data&& data)
        : m_value { WTFMove(data) }
    {
    }

    bool isNone() const { return WTF::holdsAlternative<CSS::Keyword::None>(m_value); }
    bool isNormal() const { return WTF::holdsAlternative<CSS::Keyword::Normal>(m_value); }
    bool isData() const { return WTF::holdsAlternative<Data>(m_value); }

    const Data* tryData() const LIFETIME_BOUND
    {
        return std::get_if<Data>(&m_value);
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const Content&) const = default;

private:
    // FIXME: If Data is converted to be pointer sized when unused using TrailingArray (see above), this can be converted to use a CompactVariant.
    Variant<CSS::Keyword::None, CSS::Keyword::Normal, Data> m_value;
};

template<size_t I> const auto& get(const Content::Data& value)
{
    if constexpr (!I)
        return value.list;
    else if constexpr (I == 1)
        return value.altText;
}

DEFINE_TYPE_WRAPPER_GET(Content::Text, text);
DEFINE_TYPE_WRAPPER_GET(Content::Image, image);
DEFINE_TYPE_WRAPPER_GET(Content::Quote, quote);

template<typename... F> decltype(auto) Content::Counter::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    using CounterFunction = FunctionNotation<CSSValueCounter, CommaSeparatedTuple<CustomIdentifier, std::optional<CounterStyle>>>;
    using CountersFunction = FunctionNotation<CSSValueCounters, CommaSeparatedTuple<CustomIdentifier, AtomString, std::optional<CounterStyle>>>;

    if (separator.isEmpty()) {
        return visitor(CounterFunction {
            .parameters = { CustomIdentifier { identifier }, style != nameString(CSSValueDecimal) ? std::make_optional(style) : std::nullopt }
        });
    } else {
        return visitor(CountersFunction {
            .parameters = { CustomIdentifier { identifier }, separator, style != nameString(CSSValueDecimal) ? std::make_optional(style) : std::nullopt }
        });
    }
}

// MARK: - Conversion

template<> struct CSSValueConversion<Content> { auto operator()(BuilderState&, const CSSValue&) -> Content; };

// `Content::Counter` is special-cased to return a `CSSCounterValue`.
template<> struct CSSValueCreation<Content::Counter> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const Content::Counter&); };

} // namespace Style
} // namespace WebCore

DEFINE_SLASH_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Content::Data, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::Content::Text)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::Content::Image)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::Content::Quote)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Content::Counter)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Content)
