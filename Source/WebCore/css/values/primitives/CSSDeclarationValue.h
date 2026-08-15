/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/CSSValueTypes.h>
#include <WebCore/CSSVariableData.h>
#include <wtf/Ref.h>

namespace WebCore {
namespace CSS {

// <declaration-value> = <any value>
// https://drafts.csswg.org/css-syntax/#typedef-declaration-value
//
// An arbitrary token sequence, held unresolved. A <declaration-value> has no type
// until whatever consumes it decides how to interpret it, so it is kept as tokens
// rather than being canonicalized into one of the typed primitives.
struct DeclarationValue {
    Ref<CSSVariableData> value;

    bool operator==(const DeclarationValue&) const;
};

template<> struct Serialize<DeclarationValue> {
    void operator()(StringBuilder&, const SerializationContext&, const DeclarationValue&);
};

template<> struct ComputedStyleDependenciesCollector<DeclarationValue> {
    constexpr void operator()(ComputedStyleDependencies&, const DeclarationValue&) { }
};

template<> struct CSSValueChildrenVisitor<DeclarationValue> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const DeclarationValue&)
    {
        return IterationStatus::Continue;
    }
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const DeclarationValue&);

} // namespace CSS
} // namespace WebCore
