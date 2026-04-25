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
#include "StyleString.h"

#include "CSSStringValue.h"
#include "StyleBuilderChecking.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto ToCSS<String>::operator()(const String& value, const RenderStyle&) -> CSS::String
{
    return { .value = value.value };
}

auto ToStyle<CSS::String>::operator()(const CSS::String& value, const BuilderState&) -> String
{
    return { .value = value.value };
}

Ref<CSSValue> CSSValueCreation<String>::operator()(CSSValuePool& pool, const RenderStyle& style, const String& value)
{
    return CSS::createCSSValue(pool, toCSS(value, style));
}

auto CSSValueConversion<String>::operator()(BuilderState& state, const CSSStringValue& value) -> String
{
    return toStyle(value.string(), state);
}

auto CSSValueConversion<String>::operator()(BuilderState& state, const CSSValue& value) -> String
{
    RefPtr stringValue = requiredDowncast<CSSStringValue>(state, value);
    if (!stringValue) [[unlikely]]
        return { .value = nullString() };
    return toStyle(stringValue->string(), state);
}

// MARK: - Serialization

void Serialize<String>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const String& value)
{
    CSS::serializationForCSS(builder, context, toCSS(value, style));
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const String& value)
{
    return ts << value.value;
}

// MARK: - Hashing

void add(Hasher& hasher, const String& value)
{
    add(hasher, value.value);
}

} // namespace Style
} // namespace WebCore
