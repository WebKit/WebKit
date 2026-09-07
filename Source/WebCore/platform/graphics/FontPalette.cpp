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

#include "config.h"
#include "FontPalette.h"

#include "FontPaletteMix.h"
#include <wtf/Hasher.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

// NOTE: All non-template functions must be implemented in the implementation
// file due to the use of the incomplete type `FontPaletteMixFunction`.

FontPalette::FontPalette(Keyword keyword)
    : m_value { keyword }
{
}

FontPalette::FontPalette(AtomString&& ident)
    : m_value { WTF::move(ident) }
{
}

FontPalette::FontPalette(FontPaletteMixFunction&& mix)
    : m_value { WTF::makeUniqueRef<FontPaletteMixFunction>(WTF::move(mix)) }
{
}

FontPalette::FontPalette(FontPalette&&) = default;
FontPalette& FontPalette::operator=(FontPalette&&) = default;

FontPalette::FontPalette(const FontPalette& other)
    : m_value { copy(other.m_value) }
{
}

FontPalette& FontPalette::operator=(const FontPalette& other)
{
    m_value = copy(other.m_value);
    return *this;
}

FontPalette::~FontPalette() = default;

bool FontPalette::operator==(const FontPalette&) const = default;

FontPalette::Kind FontPalette::copy(const Kind& other)
{
    return WTF::switchOn(other,
        [](const auto& value) -> Kind {
            return value;
        },
        [](const UniqueRef<FontPaletteMixFunction>& function) -> Kind {
            return makeUniqueRef<FontPaletteMixFunction>(function.get());
        }
    );
}

bool FontPalette::isNormal() const
{
    if (auto* keyword = std::get_if<Keyword>(&m_value))
        return *keyword == Keyword::Normal;
    return false;
}

bool FontPalette::isLight() const
{
    if (auto* keyword = std::get_if<Keyword>(&m_value))
        return *keyword == Keyword::Light;
    return false;
}

bool FontPalette::isDark() const
{
    if (auto* keyword = std::get_if<Keyword>(&m_value))
        return *keyword == Keyword::Dark;
    return false;
}

std::optional<AtomString> FontPalette::ident() const
{
    if (auto* ident = std::get_if<AtomString>(&m_value))
        return std::optional { *ident };
    return std::nullopt;
}

void add(Hasher& hasher, const FontPalette& fontPalette)
{
    WTF::switchOn(fontPalette,
        [&](const FontPalette::Keyword& keyword) {
            add(hasher, 1, keyword);
        },
        [&](const AtomString& ident) {
            add(hasher, 2, ident);
        },
        [&](const FontPaletteMixFunction& mix) {
            add(hasher, 3, mix);
        }
    );
}

TextStream& operator<<(TextStream& ts, const FontPalette& fontPalette)
{
    WTF::switchOn(fontPalette,
        [&](const FontPalette::Keyword& keyword) {
            switch (keyword) {
            case FontPalette::Keyword::Normal:
                ts << "normal"_s;
                break;
            case FontPalette::Keyword::Light:
                ts << "light"_s;
                break;
            case FontPalette::Keyword::Dark:
                ts << "dark"_s;
                break;
            }
        },
        [&](const AtomString& ident) {
            ts << "custom: "_s << ident;
        },
        [&](const FontPaletteMixFunction& mix) {
            ts << mix;
        }
    );
    return ts;
}

} // namespace WebCore
