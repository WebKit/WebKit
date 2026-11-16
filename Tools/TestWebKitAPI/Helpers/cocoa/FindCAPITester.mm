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

#import "config.h"
#import "Helpers/cocoa/FindCAPITester.h"

#if PLATFORM(MAC)

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKArray.h>
#import <WebKit/WKGeometry.h>
#import <WebKit/WKPage.h>
#import <WebKit/WKPageFindClient.h>
#import <WebKit/WKStringCF.h>
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

static WKRetainPtr<WKStringRef> toWKString(NSString *string)
{
    return adoptWK(WKStringCreateWithCFString((__bridge CFStringRef)string));
}

static FindCAPITester& testerFromClientInfo(const void* clientInfo)
{
    return *const_cast<FindCAPITester*>(static_cast<const FindCAPITester*>(clientInfo));
}

FindCAPITester::FindCAPITester(TestWKWebView *webView)
    : m_webView(webView)
{
    WKPageFindClientV0 findClient { };
    findClient.base = { 0, this };
    findClient.didFindString = didFindString;
    findClient.didFailToFindString = didFailToFindString;
    findClient.didCountStringMatches = didCountStringMatches;
    WKPageSetPageFindClient(page(), &findClient.base);

    WKPageFindMatchesClientV0 findMatchesClient { };
    findMatchesClient.base = { 0, this };
    findMatchesClient.didFindStringMatches = didFindStringMatches;
    findMatchesClient.didGetImageForMatchResult = didGetImageForMatchResult;
    WKPageSetPageFindMatchesClient(page(), &findMatchesClient.base);
}

FindCAPITester::~FindCAPITester()
{
    WKPageSetPageFindClient(page(), nullptr);
    WKPageSetPageFindMatchesClient(page(), nullptr);
}

WKPageRef FindCAPITester::page() const
{
    return [m_webView _pageRefForTransitionToWKWebView];
}

void FindCAPITester::waitForCallback()
{
    Util::run(&m_done);
    m_done = false;
}

FindCAPITester::FindResult FindCAPITester::findString(NSString *string, WKFindOptions options, unsigned maxCount)
{
    WKPageFindString(page(), toWKString(string).get(), options, maxCount);
    waitForCallback();
    return m_findResult;
}

unsigned FindCAPITester::countStringMatches(NSString *string, WKFindOptions options, unsigned maxCount)
{
    WKPageCountStringMatches(page(), toWKString(string).get(), options, maxCount);
    waitForCallback();
    return m_count;
}

FindCAPITester::MatchesResult FindCAPITester::findStringMatches(NSString *string, WKFindOptions options, unsigned maxCount)
{
    m_matchesResult = { };
    WKPageFindStringMatches(page(), toWKString(string).get(), options, maxCount);
    waitForCallback();
    return m_matchesResult;
}

WKRetainPtr<WKImageRef> FindCAPITester::getImageForFindMatch(int32_t index)
{
    m_image = nullptr;
    WKPageGetImageForFindMatch(page(), index);
    Util::runFor(&m_done, 5_s);
    m_done = false;
    return std::exchange(m_image, nullptr);
}

void FindCAPITester::selectFindMatch(int32_t index)
{
    WKPageSelectFindMatch(page(), index);
    [m_webView waitForNextPresentationUpdate];
}

void FindCAPITester::indicateFindMatch(uint32_t index)
{
    WKPageIndicateFindMatch(page(), index);
    [m_webView waitForNextPresentationUpdate];
}

void FindCAPITester::hideFindUI()
{
    WKPageHideFindUI(page());
    [m_webView waitForNextPresentationUpdate];
}

void FindCAPITester::didFindString(WKPageRef, WKStringRef, unsigned matchCount, const void* clientInfo)
{
    auto& self = testerFromClientInfo(clientInfo);
    self.m_findResult = { true, matchCount };
    self.m_done = true;
}

void FindCAPITester::didFailToFindString(WKPageRef, WKStringRef, const void* clientInfo)
{
    auto& self = testerFromClientInfo(clientInfo);
    self.m_findResult = { false, 0 };
    self.m_done = true;
}

void FindCAPITester::didCountStringMatches(WKPageRef, WKStringRef, unsigned matchCount, const void* clientInfo)
{
    auto& self = testerFromClientInfo(clientInfo);
    self.m_count = matchCount;
    self.m_done = true;
}

void FindCAPITester::didFindStringMatches(WKPageRef, WKStringRef, WKArrayRef matches, int firstIndex, const void* clientInfo)
{
    auto& self = testerFromClientInfo(clientInfo);
    for (size_t i = 0; i < WKArrayGetSize(matches); ++i) {
        auto rects = static_cast<WKArrayRef>(WKArrayGetItemAtIndex(matches, i));
        Vector<CGRect> matchRects;
        for (size_t j = 0; j < WKArrayGetSize(rects); ++j) {
            auto rect = WKRectGetValue(static_cast<WKRectRef>(WKArrayGetItemAtIndex(rects, j)));
            matchRects.append(CGRectMake(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height));
        }
        self.m_matchesResult.matchRects.append(WTF::move(matchRects));
    }
    self.m_matchesResult.firstIndexAfterSelection = firstIndex;
    self.m_done = true;
}

void FindCAPITester::didGetImageForMatchResult(WKPageRef, WKImageRef image, uint32_t, const void* clientInfo)
{
    auto& self = testerFromClientInfo(clientInfo);
    self.m_image = image;
    self.m_done = true;
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
