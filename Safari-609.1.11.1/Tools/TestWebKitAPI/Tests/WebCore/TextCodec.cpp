/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <WebCore/TextCodec.h>
#include <WebCore/TextEncoding.h>
#include <WebCore/TextEncodingRegistry.h>
#include <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

// Expects hex bytes with optional spaces between them.
// Returns an empty vector if it encounters non-hex-digit characters.
static Vector<char> decodeHexTestBytes(const char* input)
{
    Vector<char> result;
    for (size_t i = 0; input[i]; ) {
        if (!isASCIIHexDigit(input[i]))
            return { };
        if (!isASCIIHexDigit(input[i + 1]))
            return { };
        result.append(toASCIIHexValue(input[i], input[i + 1]));
        i += 2;
        if (input[i] == ' ')
            ++i;
    }
    return result;
}

// The word "escape" here just means a format easy to read in test results.
// It doesn't have to be a format suitable for other users or even one
// that is completely unambiguous.
static const char* escapeNonASCIIPrintableCharacters(StringView string)
{
    static char resultBuffer[100];
    size_t i = 0;
    auto append = [&i] (char character) {
        if (i < sizeof(resultBuffer))
            resultBuffer[i++] = character;
    };
    auto appendNibble = [append] (char nibble) {
        if (nibble)
            append(lowerNibbleToASCIIHexDigit(nibble));
    };
    auto appendLastNibble = [append] (char nibble) {
        append(lowerNibbleToASCIIHexDigit(nibble));
    };
    for (auto character : string.codePoints()) {
        if (isASCIIPrintable(character))
            append(character);
        else {
            append('{');
            for (unsigned i = 32 - 4; i; i -= 4)
                appendNibble(character >> i);
            appendLastNibble(character);
            append('}');
        }
    }
    if (i == sizeof(resultBuffer))
        return "";
    resultBuffer[i] = '\0';
    return resultBuffer;
}

static const char* testDecode(const char* encodingName, std::initializer_list<const char*> inputs)
{
    StringBuilder resultBuilder;
    auto codec = newTextCodec(WebCore::TextEncoding { encodingName });
    size_t size = inputs.size();
    for (size_t i = 0; i < size; ++i) {
        auto vector = decodeHexTestBytes(inputs.begin()[i]);
        bool last = i == size - 1;
        bool sawError = false;
        resultBuilder.append(escapeNonASCIIPrintableCharacters(codec->decode(vector.data(), vector.size(), last, false, sawError)));
        if (sawError)
            resultBuilder.appendLiteral(" ERROR");
    }
    return escapeNonASCIIPrintableCharacters(resultBuilder.toString());
}

TEST(TextCodec, UTF8)
{
    EXPECT_STREQ("", testDecode("UTF-8", { "" }));

    EXPECT_STREQ("{0}", testDecode("UTF-8", { "00" }));

    EXPECT_STREQ("a", testDecode("UTF-8", { "61" }));
    EXPECT_STREQ("a", testDecode("UTF-8", { "", "61" }));
    EXPECT_STREQ("a", testDecode("UTF-8", { "61", "" }));
    EXPECT_STREQ("a", testDecode("UTF-8", { "", "61", "" }));

    EXPECT_STREQ("{B6}", testDecode("UTF-8", { "C2 B6" }));
    EXPECT_STREQ("{B6}", testDecode("UTF-8", { "C2", "B6" }));
    EXPECT_STREQ("{B6}", testDecode("UTF-8", { "", "C2", "", "B6", "" }));
    EXPECT_STREQ("x{B6}", testDecode("UTF-8", { "78 C2 B6" }));
    EXPECT_STREQ("{B6}x", testDecode("UTF-8", { "C2 B6 78" }));

    EXPECT_STREQ("{2603}", testDecode("UTF-8", { "E2 98 83" }));
    EXPECT_STREQ("{2603}", testDecode("UTF-8", { "E2", "98", "83" }));
    EXPECT_STREQ("{2603}", testDecode("UTF-8", { "", "E2", "", "98", "83" }));
    EXPECT_STREQ("{2603}", testDecode("UTF-8", { "E2 98", "83" }));
    EXPECT_STREQ("{2603}", testDecode("UTF-8", { "", "E2 98", "", "83", "" }));
    EXPECT_STREQ("{2603}", testDecode("UTF-8", { "E2", "9883" }));
    EXPECT_STREQ("{2603}", testDecode("UTF-8", { "", "E2", "", "9883", "" }));
    EXPECT_STREQ("x{2603}", testDecode("UTF-8", { "78 E2 98 83" }));
    EXPECT_STREQ("{2603}x", testDecode("UTF-8", { "E2 98 83 78" }));

    EXPECT_STREQ("{1F4A9}", testDecode("UTF-8", { "F0 9F 92 A9" }));
    EXPECT_STREQ("{1F4A9}", testDecode("UTF-8", { "F0", "9F", "92", "A9" }));
    EXPECT_STREQ("{1F4A9}", testDecode("UTF-8", { "", "F0", "", "9F", "", "92", "" , "A9", "" }));
    EXPECT_STREQ("{1F4A9}", testDecode("UTF-8", { "F09F92", "A9" }));
    EXPECT_STREQ("x{1F4A9}", testDecode("UTF-8", { "78 F0 9F 92 A9" }));
    EXPECT_STREQ("{1F4A9}x", testDecode("UTF-8", { "F0 9F 92 A9 78" }));
}

TEST(TextCodec, UTF8InvalidSequences)
{
    EXPECT_STREQ("{FFFD}? ERROR", testDecode("UTF-8", { "E0 A5 3F" }));
    EXPECT_STREQ("{FFFD}? ERROR", testDecode("UTF-8", { "E0 A5", "3F" }));
    EXPECT_STREQ("{FFFD}? ERROR", testDecode("UTF-8", { "E0", "A5 3F" }));
    EXPECT_STREQ("{FFFD}? ERROR", testDecode("UTF-8", { "E0", "A5", "3F" }));
    EXPECT_STREQ("{FFFD}? ERROR", testDecode("UTF-8", { "E0", "", "A5", "", "3F" }));
    EXPECT_STREQ("{FFFD}? ERROR", testDecode("UTF-8", { "E0", "", "A5", "", "3F", "" }));
    EXPECT_STREQ("{FFFD}? ERROR", testDecode("UTF-8", { "", "E0", "", "A5", "", "3F", "" }));

    EXPECT_STREQ("a{FFFD}? ERROR", testDecode("UTF-8", { "61 E0 A5 3F" }));
    EXPECT_STREQ("a{FFFD}? ERROR", testDecode("UTF-8", { "61 E0 A5", "3F" }));
    EXPECT_STREQ("a{FFFD}? ERROR", testDecode("UTF-8", { "61 E0", "A5 3F" }));
    EXPECT_STREQ("a{FFFD}? ERROR", testDecode("UTF-8", { "61 E0", "A5", "3F" }));

    EXPECT_STREQ("{B6}{FFFD}? ERROR", testDecode("UTF-8", { "C2 B6 E0 A5 3F" }));
    EXPECT_STREQ("{B6}{FFFD}? ERROR", testDecode("UTF-8", { "C2 B6 E0 A5", "3F" }));
    EXPECT_STREQ("{B6}{FFFD}? ERROR", testDecode("UTF-8", { "C2 B6 E0", "A5 3F" }));
    EXPECT_STREQ("{B6}{FFFD}? ERROR", testDecode("UTF-8", { "C2 B6 E0", "A5", "3F" }));

    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "C2" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "E2" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "E2 98" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "F0" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "F0 9F" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "F0 9F 92" }));

    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "E2", "98" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "F0", "9F" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "F0 9F", "92" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "F0", "9F92" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "F0", "9F", "92" }));

    EXPECT_STREQ("{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "C0 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "E0 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F0 80 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F8 80 80 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FC 80 80 80 80 80" }));

    EXPECT_STREQ("{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "C1 BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "E0 81 BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F0 80 81 BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F8 80 80 81 BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FC 80 80 80 81 BF" }));

    EXPECT_STREQ("{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "E0 82 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F0 80 82 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F8 80 80 82 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FC 80 80 80 82 80" }));

    EXPECT_STREQ("{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "E0 9F BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F0 80 9F BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F8 80 80 9F BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FC 80 80 80 9F BF" }));

    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F0 80 A0 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F8 80 80 A0 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FC 80 80 80 A0 80" }));

    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F0 8F BF BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F8 80 8F BF BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FC 80 80 8F BF BF" }));

    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F8 80 90 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FC 80 80 90 80 80" }));

    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F8 84 8F BF BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FC 80 84 8F BF BF" }));

    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F4 90 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FB BF BF BF BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FD BF BF BF BF BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "ED A0 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "ED BF BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "ED A0 BD ED B2 A9" }));

    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F8 84 90 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FC 80 84 90 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F0 8D A0 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F0 8D BF BF" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "F0 8D A0 BD F0 8D B2 A9" }));

    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "80" }));
    EXPECT_STREQ("{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "80 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "80 80 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "80 80 80 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "80 80 80 80 80 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "80 80 80 80 80 80 80" }));
    EXPECT_STREQ("{B6}{FFFD} ERROR", testDecode("UTF-8", { "C2 B6 80" }));
    EXPECT_STREQ("{2603}{FFFD} ERROR", testDecode("UTF-8", { "E2 98 83 80" }));
    EXPECT_STREQ("{1F4A9}{FFFD} ERROR", testDecode("UTF-8", { "F0 9F 92 A9 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FB BF BF BF BF 80" }));
    EXPECT_STREQ("{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FD BF BF BF BF BF 80" }));

    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "FE" }));
    EXPECT_STREQ("{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FE 80" }));
    EXPECT_STREQ("{FFFD} ERROR", testDecode("UTF-8", { "FF" }));
    EXPECT_STREQ("{FFFD}{FFFD} ERROR", testDecode("UTF-8", { "FF 80" }));
}

}
