/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
#include "XMLCharacterReferenceParser.h"

using namespace WTF;

#include "CharacterReferenceParserInlineMethods.h"

namespace WebCore {

namespace {

class XMLCharacterReferenceParser {
public:
    inline static UChar32 legalEntityFor(UChar32 value)
    {
        // FIXME: A number of specific entity values generate parse errors.
        if (!value || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        return 0xFFFD;
        return value;
    }

    inline static void convertToUTF16(UChar32 value, StringBuilder& decodedCharacter)
    {
        if (U_IS_BMP(value)) {
            UChar character = static_cast<UChar>(value);
            ASSERT(character == value);
            decodedCharacter.append(character);
            return;
        }
        decodedCharacter.append(U16_LEAD(value));
        decodedCharacter.append(U16_TRAIL(value));
    }

    inline static bool acceptMalformed() { return false; }

    inline static bool consumeNamedEntity(SegmentedString&, StringBuilder&, bool&, UChar, UChar&)
    {
        ASSERT_NOT_REACHED();
        return false;
    }
};

}

bool consumeXMLCharacterReference(SegmentedString& source, StringBuilder& decodedCharacter, bool& notEnoughCharacters)
{
    return consumeCharacterReference<XMLCharacterReferenceParser>(source, decodedCharacter, notEnoughCharacters, 0);
}

} // namespace WebCore
