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

#include <WebCore/StyleValueTypes.h>

namespace WebCore {

class CSSPropertyIdentifierValue;
enum CSSPropertyID : uint16_t;

namespace CSS {
struct PropertyIdentifier;
}

namespace Style {

struct PropertyIdentifier {
    CSSPropertyID value;

    bool operator==(const PropertyIdentifier&) const = default;
};

// MARK: Conversion

template<> struct ToCSS<PropertyIdentifier> { auto operator()(const PropertyIdentifier&, const RenderStyle&) -> CSS::PropertyIdentifier; };
template<> struct ToStyle<CSS::PropertyIdentifier> { auto operator()(const CSS::PropertyIdentifier&, const BuilderState&) -> PropertyIdentifier; };

template<> struct CSSValueCreation<PropertyIdentifier> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const PropertyIdentifier&); };
template<> struct CSSValueConversion<PropertyIdentifier> {
    auto operator()(BuilderState&, const CSSPropertyIdentifierValue&) -> PropertyIdentifier;
    auto operator()(BuilderState&, const CSSValue&) -> PropertyIdentifier;
};

// MARK: Serialization

template<> struct Serialize<PropertyIdentifier> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const PropertyIdentifier&); };

// MARK: Logging

TextStream& operator<<(TextStream&, const PropertyIdentifier&);

// MARK: Hashing

void NODELETE add(Hasher&, const PropertyIdentifier&);

} // namespace Style
} // namespace WebCore
