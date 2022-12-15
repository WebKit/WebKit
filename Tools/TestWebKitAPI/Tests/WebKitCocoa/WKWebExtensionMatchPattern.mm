/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "config.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "TestCocoa.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/_WKWebExtensionMatchPatternPrivate.h>

namespace TestWebKitAPI {

static _WKWebExtensionMatchPattern *toPattern(NSString *string)
{
    return [_WKWebExtensionMatchPattern matchPatternWithString:string];
}

static _WKWebExtensionMatchPattern *toPatternAlloc(NSString *string, NSError **error = nullptr)
{
    return [[_WKWebExtensionMatchPattern alloc] initWithString:string error:error];
}

static _WKWebExtensionMatchPattern *toPattern(NSString *scheme, NSString *host, NSString *path)
{
    return [_WKWebExtensionMatchPattern matchPatternWithScheme:scheme host:host path:path];
}

static _WKWebExtensionMatchPattern *toPatternAlloc(NSString *scheme, NSString *host, NSString *path, NSError **error = nullptr)
{
    return [[_WKWebExtensionMatchPattern alloc] initWithScheme:scheme host:host path:path error:error];
}

TEST(WKWebExtensionMatchPattern, PatternValidity)
{
    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"\"\" cannot be parsed because it doesn't have a scheme.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"http://www.example.com", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"\"http://www.example.com\" cannot be parsed because it doesn't have a path.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"file://localhost", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"\"file://localhost\" cannot be parsed because it doesn't have a path.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"file://", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"\"file://\" cannot be parsed because it doesn't have a path.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"http://*foo/bar", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"\"http://*foo/bar\" cannot be parsed because the host \"*foo\" is invalid.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"http://foo.*.bar/baz", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"\"http://foo.*.bar/baz\" cannot be parsed because the host \"foo.*.bar\" is invalid.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"http:/bar", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"\"http:/bar\" cannot be parsed because it doesn't have a scheme.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"foo://*", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"\"foo://*\" cannot be parsed because the scheme \"foo\" is invalid.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"foo", @"*", @"/", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"Scheme \"foo\" is invalid.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"https", @"example.*", @"/", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"Host \"example.*\" is invalid.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"https", @"example.com", @"*", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"Path \"*\" is invalid.");
    }
}

TEST(WKWebExtensionMatchPattern, MatchPatternMatchesPattern)
{
    // Matches any URL that uses the http scheme.
    EXPECT_TRUE([toPattern(@"http://*/*") matchesPattern:toPattern(@"http://www.example.com/")]);
    EXPECT_TRUE([toPattern(@"http://*/*") matchesPattern:toPattern(@"http://example.com/foo/bar.html")]);

    // Matches any URL that uses the http scheme, on any host, as long as the path starts with /foo.
    EXPECT_TRUE([toPattern(@"http://*/foo*") matchesPattern:toPattern(@"http://example.com/foo/bar.html")]);
    EXPECT_TRUE([toPattern(@"http://*/*") matchesPattern:toPattern(@"http://www.example.com/foo")]);

    // Matches any URL that uses the https scheme, is on a example.com host (such as www.example.com, bar.example.com,
    // or example.com), as long as the path starts with /foo and ends with bar.
    EXPECT_TRUE([toPattern(@"https://*.example.com/foo*bar") matchesPattern:toPattern(@"https://www.example.com/foo/baz/bar")]);
    EXPECT_TRUE([toPattern(@"https://*.example.com/foo*bar") matchesPattern:toPattern(@"https://bar.example.com/foobar")]);

    // Matches the specified URL.
    EXPECT_TRUE([toPattern(@"http://example.com/foo/bar.html") matchesPattern:toPattern(@"http://example.com/foo/bar.html")]);

    // Matches any file whose path starts with /foo.
    EXPECT_TRUE([toPattern(@"file:///foo*") matchesPattern:toPattern(@"file:///foo/bar.html")]);
    EXPECT_TRUE([toPattern(@"file:///foo*") matchesPattern:toPattern(@"file:///foo")]);
    EXPECT_TRUE([toPattern(@"file://localhost/foo*") matchesPattern:toPattern(@"file://localhost/foo")]);
    EXPECT_FALSE([toPattern(@"file://localhost/foo*") matchesPattern:toPattern(@"file:///foo")]);
    EXPECT_FALSE([toPattern(@"file:///foo*") matchesPattern:toPattern(@"file://localhost/foo")]);
    EXPECT_TRUE([toPattern(@"file://*/foo*") matchesPattern:toPattern(@"file:///foo/bar.html")]);
    EXPECT_TRUE([toPattern(@"file://*/foo*") matchesPattern:toPattern(@"file://localhost/foo/bar.html")]);
    EXPECT_TRUE([toPattern(@"file://*/foo*") matchesPattern:toPattern(@"file://test.local/foo/bar.html")]);
    EXPECT_TRUE([toPattern(@"file://*/foo*") matchesPattern:toPattern(@"file://apple.com/foo/bar.html")]);
    EXPECT_TRUE([toPattern(@"file://*.local/foo*") matchesPattern:toPattern(@"file://test.local/foo")]);
    EXPECT_FALSE([toPattern(@"file://*.local/foo*") matchesPattern:toPattern(@"file://apple.com/foo")]);

    // Matches ignoring scheme.
    EXPECT_FALSE([toPattern(@"http://*.example.com/*") matchesPattern:toPattern(@"https://*.example.com/*") options:0]);
    EXPECT_FALSE([toPattern(@"https://*.example.com/*") matchesPattern:toPattern(@"http://*.example.com/*") options:0]);
    EXPECT_FALSE([toPattern(@"http://*.example.com/*") matchesPattern:toPattern(@"*://*.example.com/*") options:0]);
    EXPECT_TRUE([toPattern(@"http://*.example.com/*") matchesPattern:toPattern(@"https://*.example.com/*") options:_WKWebExtensionMatchPatternOptionsIgnoreSchemes]);
    EXPECT_TRUE([toPattern(@"https://*.example.com/*") matchesPattern:toPattern(@"http://*.example.com/*") options:_WKWebExtensionMatchPatternOptionsIgnoreSchemes]);
    EXPECT_TRUE([toPattern(@"http://*.example.com/*") matchesPattern:toPattern(@"*://*.example.com/*") options:_WKWebExtensionMatchPatternOptionsIgnoreSchemes]);

    // Matches ignoring path.
    EXPECT_FALSE([toPattern(@"https://*.example.com/foo*bar") matchesPattern:toPattern(@"https://www.example.com/baz") options:0]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/foo*bar") matchesPattern:toPattern(@"http://www.example.com/test") options:0]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/test*") matchesPattern:toPattern(@"*://*.example.com/bar") options:0]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/*bar") matchesPattern:toPattern(@"*://example.com/baz") options:0]);
    EXPECT_TRUE([toPattern(@"https://*.example.com/foo*bar") matchesPattern:toPattern(@"https://www.example.com/baz") options:_WKWebExtensionMatchPatternOptionsIgnorePaths]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/foo*bar") matchesPattern:toPattern(@"http://www.example.com/test") options:_WKWebExtensionMatchPatternOptionsIgnorePaths]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/test*") matchesPattern:toPattern(@"*://*.example.com/bar") options:_WKWebExtensionMatchPatternOptionsIgnorePaths]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/*bar") matchesPattern:toPattern(@"*://example.com/baz") options:_WKWebExtensionMatchPatternOptionsIgnorePaths]);

    // Matches any URL that uses the http scheme and is on the host 127.0.0.1.
    EXPECT_TRUE([toPattern(@"http://127.0.0.1/*") matchesPattern:toPattern(@"http://127.0.0.1/")]);
    EXPECT_TRUE([toPattern(@"http://127.0.0.1/*") matchesPattern:toPattern(@"http://127.0.0.1/foo/bar.html")]);

    // Matches any URL that starts with http://foo.example.com or https://foo.example.com.
    EXPECT_TRUE([toPattern(@"*://foo.example.com/*") matchesPattern:toPattern(@"http://foo.example.com/foo/baz/bar")]);
    EXPECT_TRUE([toPattern(@"*://foo.example.com/*") matchesPattern:toPattern(@"https://foo.example.com/foobar")]);

    // Test missing hosts.
    EXPECT_FALSE([toPattern(@"*:///*") matchesPattern:toPattern(@"https://example.com/foobar")]);
    EXPECT_FALSE([toPattern(@"https:///*") matchesPattern:toPattern(@"https://example.com/foobar")]);
    EXPECT_FALSE([toPattern(@"ftp:///*") matchesPattern:toPattern(@"ftp://example.com/foobar")]);
    EXPECT_TRUE([toPattern(@"file:///*") matchesPattern:toPattern(@"file:///foobar")]);

    // Matches any URL that uses a permitted scheme. (See the beginning of this section for the list of permitted schemes.)
    EXPECT_TRUE([toPattern(@"<all_urls>") matchesPattern:toPattern(@"http://example.com/foo/bar.html")]);
    EXPECT_FALSE([toPattern(@"<all_urls>") matchesPattern:toPattern(@"file:///bar/baz.html")]);
    EXPECT_FALSE([toPattern(@"<all_urls>") matchesPattern:toPattern(@"favorites://")]);
    EXPECT_FALSE([toPattern(@"<all_urls>") matchesPattern:toPattern(@"bookmarks://")]);
    EXPECT_FALSE([toPattern(@"<all_urls>") matchesPattern:toPattern(@"history://")]);

    // All matches.
    EXPECT_TRUE([toPattern(@"<all_urls>") matchesPattern:toPattern(@"<all_urls>")]);
    EXPECT_TRUE([toPattern(@"<all_urls>") matchesPattern:toPattern(@"*://*/*")]);
    EXPECT_FALSE([toPattern(@"*://*/*") matchesPattern:toPattern(@"<all_urls>")]);
    EXPECT_TRUE([toPattern(@"*://*/*") matchesPattern:toPattern(@"*://*/*")]);

    // Matching domain patterns.
    EXPECT_TRUE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"*://www.example.com/test/*")]);
    EXPECT_TRUE([toPattern(@"*://*/*") matchesPattern:toPattern(@"*://*.example.com/*")]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"*://*/*")]);
    EXPECT_TRUE([toPattern(@"<all_urls>") matchesPattern:toPattern(@"*://*.example.com/*")]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"*://www.example.com/test/*")]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"*://the-example.com/test/*")]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"*://www.the-example.com/test/*")]);

    // Bidirectional matching.
    EXPECT_FALSE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"*://*/*") options:0]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"<all_urls>") options:0]);
    EXPECT_FALSE([toPattern(@"http://*.example.com/*") matchesPattern:toPattern(@"<all_urls>") options:0]);
    EXPECT_FALSE([toPattern(@"ftp://*.example.com/*") matchesPattern:toPattern(@"<all_urls>") options:0]);
    EXPECT_FALSE([toPattern(@"*://*.en.wikipedia.org/*") matchesPattern:toPattern(@"*://*.wikipedia.org/*") options:0]);
    EXPECT_FALSE([toPattern(@"https://*.en.wikipedia.org/*") matchesPattern:toPattern(@"*://*.wikipedia.org/*") options:0]);
    EXPECT_FALSE([toPattern(@"*://*.en.wikipedia.org/*") matchesPattern:toPattern(@"https://*.wikipedia.org/*") options:0]);
    EXPECT_FALSE([toPattern(@"https://*/*") matchesPattern:toPattern(@"*://*.example.com/*") options:0]);
    EXPECT_FALSE([toPattern(@"http://*/*") matchesPattern:toPattern(@"*://*.example.com/*") options:0]);
    EXPECT_FALSE([toPattern(@"http://*/*") matchesPattern:toPattern(@"*://*/foo*") options:0]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"https://*/*") options:0]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"http://*/*") options:0]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"*://*/*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"<all_urls>") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"http://*.example.com/*") matchesPattern:toPattern(@"<all_urls>") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_FALSE([toPattern(@"ftp://*.example.com/*") matchesPattern:toPattern(@"<all_urls>") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"*://*.en.wikipedia.org/*") matchesPattern:toPattern(@"*://*.wikipedia.org/*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"https://*.en.wikipedia.org/*") matchesPattern:toPattern(@"*://*.wikipedia.org/*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"*://*.en.wikipedia.org/*") matchesPattern:toPattern(@"https://*.wikipedia.org/*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"https://*/*") matchesPattern:toPattern(@"*://*.example.com/*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"http://*/*") matchesPattern:toPattern(@"*://*.example.com/*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"http://*/*") matchesPattern:toPattern(@"*://*/foo*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"https://*/*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/*") matchesPattern:toPattern(@"http://*/*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);

    // Matches with regex special characters in pattern.
    EXPECT_TRUE([toPattern(@"*://*/foo?bar*") matchesPattern:toPattern(@"*://*/foo?bar") options:0]);
    EXPECT_FALSE([toPattern(@"*://*/foo?bar*") matchesPattern:toPattern(@"*://*/fobar") options:0]);
    EXPECT_TRUE([toPattern(@"*://*/foo[ba]r*") matchesPattern:toPattern(@"*://*/foo[ba]r") options:0]);
    EXPECT_FALSE([toPattern(@"*://*/foo[ba]r*") matchesPattern:toPattern(@"*://*/fooar") options:0]);
    EXPECT_TRUE([toPattern(@"*://*/foo|bar*") matchesPattern:toPattern(@"*://*/foo|bar") options:0]);
    EXPECT_FALSE([toPattern(@"*://*/foo|bar*") matchesPattern:toPattern(@"*://*/foo") options:0]);

    // Matches a URL that is less permissive.
    EXPECT_FALSE([toPattern(@"https://www.apple.com/foo/bar/baz/*") matchesPattern:toPattern(@"*://www.apple.com/foo/*")]);
    EXPECT_TRUE([toPattern(@"https://www.apple.com/foo/bar/baz/*") matchesPattern:toPattern(@"*://www.apple.com/foo/*") options:_WKWebExtensionMatchPatternOptionsMatchBidirectionally]);

    // Connivence methods
    EXPECT_NS_EQUAL([_WKWebExtensionMatchPattern allURLsMatchPattern].string, @"<all_urls>");
    EXPECT_NS_EQUAL([_WKWebExtensionMatchPattern allHostsAndSchemesMatchPattern].string, @"*://*/*");
}

TEST(WKWebExtensionMatchPattern, MatchPatternMatchesURL)
{
    // Matches any URL that uses the http scheme.
    EXPECT_TRUE([toPattern(@"http://*/*") matchesURL:[NSURL URLWithString:@"http://www.example.com/"]]);
    EXPECT_TRUE([toPattern(@"http://*/*") matchesURL:[NSURL URLWithString:@"http://example.com/foo/bar.html"]]);

    // Matches any URL that uses the http scheme, on any host, as long as the path starts with /foo.
    EXPECT_TRUE([toPattern(@"http://*/foo*") matchesURL:[NSURL URLWithString:@"http://example.com/foo/bar.html"]]);
    EXPECT_TRUE([toPattern(@"http://*/*") matchesURL:[NSURL URLWithString:@"http://www.example.com/foo"]]);

    // Matches any URL that uses the https scheme, is on a example.com host (such as www.example.com, bar.example.com,
    // or example.com), as long as the path starts with /foo and ends with bar.
    EXPECT_TRUE([toPattern(@"https://*.example.com/foo*bar") matchesURL:[NSURL URLWithString:@"https://www.example.com/foo/baz/bar"]]);
    EXPECT_TRUE([toPattern(@"https://*.example.com/foo*bar") matchesURL:[NSURL URLWithString:@"https://bar.example.com/foobar"]]);

    // Matches the specified URL.
    EXPECT_TRUE([toPattern(@"http://example.com/foo/bar.html") matchesURL:[NSURL URLWithString:@"http://example.com/foo/bar.html"]]);

    // Matches any file whose path starts with /foo.
    EXPECT_TRUE([toPattern(@"file:///foo*") matchesURL:[NSURL URLWithString:@"file:///foo/bar.html"]]);
    EXPECT_TRUE([toPattern(@"file:///foo*") matchesURL:[NSURL URLWithString:@"file:///foo"]]);
    EXPECT_TRUE([toPattern(@"file://localhost/foo*") matchesURL:[NSURL URLWithString:@"file://localhost/foo"]]);
    EXPECT_FALSE([toPattern(@"file://localhost/foo*") matchesURL:[NSURL URLWithString:@"file:///foo"]]);
    EXPECT_FALSE([toPattern(@"file:///foo*") matchesURL:[NSURL URLWithString:@"file://localhost/foo"]]);
    EXPECT_TRUE([toPattern(@"file://*/foo*") matchesURL:[NSURL URLWithString:@"file:///foo/bar.html"]]);
    EXPECT_TRUE([toPattern(@"file://*/foo*") matchesURL:[NSURL URLWithString:@"file://localhost/foo/bar.html"]]);
    EXPECT_TRUE([toPattern(@"file://*/foo*") matchesURL:[NSURL URLWithString:@"file://test.local/foo/bar.html"]]);
    EXPECT_TRUE([toPattern(@"file://*/foo*") matchesURL:[NSURL URLWithString:@"file://apple.com/foo/bar.html"]]);
    EXPECT_TRUE([toPattern(@"file://*.local/foo*") matchesURL:[NSURL URLWithString:@"file://test.local/foo"]]);
    EXPECT_FALSE([toPattern(@"file://*.local/foo*") matchesURL:[NSURL URLWithString:@"file://apple.com/foo"]]);

    // Matches ignoring scheme.
    EXPECT_FALSE([toPattern(@"http://*.example.com/*") matchesURL:[NSURL URLWithString:@"https://example.com/"] options:0]);
    EXPECT_FALSE([toPattern(@"https://*.example.com/*") matchesURL:[NSURL URLWithString:@"http://example.com/"] options:0]);
    EXPECT_FALSE([toPattern(@"http://*.example.com/*") matchesURL:[NSURL URLWithString:@"ftp://example.com/"] options:0]);
    EXPECT_TRUE([toPattern(@"http://*.example.com/*") matchesURL:[NSURL URLWithString:@"https://example.com/"] options:_WKWebExtensionMatchPatternOptionsIgnoreSchemes]);
    EXPECT_TRUE([toPattern(@"https://*.example.com/*") matchesURL:[NSURL URLWithString:@"http://example.com/"] options:_WKWebExtensionMatchPatternOptionsIgnoreSchemes]);
    EXPECT_TRUE([toPattern(@"http://*.example.com/*") matchesURL:[NSURL URLWithString:@"ftp://example.com/"] options:_WKWebExtensionMatchPatternOptionsIgnoreSchemes]);

    // Matches ignoring path.
    EXPECT_FALSE([toPattern(@"https://*.example.com/foo*bar") matchesURL:[NSURL URLWithString:@"https://www.example.com/baz"] options:0]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/foo*bar") matchesURL:[NSURL URLWithString:@"http://www.example.com/test"] options:0]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/test*") matchesURL:[NSURL URLWithString:@"https://www.example.com/bar"] options:0]);
    EXPECT_FALSE([toPattern(@"*://*.example.com/*bar") matchesURL:[NSURL URLWithString:@"http://example.com/baz"] options:0]);
    EXPECT_TRUE([toPattern(@"https://*.example.com/foo*bar") matchesURL:[NSURL URLWithString:@"https://www.example.com/baz"] options:_WKWebExtensionMatchPatternOptionsIgnorePaths]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/foo*bar") matchesURL:[NSURL URLWithString:@"http://www.example.com/test"] options:_WKWebExtensionMatchPatternOptionsIgnorePaths]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/test*") matchesURL:[NSURL URLWithString:@"https://www.example.com/bar"] options:_WKWebExtensionMatchPatternOptionsIgnorePaths]);
    EXPECT_TRUE([toPattern(@"*://*.example.com/*bar") matchesURL:[NSURL URLWithString:@"http://example.com/baz"] options:_WKWebExtensionMatchPatternOptionsIgnorePaths]);

    // Matches host.
    EXPECT_TRUE([toPattern(@"https://*.example.com/*") matchesURL:[NSURL URLWithString:@"https://example.com/"] options:0]);
    EXPECT_TRUE([toPattern(@"https://*.example.com/*") matchesURL:[NSURL URLWithString:@"https://www.example.com/"] options:0]);
    EXPECT_FALSE([toPattern(@"https://*.example.com/*") matchesURL:[NSURL URLWithString:@"https://the-example.com/"] options:0]);
    EXPECT_FALSE([toPattern(@"https://*.example.com/*") matchesURL:[NSURL URLWithString:@"https://www.the-example.com/"] options:0]);

    // Matches any URL that uses the http scheme and is on the host 127.0.0.1.
    EXPECT_TRUE([toPattern(@"http://127.0.0.1/*") matchesURL:[NSURL URLWithString:@"http://127.0.0.1/"]]);
    EXPECT_TRUE([toPattern(@"http://127.0.0.1/*") matchesURL:[NSURL URLWithString:@"http://127.0.0.1/foo/bar.html"]]);

    // Matches any URL that starts with http://foo.example.com or https://foo.example.com.
    EXPECT_TRUE([toPattern(@"*://foo.example.com/*") matchesURL:[NSURL URLWithString:@"http://foo.example.com/foo/baz/bar"]]);
    EXPECT_TRUE([toPattern(@"*://foo.example.com/*") matchesURL:[NSURL URLWithString:@"https://foo.example.com/foobar"]]);

    // Test missing hosts.
    EXPECT_FALSE([toPattern(@"*:///*") matchesURL:[NSURL URLWithString:@"https://example.com/foobar"]]);
    EXPECT_FALSE([toPattern(@"https:///*") matchesURL:[NSURL URLWithString:@"https://example.com/foobar"]]);
    EXPECT_FALSE([toPattern(@"ftp:///*") matchesURL:[NSURL URLWithString:@"ftp://example.com/foobar"]]);
    EXPECT_TRUE([toPattern(@"file:///*") matchesURL:[NSURL URLWithString:@"file:///foobar"]]);

    // Matches any URL that uses a permitted scheme. (See the beginning of this section for the list of permitted schemes.)
    EXPECT_TRUE([toPattern(@"<all_urls>") matchesURL:[NSURL URLWithString:@"http://example.com/foo/bar.html"]]);
    EXPECT_FALSE([toPattern(@"<all_urls>") matchesURL:[NSURL URLWithString:@"file:///bar/baz.html"]]);
    EXPECT_FALSE([toPattern(@"<all_urls>") matchesURL:[NSURL URLWithString:@"favorites://"]]);
    EXPECT_FALSE([toPattern(@"<all_urls>") matchesURL:[NSURL URLWithString:@"bookmarks://"]]);
    EXPECT_FALSE([toPattern(@"<all_urls>") matchesURL:[NSURL URLWithString:@"history://"]]);

    // Matches with regex special characters in pattern.
    EXPECT_TRUE([toPattern(@"*://*/foo?bar*") matchesURL:[NSURL URLWithString:@"https://example.com/foo%3Fbar"]]);
    EXPECT_FALSE([toPattern(@"*://*/foo?bar*") matchesURL:[NSURL URLWithString:@"https://example.com/fobar"]]);
    EXPECT_TRUE([toPattern(@"*://*/foo[ba]r*") matchesURL:[NSURL URLWithString:@"https://example.com/foo%5Bba%5Dr"]]);
    EXPECT_FALSE([toPattern(@"*://*/foo[ba]r*") matchesURL:[NSURL URLWithString:@"https://example.com/fooar"]]);
    EXPECT_TRUE([toPattern(@"*://*/foo|bar*") matchesURL:[NSURL URLWithString:@"https://example.com/foo%7Cbar"]]);
    EXPECT_FALSE([toPattern(@"*://*/foo|bar*") matchesURL:[NSURL URLWithString:@"https://example.com/foo"]]);
}

TEST(WKWebExtensionMatchPattern, PatternDescriptions)
{
    EXPECT_NS_EQUAL(toPattern(@"<all_urls>").description, @"<all_urls>");
    EXPECT_NS_EQUAL(toPattern(@"*://*/*").description, @"*://*/*");
    EXPECT_NS_EQUAL(toPattern(@"http://*.example.com/*").description, @"http://*.example.com/*");
    EXPECT_NS_EQUAL(toPattern(@"file:///*").description, @"file:///*");
    EXPECT_NS_EQUAL(toPattern(@"file://localhost/*").description, @"file://localhost/*");
}

TEST(WKWebExtensionMatchPattern, MatchesAllHosts)
{
    EXPECT_TRUE(toPattern(@"<all_urls>").matchesAllHosts);
    EXPECT_TRUE(toPattern(@"*://*/*").matchesAllHosts);
    EXPECT_TRUE(toPattern(@"http://*/*").matchesAllHosts);
    EXPECT_TRUE(toPattern(@"https://*/*").matchesAllHosts);
    EXPECT_TRUE(toPattern(@"file://*/*").matchesAllHosts);
    EXPECT_FALSE(toPattern(@"file:///*").matchesAllHosts);
}

TEST(WKWebExtensionMatchPattern, MatchesAllURLs)
{
    EXPECT_TRUE(toPattern(@"<all_urls>").matchesAllURLs);
    EXPECT_FALSE(toPattern(@"*://*/*").matchesAllURLs);
    EXPECT_FALSE(toPattern(@"http://*/*").matchesAllURLs);
    EXPECT_FALSE(toPattern(@"https://*/*").matchesAllURLs);
    EXPECT_FALSE(toPattern(@"file://*/*").matchesAllURLs);
    EXPECT_FALSE(toPattern(@"file:///*").matchesAllURLs);
}

TEST(WKWebExtensionMatchPattern, PatternCacheAndEquality)
{
    // All these patterns should come from the cache and have pointer equality.
    EXPECT_EQ(toPattern(@"<all_urls>"), toPattern(@"<all_urls>"));
    EXPECT_EQ(toPattern(@"<all_urls>"), toPatternAlloc(@"<all_urls>"));
    EXPECT_EQ(toPattern(@"<all_urls>"), [_WKWebExtensionMatchPattern allURLsMatchPattern]);
    EXPECT_EQ(toPatternAlloc(@"<all_urls>"), toPatternAlloc(@"<all_urls>"));
    EXPECT_EQ(toPattern(@"*://*/*"), toPattern(@"*://*/*"));
    EXPECT_EQ(toPattern(@"*://*/*"), toPatternAlloc(@"*://*/*"));
    EXPECT_EQ(toPattern(@"*://*/*"), [_WKWebExtensionMatchPattern allHostsAndSchemesMatchPattern]);
    EXPECT_EQ(toPatternAlloc(@"*://*/*"), toPatternAlloc(@"*://*/*"));
    EXPECT_EQ(toPattern(@"*", @"*", @"/*"), toPattern(@"*", @"*", @"/*"));
    EXPECT_EQ(toPattern(@"*", @"*", @"/*"), toPatternAlloc(@"*", @"*", @"/*"));
    EXPECT_EQ(toPatternAlloc(@"*", @"*", @"/*"), toPatternAlloc(@"*", @"*", @"/*"));
    EXPECT_EQ(toPattern(@"*://*/*"), toPattern(@"*", @"*", @"/*"));
    EXPECT_EQ(toPattern(@"*://*/*"), toPatternAlloc(@"*", @"*", @"/*"));

    // All these patterns should be equal.
    EXPECT_NS_EQUAL(toPattern(@"<all_urls>"), toPattern(@"<all_urls>"));
    EXPECT_NS_EQUAL(toPattern(@"<all_urls>"), toPatternAlloc(@"<all_urls>"));
    EXPECT_NS_EQUAL(toPattern(@"<all_urls>"), [_WKWebExtensionMatchPattern allURLsMatchPattern]);
    EXPECT_NS_EQUAL(toPatternAlloc(@"<all_urls>"), toPatternAlloc(@"<all_urls>"));
    EXPECT_NS_EQUAL(toPattern(@"*://*/*"), toPattern(@"*://*/*"));
    EXPECT_NS_EQUAL(toPattern(@"*://*/*"), toPatternAlloc(@"*://*/*"));
    EXPECT_NS_EQUAL(toPattern(@"*://*/*"), [_WKWebExtensionMatchPattern allHostsAndSchemesMatchPattern]);
    EXPECT_NS_EQUAL(toPatternAlloc(@"*://*/*"), toPatternAlloc(@"*://*/*"));
    EXPECT_NS_EQUAL(toPattern(@"*", @"*", @"/*"), toPattern(@"*", @"*", @"/*"));
    EXPECT_NS_EQUAL(toPattern(@"*", @"*", @"/*"), toPatternAlloc(@"*", @"*", @"/*"));
    EXPECT_NS_EQUAL(toPatternAlloc(@"*", @"*", @"/*"), toPatternAlloc(@"*", @"*", @"/*"));
    EXPECT_NS_EQUAL(toPattern(@"*://*/*"), toPattern(@"*", @"*", @"/*"));
    EXPECT_NS_EQUAL(toPattern(@"*://*/*"), toPatternAlloc(@"*", @"*", @"/*"));

    // All the first patterns should not come from the cache since they check for errors.
    // They should still be equal, just not via pointer equality.
    {
        NSError *error;
        auto* a = toPatternAlloc(@"<all_urls>", &error);
        auto* b = toPatternAlloc(@"<all_urls>");

        EXPECT_NULL(error);
        EXPECT_NE(a, b);
        EXPECT_NS_EQUAL(a, b);
    }

    {
        NSError *error;
        auto* a = toPatternAlloc(@"<all_urls>", &error);
        auto* b = toPattern(@"<all_urls>");

        EXPECT_NULL(error);
        EXPECT_NE(a, b);
        EXPECT_NS_EQUAL(a, b);
    }

    {
        NSError *error;
        auto* a = toPatternAlloc(@"*://*/*", &error);
        auto* b = toPatternAlloc(@"*://*/*");

        EXPECT_NULL(error);
        EXPECT_NE(a, b);
        EXPECT_NS_EQUAL(a, b);
    }

    {
        NSError *error;
        auto* a = toPatternAlloc(@"*://*/*", &error);
        auto* b = toPattern(@"*://*/*");

        EXPECT_NULL(error);
        EXPECT_NE(a, b);
        EXPECT_NS_EQUAL(a, b);
    }

    {
        NSError *error;
        auto* a = toPatternAlloc(@"*://*/*", &error);
        auto* b = toPattern(@"*", @"*", @"/*");

        EXPECT_NULL(error);
        EXPECT_NE(a, b);
        EXPECT_NS_EQUAL(a, b);
    }

    {
        NSError *error;
        auto* a = toPatternAlloc(@"*://*/*", &error);
        auto* b = toPatternAlloc(@"*", @"*", @"/*");

        EXPECT_NULL(error);
        EXPECT_NE(a, b);
        EXPECT_NS_EQUAL(a, b);
    }

    {
        NSError *error;
        auto* a = toPatternAlloc(@"*", @"*", @"/*", &error);
        auto* b = toPattern(@"*", @"*", @"/*");

        EXPECT_NULL(error);
        EXPECT_NE(a, b);
        EXPECT_NS_EQUAL(a, b);
    }

    {
        NSError *error;
        auto* a = toPatternAlloc(@"*", @"*", @"/*", &error);
        auto* b = toPatternAlloc(@"*", @"*", @"/*");

        EXPECT_NULL(error);
        EXPECT_NE(a, b);
        EXPECT_NS_EQUAL(a, b);
    }
}

TEST(WKWebExtensionMatchPattern, CustomURLScheme)
{
    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"foo", @"*", @"/", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"Scheme \"foo\" is invalid.");
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"bar", @"*", @"/", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"Scheme \"bar\" is invalid.");
    }

    [_WKWebExtensionMatchPattern registerCustomURLScheme:@"foo"];

    {
        NSError *error;
        EXPECT_NOT_NULL(toPatternAlloc(@"foo", @"*", @"/", &error));
        EXPECT_NULL(error);
    }

    {
        NSError *error;
        EXPECT_NULL(toPatternAlloc(@"bar", @"*", @"/", &error));
        EXPECT_NS_EQUAL(error.userInfo[NSDebugDescriptionErrorKey], @"Scheme \"bar\" is invalid.");
    }

    [_WKWebExtensionMatchPattern registerCustomURLScheme:@"bar"];

    {
        NSError *error;
        EXPECT_NOT_NULL(toPatternAlloc(@"foo", @"*", @"/", &error));
        EXPECT_NULL(error);
    }

    {
        NSError *error;
        EXPECT_NOT_NULL(toPatternAlloc(@"bar", @"*", @"/", &error));
        EXPECT_NULL(error);
    }
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
