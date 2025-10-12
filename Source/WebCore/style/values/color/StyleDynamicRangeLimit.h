/*
 * Copyright (C) 2025 Sam Weinig. All rights reserved.
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

#include <WebCore/StyleDynamicRangeLimitMix.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/CompactVariant.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class PlatformDynamicRangeLimit;

namespace CSS {
struct DynamicRangeLimit;
}

namespace Style {

struct DynamicRangeLimit {
    DynamicRangeLimit(CSS::Keyword::Standard);
    DynamicRangeLimit(CSS::Keyword::Constrained);
    DynamicRangeLimit(CSS::Keyword::NoLimit);
    DynamicRangeLimit(DynamicRangeLimitMixFunction&&);

    DynamicRangeLimit(DynamicRangeLimit&&) = default;
    DynamicRangeLimit& operator=(DynamicRangeLimit&&) = default;

    DynamicRangeLimit(const DynamicRangeLimit&);
    DynamicRangeLimit& operator=(const DynamicRangeLimit&);

    template<typename... F> decltype(auto) switchOn(F&&...) const;

    WEBCORE_EXPORT PlatformDynamicRangeLimit toPlatformDynamicRangeLimit() const;

    bool operator==(const DynamicRangeLimit&) const = default;

private:
    using Kind = CompactVariant<
       CSS::Keyword::Standard,
       CSS::Keyword::Constrained,
       CSS::Keyword::NoLimit,
       UniqueRef<DynamicRangeLimitMixFunction>
    >;

    static Kind copyKind(const Kind&);

    Kind value;
};

inline DynamicRangeLimit::DynamicRangeLimit(CSS::Keyword::Standard keyword)
    : value { keyword }
{
}

inline DynamicRangeLimit::DynamicRangeLimit(CSS::Keyword::Constrained keyword)
    : value { keyword }
{
}

inline DynamicRangeLimit::DynamicRangeLimit(CSS::Keyword::NoLimit keyword)
    : value { keyword }
{
}

inline DynamicRangeLimit::DynamicRangeLimit(DynamicRangeLimitMixFunction&& mix)
    : value { WTF::makeUniqueRef<DynamicRangeLimitMixFunction>(WTFMove(mix)) }
{
}

inline DynamicRangeLimit::DynamicRangeLimit(const DynamicRangeLimit& other)
    : value { copyKind(other.value) }
{
}

inline DynamicRangeLimit& DynamicRangeLimit::operator=(const DynamicRangeLimit& other)
{
    if (this == &other)
        return *this;
    value = copyKind(other.value);
    return *this;
}

template<typename... F> decltype(auto) DynamicRangeLimit::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
    using ResultType = decltype(visitor(std::declval<CSS::Keyword::Standard>()));

    return WTF::switchOn(value,
        [&]<CSSValueID Id>(const Constant<Id>& keyword) -> ResultType {
            return visitor(keyword);
        },
        [&](const UniqueRef<DynamicRangeLimitMixFunction>& mix) -> ResultType {
            return visitor(mix.get());
        }
    );
}

inline DynamicRangeLimit::Kind DynamicRangeLimit::copyKind(const Kind& other)
{
    return WTF::switchOn(other,
        []<CSSValueID Id>(const Constant<Id>& keyword) {
            return Kind { keyword };
        },
        [](const UniqueRef<DynamicRangeLimitMixFunction>& mix) {
            return Kind { WTF::makeUniqueRef<DynamicRangeLimitMixFunction>(mix) };
        }
    );
}

// MARK: Conversion

template<> struct ToCSS<DynamicRangeLimit> { auto operator()(const DynamicRangeLimit&, const RenderStyle&) -> CSS::DynamicRangeLimit; };
template<> struct ToStyle<CSS::DynamicRangeLimit> { auto operator()(const CSS::DynamicRangeLimit&, const BuilderState&) -> DynamicRangeLimit; };

// `DynamicRangeLimit` is special-cased to return a `CSSDynamicRangeLimitValue`.
template<> struct CSSValueCreation<DynamicRangeLimit> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const DynamicRangeLimit&); };
template<> struct CSSValueConversion<DynamicRangeLimit> { auto operator()(BuilderState&, const CSSValue&) -> DynamicRangeLimit; };

// MARK: Serialization

template<> struct Serialize<DynamicRangeLimit> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const DynamicRangeLimit&); };

// MARK: Blending

template<> struct Blending<DynamicRangeLimit> {
    auto blend(const DynamicRangeLimit&, const DynamicRangeLimit&, const BlendingContext&) -> DynamicRangeLimit;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::DynamicRangeLimit)
