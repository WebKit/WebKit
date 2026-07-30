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

#if ENABLE(QUICKLOOK_FULLSCREEN)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

static void swizzledPresentViewController(UIViewController *, SEL, UIViewController *, BOOL, dispatch_block_t completion)
{
    RunLoop::mainSingleton().dispatch([completion = makeBlockPtr(completion)] {
        if (completion)
            completion();
    });
}

static void swizzledPresentWindow(id, SEL, dispatch_block_t completion)
{
    RunLoop::mainSingleton().dispatch([completion = makeBlockPtr(completion)] {
        if (completion)
            completion();
    });
}

static void swizzledDismissWindow(id, SEL, dispatch_block_t completion)
{
    RunLoop::mainSingleton().dispatch([completion = makeBlockPtr(completion)] {
        if (completion)
            completion();
    });
}

@interface QuickLookFullscreenDelegate : NSObject <_WKFullscreenDelegate>
@end

@implementation QuickLookFullscreenDelegate {
    bool _didFullscreenImageWithQuickLook;
}

- (void)_webView:(WKWebView *)webView didFullscreenImageWithQuickLook:(CGSize)imageDimensions
{
    _didFullscreenImageWithQuickLook = true;
}

- (void)waitForDidFullscreenImageWithQuickLook
{
    TestWebKitAPI::Util::run(&_didFullscreenImageWithQuickLook);
}

@end

namespace TestWebKitAPI {

static RetainPtr<NSString> spatialImageHTML()
{
    RetainPtr data = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"spatial" withExtension:@"heic"]];
    RetainPtr base64 = [data base64EncodedStringWithOptions:0];
    return adoptNS([[NSString alloc] initWithFormat:@"<img id='image' src='data:image/heic;base64,%@'>", base64.get()]);
}

TEST(QuickLookFullscreen, DidFullscreenImageWithQuickLook)
{
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    InstanceMethodSwizzler presentViewControllerSwizzler {
        UIViewController.class,
        @selector(presentViewController:animated:completion:),
        reinterpret_cast<IMP>(swizzledPresentViewController)
    };

    Class previewWindowControllerClass = NSClassFromString(@"WKPreviewWindowController");
    ASSERT_NE(previewWindowControllerClass, nullptr);

    InstanceMethodSwizzler presentWindowSwizzler {
        previewWindowControllerClass,
        NSSelectorFromString(@"presentWindowWithCompletionHandler:"),
        reinterpret_cast<IMP>(swizzledPresentWindow)
    };

    InstanceMethodSwizzler dismissWindowSwizzler {
        previewWindowControllerClass,
        NSSelectorFromString(@"dismissWindowWithCompletionHandler:"),
        reinterpret_cast<IMP>(swizzledDismissWindow)
    };

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;

    RetainPtr delegate = adoptNS([[QuickLookFullscreenDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration.get()]);
    [webView _setFullscreenDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:spatialImageHTML().get()];
    [webView waitForNextPresentationUpdate];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.getElementById('image').requestFullscreen()"];

    [delegate waitForDidFullscreenImageWithQuickLook];
}

} // namespace TestWebKitAPI

#endif // ENABLE(QUICKLOOK_FULLSCREEN)
