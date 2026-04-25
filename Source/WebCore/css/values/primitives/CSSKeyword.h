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

#include <WebCore/CSSValueKeywords.h>
#include <WebCore/CSSValueTypes.h>

namespace WebCore {
namespace CSS {

// NOTE: The `CSS::Keyword` struct is defined in the generated header "CSSValueKeywords.h".

// MARK: - Concepts

// Concept for use in generic contexts to filter on Constant keyword CSS types.
template<typename Keyword> concept SpecificKeyword
    = std::same_as<Keyword, Constant<Keyword::value>>;

// MARK: - Conversion

template<> struct CSSValueCreation<Keyword> { Ref<CSSValue> operator()(CSSValuePool&, const Keyword&); };

// MARK: - Serialization

template<> struct Serialize<Keyword> { void operator()(StringBuilder&, const SerializationContext&, const Keyword&); };

// MARK: - ComputedStyle Dependencies

template<> struct ComputedStyleDependenciesCollector<Keyword> { constexpr void operator()(ComputedStyleDependencies&, const Keyword&) { } };

// MARK: - Children Visitor

template<> struct CSSValueChildrenVisitor<Keyword> { constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const Keyword&) { return IterationStatus::Continue; } };

WTF::TextStream& operator<<(WTF::TextStream&, const Keyword&);

void NODELETE add(Hasher&, const Keyword&);

} // namespace CSS
} // namespace WebCore
