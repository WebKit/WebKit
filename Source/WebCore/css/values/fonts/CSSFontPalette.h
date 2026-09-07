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

#include "CSSCustomIdent.h"
#include "CSSKeyword.h"
#include <wtf/Variant.h>

namespace WebCore {
namespace CSS {

struct FontPaletteMixFunction;

// <'font-palette'> = normal | light | dark | <palette-identifier> | <palette-mix()>
// https://drafts.csswg.org/css-fonts-4/#propdef-font-palette
struct FontPalette {
    FontPalette(Keyword::Normal);
    FontPalette(Keyword::Light);
    FontPalette(Keyword::Dark);
    FontPalette(CustomIdent&&);
    FontPalette(FontPaletteMixFunction&&);

    FontPalette(FontPalette&&);
    FontPalette& operator=(FontPalette&&);
    FontPalette(const FontPalette&);
    FontPalette& operator=(const FontPalette&);

    ~FontPalette();

    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const FontPalette&) const;

private:
    using Kind = Variant<
        Keyword::Normal,
        Keyword::Light,
        Keyword::Dark,
        CustomIdent,
        UniqueRef<FontPaletteMixFunction>
    >;
    Kind copy(const Kind&);

    Kind m_value;
};

template<typename... F> decltype(auto) FontPalette::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
    using ResultType = decltype(visitor(std::declval<Keyword::Normal>()));

    return WTF::switchOn(m_value,
        [&]<CSSValueID Id>(const Constant<Id>& keyword) -> ResultType {
            return visitor(keyword);
        },
        [&](const CustomIdent& ident) -> ResultType {
            return visitor(ident);
        },
        [&](const UniqueRef<FontPaletteMixFunction>& mix) -> ResultType {
            return visitor(mix.get());
        }
    );
}

// MARK: - Conversion

template<> struct CSSValueCreation<FontPalette> { Ref<CSSValue> operator()(CSSValuePool&, const FontPalette&); };

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::FontPalette)
