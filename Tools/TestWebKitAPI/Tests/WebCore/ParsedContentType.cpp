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

} // namespace TestWebKitAPI
