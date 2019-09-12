/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include <WebCore/HTTPParsers.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(HTTPParsers, ParseCrossOriginResourcePolicyHeader)
{
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("") == CrossOriginResourcePolicy::None);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader(" ") == CrossOriginResourcePolicy::None);

    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("same-origin") == CrossOriginResourcePolicy::SameOrigin);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("Same-Origin") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("SAME-ORIGIN") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader(" same-orIGIN ") == CrossOriginResourcePolicy::Invalid);

    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("same-site") == CrossOriginResourcePolicy::SameSite);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("Same-Site") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("SAME-SITE") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader(" same-site ") == CrossOriginResourcePolicy::SameSite);

    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("SameOrigin") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("zameorigin") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("samesite") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("same site") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("same–site") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("SAMESITE") == CrossOriginResourcePolicy::Invalid);
    EXPECT_TRUE(parseCrossOriginResourcePolicyHeader("") == CrossOriginResourcePolicy::Invalid);
}

TEST(HTTPParsers, ValidateUserAgentValues)
{
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari WebKit"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari/10.0"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari WebKit/163"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari/10.0 WebKit"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari/10.0 WebKit/163"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari/10.0 WebKit/163 (Mozilla; like Gecko)"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari (comment (nested comment))"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari () (<- Empty comment)"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("Safari (left paren \\( as quoted pair)"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("!#$%&'*+-.^_`|~ (non-alphanumeric token characters)"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("0123456789 (numeric token characters)"));
    EXPECT_TRUE(isValidUserAgentHeaderValue("a (single character token)"));

    EXPECT_FALSE(isValidUserAgentHeaderValue(" "));
    EXPECT_FALSE(isValidUserAgentHeaderValue(" Safari (leading whitespace)"));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari (trailing whitespace) "));
    EXPECT_FALSE(isValidUserAgentHeaderValue("\nSafari (leading newline)"));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari (trailing newline)\n"));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari/ (no version token after slash)"));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari (unterminated comment"));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari unopened commanent)"));
    EXPECT_FALSE(isValidUserAgentHeaderValue("\x1B (contains control character)"));
    EXPECT_FALSE(isValidUserAgentHeaderValue("Safari/\n10.0 (embeded newline)"));
    EXPECT_FALSE(isValidUserAgentHeaderValue("WPE\\ WebKit (quoted pair in token)"));
    EXPECT_FALSE(isValidUserAgentHeaderValue("/123 (missing product token)"));
}

} // namespace TestWebKitAPI
