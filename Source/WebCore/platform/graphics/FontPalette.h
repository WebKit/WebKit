/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/UniqueRef.h>
#include <wtf/Variant.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

struct FontPaletteMixFunction;

struct FontPalette {
    enum class Keyword : uint8_t {
        Normal,
        Light,
        Dark,
    };

    FontPalette(Keyword);
    FontPalette(AtomString&&);
    FontPalette(FontPaletteMixFunction&&);

    FontPalette(FontPalette&&);
    FontPalette& operator=(FontPalette&&);
    WEBCORE_EXPORT FontPalette(const FontPalette&);
    WEBCORE_EXPORT FontPalette& operator=(const FontPalette&);

    WEBCORE_EXPORT ~FontPalette();

    bool isNormal() const;
    bool isLight() const;
    bool isDark() const;
    std::optional<AtomString> ident() const;

    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const FontPalette&) const;

private:
    using Kind = Variant<
        Keyword,
        AtomString,
        UniqueRef<FontPaletteMixFunction>
    >;
    Kind copy(const Kind&);

    Kind m_value;
};

template<typename... F> decltype(auto) FontPalette::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
    using ResultType = decltype(visitor(std::declval<Keyword>()));

    return WTF::switchOn(m_value,
        [&](const Keyword& keyword) -> ResultType {
            return visitor(keyword);
        },
        [&](const AtomString& ident) -> ResultType {
            return visitor(ident);
        },
        [&](const UniqueRef<FontPaletteMixFunction>& mix) -> ResultType {
            return visitor(mix.get());
        }
    );
}

void add(Hasher&, const FontPalette&);

TextStream& operator<<(TextStream&, const FontPalette&);

} // namespace WebCore
