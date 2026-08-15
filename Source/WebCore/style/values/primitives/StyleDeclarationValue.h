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

#include <WebCore/CSSVariableData.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/Ref.h>

namespace WebCore {

namespace CSS {
struct DeclarationValue;
}

namespace Style {

// <declaration-value> = <any value>
// https://drafts.csswg.org/css-syntax/#typedef-declaration-value
//
// Computed form of CSS::DeclarationValue. The tokens are carried through
// unchanged; there is nothing to compute until whatever consumes them decides
// how to interpret them.
struct DeclarationValue {
    Ref<CSSVariableData> value;

    bool operator==(const DeclarationValue&) const;
};

// MARK: - Conversion

template<> struct ToCSS<DeclarationValue> {
    auto operator()(const DeclarationValue&, const Style::ComputedStyle&) -> CSS::DeclarationValue;
};

template<> struct ToStyle<CSS::DeclarationValue> {
    auto operator()(const CSS::DeclarationValue&, const BuilderState&) -> DeclarationValue;
};

// MARK: - Serialization

template<> struct Serialize<DeclarationValue> {
    void operator()(StringBuilder&, const CSS::SerializationContext&, const Style::ComputedStyle&, const DeclarationValue&);
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const DeclarationValue&);

} // namespace Style
} // namespace WebCore
