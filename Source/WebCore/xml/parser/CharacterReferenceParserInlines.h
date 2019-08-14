/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#pragma once

#include <wtf/text/StringBuilder.h>

namespace WebCore {

inline void unconsumeCharacters(SegmentedString& source, StringBuilder& consumedCharacters)
{
    source.pushBack(consumedCharacters.toString());
}

template <typename ParserFunctions>
bool consumeCharacterReference(SegmentedString& source, StringBuilder& decodedCharacter, bool& notEnoughCharacters, UChar additionalAllowedCharacter)
{
    ASSERT(!additionalAllowedCharacter || additionalAllowedCharacter == '"' || additionalAllowedCharacter == '\'' || additionalAllowedCharacter == '>');
    ASSERT(!notEnoughCharacters);
    ASSERT(decodedCharacter.isEmpty());
    
    enum {
        Initial,
        Number,
        MaybeHexLowerCaseX,
        MaybeHexUpperCaseX,
        Hex,
        Decimal,
        Named
    } state = Initial;
    UChar32 result = 0;
    bool overflow = false;
    StringBuilder consumedCharacters;
    
    while (!source.isEmpty()) {
        UChar character = source.currentCharacter();
        switch (state) {
        case Initial:
            if (character == '\x09' || character == '\x0A' || character == '\x0C' || character == ' ' || character == '<' || character == '&')
                return false;
            if (additionalAllowedCharacter && character == additionalAllowedCharacter)
                return false;
            if (character == '#') {
                state = Number;
                break;
            }
            if (isASCIIAlpha(character)) {
                state = Named;
                goto Named;
            }
            return false;
        case Number:
            if (character == 'x') {
                state = MaybeHexLowerCaseX;
                break;
            }
            if (character == 'X') {
                state = MaybeHexUpperCaseX;
                break;
            }
            if (isASCIIDigit(character)) {
                state = Decimal;
                goto Decimal;
            }
            source.pushBack("#"_s);
            return false;
        case MaybeHexLowerCaseX:
            if (isASCIIHexDigit(character)) {
                state = Hex;
                goto Hex;
            }
            source.pushBack("#x"_s);
            return false;
        case MaybeHexUpperCaseX:
            if (isASCIIHexDigit(character)) {
                state = Hex;
                goto Hex;
            }
            source.pushBack("#X"_s);
            return false;
        case Hex:
        Hex:
            if (isASCIIHexDigit(character)) {
                result = result * 16 + toASCIIHexValue(character);
                if (result > UCHAR_MAX_VALUE)
                    overflow = true;
                break;
            }
            if (character == ';') {
                source.advancePastNonNewline();
                decodedCharacter.appendCharacter(ParserFunctions::legalEntityFor(overflow ? 0 : result));
                return true;
            }
            if (ParserFunctions::acceptMalformed()) {
                decodedCharacter.appendCharacter(ParserFunctions::legalEntityFor(overflow ? 0 : result));
                return true;
            }
            unconsumeCharacters(source, consumedCharacters);
            return false;
        case Decimal:
        Decimal:
            if (isASCIIDigit(character)) {
                result = result * 10 + character - '0';
                if (result > UCHAR_MAX_VALUE)
                    overflow = true;
                break;
            }
            if (character == ';') {
                source.advancePastNonNewline();
                decodedCharacter.appendCharacter(ParserFunctions::legalEntityFor(overflow ? 0 : result));
                return true;
            }
            if (ParserFunctions::acceptMalformed()) {
                decodedCharacter.appendCharacter(ParserFunctions::legalEntityFor(overflow ? 0 : result));
                return true;
            }
            unconsumeCharacters(source, consumedCharacters);
            return false;
        case Named:
        Named:
            return ParserFunctions::consumeNamedEntity(source, decodedCharacter, notEnoughCharacters, additionalAllowedCharacter, character);
        }
        consumedCharacters.append(character);
        source.advancePastNonNewline();
    }
    ASSERT(source.isEmpty());
    notEnoughCharacters = true;
    unconsumeCharacters(source, consumedCharacters);
    return false;
}

} // namespace WebCore
