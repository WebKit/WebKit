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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StylePropertyIdentifier.h"

#include "CSSPropertyIdentifierValue.h"
#include "CSSPropertyNames.h"
#include "StyleBuilderChecking.h"
#include <wtf/Hasher.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

auto ToCSS<PropertyIdentifier>::operator()(const PropertyIdentifier& value, const RenderStyle&) -> CSS::PropertyIdentifier
{
    return { .value = value.value };
}

auto ToStyle<CSS::PropertyIdentifier>::operator()(const CSS::PropertyIdentifier& value, const BuilderState&) -> PropertyIdentifier
{
    return { .value = value.value };
}

Ref<CSSValue> CSSValueCreation<PropertyIdentifier>::operator()(CSSValuePool& pool, const RenderStyle& style, const PropertyIdentifier& value)
{
    return CSS::createCSSValue(pool, toCSS(value, style));
}

auto CSSValueConversion<PropertyIdentifier>::operator()(BuilderState& state, const CSSPropertyIdentifierValue& value) -> PropertyIdentifier
{
    return toStyle(value.propertyIdentifier(), state);
}

auto CSSValueConversion<PropertyIdentifier>::operator()(BuilderState& state, const CSSValue& value) -> PropertyIdentifier
{
    if (RefPtr propertyIdentifierValue = requiredDowncast<CSSPropertyIdentifierValue>(state, value))
        return toStyle(propertyIdentifierValue->propertyIdentifier(), state);
    return { .value = CSSPropertyInvalid };
}

// MARK: - Serialization

void Serialize<PropertyIdentifier>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const PropertyIdentifier& value)
{
    CSS::serializationForCSS(builder, context, toCSS(value, style));
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const PropertyIdentifier& value)
{
    return ts << nameLiteral(value.value);
}

// MARK: - Hashing

void add(Hasher& hasher, const PropertyIdentifier& value)
{
    add(hasher, value.value);
}

} // namespace Style
} // namespace WebCore
