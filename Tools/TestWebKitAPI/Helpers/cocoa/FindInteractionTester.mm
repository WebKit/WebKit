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
#import "Helpers/cocoa/FindInteractionTester.h"

#if HAVE(UIFINDINTERACTION)

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/FindInPageUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

@interface WKWebView () <UITextSearching>
- (void)didBeginTextSearchOperation;
- (void)didEndTextSearchOperation;
@end

namespace TestWebKitAPI {

FindInteractionTester::FindInteractionTester(TestWKWebView *webView)
    : m_webView(webView)
    , m_scrollViewDelegate(adoptNS([TestScrollViewDelegate new]))
{
    [m_webView setFindInteractionEnabled:YES];
    [m_webView scrollView].delegate = m_scrollViewDelegate.get();
}

FindInteractionTester::~FindInteractionTester()
{
    [m_webView scrollView].delegate = nil;
}

void FindInteractionTester::didBeginTextSearchOperation()
{
    [m_webView didBeginTextSearchOperation];
    [m_webView waitForNextPresentationUpdate];
}

void FindInteractionTester::didEndTextSearchOperation()
{
    [m_webView didEndTextSearchOperation];
    // The overlay fades out before its layer is removed.
    Util::waitFor([&] {
        return ![m_webView _layerForFindOverlay];
    }, 20);
}

RetainPtr<NSOrderedSet<UITextRange *>> FindInteractionTester::performTextSearch(NSString *string, UITextSearchOptions *options)
{
    __block bool done = false;
    RetainPtr aggregator = adoptNS([[TestSearchAggregator alloc] initWithCompletionHandler:^{
        done = true;
    }]);
    RetainPtr defaultOptions = adoptNS([UITextSearchOptions new]);
    [m_webView performTextSearchWithQueryString:string usingOptions:options ?: defaultOptions.get() resultAggregator:aggregator.get()];
    Util::run(&done);
    return adoptNS([[aggregator allFoundRanges] copy]);
}

void FindInteractionTester::decorateFoundTextRange(UITextRange *range, UITextSearchFoundTextStyle style)
{
    [m_webView decorateFoundTextRange:range inDocument:nil usingStyle:style];
    [m_webView waitForNextPresentationUpdate];
}

void FindInteractionTester::clearAllDecoratedFoundText()
{
    [m_webView clearAllDecoratedFoundText];
    [m_webView waitForNextPresentationUpdate];
}

void FindInteractionTester::scrollRangeToVisible(UITextRange *range)
{
    m_scrollViewDelegate->_finishedScrolling = false;
    [m_webView scrollRangeToVisible:range inDocument:nil];
    Util::runFor(&m_scrollViewDelegate->_finishedScrolling, 1_s);
    [m_webView waitForNextPresentationUpdate];
}

void FindInteractionTester::replaceFoundTextInRange(UITextRange *range, NSString *replacement)
{
    [m_webView replaceFoundTextInRange:range inDocument:nil withText:replacement];
    [m_webView waitForNextPresentationUpdate];
}

CGRect FindInteractionTester::rectForFoundTextRange(UITextRange *range)
{
    __block CGRect result = CGRectNull;
    __block bool done = false;
    [m_webView _requestRectForFoundTextRange:range completionHandler:^(CGRect rect) {
        result = rect;
        done = true;
    }];
    Util::run(&done);
    return result;
}

} // namespace TestWebKitAPI

#endif // HAVE(UIFINDINTERACTION)
