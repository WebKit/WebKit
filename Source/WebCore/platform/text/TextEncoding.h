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

#include "URL.h"
#include <pal/text/UnencodableHandling.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TextEncoding : public URLTextEncoding {
public:
    TextEncoding() = default;
    WEBCORE_EXPORT TextEncoding(const char* name);
    WEBCORE_EXPORT TextEncoding(const String& name);

    bool isValid() const { return m_name; }
    const char* name() const { return m_name; }
    WEBCORE_EXPORT const char* domName() const; // name exposed via DOM
    bool usesVisualOrdering() const;
    bool isJapanese() const;

    const TextEncoding& closestByteBasedEquivalent() const;
    const TextEncoding& encodingForFormSubmissionOrURLParsing() const;

    WEBCORE_EXPORT String decode(const char*, size_t length, bool stopOnError, bool& sawError) const;
    String decode(const char*, size_t length) const;
    WEBCORE_EXPORT Vector<uint8_t> encode(StringView, UnencodableHandling) const;
    Vector<uint8_t> encodeForURLParsing(StringView string) const final { return encode(string, UnencodableHandling::URLEncodedEntities); }

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
WEBCORE_EXPORT const TextEncoding& UTF8Encoding();
WEBCORE_EXPORT const TextEncoding& WindowsLatin1Encoding();

inline String TextEncoding::decode(const char* characters, size_t length) const
{
    bool ignored;
    return decode(characters, length, false, ignored);
}

} // namespace WebCore
