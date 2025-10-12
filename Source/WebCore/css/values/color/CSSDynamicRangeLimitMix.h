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

#include "CSSDynamicRangeLimit.h"
#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// dynamic-range-limit-mix() = dynamic-range-limit-mix( [ <'dynamic-range-limit'> && <percentage [0,100]> ]#{2,} )
// https://drafts.csswg.org/css-color-hdr/#dynamic-range-limit-mix
using DynamicRangeLimitMixPercentage = CSS::Percentage<Range{0, 100}>;
using DynamicRangeLimitMixComponent = SpaceSeparatedTuple<DynamicRangeLimit, DynamicRangeLimitMixPercentage>;
using DynamicRangeLimitMixParameters = CommaSeparatedVector<DynamicRangeLimitMixComponent>;

// NOTE: Type wrapper is used here to allow forward declaration of the mix function in `CSSDynamicRangeLimit.h`.
using DynamicRangeLimitMixFunctionValue = FunctionNotation<CSSValueDynamicRangeLimitMix, DynamicRangeLimitMixParameters>;
DEFINE_TYPE_WRAPPER(DynamicRangeLimitMixFunction, DynamicRangeLimitMixFunctionValue);

// Overload of operator== for UniqueRef<DynamicRangeLimitMixFunction> to make DynamicRangeLimit::Kind's operator== work.
// FIXME: Replace use of direct UniqueRef with something like std::indirect from https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3019r12.pdf to get this for free.
inline bool operator==(const UniqueRef<DynamicRangeLimitMixFunction>& a, const UniqueRef<DynamicRangeLimitMixFunction>& b)
{
    return arePointingToEqualData(a, b);
}

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::DynamicRangeLimitMixFunction)
