/*
 * Copyright (C) 2026 Helmut Januschka <helmut@januschka.com>
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
#include <WebCore/ConnectionAllowlist.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/ResourceResponse.h>

namespace TestWebKitAPI {
using namespace WebCore;

static std::unique_ptr<ConnectionAllowlist> parse(const String& headerValue, HTTPHeaderName name = HTTPHeaderName::ConnectionAllowlist)
{
    ResourceResponse response { URL { "https://example.com/index.html"_str }, "text/html"_s, 0, "UTF-8"_s };
    response.setHTTPHeaderField(name, headerValue);
    return parseConnectionAllowlist(response, name);
}

TEST(ConnectionAllowlist, NoHeader)
{
    EXPECT_NULL(parse(""_s));
}

TEST(ConnectionAllowlist, FirstItemMustBeAnInnerList)
{
    EXPECT_NULL(parse("\"https://a.example\""_s));
    EXPECT_NULL(parse("response-origin"_s));
}

TEST(ConnectionAllowlist, PatternStrings)
{
    auto allowlist = parse("(\"https://a.example\" \"https://b.example/*\")"_s);
    ASSERT_NOT_NULL(allowlist);
    EXPECT_EQ(allowlist->patterns.size(), 2u);
    EXPECT_EQ(allowlist->patterns[0], "https://a.example"_s);
    EXPECT_EQ(allowlist->patterns[1], "https://b.example/*"_s);
}

TEST(ConnectionAllowlist, ResponseOriginToken)
{
    auto allowlist = parse("(response-origin)"_s);
    ASSERT_NOT_NULL(allowlist);
    EXPECT_EQ(allowlist->patterns.size(), 1u);
    EXPECT_EQ(allowlist->patterns[0], "https://example.com"_s);
}

TEST(ConnectionAllowlist, UnexpectedValuesAreIgnored)
{
    auto allowlist = parse("(\"https://a.example\" 42 ?1 unknown-token)"_s);
    ASSERT_NOT_NULL(allowlist);
    EXPECT_EQ(allowlist->patterns.size(), 1u);
    EXPECT_EQ(allowlist->patterns[0], "https://a.example"_s);
}

TEST(ConnectionAllowlist, OnlyTheFirstListItemIsUsed)
{
    auto allowlist = parse("(\"https://a.example\"), (\"https://b.example\")"_s);
    ASSERT_NOT_NULL(allowlist);
    EXPECT_EQ(allowlist->patterns.size(), 1u);
    EXPECT_EQ(allowlist->patterns[0], "https://a.example"_s);
}

TEST(ConnectionAllowlist, Parameters)
{
    auto allowlist = parse("(\"https://a.example\");report-to=endpoint;redirects=block;webrtc=block"_s);
    ASSERT_NOT_NULL(allowlist);
    EXPECT_EQ(allowlist->reportTo, "endpoint"_s);
    EXPECT_EQ(allowlist->redirects, ConnectionAllowlist::RedirectHandling::Block);
    EXPECT_EQ(allowlist->webrtc, ConnectionAllowlist::WebRTCHandling::Block);
}

TEST(ConnectionAllowlist, ParameterDefaultsAndUnknownValues)
{
    auto allowlist = parse("(\"https://a.example\");redirects=something;webrtc=something;other=1"_s);
    ASSERT_NOT_NULL(allowlist);
    EXPECT_TRUE(allowlist->reportTo.isNull());
    // Anything other than the `block` token means allow.
    EXPECT_EQ(allowlist->redirects, ConnectionAllowlist::RedirectHandling::Allow);
    EXPECT_EQ(allowlist->webrtc, ConnectionAllowlist::WebRTCHandling::Allow);
}

TEST(ConnectionAllowlist, NonTokenParametersLeaveDefaults)
{
    auto allowlist = parse("(\"https://a.example\");redirects=\"block\";report-to=7"_s);
    ASSERT_NOT_NULL(allowlist);
    EXPECT_TRUE(allowlist->reportTo.isNull());
    EXPECT_EQ(allowlist->redirects, ConnectionAllowlist::RedirectHandling::Allow);
}

TEST(ConnectionAllowlist, Disposition)
{
    auto enforced = parse("(\"https://a.example\")"_s);
    ASSERT_NOT_NULL(enforced);
    EXPECT_EQ(enforced->disposition, ConnectionAllowlist::Disposition::Enforce);

    auto reportOnly = parse("(\"https://a.example\")"_s, HTTPHeaderName::ConnectionAllowlistReportOnly);
    ASSERT_NOT_NULL(reportOnly);
    EXPECT_EQ(reportOnly->disposition, ConnectionAllowlist::Disposition::Report);
}

} // namespace TestWebKitAPI
