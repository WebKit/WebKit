// Copyright (C) 2024-2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if ENABLE_WK_WEB_EXTENSIONS

import Foundation
import Testing
import WebKit
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionMatchPatternTests {
    private func toPattern(_ string: String, sourceLocation: SourceLocation = #_sourceLocation) throws -> WKWebExtension.MatchPattern {
        try #require(WKWebExtension.MatchPattern.cachedPattern(string: string), sourceLocation: sourceLocation)
    }

    private func toPattern(
        _ scheme: String,
        _ host: String,
        _ path: String,
        sourceLocation: SourceLocation = #_sourceLocation
    ) throws -> WKWebExtension.MatchPattern {
        try #require(WKWebExtension.MatchPattern.cachedPattern(scheme: scheme, host: host, path: path), sourceLocation: sourceLocation)
    }

    // `favorites`, `bookmarks` and `history` are not valid match-pattern schemes, so these do not
    // parse; the assertions using them rely on `matches(_:)` accepting the resulting nil.
    private func unparseablePattern(_ string: String) -> WKWebExtension.MatchPattern? {
        WKWebExtension.MatchPattern.cachedPattern(string: string)
    }

    private func toPatternAlloc(_ string: String) throws -> WKWebExtension.MatchPattern {
        try WKWebExtension.MatchPattern(string: string)
    }

    private func toPatternAlloc(_ scheme: String, _ host: String, _ path: String) throws -> WKWebExtension.MatchPattern {
        try WKWebExtension.MatchPattern(scheme: scheme, host: host, path: path)
    }

    private func expectFailureToParse(
        _ message: String,
        sourceLocation: SourceLocation = #_sourceLocation,
        _ body: () throws -> WKWebExtension.MatchPattern
    ) {
        #expect(sourceLocation: sourceLocation) {
            _ = try body()
        } throws: { error in
            (error as NSError).localizedDescription == message
        }
    }

    @Test
    func patternValidity() {
        expectFailureToParse("\"\" cannot be parsed because it doesn't have a scheme.") {
            try toPatternAlloc("")
        }

        expectFailureToParse("\"http://www.example.com\" cannot be parsed because it doesn't have a path.") {
            try toPatternAlloc("http://www.example.com")
        }

        expectFailureToParse("\"http://www.example.com:8080/\" cannot be parsed because the host \"www.example.com:8080\" is invalid.") {
            try toPatternAlloc("http://www.example.com:8080/")
        }

        expectFailureToParse("\"http://[::1]:8080/\" cannot be parsed because the host \"[::1]:8080\" is invalid.") {
            try toPatternAlloc("http://[::1]:8080/")
        }

        expectFailureToParse("\"http://user@www.example.com/\" cannot be parsed because the host \"user@www.example.com\" is invalid.") {
            try toPatternAlloc("http://user@www.example.com/")
        }

        expectFailureToParse(
            "\"http://user:password@www.example.com/\" cannot be parsed because the host \"user:password@www.example.com\" is invalid."
        ) {
            try toPatternAlloc("http://user:password@www.example.com/")
        }

        expectFailureToParse("\"file://localhost\" cannot be parsed because it doesn't have a path.") {
            try toPatternAlloc("file://localhost")
        }

        expectFailureToParse("\"file://\" cannot be parsed because it doesn't have a path.") {
            try toPatternAlloc("file://")
        }

        expectFailureToParse("\"http://*foo/bar\" cannot be parsed because the host \"*foo\" is invalid.") {
            try toPatternAlloc("http://*foo/bar")
        }

        expectFailureToParse("\"http://foo.*.bar/baz\" cannot be parsed because the host \"foo.*.bar\" is invalid.") {
            try toPatternAlloc("http://foo.*.bar/baz")
        }

        expectFailureToParse("\"http:/bar\" cannot be parsed because it doesn't have a scheme.") {
            try toPatternAlloc("http:/bar")
        }

        expectFailureToParse("\"foo://*\" cannot be parsed because the scheme \"foo\" is invalid.") {
            try toPatternAlloc("foo://*")
        }

        expectFailureToParse("Scheme \"foo\" is invalid.") {
            try toPatternAlloc("foo", "*", "/")
        }

        expectFailureToParse("Host \"example.*\" is invalid.") {
            try toPatternAlloc("https", "example.*", "/")
        }

        expectFailureToParse("Host \"*.example.com:8080\" is invalid.") {
            try toPatternAlloc("https", "*.example.com:8080", "/")
        }

        expectFailureToParse("Host \"[::1]:8080\" is invalid.") {
            try toPatternAlloc("https", "[::1]:8080", "/")
        }

        expectFailureToParse("Host \"user@example.*\" is invalid.") {
            try toPatternAlloc("https", "user@example.*", "/")
        }

        expectFailureToParse("Host \"user@example.*\" is invalid.") {
            try toPatternAlloc("https", "user@example.*", "/")
        }

        expectFailureToParse("Host \"user:password@example.*\" is invalid.") {
            try toPatternAlloc("https", "user:password@example.*", "/")
        }

        expectFailureToParse("Path \"*\" is invalid.") {
            try toPatternAlloc("https", "example.com", "*")
        }
    }

    @Test
    func matchPatternMatchesPattern() throws {
        // Matches any URL that uses the http scheme.
        #expect(try toPattern("http://*/*").matches(toPattern("http://www.example.com/")))
        #expect(try toPattern("http://*/*").matches(toPattern("http://example.com/foo/bar.html")))

        // Matches any URL that uses the http scheme, on any host, as long as the path starts with /foo.
        #expect(try toPattern("http://*/foo*").matches(toPattern("http://example.com/foo/bar.html")))
        #expect(try toPattern("http://*/*").matches(toPattern("http://www.example.com/foo")))

        // Matches any URL that uses the https scheme, is on a example.com host (such as www.example.com, bar.example.com,
        // or example.com), as long as the path starts with /foo and ends with bar.
        #expect(try toPattern("https://*.example.com/foo*bar").matches(toPattern("https://www.example.com/foo/baz/bar")))
        #expect(try toPattern("https://*.example.com/foo*bar").matches(toPattern("https://bar.example.com/foobar")))

        // Matches the specified URL.
        #expect(try toPattern("http://example.com/foo/bar.html").matches(toPattern("http://example.com/foo/bar.html")))

        // Matches any file whose path starts with /foo.
        #expect(try toPattern("file:///foo*").matches(toPattern("file:///foo/bar.html")))
        #expect(try toPattern("file:///foo*").matches(toPattern("file:///foo")))
        #expect(try toPattern("file://localhost/foo*").matches(toPattern("file://localhost/foo")))
        #expect(try toPattern("file://localhost/foo*").matches(toPattern("file:///foo")))
        #expect(try toPattern("file:///foo*").matches(toPattern("file://localhost/foo")))
        #expect(try toPattern("file://*/foo*").matches(toPattern("file:///foo/bar.html")))
        #expect(try toPattern("file://*/foo*").matches(toPattern("file://localhost/foo/bar.html")))
        #expect(try toPattern("file://*/foo*").matches(toPattern("file://test.local/foo/bar.html")))
        #expect(try toPattern("file://*/foo*").matches(toPattern("file://apple.com/foo/bar.html")))
        #expect(try toPattern("file://*.local/foo*").matches(toPattern("file://test.local/foo")))
        #expect(try !toPattern("file://*.local/foo*").matches(toPattern("file://apple.com/foo")))

        // Matches ignoring scheme.
        #expect(try !toPattern("http://*.example.com/*").matches(toPattern("https://*.example.com/*"), options: []))
        #expect(try !toPattern("https://*.example.com/*").matches(toPattern("http://*.example.com/*"), options: []))
        #expect(try !toPattern("http://*.example.com/*").matches(toPattern("*://*.example.com/*"), options: []))
        #expect(try toPattern("http://*.example.com/*").matches(toPattern("https://*.example.com/*"), options: .ignoreSchemes))
        #expect(try toPattern("https://*.example.com/*").matches(toPattern("http://*.example.com/*"), options: .ignoreSchemes))
        #expect(try toPattern("http://*.example.com/*").matches(toPattern("*://*.example.com/*"), options: .ignoreSchemes))

        // Matches ignoring path.
        #expect(try !toPattern("https://*.example.com/foo*bar").matches(toPattern("https://www.example.com/baz"), options: []))
        #expect(try !toPattern("*://*.example.com/foo*bar").matches(toPattern("http://www.example.com/test"), options: []))
        #expect(try !toPattern("*://*.example.com/test*").matches(toPattern("*://*.example.com/bar"), options: []))
        #expect(try !toPattern("*://*.example.com/*bar").matches(toPattern("*://example.com/baz"), options: []))
        #expect(
            try toPattern("https://*.example.com/foo*bar").matches(toPattern("https://www.example.com/baz"), options: .ignorePaths)
        )
        #expect(try toPattern("*://*.example.com/foo*bar").matches(toPattern("http://www.example.com/test"), options: .ignorePaths))
        #expect(try toPattern("*://*.example.com/test*").matches(toPattern("*://*.example.com/bar"), options: .ignorePaths))
        #expect(try toPattern("*://*.example.com/*bar").matches(toPattern("*://example.com/baz"), options: .ignorePaths))

        // Matches any URL that uses the http scheme and is on the host 127.0.0.1.
        #expect(try toPattern("http://127.0.0.1/*").matches(toPattern("http://127.0.0.1/")))
        #expect(try toPattern("http://127.0.0.1/*").matches(toPattern("http://127.0.0.1/foo/bar.html")))

        // Matches any URL that uses the http scheme and is on the host [::1].
        #expect(try toPattern("http://[::1]/*").matches(toPattern("http://[::1]/")))
        #expect(try toPattern("http://[::1]/*").matches(toPattern("http://[::1]/foo/bar.html")))

        // Matches any URL that starts with http://foo.example.com or https://foo.example.com.
        #expect(try toPattern("*://foo.example.com/*").matches(toPattern("http://foo.example.com/foo/baz/bar")))
        #expect(try toPattern("*://foo.example.com/*").matches(toPattern("https://foo.example.com/foobar")))

        // Test missing hosts.
        #expect(try !toPattern("*:///*").matches(toPattern("https://example.com/foobar")))
        #expect(try !toPattern("https:///*").matches(toPattern("https://example.com/foobar")))
        #expect(try !toPattern("ftp:///*").matches(toPattern("ftp://example.com/foobar")))
        #expect(try toPattern("file:///*").matches(toPattern("file:///foobar")))

        // Matches any URL that uses a permitted scheme. (See the beginning of this section for the list of permitted schemes.)
        #expect(try toPattern("<all_urls>").matches(toPattern("http://example.com/foo/bar.html")))
        #expect(try !toPattern("<all_urls>").matches(toPattern("file:///bar/baz.html")))
        #expect(try !toPattern("<all_urls>").matches(unparseablePattern("favorites://")))
        #expect(try !toPattern("<all_urls>").matches(unparseablePattern("bookmarks://")))
        #expect(try !toPattern("<all_urls>").matches(unparseablePattern("history://")))

        // All matches.
        #expect(try toPattern("<all_urls>").matches(toPattern("<all_urls>")))
        #expect(try toPattern("<all_urls>").matches(toPattern("*://*/*")))
        #expect(try !toPattern("*://*/*").matches(toPattern("<all_urls>")))
        #expect(try toPattern("*://*/*").matches(toPattern("*://*/*")))

        // Matching domain patterns.
        #expect(try toPattern("*://*.example.com/*").matches(toPattern("*://www.example.com/test/*")))
        #expect(try toPattern("*://*/*").matches(toPattern("*://*.example.com/*")))
        #expect(try !toPattern("*://*.example.com/*").matches(toPattern("*://*/*")))
        #expect(try toPattern("<all_urls>").matches(toPattern("*://*.example.com/*")))
        #expect(try toPattern("*://*.example.com/*").matches(toPattern("*://www.example.com/test/*")))
        #expect(try !toPattern("*://*.example.com/*").matches(toPattern("*://the-example.com/test/*")))
        #expect(try !toPattern("*://*.example.com/*").matches(toPattern("*://www.the-example.com/test/*")))

        // Bidirectional matching.
        #expect(try !toPattern("*://*.example.com/*").matches(toPattern("*://*/*"), options: []))
        #expect(try !toPattern("*://*.example.com/*").matches(toPattern("<all_urls>"), options: []))
        #expect(try !toPattern("http://*.example.com/*").matches(toPattern("<all_urls>"), options: []))
        #expect(try !toPattern("ftp://*.example.com/*").matches(toPattern("<all_urls>"), options: []))
        #expect(try !toPattern("*://*.en.wikipedia.org/*").matches(toPattern("*://*.wikipedia.org/*"), options: []))
        #expect(try !toPattern("https://*.en.wikipedia.org/*").matches(toPattern("*://*.wikipedia.org/*"), options: []))
        #expect(try !toPattern("*://*.en.wikipedia.org/*").matches(toPattern("https://*.wikipedia.org/*"), options: []))
        #expect(try !toPattern("https://*/*").matches(toPattern("*://*.example.com/*"), options: []))
        #expect(try !toPattern("http://*/*").matches(toPattern("*://*.example.com/*"), options: []))
        #expect(try !toPattern("http://*/*").matches(toPattern("*://*/foo*"), options: []))
        #expect(try !toPattern("*://*.example.com/*").matches(toPattern("https://*/*"), options: []))
        #expect(try !toPattern("*://*.example.com/*").matches(toPattern("http://*/*"), options: []))
        #expect(try toPattern("*://*.example.com/*").matches(toPattern("*://*/*"), options: .matchBidirectionally))
        #expect(try toPattern("*://*.example.com/*").matches(toPattern("<all_urls>"), options: .matchBidirectionally))
        #expect(try toPattern("http://*.example.com/*").matches(toPattern("<all_urls>"), options: .matchBidirectionally))
        #expect(try !toPattern("ftp://*.example.com/*").matches(toPattern("<all_urls>"), options: .matchBidirectionally))
        #expect(try toPattern("*://*.en.wikipedia.org/*").matches(toPattern("*://*.wikipedia.org/*"), options: .matchBidirectionally))
        #expect(
            try toPattern("https://*.en.wikipedia.org/*").matches(toPattern("*://*.wikipedia.org/*"), options: .matchBidirectionally)
        )
        #expect(
            try toPattern("*://*.en.wikipedia.org/*").matches(toPattern("https://*.wikipedia.org/*"), options: .matchBidirectionally)
        )
        #expect(try toPattern("https://*/*").matches(toPattern("*://*.example.com/*"), options: .matchBidirectionally))
        #expect(try toPattern("http://*/*").matches(toPattern("*://*.example.com/*"), options: .matchBidirectionally))
        #expect(try toPattern("http://*/*").matches(toPattern("*://*/foo*"), options: .matchBidirectionally))
        #expect(try toPattern("*://*.example.com/*").matches(toPattern("https://*/*"), options: .matchBidirectionally))
        #expect(try toPattern("*://*.example.com/*").matches(toPattern("http://*/*"), options: .matchBidirectionally))

        // Matches with regex special characters in pattern.
        #expect(try toPattern("*://*/foo?bar*").matches(toPattern("*://*/foo?bar"), options: []))
        #expect(try !toPattern("*://*/foo?bar*").matches(toPattern("*://*/fobar"), options: []))
        #expect(try toPattern("*://*/foo[ba]r*").matches(toPattern("*://*/foo[ba]r"), options: []))
        #expect(try !toPattern("*://*/foo[ba]r*").matches(toPattern("*://*/fooar"), options: []))
        #expect(try toPattern("*://*/foo|bar*").matches(toPattern("*://*/foo|bar"), options: []))
        #expect(try !toPattern("*://*/foo|bar*").matches(toPattern("*://*/foo"), options: []))

        // Matches a URL that is less permissive.
        #expect(try !toPattern("https://www.apple.com/foo/bar/baz/*").matches(toPattern("*://www.apple.com/foo/*")))
        #expect(
            try toPattern("https://www.apple.com/foo/bar/baz/*")
                .matches(toPattern("*://www.apple.com/foo/*"), options: .matchBidirectionally)
        )

        // Connivence methods
        #expect(WKWebExtension.MatchPattern.allURLs().string == "<all_urls>")
        #expect(WKWebExtension.MatchPattern.allHostsAndSchemes().string == "*://*/*")
    }

    @Test
    func matchPatternMatchesURL() throws {
        // Matches any URL that uses the http scheme.
        #expect(try toPattern("http://*/*").matches(URL(string: "http://www.example.com/")))
        #expect(try toPattern("http://*/*").matches(URL(string: "http://example.com/foo/bar.html")))

        // Matches any URL that uses the http scheme, on any host, as long as the path starts with /foo.
        #expect(try toPattern("http://*/foo*").matches(URL(string: "http://example.com/foo/bar.html")))
        #expect(try toPattern("http://*/*").matches(URL(string: "http://www.example.com/foo")))

        // Matches any URL that uses the https scheme, is on a example.com host (such as www.example.com, bar.example.com,
        // or example.com), as long as the path starts with /foo and ends with bar.
        #expect(try toPattern("https://*.example.com/foo*bar").matches(URL(string: "https://www.example.com/foo/baz/bar")))
        #expect(try toPattern("https://*.example.com/foo*bar").matches(URL(string: "https://bar.example.com/foobar")))

        // Matches the specified URL.
        #expect(try toPattern("http://example.com/foo/bar.html").matches(URL(string: "http://example.com/foo/bar.html")))

        // Matches any file whose path starts with /foo.
        #expect(try toPattern("file:///foo*").matches(URL(string: "file:///foo/bar.html")))
        #expect(try toPattern("file:///foo*").matches(URL(string: "file:///foo")))
        #expect(try toPattern("file://localhost/foo*").matches(URL(string: "file://localhost/foo")))
        #expect(try toPattern("file://localhost/foo*").matches(URL(string: "file:///foo")))
        #expect(try toPattern("file:///foo*").matches(URL(string: "file://localhost/foo")))
        #expect(try toPattern("file://*/foo*").matches(URL(string: "file:///foo/bar.html")))
        #expect(try toPattern("file://*/foo*").matches(URL(string: "file://localhost/foo/bar.html")))
        #expect(try toPattern("file://*/foo*").matches(URL(string: "file://test.local/foo/bar.html")))
        #expect(try toPattern("file://*/foo*").matches(URL(string: "file://apple.com/foo/bar.html")))
        #expect(try toPattern("file://*.local/foo*").matches(URL(string: "file://test.local/foo")))
        #expect(try !toPattern("file://*.local/foo*").matches(URL(string: "file://apple.com/foo")))

        // Matches ignoring scheme.
        #expect(try !toPattern("http://*.example.com/*").matches(URL(string: "https://example.com/"), options: []))
        #expect(try !toPattern("https://*.example.com/*").matches(URL(string: "http://example.com/"), options: []))
        #expect(try !toPattern("http://*.example.com/*").matches(URL(string: "ftp://example.com/"), options: []))
        #expect(try toPattern("http://*.example.com/*").matches(URL(string: "https://example.com/"), options: .ignoreSchemes))
        #expect(try toPattern("https://*.example.com/*").matches(URL(string: "http://example.com/"), options: .ignoreSchemes))
        #expect(try toPattern("http://*.example.com/*").matches(URL(string: "ftp://example.com/"), options: .ignoreSchemes))

        // Matches ignoring path.
        #expect(try !toPattern("https://*.example.com/foo*bar").matches(URL(string: "https://www.example.com/baz"), options: []))
        #expect(try !toPattern("*://*.example.com/foo*bar").matches(URL(string: "http://www.example.com/test"), options: []))
        #expect(try !toPattern("*://*.example.com/test*").matches(URL(string: "https://www.example.com/bar"), options: []))
        #expect(try !toPattern("*://*.example.com/*bar").matches(URL(string: "http://example.com/baz"), options: []))
        #expect(
            try toPattern("https://*.example.com/foo*bar").matches(URL(string: "https://www.example.com/baz"), options: .ignorePaths)
        )
        #expect(try toPattern("*://*.example.com/foo*bar").matches(URL(string: "http://www.example.com/test"), options: .ignorePaths))
        #expect(try toPattern("*://*.example.com/test*").matches(URL(string: "https://www.example.com/bar"), options: .ignorePaths))
        #expect(try toPattern("*://*.example.com/*bar").matches(URL(string: "http://example.com/baz"), options: .ignorePaths))

        // Matches host.
        #expect(try toPattern("https://*.example.com/*").matches(URL(string: "https://example.com/"), options: []))
        #expect(try toPattern("https://*.example.com/*").matches(URL(string: "https://www.example.com/"), options: []))
        #expect(try !toPattern("https://*.example.com/*").matches(URL(string: "https://the-example.com/"), options: []))
        #expect(try !toPattern("https://*.example.com/*").matches(URL(string: "https://www.the-example.com/"), options: []))

        // Matches any URL that uses the http scheme and is on the host 127.0.0.1.
        #expect(try toPattern("http://127.0.0.1/*").matches(URL(string: "http://127.0.0.1/")))
        #expect(try toPattern("http://127.0.0.1/*").matches(URL(string: "http://127.0.0.1/foo/bar.html")))
        #expect(try toPattern("http://127.0.0.1/*").matches(URL(string: "http://127.0.0.1:8080/foo/bar.html")))

        // Matches any URL that uses the http scheme and is on the host [::1].
        #expect(try toPattern("http://[::1]/*").matches(URL(string: "http://[::1]/")))
        #expect(try toPattern("http://[::1]/*").matches(URL(string: "http://[::1]/foo/bar.html")))
        #expect(try toPattern("http://[::1]/*").matches(URL(string: "http://[::1]:8080/foo/bar.html")))

        // Matches with username and password
        #expect(try toPattern("https://*.example.com/*").matches(URL(string: "https://user@example.com/"), options: []))
        #expect(try toPattern("https://*.example.com/*").matches(URL(string: "https://user:password@example.com/"), options: []))

        // Matches any URL that starts with http://foo.example.com or https://foo.example.com.
        #expect(try toPattern("*://foo.example.com/*").matches(URL(string: "http://foo.example.com/foo/baz/bar")))
        #expect(try toPattern("*://foo.example.com/*").matches(URL(string: "https://foo.example.com/foobar")))

        // Test missing hosts.
        #expect(try !toPattern("*:///*").matches(URL(string: "https://example.com/foobar")))
        #expect(try !toPattern("https:///*").matches(URL(string: "https://example.com/foobar")))
        #expect(try !toPattern("ftp:///*").matches(URL(string: "ftp://example.com/foobar")))
        #expect(try toPattern("file:///*").matches(URL(string: "file:///foobar")))

        // Matches any URL that uses a permitted scheme. (See the beginning of this section for the list of permitted schemes.)
        #expect(try toPattern("<all_urls>").matches(URL(string: "http://example.com/foo/bar.html")))
        #expect(try !toPattern("<all_urls>").matches(URL(string: "file:///bar/baz.html")))
        #expect(try !toPattern("<all_urls>").matches(URL(string: "favorites://")))
        #expect(try !toPattern("<all_urls>").matches(URL(string: "bookmarks://")))
        #expect(try !toPattern("<all_urls>").matches(URL(string: "history://")))

        // Matches with regex and percent encoded special characters in pattern.
        #expect(try toPattern("*://*/foo%3Fbar*").matches(URL(string: "https://example.com/foo%3Fbar")))
        #expect(try !toPattern("*://*/foo?bar*").matches(URL(string: "https://example.com/foo%3Fbar")))
        #expect(try !toPattern("*://*/foo?bar*").matches(URL(string: "https://example.com/fobar")))
        #expect(try toPattern("*://*/foo%5Bba%5Dr*").matches(URL(string: "https://example.com/foo%5Bba%5Dr")))
        #expect(try !toPattern("*://*/foo[ba]r*").matches(URL(string: "https://example.com/foo%5Bba%5Dr")))
        #expect(try !toPattern("*://*/foo[ba]r*").matches(URL(string: "https://example.com/fooar")))
        #expect(try toPattern("*://*/foo%7Cbar*").matches(URL(string: "https://example.com/foo%7Cbar")))
        #expect(try !toPattern("*://*/foo|bar*").matches(URL(string: "https://example.com/foo%7Cbar")))
        #expect(try !toPattern("*://*/foo|bar*").matches(URL(string: "https://example.com/foo")))
    }

    @Test
    func allowFileSchemeOption() throws {
        // The value of WKWebExtensionMatchPatternOptionsAllowFileScheme, spelled out because the
        // constant itself cannot be referenced from here: under C++ interop NS_OPTIONS expands to a
        // bare NSUInteger typedef that is unavailable to Swift, so the constant imports as an Int in
        // that mode and as Options otherwise, and no single spelling compiles in both.
        let allowFile = WKWebExtension.MatchPattern.Options(rawValue: 1 << 3)
        let fileURL = URL(string: "file:///foo/bar.html")
        let httpURL = URL(string: "http://example.com/foo/bar.html")

        // <all_urls> default: does not match file:// (also covered by MatchPatternMatchesURL).
        #expect(try !toPattern("<all_urls>").matches(fileURL))

        // <all_urls> with the option: matches file://.
        #expect(try toPattern("<all_urls>").matches(fileURL, options: allowFile))

        // <all_urls> with the option still matches non-file URLs.
        #expect(try toPattern("<all_urls>").matches(httpURL, options: allowFile))

        // *://*/* must NOT match file:// even with the option (Chrome and Firefox parity).
        #expect(try !toPattern("*://*/*").matches(fileURL))
        #expect(try !toPattern("*://*/*").matches(fileURL, options: allowFile))

        // Explicit file pattern matches file:// without the option.
        #expect(try toPattern("file:///*").matches(fileURL))
        #expect(try !toPattern("file:///*").matches(httpURL))

        // Pattern-vs-pattern: <all_urls> covers file:///* only with the option.
        #expect(try !toPattern("<all_urls>").matches(toPattern("file:///*")))
        #expect(try toPattern("<all_urls>").matches(toPattern("file:///*"), options: allowFile))

        // *://*/* does not cover file:///* even with the option.
        #expect(try !toPattern("*://*/*").matches(toPattern("file:///*"), options: allowFile))

        // file:///* covers more specific file patterns (no option needed).
        #expect(try toPattern("file:///*").matches(toPattern("file:///foo/bar.html")))
    }

    @Test
    func patternDescriptions() throws {
        #expect(try toPattern("<all_urls>").description == "<all_urls>")
        #expect(try toPattern("*://*/*").description == "*://*/*")
        #expect(try toPattern("http://*.example.com/*").description == "http://*.example.com/*")
        #expect(try toPattern("file:///*").description == "file:///*")
        #expect(try toPattern("file://localhost/*").description == "file:///*")
        #expect(try toPattern("file", "", "/*").description == "file:///*")
        #expect(try toPattern("file", "localhost", "/*").description == "file:///*")
    }

    @Test
    func matchesAllHosts() throws {
        #expect(try toPattern("<all_urls>").matchesAllHosts)
        #expect(try toPattern("*://*/*").matchesAllHosts)
        #expect(try toPattern("http://*/*").matchesAllHosts)
        #expect(try toPattern("https://*/*").matchesAllHosts)
        #expect(try toPattern("file://*/*").matchesAllHosts)
        #expect(try !toPattern("file:///*").matchesAllHosts)
    }

    @Test
    func matchesAllURLs() throws {
        #expect(try toPattern("<all_urls>").matchesAllURLs)
        #expect(try !toPattern("*://*/*").matchesAllURLs)
        #expect(try !toPattern("http://*/*").matchesAllURLs)
        #expect(try !toPattern("https://*/*").matchesAllURLs)
        #expect(try !toPattern("file://*/*").matchesAllURLs)
        #expect(try !toPattern("file:///*").matchesAllURLs)
    }

    @Test
    func patternCacheAndEquality() throws {
        // All these patterns should come from the cache and have pointer equality.
        #expect(try toPattern("<all_urls>") === toPattern("<all_urls>"))
        #expect(try toPattern("<all_urls>") === WKWebExtension.MatchPattern.allURLs())
        #expect(try toPattern("*://*/*") === toPattern("*://*/*"))
        #expect(try toPattern("*://*/*") === WKWebExtension.MatchPattern.allHostsAndSchemes())
        #expect(try toPattern("*", "*", "/*") === toPattern("*", "*", "/*"))
        #expect(try toPattern("*://*/*") === toPattern("*", "*", "/*"))

        // All these patterns should be equal.
        #expect(try toPattern("<all_urls>") == toPattern("<all_urls>"))
        #expect(try toPattern("<all_urls>") == WKWebExtension.MatchPattern.allURLs())
        #expect(try toPattern("*://*/*") == toPattern("*://*/*"))
        #expect(try toPattern("*://*/*") == WKWebExtension.MatchPattern.allHostsAndSchemes())
        #expect(try toPattern("*", "*", "/*") == toPattern("*", "*", "/*"))
        #expect(try toPattern("*://*/*") == toPattern("*", "*", "/*"))

        // All the first patterns should not come from the cache since they check for errors.
        // They should still be equal, just not via pointer equality.
        do {
            let a = try toPatternAlloc("<all_urls>")
            let b = try toPattern("<all_urls>")

            #expect(a !== b)
            #expect(a == b)
        }

        do {
            let a = try toPatternAlloc("*://*/*")
            let b = try toPattern("*://*/*")

            #expect(a !== b)
            #expect(a == b)
        }

        do {
            let a = try toPatternAlloc("*://*/*")
            let b = try toPattern("*", "*", "/*")

            #expect(a !== b)
            #expect(a == b)
        }

        do {
            let a = try toPatternAlloc("*", "*", "/*")
            let b = try toPattern("*", "*", "/*")

            #expect(a !== b)
            #expect(a == b)
        }
    }

    @Test
    func customURLScheme() throws {
        expectFailureToParse("Scheme \"foo\" is invalid.") {
            try toPatternAlloc("foo", "*", "/")
        }

        expectFailureToParse("Scheme \"bar\" is invalid.") {
            try toPatternAlloc("bar", "*", "/")
        }

        WKWebExtension.MatchPattern.registerCustomURLScheme("foo")

        _ = try toPatternAlloc("foo", "*", "/")

        expectFailureToParse("Scheme \"bar\" is invalid.") {
            try toPatternAlloc("bar", "*", "/")
        }

        WKWebExtension.MatchPattern.registerCustomURLScheme("bar")

        _ = try toPatternAlloc("foo", "*", "/")
        _ = try toPatternAlloc("bar", "*", "/")
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
