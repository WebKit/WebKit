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
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>
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

    RefPtr<WebCore::URLPattern> makePatternWithoutRegExp(ASCIILiteral pattern, ASCIILiteral baseURL = { }, WebCore::URLPatternOptions options = { }) const
    {
        auto result = WebCore::URLPattern::createWithoutRegExpSupport(String(pattern), String(baseURL), WTF::move(options));
        if (result.hasException())
            return nullptr;
        return result.releaseReturnValue().ptr();
    }

    // Asserts that matching without a regular-expression engine agrees with the regex-based path.
    void expectWithoutRegExpAgreesWithRegExp(ASCIILiteral pattern, ASCIILiteral baseURL, ASCIILiteral url, WebCore::URLPatternOptions options = { }) const
    {
        SCOPED_TRACE(makeString("pattern="_s, pattern, " baseURL="_s, baseURL, " url="_s, url, " ignoreCase="_s, options.ignoreCase ? "true"_s : "false"_s).utf8().data());

        auto regexResult = WebCore::URLPattern::create(String(pattern), String(baseURL), WebCore::URLPatternOptions { options });
        ASSERT_FALSE(regexResult.hasException());
        auto withoutRegExpResult = WebCore::URLPattern::createWithoutRegExpSupport(String(pattern), String(baseURL), WebCore::URLPatternOptions { options });
        ASSERT_FALSE(withoutRegExpResult.hasException());

        auto regexPattern = regexResult.releaseReturnValue();
        auto withoutRegExpPattern = withoutRegExpResult.releaseReturnValue();
        ASSERT_FALSE(withoutRegExpPattern->hasRegExpGroups());

        auto regexTest = regexPattern->test(String(url), String());
        ASSERT_FALSE(regexTest.hasException());

        EXPECT_EQ(withoutRegExpPattern->testWithoutRegExp(WTF::URL { String(url) }), regexTest.returnValue());
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

TEST_F(URLPatternTest, UnclosedGroupReturnsException)
{
    WebCore::URLPatternInit init;
    init.protocol = "https"_s;
    init.hostname = "example.com"_s;
    init.pathname = "/foo{bar"_s;
    EXPECT_TRUE(WebCore::URLPattern::create(WebCore::URLPatternInit { init }, WebCore::URLPatternOptions { }).hasException());
    EXPECT_TRUE(WebCore::URLPattern::createWithoutRegExpSupport(WTF::move(init), String(), WebCore::URLPatternOptions { }).hasException());
    EXPECT_TRUE(WebCore::URLPattern::create(String("https://example.com/foo{bar"_s), String(), WebCore::URLPatternOptions { }).hasException());
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

TEST_F(URLPatternTest, WithoutRegExpMatchesExactURL)
{
    auto pattern = makePatternWithoutRegExp("https://example.com/foo/:id"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_FALSE(pattern->hasRegExpGroups());
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpNoMatchDifferentHostOrProtocol)
{
    auto pattern = makePatternWithoutRegExp("https://example.com/foo/:id"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://other.com/foo/bar"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "http://example.com/foo/bar"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpNamedSegmentDoesNotCrossDelimiter)
{
    // ":id" is a segment wildcard: it must match a single path segment, not one containing "/".
    auto pattern = makePatternWithoutRegExp("https://example.com/foo/:id"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar/baz"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpFullWildcardCrossesDelimiter)
{
    auto pattern = makePatternWithoutRegExp("https://example.com/foo/*"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar"_str }));
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar/baz"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/other/bar"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpPrefixSuffixLiterals)
{
    auto pattern = makePatternWithoutRegExp("https://example.com/data/:name.json"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/data/report.json"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/data/report.csv"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpBaseURLRelativePattern)
{
    auto pattern = makePatternWithoutRegExp("/foo/*"_s, "https://example.com"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/bar"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpSearchAndHash)
{
    auto pattern = makePatternWithoutRegExp("https://example.com/foo?bar#section"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo?bar#section"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo?baz#section"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo?bar#other"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo?bar"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo#section"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpWildcardSearchAndHash)
{
    auto pattern = makePatternWithoutRegExp("https://example.com/foo?*#*"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo"_str }));
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo?bar"_str }));
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo?bar#section"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/other?bar#section"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpZeroOrMoreGroup)
{
    auto pattern = makePatternWithoutRegExp("https://example.com/foo{/bar}*"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo"_str }));
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar"_str }));
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar/bar"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/baz"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar/baz"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpOneOrMoreGroup)
{
    auto pattern = makePatternWithoutRegExp("https://example.com/foo{/bar}+"_s);
    ASSERT_NE(pattern, nullptr);
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo"_str }));
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar"_str }));
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar/bar"_str }));
    EXPECT_FALSE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/baz"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpIgnoreCase)
{
    WebCore::URLPatternOptions options;
    options.ignoreCase = true;
    auto pattern = makePatternWithoutRegExp("https://example.com/Foo/*"_s, { }, WTF::move(options));
    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpIgnoreCaseIsOnlyWhenRequested)
{
    // A mixed-case literal only matches a differently-cased URL when ignoreCase is set.
    auto caseSensitive = makePatternWithoutRegExp("https://example.com/Foo/:id"_s);
    ASSERT_NE(caseSensitive, nullptr);
    EXPECT_FALSE(caseSensitive->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar"_str }));

    WebCore::URLPatternOptions options;
    options.ignoreCase = true;
    auto caseInsensitive = makePatternWithoutRegExp("https://example.com/Foo/:id"_s, { }, WTF::move(options));
    ASSERT_NE(caseInsensitive, nullptr);
    EXPECT_TRUE(caseInsensitive->testWithoutRegExp(WTF::URL { "https://example.com/foo/bar"_str }));
}

TEST_F(URLPatternTest, WithoutRegExpIgnoreCaseAgreesWithRegExpPath)
{
    WebCore::URLPatternOptions ignoreCase;
    ignoreCase.ignoreCase = true;

    struct {
        ASCIILiteral pattern;
        ASCIILiteral url;
    } cases[] = {
        { "https://example.com/Foo/*"_s, "https://example.com/foo/bar"_s },
        { "https://example.com/foo/*"_s, "https://example.com/FOO/bar"_s },
        { "https://example.com/data/:name.JSON"_s, "https://example.com/data/report.json"_s },
        { "https://example.com/data/:name.json"_s, "https://example.com/data/report.JSON"_s },
        { "https://example.com/MixedCase"_s, "https://example.com/mixedcase"_s },
        { "https://example.com/MixedCase"_s, "https://example.com/different"_s },
    };

    for (auto& testCase : cases)
        expectWithoutRegExpAgreesWithRegExp(testCase.pattern, { }, testCase.url, ignoreCase);
}

TEST_F(URLPatternTest, WithoutRegExpIgnoreCaseUnicodeAgreesWithRegExpPath)
{
    // Full Unicode case folding must stay consistent with the regex path's IgnoreCase behavior,
    // including for non-ASCII input (which URL canonicalization percent-encodes). Compare the two
    // engines directly rather than asserting a specific outcome.
    WebCore::URLPatternOptions ignoreCase;
    ignoreCase.ignoreCase = true;

    auto pattern = String::fromUTF8("https://example.com/Caf\xC3\x89/*"); // "CafÉ"
    auto matchingURL = String::fromUTF8("https://example.com/caf\xC3\xA9/x"); // "café"
    auto nonMatchingURL = String::fromUTF8("https://example.com/other/x");

    for (auto& url : { matchingURL, nonMatchingURL }) {
        SCOPED_TRACE(makeString("url="_s, url).utf8().data());

        auto regexResult = WebCore::URLPattern::create(String { pattern }, String(), WebCore::URLPatternOptions { ignoreCase });
        ASSERT_FALSE(regexResult.hasException());
        auto withoutRegExpResult = WebCore::URLPattern::createWithoutRegExpSupport(String { pattern }, String(), WebCore::URLPatternOptions { ignoreCase });
        ASSERT_FALSE(withoutRegExpResult.hasException());

        auto regexTest = regexResult.releaseReturnValue()->test(String { url }, String());
        ASSERT_FALSE(regexTest.hasException());
        EXPECT_EQ(withoutRegExpResult.releaseReturnValue()->testWithoutRegExp(WTF::URL { String { url } }), regexTest.returnValue());
    }
}

TEST_F(URLPatternTest, WithoutRegExpRejectsRegExpGroups)
{
    // A pattern with explicit regexp groups cannot be matched without a regular-expression engine,
    // so the regex-free constructor must fail rather than produce an unusable pattern.
    WebCore::URLPatternInit init;
    init.protocol = "https"_s;
    init.hostname = "example.com"_s;
    init.pathname = "/(\\d+)"_s;
    auto result = WebCore::URLPattern::createWithoutRegExpSupport(WTF::move(init), String(), WebCore::URLPatternOptions { });
    EXPECT_TRUE(result.hasException());
}

TEST_F(URLPatternTest, WithoutRegExpRejectsRegExpGroupFromConstructorString)
{
    // The same pattern is valid for the regex-based path (it just has regexp groups), which proves
    // the regex-free constructor is rejecting specifically because of the group, not a parse error.
    auto regex = WebCore::URLPattern::create(String("https://example.com/(\\d+)"_s), String(), WebCore::URLPatternOptions { });
    ASSERT_FALSE(regex.hasException());
    EXPECT_TRUE(regex.releaseReturnValue()->hasRegExpGroups());

    auto withoutRegExp = WebCore::URLPattern::createWithoutRegExpSupport(String("https://example.com/(\\d+)"_s), String(), WebCore::URLPatternOptions { });
    EXPECT_TRUE(withoutRegExp.hasException());
}

TEST_F(URLPatternTest, WithoutRegExpRejectsNamedRegExpGroup)
{
    // A named group with a custom regexp (":id(\\d+)") is still a regexp group.
    auto result = WebCore::URLPattern::createWithoutRegExpSupport(String("https://example.com/:id(\\d+)"_s), String(), WebCore::URLPatternOptions { });
    EXPECT_TRUE(result.hasException());
}

TEST_F(URLPatternTest, WithoutRegExpRejectsRegExpGroupInHostname)
{
    WebCore::URLPatternInit init;
    init.protocol = "https"_s;
    init.hostname = "(sub|www).example.com"_s;
    init.pathname = "/*"_s;
    auto result = WebCore::URLPattern::createWithoutRegExpSupport(WTF::move(init), String(), WebCore::URLPatternOptions { });
    EXPECT_TRUE(result.hasException());
}

TEST_F(URLPatternTest, WithoutRegExpRejectsRegExpGroupInSearch)
{
    WebCore::URLPatternInit init;
    init.protocol = "https"_s;
    init.hostname = "example.com"_s;
    init.pathname = "/*"_s;
    init.search = "(a|b)"_s;
    auto result = WebCore::URLPattern::createWithoutRegExpSupport(WTF::move(init), String(), WebCore::URLPatternOptions { });
    EXPECT_TRUE(result.hasException());
}

TEST_F(URLPatternTest, WithoutRegExpAllowsWildcardsAndNamedSegments)
{
    // Wildcards ("*") and bare named segments (":id") are not regexp groups and must still succeed.
    auto wildcard = WebCore::URLPattern::createWithoutRegExpSupport(String("https://example.com/*"_s), String(), WebCore::URLPatternOptions { });
    ASSERT_FALSE(wildcard.hasException());
    EXPECT_FALSE(wildcard.releaseReturnValue()->hasRegExpGroups());

    auto namedSegment = WebCore::URLPattern::createWithoutRegExpSupport(String("https://example.com/:id"_s), String(), WebCore::URLPatternOptions { });
    ASSERT_FALSE(namedSegment.hasException());
    EXPECT_FALSE(namedSegment.releaseReturnValue()->hasRegExpGroups());
}

TEST_F(URLPatternTest, WithoutRegExpAgreesWithRegExpPath)
{
    struct {
        ASCIILiteral pattern;
        ASCIILiteral baseURL;
        ASCIILiteral url;
    } cases[] = {
        { "https://example.com/foo/:id"_s, { }, "https://example.com/foo/bar"_s },
        { "https://example.com/foo/:id"_s, { }, "https://example.com/foo/bar/baz"_s },
        { "https://example.com/foo/*"_s, { }, "https://example.com/foo/bar/baz"_s },
        { "https://example.com/foo/*"_s, { }, "https://example.com/other"_s },
        { "https://example.com/data/:name.json"_s, { }, "https://example.com/data/x.json"_s },
        { "https://example.com/data/:name.json"_s, { }, "https://example.com/data/x.csv"_s },
        { "https://*.example.com/*"_s, { }, "https://api.example.com/x"_s },
        { "https://*.example.com/*"_s, { }, "https://example.com/x"_s },
        { "https://example.com/foo{/bar}?"_s, { }, "https://example.com/foo"_s },
        { "https://example.com/foo{/bar}?"_s, { }, "https://example.com/foo/bar"_s },
        { "https://example.com/foo{/bar}*"_s, { }, "https://example.com/foo"_s },
        { "https://example.com/foo{/bar}*"_s, { }, "https://example.com/foo/bar/bar"_s },
        { "https://example.com/foo{/bar}*"_s, { }, "https://example.com/foo/bar/baz"_s },
        { "https://example.com/foo{/bar}+"_s, { }, "https://example.com/foo"_s },
        { "https://example.com/foo{/bar}+"_s, { }, "https://example.com/foo/bar/bar"_s },
        { "https://example.com/foo?bar#section"_s, { }, "https://example.com/foo?bar#section"_s },
        { "https://example.com/foo?bar#section"_s, { }, "https://example.com/foo?baz#section"_s },
        { "https://example.com/foo?bar#section"_s, { }, "https://example.com/foo?bar#other"_s },
        { "https://example.com/foo?bar#section"_s, { }, "https://example.com/foo"_s },
        { "https://example.com/foo?*#*"_s, { }, "https://example.com/foo?anything#anywhere"_s },
        { "https://example.com/*.png"_s, { }, "https://example.com/a/b/c.png"_s },
        { "https://example.com:8443/*"_s, { }, "https://example.com:8443/x"_s },
        { "https://example.com:8443/*"_s, { }, "https://example.com/x"_s },
        { "/foo/*"_s, "https://example.com"_s, "https://example.com/foo/bar"_s },
        { "/foo/*"_s, "https://example.com"_s, "https://other.com/foo/bar"_s },
    };

    for (auto& testCase : cases)
        expectWithoutRegExpAgreesWithRegExp(testCase.pattern, testCase.baseURL, testCase.url);
}

} // namespace TestWebKitAPI
