/*
 * Copyright (C) 2026 Igalia S.L.
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

#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/URLPattern.h>
#include <WebCore/URLPatternInit.h>
#include <WebCore/URLPatternOptions.h>
#include <WebCore/URLPatternResult.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

class URLPatternTest : public testing::Test {
public:
    void SetUp() override
    {
        JSC::initialize();
        WTF::initializeMainThread();
    }

protected:
    RefPtr<WebCore::URLPattern> makePattern(ASCIILiteral pattern, WebCore::URLPatternOptions options = { }) const
    {
        auto result = WebCore::URLPattern::create(String(pattern), String(), WTF::move(options));
        if (result.hasException())
            return nullptr;
        return result.releaseReturnValue().ptr();
    }

    RefPtr<WebCore::URLPattern> makePattern(WebCore::URLPatternInit&& init) const
    {
        auto result = WebCore::URLPattern::create(WTF::move(init), WebCore::URLPatternOptions { });
        if (result.hasException())
            return nullptr;
        return result.releaseReturnValue().ptr();
    }
};

TEST_F(URLPatternTest, BasicAccessors)
{
    auto pattern = makePattern("https://example.com/foo/:id"_s);
    ASSERT_NE(pattern, nullptr);

    EXPECT_EQ(pattern->protocol(), "https"_s);
    EXPECT_EQ(pattern->hostname(), "example.com"_s);
    EXPECT_EQ(pattern->port(), emptyString());
    EXPECT_FALSE(pattern->pathname().isEmpty());
}

TEST_F(URLPatternTest, TestMatchesExactURL)
{
    auto pattern = makePattern("https://example.com/foo/:id"_s);
    ASSERT_NE(pattern, nullptr);

    auto result = pattern->test(String("https://example.com/foo/bar"_s), String());
    ASSERT_FALSE(result.hasException());
    EXPECT_TRUE(result.returnValue());
}

TEST_F(URLPatternTest, TestNoMatchDifferentHost)
{
    auto pattern = makePattern("https://example.com/foo/:id"_s);
    ASSERT_NE(pattern, nullptr);

    auto result = pattern->test(String("https://other.com/foo/bar"_s), String());
    ASSERT_FALSE(result.hasException());
    EXPECT_FALSE(result.returnValue());
}

TEST_F(URLPatternTest, TestNoMatchDifferentProtocol)
{
    auto pattern = makePattern("https://example.com/foo/:id"_s);
    ASSERT_NE(pattern, nullptr);

    auto result = pattern->test(String("http://example.com/foo/bar"_s), String());
    ASSERT_FALSE(result.hasException());
    EXPECT_FALSE(result.returnValue());
}

TEST_F(URLPatternTest, ExecReturnsNamedGroup)
{
    auto pattern = makePattern("https://example.com/foo/:id"_s);
    ASSERT_NE(pattern, nullptr);

    auto result = pattern->exec(String("https://example.com/foo/bar"_s), String());
    ASSERT_FALSE(result.hasException());
    ASSERT_TRUE(result.returnValue().has_value());

    const auto& match = *result.returnValue();
    bool foundId = false;
    for (const auto& group : match.pathname.groups) {
        if (group.key == "id"_s) {
            foundId = true;
            ASSERT_TRUE(std::holds_alternative<String>(group.value));
            EXPECT_EQ(std::get<String>(group.value), "bar"_s);
        }
    }
    EXPECT_TRUE(foundId);
}

TEST_F(URLPatternTest, ExecNoMatch)
{
    auto pattern = makePattern("https://example.com/foo/:id"_s);
    ASSERT_NE(pattern, nullptr);

    auto result = pattern->exec(String("https://example.com/other/bar"_s), String());
    ASSERT_FALSE(result.hasException());
    EXPECT_FALSE(result.returnValue().has_value());
}

TEST_F(URLPatternTest, WildcardPathMatches)
{
    WebCore::URLPatternInit init;
    init.protocol = "https"_s;
    init.hostname = "example.com"_s;
    init.pathname = "*"_s;
    auto pattern = makePattern(WTF::move(init));
    ASSERT_NE(pattern, nullptr);

    auto result = pattern->test(String("https://example.com/anything/at/all"_s), String());
    ASSERT_FALSE(result.hasException());
    EXPECT_TRUE(result.returnValue());
}

TEST_F(URLPatternTest, IgnoreCaseOption)
{
    WebCore::URLPatternOptions options;
    options.ignoreCase = true;
    auto pattern = makePattern("https://example.com/Foo/:id"_s, WTF::move(options));
    ASSERT_NE(pattern, nullptr);

    auto result = pattern->test(String("https://example.com/foo/bar"_s), String());
    ASSERT_FALSE(result.hasException());
    EXPECT_TRUE(result.returnValue());
}

TEST_F(URLPatternTest, HasRegExpGroupsFalseForNamedSegments)
{
    auto pattern = makePattern("https://example.com/:id"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_FALSE(pattern->hasRegExpGroups());
}

TEST_F(URLPatternTest, HasRegExpGroupsTrueForExplicitRegex)
{
    WebCore::URLPatternInit init;
    init.protocol = "https"_s;
    init.hostname = "example.com"_s;
    init.pathname = "/(\\d+)"_s;
    auto pattern = makePattern(WTF::move(init));
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->hasRegExpGroups());
}

TEST_F(URLPatternTest, ExplicitRegexGroupMatchesAndReturnsCapture)
{
    WebCore::URLPatternInit init;
    init.protocol = "https"_s;
    init.hostname = "example.com"_s;
    init.pathname = "/(\\d+)"_s;
    auto pattern = makePattern(WTF::move(init));
    ASSERT_NE(pattern, nullptr);
    ASSERT_TRUE(pattern->hasRegExpGroups());

    auto testResult = pattern->test(String("https://example.com/42"_s), String());
    ASSERT_FALSE(testResult.hasException());
    EXPECT_TRUE(testResult.returnValue());

    auto execResult = pattern->exec(String("https://example.com/42"_s), String());
    ASSERT_FALSE(execResult.hasException());
    ASSERT_TRUE(execResult.returnValue().has_value());

    const auto& match = *execResult.returnValue();
    ASSERT_FALSE(match.pathname.groups.isEmpty());
    ASSERT_TRUE(std::holds_alternative<String>(match.pathname.groups[0].value));
    EXPECT_EQ(std::get<String>(match.pathname.groups[0].value), "42"_s);
}

TEST_F(URLPatternTest, InvalidPatternReturnsException)
{
    WebCore::URLPatternInit init;
    init.protocol = "https"_s;
    init.hostname = "example.com"_s;
    init.pathname = "/([invalid"_s;
    auto result = WebCore::URLPattern::create(WTF::move(init), WebCore::URLPatternOptions { });
    EXPECT_TRUE(result.hasException());
}

TEST_F(URLPatternTest, BaseURLRelativePattern)
{
    auto result = WebCore::URLPattern::create(
        String("/foo/:id"_s),
        String("https://example.com"_s),
        WebCore::URLPatternOptions { });
    ASSERT_FALSE(result.hasException());
    auto pattern = result.releaseReturnValue();
    auto testResult = pattern->test(String("https://example.com/foo/42"_s), String());
    ASSERT_FALSE(testResult.hasException());
    EXPECT_TRUE(testResult.returnValue());
}

TEST_F(URLPatternTest, RelativePatternWithoutBaseURLThrows)
{
    auto result = WebCore::URLPattern::create(
        String("/foo/:id"_s),
        String(),
        WebCore::URLPatternOptions { });
    EXPECT_TRUE(result.hasException());
}

} // namespace TestWebKitAPI
