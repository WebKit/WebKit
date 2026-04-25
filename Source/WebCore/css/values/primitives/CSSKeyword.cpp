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
#include "CSSKeyword.h"

#include "CSSKeywordValue.h"
#include "CSSValuePool.h"
#include <wtf/Hasher.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace CSS {

// MARK: - Conversion

Ref<CSSValue> CSSValueCreation<Keyword>::operator()(CSSValuePool&, const Keyword& value)
{
    return CSSKeywordValue::create(value);
}

// MARK: - Serialization

void Serialize<Keyword>::operator()(StringBuilder& builder, const SerializationContext&, const Keyword& value)
{
    builder.append(nameLiteralForSerialization(value.value));
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const Keyword& value)
{
    return ts << nameLiteralForSerialization(value.value);
}

// MARK: - Hashing

void add(Hasher& hasher, const Keyword& value)
{
    add(hasher, value.value);
}

} // namespace CSS
} // namespace WebCore
