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

#include "Test.h"
#include <WebCore/ParsedContentType.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(ParsedContentType, MimeSniff)
{
    EXPECT_TRUE(isValidContentType("text/plain"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType(" text/plain"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType(" text/plain "_s, Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text /plain"_s, Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text/ plain"_s, Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text / plain"_s, Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("te xt/plain"_s, Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text/pla in"_s, Mode::MimeSniff));

    EXPECT_FALSE(isValidContentType("text"_s, Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text/"_s, Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("/plain"_s, Mode::MimeSniff));

    EXPECT_TRUE(isValidContentType("text/plain;"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;;"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;;;"_s, Mode::MimeSniff));

    EXPECT_TRUE(isValidContentType("text/plain;test"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain; test"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test="_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;;;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;;"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;;;"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain; test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test =value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test= value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value "_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;=;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;="_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;wrong=;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;=wrong;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;wrong="_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;=wrong"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain ;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain\n;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain\r;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain\t;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value ;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value\n;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value\r;test=value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value\t;test=value"_s, Mode::MimeSniff));

    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\""_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=\"value"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\"foo"_s, Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\"foo;foo=bar"_s, Mode::MimeSniff));
}

TEST(ParsedContentType, Rfc2045)
{
    EXPECT_TRUE(isValidContentType("text/plain"_s, Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType(" text/plain"_s, Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType(" text/plain "_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text /plain"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/ plain"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text / plain"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("te xt/plain"_s, Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType("text/pla in"_s, Mode::Rfc2045));

    EXPECT_FALSE(isValidContentType("text"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("/plain"_s, Mode::Rfc2045));

    EXPECT_FALSE(isValidContentType("text/plain;"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;;"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;;;"_s, Mode::Rfc2045));

    EXPECT_FALSE(isValidContentType("text/plain;test"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain; test"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test="_s, Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType("text/plain;test=value"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;;test=value"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;;;test=value"_s, Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType("text/plain;test=value;"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=value;;"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=value;;;"_s, Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType("text/plain; test=value"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test =value"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test= value"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=value "_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;=;test=value"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=value;="_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;wrong=;test=value"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;=wrong;test=value"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=value;wrong="_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=value;=wrong"_s, Mode::Rfc2045));

    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\""_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=\"value"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=\"value\"foo"_s, Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=\"value\"foo;foo=bar"_s, Mode::Rfc2045));
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

static const char* serializeIfValid(const String& input)
{
    if (std::optional<ParsedContentType> parsedContentType = ParsedContentType::create(input))
        return escapeNonASCIIPrintableCharacters(parsedContentType->serialize());
    return "NOTVALID";
}

TEST(ParsedContentType, Serialize)
{
    EXPECT_STREQ(serializeIfValid(emptyString()), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(" "_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("  "_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("\t"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(";"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(";="_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("="_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("=;"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\0"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/ "_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("{/}"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("x/x ;x=x"_s), "x/x;x=x");
    EXPECT_STREQ(serializeIfValid("x/x\n;x=x"_s), "x/x;x=x");
    EXPECT_STREQ(serializeIfValid("x/x\r;x=x"_s), "x/x;x=x");
    EXPECT_STREQ(serializeIfValid("x/x\t;x=x"_s), "x/x;x=x");
    EXPECT_STREQ(serializeIfValid("text/plain"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\0"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid(" text/plain"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ttext/plain"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ntext/plain"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("\rtext/plain"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("\btext/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("\x0Btext/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("\u000Btext/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/plain "_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\t"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\n"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\r"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\b"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/plain\x0B"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/plain\u000B"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(" text/plain "_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ttext/plain\t"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ntext/plain\n"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("\rtext/plain\r"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("\btext/plain\b"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/PLAIN"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text /plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/ plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text / plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te xt/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla in"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\t/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\tplain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\t/\tplain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te\txt/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla\tin"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\n/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\nplain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\n/\nplain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te\nxt/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla\nin"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\r/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\rplain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\r/\rplain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te\rxt/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla\rin"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("/plain"_s), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(String::fromUTF8("†/†")), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(String::fromUTF8("text/\xD8\x88\x12\x34")), "NOTVALID");

    EXPECT_STREQ(serializeIfValid("text/plain;"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;;"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;;;"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain; test"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\ttest"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\ntest"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\rtest"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\btest"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test "_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\t"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\n"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\r"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\b"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test="_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;=;test=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;="_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;wrong=;test=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;=wrong;test=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;wrong="_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;=wrong"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;;test=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;;;test=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;;"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value;;;"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;TEST=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=VALUE"_s), "text/plain;test=VALUE");
    EXPECT_STREQ(serializeIfValid("text/plain; test=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\ttest=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\ntest=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\rtest=value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\btest=value"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\0test=value"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test =value"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\t=value"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\n=value"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\r=value"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\b=value"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test= value"_s), "text/plain;test=\" value\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\tvalue"_s), "text/plain;test=\"{9}value\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\nvalue"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\rvalue"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\bvalue"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value "_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\t"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\n"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\r"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\b"_s), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value\""_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\""_s), "text/plain;test=\"\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value\"foo"_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value\"foo;foo=bar"_s), "text/plain;test=value;foo=bar");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"val\\ue\""_s), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"val\\\"ue\""_s), "text/plain;test=\"val\\\"ue\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test='value'"_s), "text/plain;test='value'");
    EXPECT_STREQ(serializeIfValid("text/plain;test='value"_s), "text/plain;test='value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value'"_s), "text/plain;test=value'");
    EXPECT_STREQ(serializeIfValid("text/plain;test={value}"_s), "text/plain;test=\"{value}\"");
}

} // namespace TestWebKitAPI
