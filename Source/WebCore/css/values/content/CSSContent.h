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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSCounterStyle.h"
#include "CSSCustomIdent.h"
#include "CSSImageWrapper.h"
#include "CSSString.h"

namespace WebCore {
namespace CSS {

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

struct ContentText {
    String text;

    bool operator==(const ContentText&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ContentText, text);

struct ContentImage {
    ImageWrapper image;

    bool operator==(const ContentImage&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ContentImage, image);

struct ContentCounterFunctionParameters {
    CustomIdent identifier;
    CounterStyle style;

    bool operator==(const ContentCounterFunctionParameters&) const = default;
};
using ContentCounterFunction = FunctionNotation<CSSValueCounter, ContentCounterFunctionParameters>;

// `ContentCounterFunctionWrapper` exists to allow easily forward declaring `ContentCounterFunction`.
struct ContentCounterFunctionWrapper {
    ContentCounterFunction value;

    bool operator==(const ContentCounterFunctionWrapper&) const = default;
};

struct ContentCountersFunctionParameters {
    CustomIdent identifier;
    String separator;
    CounterStyle style;

    bool operator==(const ContentCountersFunctionParameters&) const = default;
};
using ContentCountersFunction = FunctionNotation<CSSValueCounters, ContentCountersFunctionParameters>;

// `ContentCountersFunctionWrapper` exists to allow easily forward declaring `ContentCountersFunction`.
struct ContentCountersFunctionWrapper {
    ContentCountersFunction value;

    bool operator==(const ContentCountersFunctionWrapper&) const = default;
};

struct ContentQuote {
    using Value = Variant<Keyword::OpenQuote, Keyword::CloseQuote, Keyword::NoOpenQuote, Keyword::NoCloseQuote>;
    Value quote;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(quote, std::forward<F>(f)...);
    }

    bool operator==(const ContentQuote&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ContentQuote, quote);

struct ContentLegacyAttrFunctionParameters {
    CustomIdent name;
    std::optional<String> fallback;

    bool operator==(const ContentLegacyAttrFunctionParameters&) const = default;
};
using ContentLegacyAttrFunction = FunctionNotation<CSSValueAttr, ContentLegacyAttrFunctionParameters>;

// `ContentLegacyAttrFunctionWrapper` exists to allow easily forward declaring `ContentLegacyAttrFunction`.
struct ContentLegacyAttrFunctionWrapper {
    ContentLegacyAttrFunction value;

    bool operator==(const ContentLegacyAttrFunctionWrapper&) const = default;
};

struct Content {
    using Text = ContentText;
    using Image = ContentImage;
    using CounterFunction = ContentCounterFunction;
    using CountersFunction = ContentCountersFunction;
    using Quote = ContentQuote;
    using LegacyAttrFunction = ContentLegacyAttrFunction;
    using VisibleContentListItem = Variant<Text, LegacyAttrFunction, Image, CounterFunction, CountersFunction, Quote>;
    using VisibleContentList = SpaceSeparatedVector<VisibleContentListItem>;
    using AltContentListItem = Variant<Text, LegacyAttrFunction>;
    using AltContentList = SpaceSeparatedVector<AltContentListItem>;

    struct Data {
        VisibleContentList visible;
        std::optional<AltContentList> alt;

        bool operator==(const Data&) const = default;
    };

    Content(Keyword::None keyword) : m_value { keyword } { }
    Content(Keyword::Normal keyword) : m_value { keyword } { }
    Content(Data&& data) : m_value { WTF::move(data) } { }

    bool isNone() const { return WTF::holdsAlternative<Keyword::None>(m_value); }
    bool isNormal() const { return WTF::holdsAlternative<Keyword::Normal>(m_value); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const Content&) const = default;

private:
    Variant<Keyword::None, Keyword::Normal, Data> m_value;
};

template<size_t I> const auto& get(const ContentCounterFunctionParameters& value)
{
    if constexpr (!I)
        return value.identifier;
    else if constexpr (I == 1)
        return value.style;
}

template<size_t I> const auto& get(const ContentCountersFunctionParameters& value)
{
    if constexpr (!I)
        return value.identifier;
    else if constexpr (I == 1)
        return value.separator;
    else if constexpr (I == 2)
        return value.style;
}

template<size_t I> const auto& get(const ContentLegacyAttrFunctionParameters& value)
{
    if constexpr (!I)
        return value.name;
    else if constexpr (I == 1)
        return value.fallback;
}

template<size_t I> const auto& get(const Content::Data& value)
{
    if constexpr (!I)
        return value.visible;
    else if constexpr (I == 1)
        return value.alt;
}

// MARK: - Serialization

template<> struct Serialize<ContentCounterFunctionParameters> { void operator()(StringBuilder&, const SerializationContext&, const ContentCounterFunctionParameters&); };
template<> struct Serialize<ContentCountersFunctionParameters> { void operator()(StringBuilder&, const SerializationContext&, const ContentCountersFunctionParameters&); };

// MARK: - DeprecatedCSSOMValue Creation

// Specialized to return a `DeprecatedCSSOMPrimitiveValue`.
template<> struct DeprecatedCSSOMValueCreation<ContentCounterFunction> { Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration&, const ContentCounterFunction&); };
template<> struct DeprecatedCSSOMValueCreation<ContentCountersFunction> { Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration&, const ContentCountersFunction&); };
template<> struct DeprecatedCSSOMValueCreation<ContentLegacyAttrFunction> { Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration&, const ContentLegacyAttrFunction&); };

// Specialized to return a `DeprecatedCSSOMValueList` only when both `visible` and `alt` are present.
template<> struct DeprecatedCSSOMValueCreation<Content::Data> { Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration&, const Content::Data&); };

} // namespace CSS
} // namespace WebCore

DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ContentCounterFunctionParameters, 2)
DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ContentCountersFunctionParameters, 3)
DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ContentLegacyAttrFunctionParameters, 2)
DEFINE_SLASH_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::Content::Data, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::ContentText)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::ContentImage)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::ContentQuote)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::Content)
