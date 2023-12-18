// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSVariableData.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParserTokenRange.h"
#include "CSSValuePool.h"
#include "RenderStyle.h"
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSVariableData);

template<typename CharacterType> void CSSVariableData::updateBackingStringsInTokens()
{
    auto* currentOffset = m_backingString.characters<CharacterType>();
    for (auto& token : m_tokens) {
        if (!token.hasStringBacking())
            continue;
        unsigned length = token.value().length();
        token.updateCharacters(currentOffset, length);
        currentOffset += length;
    }
    ASSERT(currentOffset == m_backingString.characters<CharacterType>() + m_backingString.length());
}

bool CSSVariableData::operator==(const CSSVariableData& other) const
{
    return tokens() == other.tokens();
}

CSSVariableData::CSSVariableData(const CSSParserTokenRange& range, const CSSParserContext& context)
    : m_context(context)
{
    StringBuilder stringBuilder;
    m_tokens = WTF::map(range, [&](auto& token) {
        if (token.hasStringBacking())
            stringBuilder.append(token.value());
        return token;
    });
    if (!stringBuilder.isEmpty()) {
        m_backingString = stringBuilder.toString();
        if (m_backingString.is8Bit())
            updateBackingStringsInTokens<LChar>();
        else
            updateBackingStringsInTokens<UChar>();
    }
}

} // namespace WebCore
