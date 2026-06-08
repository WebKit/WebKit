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

#include "config.h"
#include "StyleFontFamilyName.h"

#include "CSSFontFamilyNameValue.h"
#include "CSSMarkup.h"
#include "StyleBuilderChecking.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto ToCSS<FontFamilyName>::operator()(const FontFamilyName& value, const RenderStyle&) -> CSS::FontFamilyName
{
    return { .value = value.value };
}

auto ToStyle<CSS::FontFamilyName>::operator()(const CSS::FontFamilyName& value, const BuilderState&) -> FontFamilyName
{
    return { .value = value.value };
}

Ref<CSSValue> CSSValueCreation<FontFamilyName>::operator()(CSSValuePool& pool, const RenderStyle& style, const FontFamilyName& value)
{
    return CSS::createCSSValue(pool, toCSS(value, style));
}

auto CSSValueConversion<FontFamilyName>::operator()(BuilderState& state, const CSSFontFamilyNameValue& value) -> FontFamilyName
{
    return toStyle(value.fontFamilyName(), state);
}

auto CSSValueConversion<FontFamilyName>::operator()(BuilderState& state, const CSSValue& value) -> FontFamilyName
{
    RefPtr fontFamilyNameValue = requiredDowncast<CSSFontFamilyNameValue>(state, value);
    if (!fontFamilyNameValue) [[unlikely]]
        return { .value = nullAtom() };
    return toStyle(fontFamilyNameValue->fontFamilyName(), state);
}

// MARK: - Serialization

void Serialize<FontFamilyName>::operator()(StringBuilder& builder, const CSS::SerializationContext&, const RenderStyle&, const FontFamilyName& value)
{
    WebCore::serializeFontFamily(builder, value.value);
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const FontFamilyName& value)
{
    return ts << value.value;
}

} // namespace Style
} // namespace WebCore
