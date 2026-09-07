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

#include "StyleFontPalette.h"
#include "StyleFontPaletteMix.h"

namespace WebCore {
namespace Style {

template<typename... F> decltype(auto) FontPalette::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
    using ResultType = decltype(visitor(std::declval<CSS::Keyword::Normal>()));

    return WTF::switchOn(m_platform,
        [&](const WebCore::FontPalette::Keyword& keyword) -> ResultType {
            switch (keyword) {
            case WebCore::FontPalette::Keyword::Normal:
                return visitor(CSS::Keyword::Normal { });
            case WebCore::FontPalette::Keyword::Light:
                return visitor(CSS::Keyword::Light { });
            case WebCore::FontPalette::Keyword::Dark:
                return visitor(CSS::Keyword::Dark { });
            }
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const AtomString& ident) -> ResultType {
            return visitor(CustomIdent { ident });
        },
        [&](const WebCore::FontPaletteMixFunction& mix) -> ResultType {
            return visitor(fromPlatform(mix));
        }
    );
}

} // namespace Style
} // namespace WebCore
