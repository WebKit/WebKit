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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSValue.h"
#include "CSSValueTypes.h"

namespace WebCore {
namespace CSS {

// Utility type that wraps an `<image>` production value for use with the strong style type system.
struct ImageWrapper {
    Ref<CSSValue> value;

    bool operator==(const ImageWrapper& other) const
    {
        return arePointingToEqualData(value, other.value);
    }
};

// MARK: - CSSValue Visitation

template<> struct CSSValueChildrenVisitor<ImageWrapper> { auto operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const ImageWrapper&) -> IterationStatus; };

// MARK: - DeprecatedCSSOMValue Creation

template<> struct DeprecatedCSSOMValueCreation<ImageWrapper> { Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration&, const ImageWrapper&); };

// MARK: - Serialization

template<> struct Serialize<ImageWrapper> { void operator()(StringBuilder&, const SerializationContext&, const ImageWrapper&); };

} // namespace CSS
} // namespace WebCore
