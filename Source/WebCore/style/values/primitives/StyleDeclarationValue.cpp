/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "StyleDeclarationValue.h"

#include "CSSDeclarationValue.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

bool DeclarationValue::operator==(const DeclarationValue& other) const
{
    return arePointingToEqualData(value, other.value);
}

// MARK: - Conversion

auto ToCSS<DeclarationValue>::operator()(const DeclarationValue& value, const Style::ComputedStyle&) -> CSS::DeclarationValue
{
    return { .value = value.value };
}

auto ToStyle<CSS::DeclarationValue>::operator()(const CSS::DeclarationValue& value, const BuilderState&) -> DeclarationValue
{
    return { .value = value.value };
}

// MARK: - Serialization

void Serialize<DeclarationValue>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const Style::ComputedStyle& style, const DeclarationValue& value)
{
    CSS::serializationForCSS(builder, context, toCSS(value, style));
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const DeclarationValue& declarationValue)
{
    Ref value = declarationValue.value;
    return ts << value->serialize();
}

} // namespace Style
} // namespace WebCore
