/*
 * Copyright 2011 Google Inc. All rights reserved.
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

// ICU integration functions.

#include "config.h"

#if USE(WTFURL)

#include "URLCanonInternal.h" // for _itoa_s
#include <stdlib.h>
#include <string.h>
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#include <unicode/uidna.h>

namespace WTF {

namespace URLCanonicalizer {

namespace {

// Called when converting a character that can not be represented, this will
// append an escaped version of the numerical character reference for that code
// point. It is of the form "&#1234;" and we will escape the non-digits to
// "%26%231234%3B". Why? This is what Netscape did back in the olden days.
void appendURLEscapedChar(const void* /* context */,
                          UConverterFromUnicodeArgs* fromArgs,
                          const UChar* /* code_units */,
                          int32_t /* length */,
                          UChar32 codePoint,
                          UConverterCallbackReason reason,
                          UErrorCode* err)
{
    if (reason == UCNV_UNASSIGNED) {
        *err = U_ZERO_ERROR;

        const static int prefixLength = 6;
        const static char prefix[prefixLength + 1] = "%26%23"; // "&#" percent-escaped
        ucnv_cbFromUWriteBytes(fromArgs, prefix, prefixLength, 0, err);

        ASSERT(codePoint < 0x110000);
        char number[8]; // Max Unicode code point is 7 digits.
        _itoa_s(codePoint, number, 10);
        int numberLength = static_cast<int>(strlen(number));
        ucnv_cbFromUWriteBytes(fromArgs, number, numberLength, 0, err);

        const static int postfixLength = 3;
        const static char postfix[postfixLength + 1] = "%3B"; // ";" percent-escaped
        ucnv_cbFromUWriteBytes(fromArgs, postfix, postfixLength, 0, err);
    }
}

// A class for scoping the installation of the invalid character callback.
class AppendHandlerInstaller {
public:
    // The owner of this object must ensure that the converter is alive for the
    // duration of this object's lifetime.
    AppendHandlerInstaller(UConverter* converter)
        : m_converter(converter)
    {
        UErrorCode err = U_ZERO_ERROR;
        ucnv_setFromUCallBack(m_converter, appendURLEscapedChar, 0, &m_oldCallback, &m_oldContext, &err);
    }

    ~AppendHandlerInstaller()
    {
        UErrorCode err = U_ZERO_ERROR;
        ucnv_setFromUCallBack(m_converter, m_oldCallback, m_oldContext, 0, 0, &err);
    }

private:
    UConverter* m_converter;

    UConverterFromUCallback m_oldCallback;
    const void* m_oldContext;
};

} // namespace

// Converts the Unicode input representing a hostname to ASCII using IDN rules.
// The output must be ASCII, but is represented as wide characters.
//
// On success, the output will be filled with the ASCII host name and it will
// return true. Unlike most other canonicalization functions, this assumes that
// the output is empty. The beginning of the host will be at offset 0, and
// the length of the output will be set to the length of the new host name.
//
// On error, this will return false. The output in this case is undefined.
bool IDNToASCII(const UChar* src, int sourceLength, URLBuffer<UChar>& output)
{
    ASSERT(!output.length()); // Output buffer is assumed empty.
    while (true) {
        // Use ALLOW_UNASSIGNED to be more tolerant of hostnames that violate
        // the spec (which do exist). This does not present any risk and is a
        // little more future proof.
        UErrorCode err = U_ZERO_ERROR;
        int numConverted = uidna_IDNToASCII(src, sourceLength, output.data(),
                                             output.capacity(),
                                             UIDNA_ALLOW_UNASSIGNED, 0, &err);
        if (err == U_ZERO_ERROR) {
            output.setLength(numConverted);
            return true;
        }
        if (err != U_BUFFER_OVERFLOW_ERROR)
            return false; // Unknown error, give up.

        // Not enough room in our buffer, expand.
        output.resize(output.capacity() * 2);
    }
}

bool readUTFChar(const char* str, int* begin, int length, unsigned* codePointOut)
{
    int codePoint; // Avoids warning when U8_NEXT writes -1 to it.
    U8_NEXT(str, *begin, length, codePoint);
    *codePointOut = static_cast<unsigned>(codePoint);

    // The ICU macro above moves to the next char, we want to point to the last
    // char consumed.
    (*begin)--;

    // Validate the decoded value.
    if (U_IS_UNICODE_CHAR(codePoint))
        return true;
    *codePointOut = kUnicodeReplacementCharacter;
    return false;
}

bool readUTFChar(const UChar* str, int* begin, int length, unsigned* codePoint)
{
    if (U16_IS_SURROGATE(str[*begin])) {
        if (!U16_IS_SURROGATE_LEAD(str[*begin]) || *begin + 1 >= length || !U16_IS_TRAIL(str[*begin + 1])) {
            // Invalid surrogate pair.
            *codePoint = kUnicodeReplacementCharacter;
            return false;
        }

        // Valid surrogate pair.
        *codePoint = U16_GET_SUPPLEMENTARY(str[*begin], str[*begin + 1]);
        (*begin)++;
    } else {
        // Not a surrogate, just one 16-bit word.
        *codePoint = str[*begin];
    }

    if (U_IS_UNICODE_CHAR(*codePoint))
        return true;

    // Invalid code point.
    *codePoint = kUnicodeReplacementCharacter;
    return false;
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
