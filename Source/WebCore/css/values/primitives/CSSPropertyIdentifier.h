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

#include "CSSValueTypes.h"

namespace WebCore {

enum CSSPropertyID : uint16_t;

namespace CSS {

struct PropertyIdentifier {
    CSSPropertyID value;

    constexpr bool operator==(const PropertyIdentifier&) const = default;
};

// MARK: - Conversion

template<> struct CSSValueCreation<PropertyIdentifier> { Ref<CSSValue> operator()(CSSValuePool&, const PropertyIdentifier&); };

// MARK: - Serialization

template<> struct Serialize<PropertyIdentifier> { void operator()(StringBuilder&, const SerializationContext&, const PropertyIdentifier&); };

// MARK: - ComputedStyle Dependencies

template<> struct ComputedStyleDependenciesCollector<PropertyIdentifier> { constexpr void operator()(ComputedStyleDependencies&, const PropertyIdentifier&) { } };

// MARK: - Children Visitor

template<> struct CSSValueChildrenVisitor<PropertyIdentifier> { constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const PropertyIdentifier&) { return IterationStatus::Continue; } };

WTF::TextStream& operator<<(WTF::TextStream&, const PropertyIdentifier&);

void NODELETE add(Hasher&, const PropertyIdentifier&);

} // namespace CSS
} // namespace WebCore
