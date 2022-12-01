/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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

#include "UnencodableHandling.h"
#include <wtf/URL.h>
#include <wtf/text/StringView.h>

namespace PAL {

enum class NFCNormalize : bool { No, Yes };

class TextEncoding : public WTF::URLTextEncoding {
public:
    TextEncoding() = default;
    PAL_EXPORT TextEncoding(const char* name);
    PAL_EXPORT TextEncoding(StringView name);
    PAL_EXPORT TextEncoding(const String& name);

    bool isValid() const { return m_name; }
    const char* name() const { return m_name; }
    PAL_EXPORT const char* domName() const; // name exposed via DOM
    bool usesVisualOrdering() const;
    bool isJapanese() const;

    const TextEncoding& closestByteBasedEquivalent() const;
    const TextEncoding& encodingForFormSubmissionOrURLParsing() const;

    PAL_EXPORT String decode(const char*, size_t length, bool stopOnError, bool& sawError) const;
    String decode(const char*, size_t length) const;
    String decode(const uint8_t* data, size_t length) const { return decode(reinterpret_cast<const char*>(data), length); }
    PAL_EXPORT Vector<uint8_t> encode(StringView, PAL::UnencodableHandling, NFCNormalize = NFCNormalize::Yes) const;
    Vector<uint8_t> encodeForURLParsing(StringView string) const final { return encode(string, PAL::UnencodableHandling::URLEncodedEntities, NFCNormalize::No); }

    UChar backslashAsCurrencySymbol() const;
    bool isByteBasedEncoding() const { return !isNonByteBasedEncoding(); }

private:
    bool isNonByteBasedEncoding() const;
    bool isUTF7Encoding() const;

    const char* m_name { nullptr };
    UChar m_backslashAsCurrencySymbol;
};

inline bool operator==(const TextEncoding& a, const TextEncoding& b) { return a.name() == b.name(); }
inline bool operator!=(const TextEncoding& a, const TextEncoding& b) { return a.name() != b.name(); }

const TextEncoding& ASCIIEncoding();
const TextEncoding& Latin1Encoding();
const TextEncoding& UTF16BigEndianEncoding();
const TextEncoding& UTF16LittleEndianEncoding();
PAL_EXPORT const TextEncoding& UTF8Encoding();
PAL_EXPORT const TextEncoding& WindowsLatin1Encoding();

// Unescapes the given string using URL escaping rules.
// DANGER: If the URL has "%00" in it,
// the resulting string will have embedded null characters!
PAL_EXPORT String decodeURLEscapeSequences(StringView, const TextEncoding& = UTF8Encoding());

inline String TextEncoding::decode(const char* characters, size_t length) const
{
    bool ignored;
    return decode(characters, length, false, ignored);
}

} // namespace PAL
