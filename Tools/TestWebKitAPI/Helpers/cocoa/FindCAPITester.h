/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(MAC) && defined(__cplusplus)

#import <WebKit/WKFindOptions.h>
#import <WebKit/WKPageFindMatchesClient.h>
#import <WebKit/WKRetainPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

@class TestWKWebView;

namespace TestWebKitAPI {

class FindCAPITester {
public:
    explicit FindCAPITester(TestWKWebView *);
    ~FindCAPITester();

    struct FindResult {
        bool found { false };
        unsigned matchCount { 0 };
    };
    FindResult findString(NSString *, WKFindOptions, unsigned maxCount = 1000);
    unsigned countStringMatches(NSString *, WKFindOptions, unsigned maxCount = 1000);

    struct MatchesResult {
        Vector<Vector<CGRect>> matchRects;
        int firstIndexAfterSelection { kWKFindResultNoMatchAfterUserSelection };
    };
    MatchesResult findStringMatches(NSString *, WKFindOptions, unsigned maxCount = 1000);
    // Returns null if no image arrives; the web process doesn't reply when it can't snapshot the match.
    WKRetainPtr<WKImageRef> getImageForFindMatch(int32_t index);
    void selectFindMatch(int32_t index);
    void indicateFindMatch(uint32_t index);
    void hideFindUI();

private:
    WKPageRef page() const;
    void waitForCallback();

    static void didFindString(WKPageRef, WKStringRef, unsigned matchCount, const void* clientInfo);
    static void didFailToFindString(WKPageRef, WKStringRef, const void* clientInfo);
    static void didCountStringMatches(WKPageRef, WKStringRef, unsigned matchCount, const void* clientInfo);
    static void didFindStringMatches(WKPageRef, WKStringRef, WKArrayRef matches, int firstIndex, const void* clientInfo);
    static void didGetImageForMatchResult(WKPageRef, WKImageRef, uint32_t index, const void* clientInfo);

    RetainPtr<TestWKWebView> m_webView;
    bool m_done { false };
    FindResult m_findResult;
    unsigned m_count { 0 };
    MatchesResult m_matchesResult;
    WKRetainPtr<WKImageRef> m_image;
};

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && defined(__cplusplus)
