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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSValueTypes.h"
#include <wtf/CompactVariant.h>

namespace WebCore {
namespace CSS {

struct DynamicRangeLimitMixFunction;

// <'dynamic-range-limit'> = standard | no-limit | constrained | <dynamic-range-limit-mix()>
// https://drafts.csswg.org/css-color-hdr/#propdef-dynamic-range-limit
struct DynamicRangeLimit {
    DynamicRangeLimit(CSS::Keyword::Standard);
    DynamicRangeLimit(CSS::Keyword::Constrained);
    DynamicRangeLimit(CSS::Keyword::NoLimit);
    DynamicRangeLimit(DynamicRangeLimitMixFunction&&);

    DynamicRangeLimit(DynamicRangeLimit&&);
    DynamicRangeLimit& operator=(DynamicRangeLimit&&);

    // Delete to avoid unnecessary unnecessary allocations on accidental copy.
    DynamicRangeLimit(const DynamicRangeLimit&) = delete;
    DynamicRangeLimit& operator=(const DynamicRangeLimit&) = delete;

    ~DynamicRangeLimit();

    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const DynamicRangeLimit&) const;

private:
    using Kind = CompactVariant<
       CSS::Keyword::Standard,
       CSS::Keyword::Constrained,
       CSS::Keyword::NoLimit,
       UniqueRef<DynamicRangeLimitMixFunction>
    >;

    Kind value;
};

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

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::DynamicRangeLimit)
