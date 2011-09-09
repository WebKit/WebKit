/*
 * Copyright (C) 2011 Daniel Bates (dbates@intudata.com). All Rights Reserved.
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

#ifndef DecodeEscapeSequences_h
#define DecodeEscapeSequences_h

#include "PlatformString.h"
#include "TextEncoding.h"
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// See <http://en.wikipedia.org/wiki/Percent-encoding#Non-standard_implementations>.
struct Unicode16BitEscapeSequence {
    enum { size = 6 }; // e.g. %u26C4
    static size_t findInString(const String& string, unsigned start = 0) { return string.find("%u", start); }
    static bool matchStringPrefix(const String& string, unsigned start = 0)
    {
        if (string.length() - start < size)
            return false;
        return string[start] == '%' && string[start + 1] == 'u'
            && isASCIIHexDigit(string[start + 2]) && isASCIIHexDigit(string[start + 3])
            && isASCIIHexDigit(string[start + 4]) && isASCIIHexDigit(string[start + 5]);
    }
    static String decodeRun(const UChar* run, size_t runLength, const TextEncoding&)
    {
        // Each %u-escape sequence represents a UTF-16 code unit.
        // See <http://www.w3.org/International/iri-edit/draft-duerst-iri.html#anchor29>.
        size_t numberOfSequences = runLength / size;
        StringBuilder builder;
        builder.reserveCapacity(numberOfSequences);
        while (numberOfSequences--) {
            UChar codeUnit = (toASCIIHexValue(run[2]) << 12) | (toASCIIHexValue(run[3]) << 8) | (toASCIIHexValue(run[4]) << 4) | toASCIIHexValue(run[5]);
            builder.append(codeUnit);
            run += size;
        }
        return builder.toString();
    }
};

struct URLEscapeSequence {
    enum { size = 3 }; // e.g. %41
    static size_t findInString(const String& string, unsigned start = 0) { return string.find('%', start); }
    static bool matchStringPrefix(const String& string, unsigned start = 0)
    {
        if (string.length() - start < size)
            return false;
        return string[start] == '%' && isASCIIHexDigit(string[start + 1]) && isASCIIHexDigit(string[start + 2]);
    }
    static String decodeRun(const UChar* run, size_t runLength, const TextEncoding& encoding)
    {
        size_t numberOfSequences = runLength / size;
        Vector<char, 512> buffer;
        buffer.resize(numberOfSequences);
        char* p = buffer.data();
        while (numberOfSequences--) {
            *p++ = (toASCIIHexValue(run[1]) << 4) | toASCIIHexValue(run[2]);
            run += size;
        }
        ASSERT(buffer.size() == static_cast<size_t>(p - buffer.data()));
        return (encoding.isValid() ? encoding : UTF8Encoding()).decode(buffer.data(), p - buffer.data());
    }
};

template<typename EscapeSequence>
String decodeEscapeSequences(const String& string, const TextEncoding& encoding)
{
    StringBuilder result;
    size_t length = string.length();
    size_t decodedPosition = 0;
    size_t searchPosition = 0;
    size_t encodedRunPosition;
    while ((encodedRunPosition = EscapeSequence::findInString(string, searchPosition)) != notFound) {
        unsigned encodedRunEnd = encodedRunPosition;
        while (length - encodedRunEnd >= EscapeSequence::size && EscapeSequence::matchStringPrefix(string, encodedRunEnd))
            encodedRunEnd += EscapeSequence::size;
        searchPosition = encodedRunEnd;
        if (encodedRunEnd == encodedRunPosition) {
            ++searchPosition;
            continue;
        }

        String decoded = EscapeSequence::decodeRun(string.characters() + encodedRunPosition, encodedRunEnd - encodedRunPosition, encoding);
        if (decoded.isEmpty())
            continue;

        result.append(string.characters() + decodedPosition, encodedRunPosition - decodedPosition);
        result.append(decoded);
        decodedPosition = encodedRunEnd;
    }
    result.append(string.characters() + decodedPosition, length - decodedPosition);
    return result.toString();
}

} // namespace WebCore

#endif // DecodeEscapeSequences_h
