/*
 * Copyright (C) 2019 Igalia, S.L. All rights reserved.
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

#include "Helpers/Test.h"
#include <WebCore/ParsedContentType.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(ParsedContentType, MimeSniff)
{
    EXPECT_TRUE(isValidContentType("text/plain"_si));
    EXPECT_TRUE(isValidContentType(" text/plain"_si));
    EXPECT_TRUE(isValidContentType(" text/plain "_si));
    EXPECT_FALSE(isValidContentType("text /plain"_si));
    EXPECT_FALSE(isValidContentType("text/ plain"_si));
    EXPECT_FALSE(isValidContentType("text / plain"_si));
    EXPECT_FALSE(isValidContentType("te xt/plain"_si));
    EXPECT_FALSE(isValidContentType("text/pla in"_si));

    EXPECT_FALSE(isValidContentType("text"_si));
    EXPECT_FALSE(isValidContentType("text/"_si));
    EXPECT_FALSE(isValidContentType("/plain"_si));

    EXPECT_TRUE(isValidContentType("text/plain;"_si));
    EXPECT_TRUE(isValidContentType("text/plain;;"_si));
    EXPECT_TRUE(isValidContentType("text/plain;;;"_si));

    EXPECT_TRUE(isValidContentType("text/plain;test"_si));
    EXPECT_TRUE(isValidContentType("text/plain; test"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test="_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;;;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;;"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;;;"_si));
    EXPECT_TRUE(isValidContentType("text/plain; test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test =value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test= value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value "_si));
    EXPECT_TRUE(isValidContentType("text/plain;=;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;="_si));
    EXPECT_TRUE(isValidContentType("text/plain;wrong=;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;=wrong;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;wrong="_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;=wrong"_si));
    EXPECT_TRUE(isValidContentType("text/plain ;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain\n;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain\r;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain\t;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value ;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value\n;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value\r;test=value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=value\t;test=value"_si));

    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\""_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=\"value"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\"foo"_si));
    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\"foo;foo=bar"_si));
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

static const char* serializeIfValid(StringImpl* input)
{
    if (std::optional<ParsedContentType> parsedContentType = ParsedContentType::create(input))
        return escapeNonASCIIPrintableCharacters(parsedContentType->serialize());
    return "NOTVALID";
}

TEST(ParsedContentType, Serialize)
{
    EXPECT_STREQ(serializeIfValid(emptyString()), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(" "_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("  "_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("\t"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(";"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(";="_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("="_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("=;"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(StringImpl::create("text/\0"_s).ptr()), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/ "_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("{/}"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("x/x ;x=x"_si), "x/x;x=x");
    EXPECT_STREQ(serializeIfValid("x/x\n;x=x"_si), "x/x;x=x");
    EXPECT_STREQ(serializeIfValid("x/x\r;x=x"_si), "x/x;x=x");
    EXPECT_STREQ(serializeIfValid("x/x\t;x=x"_si), "x/x;x=x");
    EXPECT_STREQ(serializeIfValid("text/plain"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid(StringImpl::create("text/plain\0"_s).ptr()), "text/plain");
    EXPECT_STREQ(serializeIfValid(" text/plain"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ttext/plain"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ntext/plain"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("\rtext/plain"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("\btext/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("\x0Btext/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("\u000Btext/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/plain "_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\t"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\n"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\r"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\b"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/plain\x0B"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/plain\u000B"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(" text/plain "_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ttext/plain\t"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ntext/plain\n"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("\rtext/plain\r"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("\btext/plain\b"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/PLAIN"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text /plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/ plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text / plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te xt/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla in"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\t/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\tplain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\t/\tplain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te\txt/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla\tin"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\n/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\nplain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\n/\nplain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te\nxt/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla\nin"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\r/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\rplain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\r/\rplain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te\rxt/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla\rin"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("/plain"_si), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(StringImpl::fromUTF8("†/†")), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(StringImpl::fromUTF8("text/\xD8\x88\x12\x34")), "NOTVALID");

    EXPECT_STREQ(serializeIfValid("text/plain;"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;;"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;;;"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain; test"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\ttest"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\ntest"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\rtest"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\btest"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test "_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\t"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\n"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\r"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\b"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test="_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;=;test=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;="_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;wrong=;test=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;=wrong;test=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;wrong="_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;=wrong"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;;test=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;;;test=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;;"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;;;"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;TEST=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=VALUE"_si), "text/plain;test=VALUE");
    EXPECT_STREQ(serializeIfValid("text/plain; test=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\ttest=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\ntest=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\rtest=value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\btest=value"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid(StringImpl::create("text/plain;\0test=value"_s).ptr()), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test =value"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\t=value"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\n=value"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\r=value"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\b=value"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test= value"_si), "text/plain;test=\" value\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\tvalue"_si), "text/plain;test=\"{9}value\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\nvalue"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\rvalue"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\bvalue"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value "_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\t"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\n"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\r"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\b"_si), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value\""_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\""_si), "text/plain;test=\"\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value\"foo"_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value\"foo;foo=bar"_si), "text/plain;test=value;foo=bar");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"val\\ue\""_si), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"val\\\"ue\""_si), "text/plain;test=\"val\\\"ue\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test='value'"_si), "text/plain;test='value'");
    EXPECT_STREQ(serializeIfValid("text/plain;test='value"_si), "text/plain;test='value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value'"_si), "text/plain;test=value'");
    EXPECT_STREQ(serializeIfValid("text/plain;test={value}"_si), "text/plain;test=\"{value}\"");
}

} // namespace TestWebKitAPI
