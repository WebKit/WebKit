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
    EXPECT_TRUE(isValidContentType("text/plain", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType(" text/plain", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType(" text/plain ", Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text /plain", Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text/ plain", Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text / plain", Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("te xt/plain", Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text/pla in", Mode::MimeSniff));

    EXPECT_FALSE(isValidContentType("text", Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("text/", Mode::MimeSniff));
    EXPECT_FALSE(isValidContentType("/plain", Mode::MimeSniff));

    EXPECT_TRUE(isValidContentType("text/plain;", Mode::MimeSniff));

    EXPECT_TRUE(isValidContentType("text/plain;test", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain; test", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=;test=value", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain; test=value", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test =value", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test= value", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=value ", Mode::MimeSniff));

    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\"", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=\"value", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\"foo", Mode::MimeSniff));
    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\"foo;foo=bar", Mode::MimeSniff));
}

TEST(ParsedContentType, Rfc2045)
{
    EXPECT_TRUE(isValidContentType("text/plain", Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType(" text/plain", Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType(" text/plain ", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text /plain", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/ plain", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text / plain", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("te xt/plain", Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType("text/pla in", Mode::Rfc2045));

    EXPECT_FALSE(isValidContentType("text", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("/plain", Mode::Rfc2045));

    EXPECT_FALSE(isValidContentType("text/plain;", Mode::Rfc2045));

    EXPECT_FALSE(isValidContentType("text/plain;test", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain; test", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=;test=value", Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType("text/plain;test=value", Mode::Rfc2045));
    EXPECT_TRUE(isValidContentType("text/plain; test=value", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test =value", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test= value", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=value ", Mode::Rfc2045));

    EXPECT_TRUE(isValidContentType("text/plain;test=\"value\"", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=\"value", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=\"value\"foo", Mode::Rfc2045));
    EXPECT_FALSE(isValidContentType("text/plain;test=\"value\"foo;foo=bar", Mode::Rfc2045));
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
    if (Optional<ParsedContentType> parsedContentType = ParsedContentType::create(input))
        return escapeNonASCIIPrintableCharacters(parsedContentType->serialize());
    return "NOTVALID";
}

TEST(ParsedContentType, Serialize)
{
    EXPECT_STREQ(serializeIfValid(""), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(";"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\0"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/ "), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("{/}"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("x/x ;x=x"), "x/x;x=x");
    EXPECT_STREQ(serializeIfValid("text/plain"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\0"), "text/plain");
    EXPECT_STREQ(serializeIfValid(" text/plain"), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ttext/plain"), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ntext/plain"), "text/plain");
    EXPECT_STREQ(serializeIfValid("\rtext/plain"), "text/plain");
    EXPECT_STREQ(serializeIfValid("\btext/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("\x0Btext/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("\u000Btext/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/plain "), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\t"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\n"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\r"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain\b"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/plain\x0B"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/plain\u000B"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid(" text/plain "), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ttext/plain\t"), "text/plain");
    EXPECT_STREQ(serializeIfValid("\ntext/plain\n"), "text/plain");
    EXPECT_STREQ(serializeIfValid("\rtext/plain\r"), "text/plain");
    EXPECT_STREQ(serializeIfValid("\btext/plain\b"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/PLAIN"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text /plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/ plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text / plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te xt/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla in"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\t/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\tplain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\t/\tplain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te\txt/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla\tin"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\n/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\nplain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\n/\nplain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te\nxt/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla\nin"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\r/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\rplain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text\r/\rplain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("te\rxt/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/pla\rin"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("/plain"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("†/†"), "NOTVALID");
    EXPECT_STREQ(serializeIfValid("text/\xD8\x88\x12\x34"), "NOTVALID");

    EXPECT_STREQ(serializeIfValid("text/plain;"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain; test"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\ttest"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\ntest"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\rtest"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\btest"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test "), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\t"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\n"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\r"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\b"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test="), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=;test=value"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;TEST=value"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=VALUE"), "text/plain;test=VALUE");
    EXPECT_STREQ(serializeIfValid("text/plain; test=value"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\ttest=value"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\ntest=value"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\rtest=value"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;\btest=value"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;\0test=value"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test =value"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\t=value"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\n=value"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\r=value"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test\b=value"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test= value"), "text/plain;test=\" value\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\tvalue"), "text/plain;test=\"{9}value\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\nvalue"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\rvalue"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\bvalue"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value "), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\t"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\n"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\r"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value\b"), "text/plain");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value\""), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\""), "text/plain;test=\"\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value\"foo"), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"value\"foo;foo=bar"), "text/plain;test=value;foo=bar");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"val\\ue\""), "text/plain;test=value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=\"val\\\"ue\""), "text/plain;test=\"val\\\"ue\"");
    EXPECT_STREQ(serializeIfValid("text/plain;test='value'"), "text/plain;test='value'");
    EXPECT_STREQ(serializeIfValid("text/plain;test='value"), "text/plain;test='value");
    EXPECT_STREQ(serializeIfValid("text/plain;test=value'"), "text/plain;test=value'");
    EXPECT_STREQ(serializeIfValid("text/plain;test={value}"), "text/plain;test=\"{value}\"");
}

} // namespace TestWebKitAPI
