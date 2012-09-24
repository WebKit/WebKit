/*
 * Copyright 2007 Google Inc. All rights reserved.
 * Copyright 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "URLCanon.h"

#include "RawURLBuffer.h"
#include "URLCanonInternal.h"
#include "URLCharacterTypes.h"
#include "URLQueryCharsetConverter.h"
#include <wtf/text/ASCIIFastPath.h>

// Query canonicalization in IE
// ----------------------------
// IE is very permissive for query parameters specified in links on the page
// (in contrast to links that it constructs itself based on form data). It does
// not unescape any character. It does not reject any escape sequence (be they
// invalid like "%2y" or freaky like %00).
//
// IE only escapes spaces and nothing else. Embedded NULLs, tabs (0x09),
// LF (0x0a), and CR (0x0d) are removed (this probably happens at an earlier
// layer since they are removed from all portions of the URL). All other
// characters are passed unmodified. Invalid UTF-16 sequences are preserved as
// well, with each character in the input being converted to UTF-8. It is the
// server's job to make sense of this invalid query.
//
// Invalid multibyte sequences (for example, invalid UTF-8 on a UTF-8 page)
// are converted to the invalid character and sent as unescaped UTF-8 (0xef,
// 0xbf, 0xbd). This may not be canonicalization, the parser may generate these
// strings before the URL handler ever sees them.
//
// Our query canonicalization
// --------------------------
// We escape all non-ASCII characters and control characters, like Firefox.
// This is more conformant to the URL spec, and there do not seem to be many
// problems relating to Firefox's behavior.
//
// Like IE, we will never unescape (although the application may want to try
// unescaping to present the user with a more understandable URL). We will
// replace all invalid sequences (including invalid UTF-16 sequences, which IE
// doesn't) with the "invalid character," and we will escape it.

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

// Returns true if the characters starting at |begin| and going until |end|
// (non-inclusive) are all representable in 7-bits.
template<typename CharacterType>
bool isAllASCII(const CharacterType* spec, const URLComponent& query)
{
    return charactersAreAllASCII(spec + query.begin(), query.length());
}

// FIXME: Benjamin: get rid of this when everything is based on LChar.
template<>
bool isAllASCII<char>(const char* spec, const URLComponent& query)
{
    return charactersAreAllASCII(reinterpret_cast<const LChar*>(spec + query.begin()), query.length());
}

// Appends the given string to the output, escaping characters that do not
// match the given |type| in SharedCharTypes. This version will accept 8 or 16
// bit characters, but assumes that they have only 7-bit values. It also assumes
// that all UTF-8 values are correct, so doesn't bother checking
template<typename CharacterType>
void appendRaw8BitQueryString(const CharacterType* source, int length, URLBuffer<char>& output)
{
    for (int i = 0; i < length; ++i) {
        if (!URLCharacterTypes::isQueryChar(static_cast<unsigned char>(source[i])))
            appendURLEscapedCharacter(static_cast<unsigned char>(source[i]), output);
        else // Doesn't need escaping.
            output.append(static_cast<char>(source[i]));
    }
}

// Runs the converter on the given UTF-8 input. Since the converter expects
// UTF-16, we have to convert first. The converter must be non-null.
void runConverter(const char* spec, const URLComponent& query, URLQueryCharsetConverter* converter, URLBuffer<char>& output)
{
    // This function will replace any misencoded values with the invalid
    // character. This is what we want so we don't have to check for error.
    RawURLBuffer<UChar> utf16;
    ConvertUTF8ToUTF16(&spec[query.begin()], query.length(), utf16);
    converter->convertFromUTF16(utf16.data(), utf16.length(), output);
}

// Runs the converter with the given UTF-16 input. We don't have to do
// anything, but this overriddden function allows us to use the same code
// for both UTF-8 and UTF-16 input.
void runConverter(const UChar* spec, const URLComponent& query, URLQueryCharsetConverter* converter, URLBuffer<char>& output)
{
    converter->convertFromUTF16(&spec[query.begin()], query.length(), output);
}

template<typename CharacterType>
void doConvertToQueryEncoding(const CharacterType* spec, const URLComponent& query, URLQueryCharsetConverter* converter, URLBuffer<char>& output)
{
    if (isAllASCII(spec, query)) {
        // Easy: the input can just appended with no character set conversions.
        appendRaw8BitQueryString(&spec[query.begin()], query.length(), output);
    } else {
        // Harder: convert to the proper encoding first.
        if (converter) {
            // Run the converter to get an 8-bit string, then append it, escaping
            // necessary values.
            RawURLBuffer<char> eightBitQueryString;
            runConverter(spec, query, converter, eightBitQueryString);
            appendRaw8BitQueryString(eightBitQueryString.data(), eightBitQueryString.length(), output);
        } else {
            // No converter, do our own UTF-8 conversion.
            appendStringOfType(&spec[query.begin()], query.length(), URLCharacterTypes::QueryCharacter, output);
        }
    }
}

template<typename CharacterType>
void doCanonicalizeQuery(const CharacterType* spec, const URLComponent& query, URLQueryCharsetConverter* converter,
                         URLBuffer<char>& output, URLComponent& outputQueryComponent)
{
    if (query.length() < 0) {
        outputQueryComponent = URLComponent();
        return;
    }

    output.append('?');
    outputQueryComponent.setBegin(output.length());

    doConvertToQueryEncoding<CharacterType>(spec, query, converter, output);

    outputQueryComponent.setLength(output.length() - outputQueryComponent.begin());
}

} // namespace

void CanonicalizeQuery(const char* spec, const URLComponent& query, URLQueryCharsetConverter* converter,
                       URLBuffer<char>& output, URLComponent* outputQueryComponent)
{
    doCanonicalizeQuery(spec, query, converter, output, *outputQueryComponent);
}

void CanonicalizeQuery(const UChar* spec, const URLComponent& query, URLQueryCharsetConverter* converter,
                       URLBuffer<char>& output, URLComponent* outputQueryComponent)
{
    doCanonicalizeQuery(spec, query, converter, output, *outputQueryComponent);
}

void ConvertUTF16ToQueryEncoding(const UChar* input, const URLComponent& query, URLQueryCharsetConverter* converter, URLBuffer<char>& output)
{
    doConvertToQueryEncoding<UChar>(input, query, converter, output);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
