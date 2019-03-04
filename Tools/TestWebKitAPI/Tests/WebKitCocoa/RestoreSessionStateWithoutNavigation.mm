/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "JavaScriptTest.h"
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKBackForwardListItemRef.h>
#include <WebKit/WKBackForwardListRef.h>
#include <WebKit/WKData.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKSessionStateRef.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKURLCF.h>
#include <WebKit/WKWebViewPrivate.h>
#include <wtf/RetainPtr.h>

@interface WKWebView ()
- (WKPageRef)_pageForTesting;
@end

static bool didFinishLoad;
static bool didChangeBackForwardList;
    
@interface SessionStateDelegate : NSObject <WKNavigationDelegate>
@end

@implementation SessionStateDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    didFinishLoad = true;
}

- (void)_webView:(WKWebView *)webView backForwardListItemAdded:(WKBackForwardListItem *)itemAdded removed:(NSArray<WKBackForwardListItem *> *)itemsRemoved
{
    didChangeBackForwardList = true;
}

@end

namespace TestWebKitAPI {

static WKRetainPtr<WKDataRef> createSessionStateData()
{
    auto delegate = adoptNS([SessionStateDelegate new]);
    auto view = adoptNS([WKWebView new]);
    [view setNavigationDelegate:delegate.get()];
    [view loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    NSData *data = [view _sessionStateData];
    return adoptWK(WKDataCreate(static_cast<const unsigned char*>(data.bytes), data.length));
}

TEST(WebKit, RestoreSessionStateWithoutNavigation)
{
    auto data = createSessionStateData();
    EXPECT_NOT_NULL(data);

    auto webView = adoptNS([WKWebView new]);
    auto sessionState = adoptWK(WKSessionStateCreateFromData(data.get()));
    WKPageRestoreFromSessionStateWithoutNavigation([webView _pageForTesting], sessionState.get());

    Util::run(&didChangeBackForwardList);

    WKRetainPtr<WKURLRef> committedURL = adoptWK(WKPageCopyCommittedURL([webView _pageForTesting]));
    EXPECT_NULL(committedURL.get());

    auto backForwardList = WKPageGetBackForwardList([webView _pageForTesting]);
    auto currentItem = WKBackForwardListGetCurrentItem(backForwardList);
    auto currentItemURL = adoptWK(WKBackForwardListItemCopyURL(currentItem));
    
    auto expectedURL = adoptWK(WKURLCreateWithCFURL((__bridge CFURLRef)[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]));
    EXPECT_NOT_NULL(expectedURL);
    EXPECT_TRUE(WKURLIsEqual(currentItemURL.get(), expectedURL.get()));
}

} // namespace TestWebKitAPI

#endif
