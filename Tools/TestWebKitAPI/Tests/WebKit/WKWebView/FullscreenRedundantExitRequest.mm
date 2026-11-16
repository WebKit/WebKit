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

#if PLATFORM(IOS_FAMILY) && ENABLE(FULLSCREEN_API)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/StdLibExtras.h>


@protocol WKFullScreenWindowControllerTestingSPI <NSObject>
@property (readonly, assign, nonatomic) BOOL isFullScreen;
@property (readonly, retain, nonatomic) UIView *webViewPlaceholder;
- (void)exitFullScreen:(WTF::CompletionHandler<void()>&&)completionHandler;
@end

@interface WKWebView (FullscreenRedundantExitRequestTestingSPI)
- (id<WKFullScreenWindowControllerTestingSPI>)fullScreenWindowController;
@end

static bool redundantExitDidEnterFullscreen;
static bool redundantExitDidStartDismissal;
static BlockPtr<void()> redundantExitDismissalCompletion;

static void swizzledPresentViewControllerForRedundantExitTest(UIViewController *, SEL, UIViewController *, BOOL, dispatch_block_t completion)
{
    RunLoop::mainSingleton().dispatch([completion = makeBlockPtr(completion)] {
        if (completion)
            completion();
    });
}

static void swizzledDismissViewControllerForRedundantExitTest(UIViewController *, SEL, BOOL, dispatch_block_t completion)
{
    redundantExitDismissalCompletion = makeBlockPtr(completion);
    redundantExitDidStartDismissal = true;
}

@interface FullscreenRedundantExitRequestDelegate : NSObject <_WKFullscreenDelegate>
@end

@implementation FullscreenRedundantExitRequestDelegate

- (void)_webViewDidEnterElementFullscreen:(WKWebView *)webView
{
    redundantExitDidEnterFullscreen = true;
}

@end

namespace TestWebKitAPI {

TEST(Fullscreen, RedundantExitRequestWhileExitingStillCompletes)
{
    InstanceMethodSwizzler presentationSwizzler {
        UIViewController.class,
        @selector(presentViewController:animated:completion:),
        reinterpret_cast<IMP>(swizzledPresentViewControllerForRedundantExitTest)
    };

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;

    RetainPtr delegate = adoptNS([[FullscreenRedundantExitRequestDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration.get()]);
    [webView _setFullscreenDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<meta name=viewport content=\"width=device-width, viewport-fit=cover\">"
        "<div id=target style=\"width:100px;height:100px;background-color:red;\"></div>"];

    redundantExitDidEnterFullscreen = false;
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.requestFullscreen(); true"];
    ASSERT_TRUE(Util::runFor(&redundantExitDidEnterFullscreen, 5_s));

    RetainPtr controller = [webView fullScreenWindowController];
    ASSERT_TRUE([controller isFullScreen]);

    auto dismissalSwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        UIViewController.class,
        @selector(dismissViewControllerAnimated:completion:),
        reinterpret_cast<IMP>(swizzledDismissViewControllerForRedundantExitTest));

    redundantExitDidStartDismissal = false;
    redundantExitDismissalCompletion = nil;

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.exitFullscreen(); true"];
    ASSERT_TRUE(Util::runFor(&redundantExitDidStartDismissal, 5_s));
    ASSERT_TRUE(!!redundantExitDismissalCompletion);

    [controller exitFullScreen:[] { }];

    redundantExitDismissalCompletion();
    redundantExitDismissalCompletion = nil;
    Util::runFor(0.5_s);

    EXPECT_FALSE([controller isFullScreen]);
    EXPECT_NULL([controller webViewPlaceholder].superview);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY) && ENABLE(FULLSCREEN_API)
