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
#include "CSSFontPalette.h"

#include "CSSFontPaletteMix.h"
#include "CSSFontPaletteValue.h"

namespace WebCore {
namespace CSS {

// NOTE: All non-template functions must be implemented in the implementation
// file due to the use of the incomplete type `FontPaletteMixFunction`.

FontPalette::FontPalette(Keyword::Normal keyword)
    : m_value { keyword }
{
}

FontPalette::FontPalette(Keyword::Light keyword)
    : m_value { keyword }
{
}

FontPalette::FontPalette(Keyword::Dark keyword)
    : m_value { keyword }
{
}

FontPalette::FontPalette(CustomIdent&& ident)
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

// MARK: - Conversion

Ref<CSSValue> CSSValueCreation<FontPalette>::operator()(CSSValuePool&, const FontPalette& value)
{
    return CSSFontPaletteValue::create(FontPalette { value });
}

} // namespace CSS
} // namespace WebCore
