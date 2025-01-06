/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSValueTypes.h"
#include <wtf/text/WTFString.h>

namespace WebCore {
namespace CSS {

// https://drafts.fxtf.org/filter-effects/#typedef-filter-url
struct FilterReference {
    String url;

    bool operator==(const FilterReference&) const = default;
};

template<> struct Serialize<FilterReference> { void operator()(StringBuilder&, const FilterReference&); };
template<> struct ComputedStyleDependenciesCollector<FilterReference> { constexpr void operator()(ComputedStyleDependencies&, const FilterReference&) { } };
template<> struct CSSValueChildrenVisitor<FilterReference> { constexpr IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const FilterReference&) { return IterationStatus::Continue; } };

} // namespace CSS
} // namespace WebCore
