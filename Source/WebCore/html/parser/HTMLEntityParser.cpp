/*
 * Copyright (C) 2008-2023 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010-2014 Google, Inc. All Rights Reserved.
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
#include "HTMLEntityParser.h"

#include "CharacterReferenceParserInlines.h"
#include "HTMLEntitySearch.h"
#include "HTMLEntityTable.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

static const UChar windowsLatin1ExtensionArray[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, // 80-87
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F, // 88-8F
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178, // 98-9F
};

template<> void appendCharacterTo(Vector<UChar>& output, UChar32 c)
{
    if (U_IS_BMP(c)) {
        output.append(static_cast<UChar>(c));
        return;
    }
    output.append(U16_LEAD(c));
    output.append(U16_TRAIL(c));
}

class HTMLEntityParser {
public:
    static UChar32 legalEntityFor(UChar32 value)
    {
        if (value <= 0 || value > UCHAR_MAX_VALUE || U_IS_SURROGATE(value))
            return replacementCharacter;
        if ((value & ~0x1F) != 0x80)
            return value;
        return windowsLatin1ExtensionArray[value - 0x80];
    }

    static bool acceptMalformed() { return true; }

    template<typename DecodedIdentityType>
    static bool consumeNamedEntity(SegmentedString& source, DecodedIdentityType& decodedEntity, bool& notEnoughCharacters, UChar additionalAllowedCharacter, UChar& cc)
    {
        StringBuilder consumedCharacters;
        HTMLEntitySearch entitySearch;
        while (!source.isEmpty()) {
            cc = source.currentCharacter();
            entitySearch.advance(cc);
            if (!entitySearch.isEntityPrefix())
                break;
            consumedCharacters.append(cc);
            source.advancePastNonNewline();
        }
        notEnoughCharacters = source.isEmpty() && cc != ';';
        if (notEnoughCharacters) {
            // We can't decide on an entity because there might be a longer entity
            // that we could match if we had more data.
            unconsumeCharacters(source, consumedCharacters);
            return false;
        }
        if (!entitySearch.match()) {
            unconsumeCharacters(source, consumedCharacters);
            return false;
        }
        auto& match = *entitySearch.match();
        auto nameLength = match.nameLength();
        if (nameLength != entitySearch.currentLength()) {
            // We've consumed too many characters. Walk the source back
            // to the point at which we had consumed an actual entity.
            unconsumeCharacters(source, consumedCharacters);
            consumedCharacters.clear();
            for (unsigned i = 0; i < nameLength; ++i) {
                consumedCharacters.append(source.currentCharacter());
                source.advancePastNonNewline();
                ASSERT(!source.isEmpty());
            }
            cc = source.currentCharacter();
        }
        if (match.nameIncludesTrailingSemicolon
            || !additionalAllowedCharacter
            || !(isASCIIAlphanumeric(cc) || cc == '=')) {
            appendCharacterTo(decodedEntity, match.firstCharacter);
            if (match.optionalSecondCharacter)
                appendCharacterTo(decodedEntity, match.optionalSecondCharacter);
            return true;
        }
        unconsumeCharacters(source, consumedCharacters);
        return false;
    }
};

bool consumeHTMLEntity(SegmentedString& source, StringBuilder& decodedEntity, bool& notEnoughCharacters, UChar additionalAllowedCharacter)
{
    return consumeCharacterReference<HTMLEntityParser>(source, decodedEntity, notEnoughCharacters, additionalAllowedCharacter);
}

bool consumeHTMLEntity(SegmentedString& source, Vector<UChar>& decodedEntity, bool& notEnoughCharacters, UChar additionalAllowedCharacter)
{
    return consumeCharacterReference<HTMLEntityParser>(source, decodedEntity, notEnoughCharacters, additionalAllowedCharacter);
}

size_t decodeNamedEntityToUCharArray(const char* name, UChar result[4])
{
    HTMLEntitySearch search;
    while (*name) {
        search.advance(*name++);
        if (!search.isEntityPrefix())
            return 0;
    }
    search.advance(';');
    if (!search.isEntityPrefix())
        return 0;
    size_t index = 0;
    U16_APPEND_UNSAFE(result, index, search.match()->firstCharacter);
    if (search.match()->optionalSecondCharacter)
        U16_APPEND_UNSAFE(result, index, search.match()->optionalSecondCharacter);
    return index;
}

void appendLegalEntityFor(UChar32 character, Vector<UChar>& outputBuffer)
{
    appendCharacterTo(outputBuffer, HTMLEntityParser::legalEntityFor(character));
}

} // namespace WebCore
