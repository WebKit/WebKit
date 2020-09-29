// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "CSSParserTokenRange.h"

#include "StyleSheetContents.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSParserToken& CSSParserTokenRange::eofToken()
{
    static NeverDestroyed<CSSParserToken> eofToken(EOFToken);
    return eofToken.get();
}

CSSParserTokenRange CSSParserTokenRange::makeSubRange(const CSSParserToken* first, const CSSParserToken* last) const
{
    if (first == &eofToken())
        first = m_last;
    if (last == &eofToken())
        last = m_last;
    ASSERT(first <= last);
    return CSSParserTokenRange(first, last);
}

CSSParserTokenRange CSSParserTokenRange::consumeBlock()
{
    ASSERT(peek().getBlockType() == CSSParserToken::BlockStart);
    const CSSParserToken* start = &peek() + 1;
    unsigned nestingLevel = 0;
    do {
        const CSSParserToken& token = consume();
        if (token.getBlockType() == CSSParserToken::BlockStart)
            nestingLevel++;
        else if (token.getBlockType() == CSSParserToken::BlockEnd)
            nestingLevel--;
    } while (nestingLevel && m_first < m_last);

    if (nestingLevel)
        return makeSubRange(start, m_first); // Ended at EOF
    return makeSubRange(start, m_first - 1);
}

CSSParserTokenRange CSSParserTokenRange::consumeBlockCheckingForEditability(StyleSheetContents* styleSheet)
{
    ASSERT(peek().getBlockType() == CSSParserToken::BlockStart);
    const auto* start = &peek() + 1;
    unsigned nestingLevel = 0;
    do {
        const auto& token = consume();
        if (token.getBlockType() == CSSParserToken::BlockStart)
            nestingLevel++;
        else if (token.getBlockType() == CSSParserToken::BlockEnd)
            nestingLevel--;

        if (styleSheet && !styleSheet->usesStyleBasedEditability() && token.type() == IdentToken && equalLettersIgnoringASCIICase(token.value(), "-webkit-user-modify"))
            styleSheet->parserSetUsesStyleBasedEditability();
    } while (nestingLevel && m_first < m_last);
    
    if (nestingLevel)
        return makeSubRange(start, m_first); // Ended at EOF
    return makeSubRange(start, m_first - 1);
}

void CSSParserTokenRange::consumeComponentValue()
{
    // FIXME: This is going to do multiple passes over large sections of a stylesheet.
    // We should consider optimising this by precomputing where each block ends.
    unsigned nestingLevel = 0;
    do {
        const CSSParserToken& token = consume();
        if (token.getBlockType() == CSSParserToken::BlockStart)
            nestingLevel++;
        else if (token.getBlockType() == CSSParserToken::BlockEnd)
            nestingLevel--;
    } while (nestingLevel && m_first < m_last);
}

String CSSParserTokenRange::serialize() const
{
    StringBuilder builder;
    for (const CSSParserToken* it = m_first; it < m_last; ++it)
        it->serialize(builder, it + 1 == m_last ? nullptr : it + 1);
    return builder.toString();
}

} // namespace WebCore
