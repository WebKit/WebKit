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
#import "Helpers/cocoa/CGImagePixelReader.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>

static bool placeholderSnapshotDidEnterFullscreen;
static bool placeholderSnapshotDidExitFullscreen;
static bool placeholderSnapshotDidRequestPresentingViewController;
static unsigned placeholderSnapshotCallCount;

static void swizzledPresentViewControllerForPlaceholderSnapshotTest(UIViewController *, SEL, UIViewController *, BOOL, dispatch_block_t completion)
{
    RunLoop::mainSingleton().dispatch([completion = makeBlockPtr(completion)] {
        if (completion)
            completion();
    });
}

static void swizzledDismissViewControllerForPlaceholderSnapshotTest(UIViewController *, SEL, BOOL, dispatch_block_t completion)
{
    RunLoop::mainSingleton().dispatch([completion = makeBlockPtr(completion)] {
        if (completion)
            completion();
    });
}

using SnapshotCompletionHandler = void (^)(CGImageRef);

static void swizzledSnapshotRectForPlaceholderSnapshotTest(WKWebView *, SEL, BOOL, CGRect rectInViewCoordinates, CGFloat imageWidth, SnapshotCompletionHandler completionHandler)
{
    static constexpr std::array<std::array<CGFloat, 3>, 3> colors { {
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 },
    } };
    auto& color = colors[placeholderSnapshotCallCount++ % colors.size()];

    size_t width = imageWidth;
    size_t height = imageWidth * CGRectGetHeight(rectInViewCoordinates) / CGRectGetWidth(rectInViewCoordinates);
    RetainPtr colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    RetainPtr context = adoptCF(CGBitmapContextCreate(nullptr, width, height, 8, 0, colorSpace.get(), kCGImageAlphaPremultipliedLast));
    CGContextSetRGBFillColor(context.get(), color[0], color[1], color[2], 1);
    CGContextFillRect(context.get(), CGRectMake(0, 0, width, height));
    RetainPtr image = adoptCF(CGBitmapContextCreateImage(context.get()));

    completionHandler(image.get());
}

@interface FullscreenPlaceholderSnapshotDelegate : NSObject <_WKFullscreenDelegate>
@end

@implementation FullscreenPlaceholderSnapshotDelegate

- (void)_webViewDidEnterElementFullscreen:(WKWebView *)webView
{
    placeholderSnapshotDidEnterFullscreen = true;
}

- (void)_webViewDidExitElementFullscreen:(WKWebView *)webView
{
    placeholderSnapshotDidExitFullscreen = true;
}

@end

@interface FullscreenPlaceholderPresentationDelegate : FullscreenPlaceholderSnapshotDelegate
- (void)answerPresentingViewControllerRequestWithError:(NSError *)error;
@end

@implementation FullscreenPlaceholderPresentationDelegate {
    BlockPtr<void(UIViewController *, NSError *)> _presentingViewControllerCompletionHandler;
}

- (void)_webView:(WKWebView *)webView requestPresentingViewControllerWithCompletionHandler:(void (^)(UIViewController *, NSError *))completionHandler
{
    _presentingViewControllerCompletionHandler = makeBlockPtr(completionHandler);
    placeholderSnapshotDidRequestPresentingViewController = true;
}

- (void)answerPresentingViewControllerRequestWithError:(NSError *)error
{
    auto completionHandler = WTF::move(_presentingViewControllerCompletionHandler);
    completionHandler(nil, error);
}

@end

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> createWebViewForPlaceholderSnapshotTest(FullscreenPlaceholderSnapshotDelegate *delegate)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration.get()]);
    [webView _setFullscreenDelegate:delegate];
    [webView synchronouslyLoadHTMLString:@"<meta name=viewport content='width=device-width, viewport-fit=cover'>"
        "<body style='margin:0'><div id=target style='width:10px; height:10px'></div></body>"];
    [webView waitForNextPresentationUpdate];
    return webView;
}

static String placeholderColorAtCenter(TestWKWebView *webView)
{
    NSObject *controller = [webView valueForKey:@"fullScreenWindowController"];
    UIView *placeholder = [controller valueForKey:@"webViewPlaceholder"];
    CGImageRef image = (__bridge CGImageRef)placeholder.layer.contents;
    if (!image)
        return { };

    CGImagePixelReader reader { image };
    return reader.cssColorAt(reader.width() / 2, reader.height() / 2);
}

TEST(FullscreenPlaceholderSnapshot, UsesSnapshotCapturedAtRequestTime)
{
    InstanceMethodSwizzler presentationSwizzler {
        UIViewController.class,
        @selector(presentViewController:animated:completion:),
        reinterpret_cast<IMP>(swizzledPresentViewControllerForPlaceholderSnapshotTest)
    };

    RetainPtr delegate = adoptNS([[FullscreenPlaceholderSnapshotDelegate alloc] init]);
    RetainPtr webView = createWebViewForPlaceholderSnapshotTest(delegate.get());

    InstanceMethodSwizzler snapshotSwizzler {
        WKWebView.class,
        @selector(_snapshotRectAfterScreenUpdates:rectInViewCoordinates:intoImageOfWidth:completionHandler:),
        reinterpret_cast<IMP>(swizzledSnapshotRectForPlaceholderSnapshotTest)
    };
    placeholderSnapshotCallCount = 0;

    placeholderSnapshotDidEnterFullscreen = false;
    placeholderSnapshotDidRequestPresentingViewController = false;
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.requestFullscreen(); document.body.style.backgroundColor = 'blue'; true"];
    Util::run(&placeholderSnapshotDidEnterFullscreen);

    EXPECT_EQ(placeholderSnapshotCallCount, 1u);
    EXPECT_WK_STREQ("rgb(255, 0, 0)", placeholderColorAtCenter(webView.get()));
}

TEST(FullscreenPlaceholderSnapshot, NotReusedAfterFailedRequest)
{
    InstanceMethodSwizzler presentationSwizzler {
        UIViewController.class,
        @selector(presentViewController:animated:completion:),
        reinterpret_cast<IMP>(swizzledPresentViewControllerForPlaceholderSnapshotTest)
    };

    RetainPtr delegate = adoptNS([[FullscreenPlaceholderSnapshotDelegate alloc] init]);
    RetainPtr webView = createWebViewForPlaceholderSnapshotTest(delegate.get());

    InstanceMethodSwizzler snapshotSwizzler {
        WKWebView.class,
        @selector(_snapshotRectAfterScreenUpdates:rectInViewCoordinates:intoImageOfWidth:completionHandler:),
        reinterpret_cast<IMP>(swizzledSnapshotRectForPlaceholderSnapshotTest)
    };
    placeholderSnapshotCallCount = 0;

    placeholderSnapshotDidEnterFullscreen = false;
    __block bool firstRequestFailed = false;
    [webView performAfterReceivingMessage:@"requestFailed" action:^{
        firstRequestFailed = true;
    }];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.requestFullscreen().catch(() => window.webkit.messageHandlers.testHandler.postMessage('requestFailed'));"
        "target.remove();"
        "true"];
    Util::run(&firstRequestFailed);
    EXPECT_FALSE(placeholderSnapshotDidEnterFullscreen);
    EXPECT_EQ(placeholderSnapshotCallCount, 1u);

    [webView objectByEvaluatingJavaScript:@"let newTarget = document.createElement('div');"
        "newTarget.id = 'newTarget';"
        "newTarget.style.width = newTarget.style.height = '10px';"
        "document.body.appendChild(newTarget);"
        "true"];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"newTarget.requestFullscreen(); document.body.style.backgroundColor = 'blue'; true"];
    Util::run(&placeholderSnapshotDidEnterFullscreen);

    EXPECT_EQ(placeholderSnapshotCallCount, 2u);
    EXPECT_WK_STREQ("rgb(0, 255, 0)", placeholderColorAtCenter(webView.get()));
}

static Vector<RetainPtr<UIView>> placeholderSnapshotViews(UIView *container)
{
    Vector<RetainPtr<UIView>> snapshotViews;
    for (UIView *subview in container.subviews) {
        if (subview.class == UIView.class && subview.layer.contents)
            snapshotViews.append(subview);
    }
    return snapshotViews;
}

TEST(FullscreenPlaceholderSnapshot, PlaceholderSnapshotViewShownUntilPlaceholderInstalled)
{
    InstanceMethodSwizzler presentationSwizzler {
        UIViewController.class,
        @selector(presentViewController:animated:completion:),
        reinterpret_cast<IMP>(swizzledPresentViewControllerForPlaceholderSnapshotTest)
    };

    RetainPtr delegate = adoptNS([[FullscreenPlaceholderPresentationDelegate alloc] init]);
    RetainPtr webView = createWebViewForPlaceholderSnapshotTest(delegate.get());
    RetainPtr container = [webView superview];

    InstanceMethodSwizzler snapshotSwizzler {
        WKWebView.class,
        @selector(_snapshotRectAfterScreenUpdates:rectInViewCoordinates:intoImageOfWidth:completionHandler:),
        reinterpret_cast<IMP>(swizzledSnapshotRectForPlaceholderSnapshotTest)
    };
    placeholderSnapshotCallCount = 0;

    placeholderSnapshotDidEnterFullscreen = false;
    placeholderSnapshotDidRequestPresentingViewController = false;
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.requestFullscreen(); document.body.style.backgroundColor = 'blue'; true"];
    Util::run(&placeholderSnapshotDidRequestPresentingViewController);
    [webView waitForNextPresentationUpdate];

    auto snapshotViews = placeholderSnapshotViews(container.get());
    ASSERT_EQ(snapshotViews.size(), 1u);
    RetainPtr snapshotView = snapshotViews[0];
    EXPECT_EQ([container.get().subviews indexOfObject:snapshotView.get()], [container.get().subviews indexOfObject:webView.get()] + 1);
    EXPECT_TRUE(CGRectEqualToRect([snapshotView frame], [webView frame]));
    EXPECT_FALSE([snapshotView isUserInteractionEnabled]);
    CGImagePixelReader reader { (__bridge CGImageRef)[snapshotView layer].contents };
    EXPECT_WK_STREQ("rgb(255, 0, 0)", reader.cssColorAt(reader.width() / 2, reader.height() / 2));

    [delegate answerPresentingViewControllerRequestWithError:nil];
    Util::run(&placeholderSnapshotDidEnterFullscreen);

    EXPECT_NULL([snapshotView superview]);
    EXPECT_TRUE(placeholderSnapshotViews(container.get()).isEmpty());
    EXPECT_WK_STREQ("rgb(255, 0, 0)", placeholderColorAtCenter(webView.get()));
}

TEST(FullscreenPlaceholderSnapshot, PlaceholderSnapshotViewRemovedAfterFailedRequest)
{
    RetainPtr delegate = adoptNS([[FullscreenPlaceholderSnapshotDelegate alloc] init]);
    RetainPtr webView = createWebViewForPlaceholderSnapshotTest(delegate.get());
    RetainPtr container = [webView superview];

    InstanceMethodSwizzler snapshotSwizzler {
        WKWebView.class,
        @selector(_snapshotRectAfterScreenUpdates:rectInViewCoordinates:intoImageOfWidth:completionHandler:),
        reinterpret_cast<IMP>(swizzledSnapshotRectForPlaceholderSnapshotTest)
    };
    placeholderSnapshotCallCount = 0;

    __block bool requestFailed = false;
    [webView performAfterReceivingMessage:@"requestFailed" action:^{
        requestFailed = true;
    }];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.requestFullscreen().catch(() => window.webkit.messageHandlers.testHandler.postMessage('requestFailed'));"
        "target.remove();"
        "true"];
    Util::run(&requestFailed);
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(placeholderSnapshotCallCount, 1u);
    EXPECT_TRUE(placeholderSnapshotViews(container.get()).isEmpty());
}

TEST(FullscreenPlaceholderSnapshot, PlaceholderSnapshotViewRemovedAfterFailedEnter)
{
    RetainPtr delegate = adoptNS([[FullscreenPlaceholderPresentationDelegate alloc] init]);
    RetainPtr webView = createWebViewForPlaceholderSnapshotTest(delegate.get());
    RetainPtr container = [webView superview];

    InstanceMethodSwizzler snapshotSwizzler {
        WKWebView.class,
        @selector(_snapshotRectAfterScreenUpdates:rectInViewCoordinates:intoImageOfWidth:completionHandler:),
        reinterpret_cast<IMP>(swizzledSnapshotRectForPlaceholderSnapshotTest)
    };
    placeholderSnapshotCallCount = 0;

    placeholderSnapshotDidEnterFullscreen = false;
    placeholderSnapshotDidRequestPresentingViewController = false;
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.requestFullscreen(); true"];
    Util::run(&placeholderSnapshotDidRequestPresentingViewController);
    EXPECT_EQ(placeholderSnapshotViews(container.get()).size(), 1u);

    [delegate answerPresentingViewControllerRequestWithError:[NSError errorWithDomain:@"FullscreenPlaceholderSnapshot" code:1 userInfo:nil]];

    EXPECT_FALSE(placeholderSnapshotDidEnterFullscreen);
    EXPECT_TRUE(placeholderSnapshotViews(container.get()).isEmpty());
}

TEST(FullscreenPlaceholderSnapshot, NoStalePlaceholderAfterExit)
{
    InstanceMethodSwizzler presentationSwizzler {
        UIViewController.class,
        @selector(presentViewController:animated:completion:),
        reinterpret_cast<IMP>(swizzledPresentViewControllerForPlaceholderSnapshotTest)
    };
    InstanceMethodSwizzler dismissalSwizzler {
        UIViewController.class,
        @selector(dismissViewControllerAnimated:completion:),
        reinterpret_cast<IMP>(swizzledDismissViewControllerForPlaceholderSnapshotTest)
    };

    RetainPtr delegate = adoptNS([[FullscreenPlaceholderPresentationDelegate alloc] init]);
    RetainPtr webView = createWebViewForPlaceholderSnapshotTest(delegate.get());

    placeholderSnapshotDidEnterFullscreen = false;
    placeholderSnapshotDidRequestPresentingViewController = false;
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.requestFullscreen(); true"];
    Util::run(&placeholderSnapshotDidRequestPresentingViewController);
    [delegate answerPresentingViewControllerRequestWithError:nil];
    Util::run(&placeholderSnapshotDidEnterFullscreen);
    EXPECT_NOT_NULL([webView _fullScreenPlaceholderView]);

    placeholderSnapshotDidExitFullscreen = false;
    [webView objectByEvaluatingJavaScript:@"document.exitFullscreen(); true"];
    Util::run(&placeholderSnapshotDidExitFullscreen);
    [webView waitForNextPresentationUpdate];
    EXPECT_NULL([webView _fullScreenPlaceholderView]);

    placeholderSnapshotDidRequestPresentingViewController = false;
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.requestFullscreen(); true"];
    Util::run(&placeholderSnapshotDidRequestPresentingViewController);
    EXPECT_TRUE([webView _isInFullscreen]);
    EXPECT_NULL([webView _fullScreenPlaceholderView]);

    [delegate answerPresentingViewControllerRequestWithError:[NSError errorWithDomain:@"FullscreenPlaceholderSnapshot" code:1 userInfo:nil]];
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY) && ENABLE(FULLSCREEN_API)
