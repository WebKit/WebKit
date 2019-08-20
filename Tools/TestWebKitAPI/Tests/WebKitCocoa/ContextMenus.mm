/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS) && USE(UICONTEXTMENU)

#import "TestContextMenuDriver.h"
#import "TestWKWebView.h"
#import "TestWKWebViewController.h"
#import "Utilities.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>

static bool contextMenuRequested;
static bool willPresentCalled;
static bool willCommitCalled;
static bool previewingViewControllerCalled;
static bool previewActionItemsCalled;
static bool didEndCalled;
static RetainPtr<NSURL> linkURL;

static RetainPtr<TestContextMenuDriver> contextMenuWebViewDriver(Class delegateClass)
{
    static auto window = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    static auto driver = adoptNS([TestContextMenuDriver new]);
    static auto uiDelegate = adoptNS((NSObject<WKUIDelegate> *)[delegateClass new]);
    static auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration _setClickInteractionDriverForTesting:driver.get()];
    static auto webViewController = adoptNS([[TestWKWebViewController alloc] initWithFrame:CGRectMake(0, 0, 200, 200) configuration:configuration.get()]);
    TestWKWebView *webView = [webViewController webView];
    [window addSubview:webView];
    [webView setUIDelegate:uiDelegate.get()];
    linkURL = [NSURL URLWithString:@"http://127.0.0.1/"];
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<a href='%@'>This is a link</a>", linkURL.get()]];
    return driver;
}

@interface TestContextMenuUIDelegate : NSObject <WKUIDelegate>
@end

@implementation TestContextMenuUIDelegate

- (void)webView:(WKWebView *)webView contextMenuConfigurationForElement:(WKContextMenuElementInfo *)elementInfo completionHandler:(void(^)(UIContextMenuConfiguration * _Nullable))completionHandler
{
    EXPECT_TRUE([elementInfo.linkURL.absoluteString isEqualToString:[linkURL absoluteString]]);
    contextMenuRequested = true;
    UIContextMenuContentPreviewProvider previewProvider = ^UIViewController * ()
    {
        return [UIViewController new];
    };
    UIContextMenuActionProvider actionProvider = ^UIMenu *(NSArray<UIMenuElement *> *suggestedActions)
    {
        return [UIMenu menuWithTitle:@"" children:suggestedActions];
    };
    completionHandler([UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:previewProvider actionProvider:actionProvider]);
}

- (void)webView:(WKWebView *)webView contextMenuWillPresentForElement:(WKContextMenuElementInfo *)elementInfo
{
    willPresentCalled = true;
}

- (void)webView:(WKWebView *)webView contextMenuForElement:(WKContextMenuElementInfo *)elementInfo willCommitWithAnimator:(id<UIContextMenuInteractionCommitAnimating>)animator
{
    willCommitCalled = true;
}

- (void)webView:(WKWebView *)webView contextMenuDidEndForElement:(WKContextMenuElementInfo *)elementInfo
{
    didEndCalled = true;
}

@end

TEST(WebKit, ContextMenuClick)
{
    auto driver = contextMenuWebViewDriver([TestContextMenuUIDelegate class]);
    [driver begin:^(BOOL result) {
        EXPECT_TRUE(result);
        [driver clickDown];
        [driver clickUp];
    }];
    TestWebKitAPI::Util::run(&willPresentCalled);
    EXPECT_TRUE(contextMenuRequested);
    EXPECT_TRUE(willPresentCalled);
    EXPECT_FALSE(willCommitCalled);
    EXPECT_FALSE(didEndCalled);
}

TEST(WebKit, ContextMenuEnd)
{
    auto driver = contextMenuWebViewDriver([TestContextMenuUIDelegate class]);
    [driver begin:^(BOOL result) {
        EXPECT_TRUE(result);
        [driver end];
    }];
    TestWebKitAPI::Util::run(&didEndCalled);
    EXPECT_TRUE(contextMenuRequested);
    EXPECT_FALSE(willPresentCalled);
    EXPECT_FALSE(willCommitCalled);
    EXPECT_TRUE(didEndCalled);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-implementations"

@interface LegacyPreviewViewController : UIViewController
@end

@implementation LegacyPreviewViewController

- (NSArray<UIPreviewAction *> *)previewActionItems
{
    previewActionItemsCalled = true;
    return @[
        [UIPreviewAction actionWithTitle:@"Action" style:UIPreviewActionStyleDefault handler:^(UIPreviewAction *, UIViewController *) { }]
    ];
}

@end

@interface LegacyContextMenuUIDelegate : NSObject <WKUIDelegate>
@end

@implementation LegacyContextMenuUIDelegate

- (BOOL)webView:(WKWebView *)webView shouldPreviewElement:(WKPreviewElementInfo *)elementInfo
{
    EXPECT_TRUE([elementInfo.linkURL.absoluteString isEqualToString:[linkURL absoluteString]]);
    contextMenuRequested = true;
    return YES;
}

- (UIViewController *)webView:(WKWebView *)webView previewingViewControllerForElement:(WKPreviewElementInfo *)elementInfo defaultActions:(NSArray<id <WKPreviewActionItem>> *)previewActions
{
    EXPECT_TRUE(previewActions.count);
    previewingViewControllerCalled = true;
    return [LegacyPreviewViewController new];
}

/* Even though this is non-legacy API, it should not be enough to trigger the non-legacy flow. */
- (void)webView:(WKWebView *)webView contextMenuWillPresentForElement:(WKContextMenuElementInfo *)elementInfo
{
    willPresentCalled = true;
}

/* Even though this is non-legacy API, it should not be enough to trigger the non-legacy flow. */
- (void)_webView:(WKWebView *)webView contextMenuDidEndForElement:(WKContextMenuElementInfo *)elementInfo
{
}

@end

TEST(WebKit, ContextMenuLegacy)
{
    auto driver = contextMenuWebViewDriver([LegacyContextMenuUIDelegate class]);
    [driver begin:^(BOOL result) {
        EXPECT_TRUE(result);
        [driver clickDown];
        [driver clickUp];
    }];
    TestWebKitAPI::Util::run(&previewActionItemsCalled);
    EXPECT_TRUE(contextMenuRequested);
    EXPECT_TRUE(previewingViewControllerCalled);
    EXPECT_TRUE(willPresentCalled);
}

#pragma clang diagnostic pop


#endif // PLATFORM(IOS) && USE(UICONTEXTMENU)
