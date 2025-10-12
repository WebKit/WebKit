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

#include <WebCore/StyleImageWrapper.h>
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace Style {

// <'mask-border-source'> = none | <image>
// https://drafts.csswg.org/css-backgrounds/#propdef-mask-border-source
struct MaskBorderSource {
    MaskBorderSource(CSS::Keyword::None)
    {
    }

    MaskBorderSource(ImageWrapper&& image)
        : m_image { WTFMove(image.value) }
    {
    }

    bool isNone() const { return !m_image; }
    bool isImage() const { return !!m_image; }

    std::optional<ImageWrapper> tryImage() const { return m_image ? std::make_optional(ImageWrapper { *m_image }) : std::nullopt; }
    RefPtr<StyleImage> tryStyleImage() const { return m_image; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(ImageWrapper { *m_image });
    }

    bool operator==(const MaskBorderSource& other) const
    {
        return arePointingToEqualData(m_image, other.m_image);
    }

private:
    RefPtr<StyleImage> m_image { };
};

// MARK: - Conversion

template<> struct CSSValueConversion<MaskBorderSource> { auto operator()(BuilderState&, const CSSValue&) -> MaskBorderSource; };

// MARK: - Blending

template<> struct Blending<MaskBorderSource> {
    auto canBlend(const MaskBorderSource&, const MaskBorderSource&) -> bool;
    auto blend(const MaskBorderSource&, const MaskBorderSource&, const BlendingContext&) -> MaskBorderSource;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::MaskBorderSource)
