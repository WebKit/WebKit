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

#include "CSSImage.h"
#include "CSSPrimitiveNumericTypes.h"
#include <wtf/CompactVariant.h>

namespace WebCore {
namespace CSS {

// <'border-image-source'> = none | <image>
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-source
struct BorderImageSource {
    BorderImageSource(CSS::Keyword::None keyword)
        : value { keyword }
    {
    }

    BorderImageSource(CSS::Image&& image)
        : value { WTFMove(image) }
    {
    }

    template<typename T> bool holdsAlternative() const
    {
        return WTF::holdsAlternative<T>(value);
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const BorderImageSource&) const = default;

private:
    CompactVariant<CSS::Keyword::None, CSS::Image> value;
};

} // namespace CSS
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::BorderImageSource> = true;
