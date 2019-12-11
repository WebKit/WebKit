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
static bool contextMenuSPIRequested;
static bool willPresentCalled;
static bool willCommitCalled;
static bool previewingViewControllerCalled;
static bool previewActionItemsCalled;
static bool didEndCalled;
static bool alternateURLRequested;
static RetainPtr<NSURL> linkURL;

static RetainPtr<TestContextMenuDriver> contextMenuWebViewDriver(Class delegateClass, NSString *customHTMLString = nil)
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
    if (!customHTMLString) {
        linkURL = [NSURL URLWithString:@"http://127.0.0.1/"];
        [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<a href='%@'>This is a link</a>", linkURL.get()]];
    } else
        [webView synchronouslyLoadHTMLString:customHTMLString];
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
        return [[[UIViewController alloc] init] autorelease];
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

TEST(ContextMenu, Click)
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

TEST(ContextMenu, End)
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

@interface TestContextMenuAPIBeforeSPIUIDelegate : NSObject <WKUIDelegate>
@end

@implementation TestContextMenuAPIBeforeSPIUIDelegate

- (void)webView:(WKWebView *)webView contextMenuConfigurationForElement:(WKContextMenuElementInfo *)elementInfo completionHandler:(void(^)(UIContextMenuConfiguration * _Nullable))completionHandler
{
    contextMenuRequested = true;
    UIContextMenuContentPreviewProvider previewProvider = ^UIViewController * ()
    {
        return [[[UIViewController alloc] init] autorelease];
    };
    UIContextMenuActionProvider actionProvider = ^UIMenu *(NSArray<UIMenuElement *> *suggestedActions)
    {
        return [UIMenu menuWithTitle:@"" children:suggestedActions];
    };
    completionHandler([UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:previewProvider actionProvider:actionProvider]);
}

- (void)_webView:(WKWebView *)webView contextMenuConfigurationForElement:(WKContextMenuElementInfo *)elementInfo completionHandler:(void(^)(UIContextMenuConfiguration * _Nullable))completionHandler
{
    contextMenuSPIRequested = true;
    completionHandler(nil);
}

- (void)webView:(WKWebView *)webView contextMenuWillPresentForElement:(WKContextMenuElementInfo *)elementInfo
{
    willPresentCalled = true;
}

@end

TEST(ContextMenu, APIBeforeSPI)
{
    auto driver = contextMenuWebViewDriver([TestContextMenuAPIBeforeSPIUIDelegate class]);
    [driver begin:^(BOOL result) {
        EXPECT_TRUE(result);
        [driver clickDown];
        [driver clickUp];
    }];
    TestWebKitAPI::Util::run(&willPresentCalled);
    EXPECT_TRUE(contextMenuRequested);
    EXPECT_FALSE(contextMenuSPIRequested);
}

@interface TestContextMenuImageUIDelegate : NSObject <WKUIDelegate>
@end

@implementation TestContextMenuImageUIDelegate

- (void)_webView:(WKWebView *)webView contextMenuConfigurationForElement:(WKContextMenuElementInfo *)elementInfo completionHandler:(void(^)(UIContextMenuConfiguration * _Nullable))completionHandler
{
    contextMenuRequested = true;
    completionHandler(nil);
}

- (NSURL *)_webView:(WKWebView *)webView alternateURLFromImage:(UIImage *)image userInfo:(NSDictionary **)userInfo
{
    alternateURLRequested = true;
    return linkURL.get();
}

- (void)webView:(WKWebView *)webView contextMenuWillPresentForElement:(WKContextMenuElementInfo *)elementInfo
{
    willPresentCalled = true;
    EXPECT_TRUE([elementInfo.linkURL.absoluteString isEqualToString:[linkURL absoluteString]]);
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

TEST(ContextMenu, Image)
{
    linkURL = [NSURL URLWithString:@"http://127.0.0.1/image"];
    auto driver = contextMenuWebViewDriver([TestContextMenuImageUIDelegate class], @"<img style='width:400px;height:400px' src='data:image/png;base64,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'>");
    [driver begin:^(BOOL result) {
        EXPECT_TRUE(result);
        [driver clickDown];
        [driver clickUp];
    }];
    TestWebKitAPI::Util::run(&willPresentCalled);
    EXPECT_TRUE(contextMenuRequested);
    EXPECT_TRUE(alternateURLRequested);
    EXPECT_TRUE(willPresentCalled);
    EXPECT_FALSE(willCommitCalled);
    EXPECT_FALSE(didEndCalled);
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

TEST(ContextMenu, Legacy)
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

@interface TestContextMenuSuggestedActionsUIDelegate : NSObject <WKUIDelegate>
@end

@implementation TestContextMenuSuggestedActionsUIDelegate

- (void)webView:(WKWebView *)webView contextMenuConfigurationForElement:(WKContextMenuElementInfo *)elementInfo completionHandler:(void(^)(UIContextMenuConfiguration * _Nullable))completionHandler
{
    EXPECT_TRUE([elementInfo.linkURL.absoluteString isEqualToString:[linkURL absoluteString]]);
    contextMenuRequested = true;
    UIContextMenuContentPreviewProvider previewProvider = ^UIViewController * ()
    {
        return [[[UIViewController alloc] init] autorelease];
    };
    UIContextMenuActionProvider actionProvider = ^UIMenu *(NSArray<UIMenuElement *> *suggestedActions)
    {
        NSArray<NSString *> *expectedIdentifiers = @[
            @"WKElementActionTypeOpen",
            @"WKElementActionTypeAddToReadingList",
            @"WKElementActionTypeCopy",
            @"WKElementActionTypeShare"
        ];
        EXPECT_TRUE(expectedIdentifiers.count == suggestedActions.count);

        [suggestedActions enumerateObjectsUsingBlock:^(UIMenuElement *menuElement, NSUInteger index, BOOL *) {
            EXPECT_TRUE([menuElement isKindOfClass:[UIAction class]]);
            EXPECT_TRUE([[(UIAction *)menuElement identifier] isEqualToString:expectedIdentifiers[index]]);
        }];
        return [UIMenu menuWithTitle:@"" children:suggestedActions];
    };
    completionHandler([UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:previewProvider actionProvider:actionProvider]);
}

- (void)webView:(WKWebView *)webView contextMenuWillPresentForElement:(WKContextMenuElementInfo *)elementInfo
{
    willPresentCalled = true;
}

@end

TEST(ContextMenu, SuggestedActions)
{
    auto driver = contextMenuWebViewDriver([TestContextMenuSuggestedActionsUIDelegate class]);
    [driver begin:^(BOOL result) {
        EXPECT_TRUE(result);
        [driver clickDown];
        [driver clickUp];
    }];
    TestWebKitAPI::Util::run(&willPresentCalled);
    EXPECT_TRUE(contextMenuRequested);
    EXPECT_TRUE(willPresentCalled);
}

#endif // PLATFORM(IOS) && USE(UICONTEXTMENU)
