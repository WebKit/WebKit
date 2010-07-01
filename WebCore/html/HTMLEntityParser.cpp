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
#include "HTMLEntityParser.h"

#include <wtf/Vector.h>

// Use __GNUC__ instead of PLATFORM(GCC) to stay consistent with the gperf generated c file
#ifdef __GNUC__
// The main parser includes this too so we are getting two copies of the data. However, this way the code gets inlined.
#include "HTMLEntityNames.cpp"
#else
// Not inlined for non-GCC compilers
struct Entity {
    const char* name;
    int code;
};
const struct Entity* findEntity(register const char* str, register unsigned int len);
#endif

using namespace WTF;

namespace WebCore {

namespace {

static const UChar windowsLatin1ExtensionArray[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, // 80-87
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F, // 88-8F
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178, // 98-9F
};

inline UChar adjustEntity(unsigned value)
{
    if ((value & ~0x1F) != 0x0080)
        return value;
    return windowsLatin1ExtensionArray[value - 0x80];
}

inline unsigned legalEntityFor(unsigned value)
{
    // FIXME: A number of specific entity values generate parse errors.
    if (value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        return 0xFFFD;
    if (value < 0xFFFF)
        return adjustEntity(value);
    return value;
}

inline bool isHexDigit(UChar cc)
{
    return (cc >= '0' && cc <= '9') || (cc >= 'a' && cc <= 'f') || (cc >= 'A' && cc <= 'F');
}

inline bool isAlphaNumeric(UChar cc)
{
    return (cc >= '0' && cc <= '9') || (cc >= 'a' && cc <= 'z') || (cc >= 'A' && cc <= 'Z');
}

void unconsumeCharacters(SegmentedString& source, const Vector<UChar, 10>& consumedCharacters)
{
    if (consumedCharacters.size() == 1)
        source.push(consumedCharacters[0]);
    else if (consumedCharacters.size() == 2) {
        source.push(consumedCharacters[0]);
        source.push(consumedCharacters[1]);
    } else
        source.prepend(SegmentedString(String(consumedCharacters.data(), consumedCharacters.size())));
}

}

unsigned consumeHTMLEntity(SegmentedString& source, bool& notEnoughCharacters, UChar additionalAllowedCharacter)
{
    ASSERT(!additionalAllowedCharacter || additionalAllowedCharacter == '"' || additionalAllowedCharacter == '\'' || additionalAllowedCharacter == '>');
    ASSERT(!notEnoughCharacters);

    enum EntityState {
        Initial,
        NumberType,
        MaybeHexLowerCaseX,
        MaybeHexUpperCaseX,
        Hex,
        Decimal,
        Named
    };
    EntityState entityState = Initial;
    unsigned result = 0;
    Vector<UChar, 10> consumedCharacters;
    Vector<char, 10> entityName;

    while (!source.isEmpty()) {
        UChar cc = *source;
        switch (entityState) {
        case Initial: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ' || cc == '<' || cc == '&')
                return 0;
            if (additionalAllowedCharacter && cc == additionalAllowedCharacter)
                return 0;
            if (cc == '#') {
                entityState = NumberType;
                break;
            }
            if ((cc >= 'a' && cc <= 'z') || (cc >= 'A' && cc <= 'Z')) {
                entityState = Named;
                continue;
            }
            return 0;
        }
        case NumberType: {
            if (cc == 'x') {
                entityState = MaybeHexLowerCaseX;
                break;
            }
            if (cc == 'X') {
                entityState = MaybeHexUpperCaseX;
                break;
            }
            if (cc >= '0' && cc <= '9') {
                entityState = Decimal;
                continue;
            }
            source.push('#');
            return 0;
        }
        case MaybeHexLowerCaseX: {
            if (isHexDigit(cc)) {
                entityState = Hex;
                continue;
            }
            source.push('#');
            source.push('x');
            return 0;
        }
        case MaybeHexUpperCaseX: {
            if (isHexDigit(cc)) {
                entityState = Hex;
                continue;
            }
            source.push('#');
            source.push('X');
            return 0;
        }
        case Hex: {
            if (cc >= '0' && cc <= '9')
                result = result * 16 + cc - '0';
            else if (cc >= 'a' && cc <= 'f')
                result = result * 16 + 10 + cc - 'a';
            else if (cc >= 'A' && cc <= 'F')
                result = result * 16 + 10 + cc - 'A';
            else if (cc == ';') {
                source.advancePastNonNewline();
                return legalEntityFor(result);
            } else 
                return legalEntityFor(result);
            break;
        }
        case Decimal: {
            if (cc >= '0' && cc <= '9')
                result = result * 10 + cc - '0';
            else if (cc == ';') {
                source.advancePastNonNewline();
                return legalEntityFor(result);
            } else
                return legalEntityFor(result);
            break;
        }
        case Named: {
            // FIXME: This code is wrong. We need to find the longest matching entity.
            //        The examples from the spec are:
            //            I'm &notit; I tell you
            //            I'm &notin; I tell you
            //        In the first case, "&not" is the entity.  In the second
            //        case, "&notin;" is the entity.
            // FIXME: Our list of HTML entities is incomplete.
            // FIXME: The number 8 below is bogus.
            while (!source.isEmpty() && entityName.size() <= 8) {
                cc = *source;
                if (cc == ';') {
                    const Entity* entity = findEntity(entityName.data(), entityName.size());
                    if (entity) {
                        source.advanceAndASSERT(';');
                        return entity->code;
                    }
                    break;
                }
                if (!isAlphaNumeric(cc)) {
                    const Entity* entity = findEntity(entityName.data(), entityName.size());
                    if (entity) {
                        // HTML5 tells us to ignore this entity, for historical reasons,
                        // if the lookhead character is '='.
                        if (additionalAllowedCharacter && cc == '=')
                            break;
                        // Some entities require a terminating semicolon, whereas other
                        // entities do not.  The HTML5 spec has a giant list:
                        //
                        // http://www.whatwg.org/specs/web-apps/current-work/multipage/named-character-references.html#named-character-references
                        //
                        // However, the list seems to boil down to this branch:
                        if (entity->code > 255)
                            break;
                        return entity->code;
                    }
                    break;
                }
                entityName.append(cc);
                consumedCharacters.append(cc);
                source.advanceAndASSERT(cc);
            }
            notEnoughCharacters = source.isEmpty();
            unconsumeCharacters(source, consumedCharacters);
            return 0;
        }
        }
        consumedCharacters.append(cc);
        source.advanceAndASSERT(cc);
    }
    ASSERT(source.isEmpty());
    notEnoughCharacters = true;
    unconsumeCharacters(source, consumedCharacters);
    return 0;
}

} // namespace WebCore
