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

#include "CSSValue.h"
#include "CSSValueTypes.h"

namespace WebCore {
namespace CSS {

// FIXME: This is a temporary implementation of <image> to allow other strong value types
// to move forward while infrastructure still needed for the final implementation is ongoing.

struct Image {
    Ref<CSSValue> image;

    Ref<CSSValue> protectedImage() const { return image; }

    bool operator==(const Image& other) const
    {
        return compareCSSValue(image, other.image);
    }
};

template<> struct Serialize<Image> { void operator()(StringBuilder&, const SerializationContext&, const Image&); };
template<> struct ComputedStyleDependenciesCollector<Image> { void operator()(ComputedStyleDependencies&, const Image&); };
template<> struct CSSValueChildrenVisitor<Image> { IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const Image&); };

} // namespace CSS
} // namespace WebCore

namespace WTF {

// As a simple wrapper of a smart pointer, CSS::Image can also work in places a smart pointer can.
template<> struct IsSmartPtr<WebCore::CSS::Image> {
    static constexpr bool value = true;
};

} // namespace WTF
