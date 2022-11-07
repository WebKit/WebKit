/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKActivatedElementInfo.h>
#import <WebKit/_WKElementAction.h>
#import <wtf/BlockPtr.h>

@interface ShareSheetObserver : NSObject<WKUIDelegatePrivate>
@property (nonatomic) BlockPtr<NSArray *(_WKActivatedElementInfo *, NSArray *)> presentationHandler;
@property (nonatomic) BlockPtr<void(NSArray *)> activityItemsHandler;
@end

@implementation ShareSheetObserver

- (NSArray *)_webView:(WKWebView *)webView actionsForElement:(_WKActivatedElementInfo *)element defaultActions:(NSArray<_WKElementAction *> *)defaultActions
{
    return _presentationHandler ? _presentationHandler(element, defaultActions) : defaultActions;
}

- (void)_webView:(WKWebView *)webView willShareActivityItems:(NSArray *)activityItems
{
    if (_activityItemsHandler)
        _activityItemsHandler(activityItems);
}

@end

namespace TestWebKitAPI {

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

static void showShareSheet(WKWebView *webView, ShareSheetObserver *observer, CGPoint location)
{
    __block RetainPtr<_WKElementAction> copyAction;
    __block RetainPtr<_WKActivatedElementInfo> copyElement;
    __block bool done = false;
    [observer setPresentationHandler:^(_WKActivatedElementInfo *element, NSArray *actions) {
        copyElement = element;
        for (_WKElementAction *action in actions) {
            if (action.type == _WKElementActionTypeShare)
                copyAction = action;
        }
        done = true;
        return @[ copyAction.get() ];
    }];
    [webView _simulateLongPressActionAtLocation:location];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(!!copyAction);
    EXPECT_TRUE(!!copyElement);
    [copyAction runActionWithElementInfo:copyElement.get()];
}

TEST(ShareSheetTests, ShareImgElementWithBase64URL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto observer = adoptNS([[ShareSheetObserver alloc] init]);
    [webView setUIDelegate:observer.get()];
    [webView synchronouslyLoadTestPageNamed:@"img-with-base64-url"];

    __block bool done = false;
    [observer setActivityItemsHandler:^(NSArray *activityItems) {
        EXPECT_EQ(1UL, activityItems.count);
        NSURL *url = [activityItems objectAtIndex:0];
        EXPECT_WK_STREQ("Shared Image.png", url.lastPathComponent);
        done = true;
    }];

    showShareSheet(webView.get(), observer.get(), CGPointMake(100, 100));
    TestWebKitAPI::Util::run(&done);
}

TEST(ShareSheetTests, ShareAnchorElementAsURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto observer = adoptNS([[ShareSheetObserver alloc] init]);
    [webView setUIDelegate:observer.get()];
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    __block bool done = false;
    [observer setActivityItemsHandler:^(NSArray *activityItems) {
        EXPECT_EQ(1UL, activityItems.count);
        EXPECT_WK_STREQ("https://www.apple.com/", [[activityItems objectAtIndex:0] absoluteString]);
        done = true;
    }];

    showShareSheet(webView.get(), observer.get(), CGPointMake(100, 100));
    TestWebKitAPI::Util::run(&done);
}

#endif // !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
