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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleImage.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// Utility type that wraps a StyleImage for use with the strong style type system.
struct ImageWrapper {
    Ref<StyleImage> value;

    bool operator==(const ImageWrapper& other) const
    {
        return arePointingToEqualData(value, other.value);
    }
};

// MARK: - Conversion

template<> struct CSSValueCreation<ImageWrapper> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const ImageWrapper&); };

// MARK: - Serialization

template<> struct Serialize<ImageWrapper> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const ImageWrapper&); };

// MARK: - Blending

template<> struct Blending<ImageWrapper> {
    auto blend(const ImageWrapper&, const ImageWrapper&, const BlendingContext&) -> ImageWrapper;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const ImageWrapper&);

} // namespace Style
} // namespace WebCore
