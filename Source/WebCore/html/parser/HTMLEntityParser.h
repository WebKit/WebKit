/*
 * Copyright (C) 2008-2023 Apple Inc. All Rights Reserved.
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

#include <array>
#include <span>
#include <unicode/umachine.h>
#include <wtf/Forward.h>
#include <wtf/text/LChar.h>

namespace WebCore {

class DecodedHTMLEntity;
class SegmentedString;

// This function expects a null character at the end, otherwise it assumes the source is partial.
DecodedHTMLEntity consumeHTMLEntity(SegmentedString&, UChar additionalAllowedCharacter = 0);

// This function assumes the source is complete, and does not expect a null character.
DecodedHTMLEntity consumeHTMLEntity(StringParsingBuffer<LChar>&);
DecodedHTMLEntity consumeHTMLEntity(StringParsingBuffer<UChar>&);

// This function does not check for "not enough characters" at all.
DecodedHTMLEntity decodeNamedHTMLEntityForXMLParser(const char*);

class DecodedHTMLEntity {
public:
    constexpr DecodedHTMLEntity();
    constexpr DecodedHTMLEntity(UChar);
    constexpr DecodedHTMLEntity(UChar, UChar);
    constexpr DecodedHTMLEntity(UChar, UChar, UChar);

    enum ConstructNotEnoughCharactersType { ConstructNotEnoughCharacters };
    constexpr DecodedHTMLEntity(ConstructNotEnoughCharactersType);

    constexpr bool failed() const { return !m_length; }
    constexpr bool notEnoughCharacters() const { return m_notEnoughCharacters; }

    constexpr std::span<const UChar> span() const { return { m_characters.data(), m_length }; }

private:
    uint8_t m_length { 0 };
    bool m_notEnoughCharacters { false };
    std::array<UChar, 3> m_characters;
};

} // namespace WebCore
