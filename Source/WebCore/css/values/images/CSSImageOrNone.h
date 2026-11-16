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
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace CSS {

struct ImageOrNone {
    ImageOrNone(CSS::Keyword::None)
    {
    }

    ImageOrNone(ImageWrapper&& image)
        : m_value { WTF::move(image.value) }
    {
    }

    ImageOrNone(RefPtr<CSSValue>&& image)
        : m_value { WTF::move(image) }
    {
    }

    bool isNone() const { return !m_value; }
    bool isImage() const { return !!m_value; }

    std::optional<ImageWrapper> tryImage() const { return m_value ? std::make_optional(ImageWrapper { *m_value }) : std::nullopt; }

    template<typename... F> decltype(auto) switchOn(NOESCAPE F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(ImageWrapper { *m_value });
    }

    bool operator==(const ImageOrNone& other) const
    {
        return arePointingToEqualData(m_value, other.m_value);
    }

private:
    RefPtr<CSSValue> m_value { };
};

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::ImageOrNone)
