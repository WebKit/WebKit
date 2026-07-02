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

#include "CSSImageWrapper.h"
#include "CSSValueTypes.h"
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace CSS {

// <'border-image-source'> = none | <image>
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-source
struct BorderImageSource {
    BorderImageSource(Keyword::None) { }
    BorderImageSource(ImageWrapper&& image) : m_image { WTF::move(image.value) } { }
    BorderImageSource(Ref<CSSValue>&& image) : m_image { WTF::move(image) } { }

    bool isNone() const { return !m_image; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (!m_image)
            return visitor(Keyword::None { });
        return visitor(ImageWrapper { *m_image });
    }

    bool operator==(const BorderImageSource& other) const
    {
        return arePointingToEqualData(m_image, other.m_image);
    }

private:
    RefPtr<CSSValue> m_image;
};

// MARK: - Conversion

template<> struct CSSValueCreation<BorderImageSource> { auto operator()(CSSValuePool&, const BorderImageSource&) -> Ref<CSSValue>; };

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::BorderImageSource)
