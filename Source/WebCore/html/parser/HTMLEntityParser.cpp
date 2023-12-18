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

#include "HTMLEntitySearch.h"
#include "HTMLEntityTable.h"
#include "SegmentedString.h"
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

static constexpr UChar windowsLatin1ExtensionArray[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, // 80-87
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F, // 88-8F
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178, // 98-9F
};

constexpr DecodedHTMLEntity::DecodedHTMLEntity() = default;

constexpr DecodedHTMLEntity::DecodedHTMLEntity(UChar first)
    : m_length { 1 }
    , m_characters { first }
{
}

constexpr DecodedHTMLEntity::DecodedHTMLEntity(UChar first, UChar second)
    : m_length { 2 }
    , m_characters { first, second }
{
}

constexpr DecodedHTMLEntity::DecodedHTMLEntity(UChar first, UChar second, UChar third)
    : m_length { 3 }
    , m_characters { first, second, third }
{
}

constexpr DecodedHTMLEntity::DecodedHTMLEntity(ConstructNotEnoughCharactersType)
    : m_notEnoughCharacters { true }
{
}

static constexpr DecodedHTMLEntity makeEntity(char32_t character)
{
    if (character <= 0 || character > UCHAR_MAX_VALUE || U_IS_SURROGATE(character))
        return { replacementCharacter };
    if ((character & ~0x1F) != 0x80) {
        if (U_IS_BMP(character)) {
            UChar codeUnit = character;
            return { codeUnit };
        }
        return { U16_LEAD(character), U16_TRAIL(character) };
    }
    return { windowsLatin1ExtensionArray[character - 0x80] };
}

static DecodedHTMLEntity makeEntity(const Checked<uint32_t, RecordOverflow>& character)
{
    if (character.hasOverflowed())
        return { replacementCharacter };
    return makeEntity(character.value());
}

static constexpr DecodedHTMLEntity makeEntity(HTMLEntityTableEntry entry)
{
    UChar second = entry.optionalSecondCharacter;
    if (U_IS_BMP(entry.firstCharacter)) {
        UChar first = entry.firstCharacter;
        if (!second)
            return { first };
        return { first, second };
    }
    if (!second)
        return { U16_LEAD(entry.firstCharacter), U16_TRAIL(entry.firstCharacter) };
    return { U16_LEAD(entry.firstCharacter), U16_TRAIL(entry.firstCharacter), second };
}

class SegmentedStringSource {
public:
    explicit SegmentedStringSource(SegmentedString&);

    bool isEmpty() const { return m_source.isEmpty(); }
    UChar currentCharacter() const { return m_source.currentCharacter(); }
    void advance();
    void pushEverythingBack();
    void pushBackButKeep(unsigned keepCount);

private:
    SegmentedString& m_source;
    Vector<UChar, 64> m_consumedCharacters;
};

template<typename CharacterType> class StringParsingBufferSource {
public:
    explicit StringParsingBufferSource(StringParsingBuffer<CharacterType>&);

    static bool isEmpty() { return false; }
    UChar currentCharacter() const { return m_source.atEnd() ? 0 : *m_source; }
    void advance() { m_source.advance(); }
    void pushEverythingBack() { m_source.setPosition(m_startPosition); }
    void pushBackButKeep(unsigned keepCount) { m_source.setPosition(m_startPosition + keepCount); }

private:
    StringParsingBuffer<CharacterType>& m_source;
    const CharacterType* m_startPosition;
};

SegmentedStringSource::SegmentedStringSource(SegmentedString& source)
    : m_source { source }
{
}

template<typename CharacterType> StringParsingBufferSource<CharacterType>::StringParsingBufferSource(StringParsingBuffer<CharacterType>& source)
    : m_source { source }
    , m_startPosition { source.position() }
{
}

void SegmentedStringSource::advance()
{
    m_consumedCharacters.append(currentCharacter());
    m_source.advancePastNonNewline();
}

void SegmentedStringSource::pushEverythingBack()
{
    m_source.pushBack(String { m_consumedCharacters });
    m_consumedCharacters.clear();
}

void SegmentedStringSource::pushBackButKeep(unsigned keepCount)
{
    ASSERT(keepCount < m_consumedCharacters.size());
    unsigned length = m_consumedCharacters.size() - keepCount;
    m_source.pushBack(String { m_consumedCharacters.data() + keepCount, length });
    m_consumedCharacters.shrink(keepCount);
}

template<typename SourceType> DecodedHTMLEntity consumeDecimalHTMLEntity(SourceType& source)
{
    Checked<uint32_t, RecordOverflow> result = 0;
    UChar character = source.currentCharacter();
    do {
        source.advance();
        if (source.isEmpty()) {
            source.pushEverythingBack();
            return DecodedHTMLEntity::ConstructNotEnoughCharacters;
        }
        result *= 10;
        result += character - '0';
    } while (isASCIIDigit(character = source.currentCharacter()));
    if (character == ';')
        source.advance();
    return makeEntity(result);
}

template<typename SourceType> DecodedHTMLEntity consumeHexHTMLEntity(SourceType& source)
{
    Checked<uint32_t, RecordOverflow> result = 0;
    UChar character = source.currentCharacter();
    do {
        source.advance();
        if (source.isEmpty()) {
            source.pushEverythingBack();
            return DecodedHTMLEntity::ConstructNotEnoughCharacters;
        }
        int value = toASCIIHexValue(character);
        result *= 16;
        result += value;
    } while (isASCIIHexDigit(character = source.currentCharacter()));
    if (character == ';')
        source.advance();
    return makeEntity(result);
}

template<typename SourceType> DecodedHTMLEntity consumeNamedHTMLEntity(SourceType& source, UChar additionalAllowedCharacter)
{
    HTMLEntitySearch entitySearch;
    UChar character;
    do {
        character = source.currentCharacter();
        entitySearch.advance(character);
        if (!entitySearch.isEntityPrefix())
            break;
        source.advance();
    } while (!source.isEmpty());
    // We can't decide on an entity because there might be a longer
    // entity we could match if we had more data.
    if (source.isEmpty() && character != ';') {
        source.pushEverythingBack();
        return DecodedHTMLEntity::ConstructNotEnoughCharacters;
    }
    if (!entitySearch.match()) {
        source.pushEverythingBack();
        return { };
    }
    auto& match = *entitySearch.match();
    auto nameLength = match.nameLength();
    if (nameLength != entitySearch.currentLength()) {
        // We've consumed too many characters. Walk the source back
        // to the point at which we had consumed an actual entity.
        source.pushBackButKeep(nameLength);
        character = source.currentCharacter();
    }
    // FIXME: The use of additionalAllowedCharacter as just a boolean here is surprising.
    if (match.nameIncludesTrailingSemicolon || !additionalAllowedCharacter || !(isASCIIAlphanumeric(character) || character == '='))
        return makeEntity(match);
    source.pushEverythingBack();
    return { };
}

template<typename SourceType> DecodedHTMLEntity consumeHTMLEntity(SourceType source, UChar additionalAllowedCharacter)
{
    if (source.isEmpty())
        return DecodedHTMLEntity::ConstructNotEnoughCharacters;

    UChar character = source.currentCharacter();
    if (isASCIIAlpha(character))
        return consumeNamedHTMLEntity(source, additionalAllowedCharacter);
    if (character != '#')
        return { };

    source.advance();
    if (source.isEmpty()) {
        source.pushEverythingBack();
        return DecodedHTMLEntity::ConstructNotEnoughCharacters;
    }

    character = source.currentCharacter();
    if (isASCIIDigit(character))
        return consumeDecimalHTMLEntity(source);
    if (!isASCIIAlphaCaselessEqual(character, 'x')) {
        source.pushEverythingBack();
        return { };
    }

    source.advance();
    if (source.isEmpty()) {
        source.pushEverythingBack();
        return DecodedHTMLEntity::ConstructNotEnoughCharacters;
    }

    character = source.currentCharacter();
    if (isASCIIHexDigit(character))
        return consumeHexHTMLEntity(source);

    source.pushEverythingBack();
    return { };
}

DecodedHTMLEntity consumeHTMLEntity(SegmentedString& source, UChar additionalAllowedCharacter)
{
    return consumeHTMLEntity(SegmentedStringSource { source }, additionalAllowedCharacter);
}

DecodedHTMLEntity consumeHTMLEntity(StringParsingBuffer<LChar>& source)
{
    return consumeHTMLEntity(StringParsingBufferSource<LChar> { source }, 0);
}

DecodedHTMLEntity consumeHTMLEntity(StringParsingBuffer<UChar>& source)
{
    return consumeHTMLEntity(StringParsingBufferSource<UChar> { source }, 0);
}

DecodedHTMLEntity decodeNamedHTMLEntityForXMLParser(const char* name)
{
    HTMLEntitySearch search;
    while (*name) {
        search.advance(*name++);
        if (!search.isEntityPrefix())
            return { };
    }
    search.advance(';');
    if (!search.isEntityPrefix())
        return { };
    return makeEntity(*search.match());
}

} // namespace WebCore
