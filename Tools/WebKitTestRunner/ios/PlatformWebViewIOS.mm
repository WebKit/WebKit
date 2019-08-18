/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "PlatformWebView.h"

#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKImageCG.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKSnapshotConfiguration.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RetainPtr.h>

@interface WKWebView (Details)
- (WKPageRef)_pageForTesting;
@end

@interface WebKitTestRunnerWindow : UIWindow {
    WTR::PlatformWebView* _platformWebView;
    CGPoint _fakeOrigin;
    BOOL _initialized;
}
@property (nonatomic) WTR::PlatformWebView* platformWebView;
@end

static Vector<WebKitTestRunnerWindow *> allWindows;

@implementation WebKitTestRunnerWindow
@synthesize platformWebView = _platformWebView;

- (id)initWithFrame:(CGRect)frame
{
    allWindows.append(self);

    if ((self = [super initWithFrame:frame]))
        _initialized = YES;

    return self;
}

- (void)dealloc
{
    allWindows.removeFirst(self);
    ASSERT(!allWindows.contains(self));
    [super dealloc];
}

- (BOOL)isKeyWindow
{
    return [super isKeyWindow] && (_platformWebView ? _platformWebView->windowIsKey() : YES);
}

- (void)setFrameOrigin:(CGPoint)point
{
    _fakeOrigin = point;
}

- (void)setFrame:(CGRect)windowFrame
{
    if (!_initialized) {
        [super setFrame:windowFrame];
        return;
    }

    CGRect currentFrame = [super frame];

    _fakeOrigin = windowFrame.origin;

    [super setFrame:CGRectMake(currentFrame.origin.x, currentFrame.origin.y, windowFrame.size.width, windowFrame.size.height)];
}

- (CGRect)frameRespectingFakeOrigin
{
    CGRect currentFrame = [self frame];
    return CGRectMake(_fakeOrigin.x, _fakeOrigin.y, currentFrame.size.width, currentFrame.size.height);
}

- (CGFloat)backingScaleFactor
{
    return 1;
}

@end

namespace WTR {

enum class WebViewSizingMode {
    Default,
    HeightRespectsStatusBar
};

static CGRect viewRectForWindowRect(CGRect, PlatformWebView::WebViewSizingMode);

} // namespace WTR

@interface PlatformWebViewController : UIViewController
@end

@implementation PlatformWebViewController

- (void)viewWillTransitionToSize:(CGSize)toSize withTransitionCoordinator:(id <UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:toSize withTransitionCoordinator:coordinator];

    TestRunnerWKWebView *webView = WTR::TestController::singleton().mainWebView()->platformView();

    if (webView.usesSafariLikeRotation)
        [webView _setInterfaceOrientationOverride:[[UIApplication sharedApplication] statusBarOrientation]];

    [coordinator animateAlongsideTransition: ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        // This code assumes that we should take the status bar into account, which we only do for flexible viewport tests,
        // but it only makes sense to test rotation with a flexible viewport anyway.
        if (webView.usesSafariLikeRotation) {
            [webView _beginAnimatedResizeWithUpdates:^{
                webView.frame = viewRectForWindowRect(self.view.bounds, WTR::PlatformWebView::WebViewSizingMode::HeightRespectsStatusBar);
                [webView _overrideLayoutParametersWithMinimumLayoutSize:webView.frame.size maximumUnobscuredSizeOverride:webView.frame.size];
                [webView _setInterfaceOrientationOverride:[[UIApplication sharedApplication] statusBarOrientation]];
            }];
        } else
            webView.frame = viewRectForWindowRect(self.view.bounds, WTR::PlatformWebView::WebViewSizingMode::HeightRespectsStatusBar);
    } completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        webView.frame = viewRectForWindowRect(self.view.bounds, WTR::PlatformWebView::WebViewSizingMode::HeightRespectsStatusBar);
        if (webView.usesSafariLikeRotation)
            [webView _endAnimatedResize];

        [webView _didEndRotation];
    }];
}

@end

namespace WTR {

static CGRect viewRectForWindowRect(CGRect windowRect, PlatformWebView::WebViewSizingMode mode)
{
    CGFloat statusBarBottom = CGRectGetMaxY([[UIApplication sharedApplication] statusBarFrame]);
    return CGRectMake(windowRect.origin.x, windowRect.origin.y + statusBarBottom, windowRect.size.width, windowRect.size.height - (mode == PlatformWebView::WebViewSizingMode::HeightRespectsStatusBar ? statusBarBottom : 0));
}

PlatformWebView::PlatformWebView(WKWebViewConfiguration* configuration, const TestOptions& options)
    : m_windowIsKey(true)
    , m_options(options)
{
    CGRect rect = CGRectMake(0, 0, TestController::viewWidth, TestController::viewHeight);

    m_window = [[WebKitTestRunnerWindow alloc] initWithFrame:rect];
    m_window.backgroundColor = [UIColor lightGrayColor];
    m_window.platformWebView = this;

    UIViewController *viewController = [[PlatformWebViewController alloc] init];
    [m_window setRootViewController:viewController];
    [viewController release];

    m_view = [[TestRunnerWKWebView alloc] initWithFrame:viewRectForWindowRect(rect, WebViewSizingMode::Default) configuration:configuration];

    [m_window.rootViewController.view addSubview:m_view];
    [m_view becomeFirstResponder];
    [m_window makeKeyAndVisible];
}

PlatformWebView::~PlatformWebView()
{
    m_window.platformWebView = nil;
    [m_view release];
    [m_window release];
}

PlatformWindow PlatformWebView::keyWindow()
{
    size_t i = allWindows.size();
    while (i) {
        if ([allWindows[i] isKeyWindow])
            return allWindows[i];
        --i;
    }

    return nil;
}

void PlatformWebView::setWindowIsKey(bool isKey)
{
    m_windowIsKey = isKey;
    if (isKey)
        [m_window makeKeyWindow];
}

void PlatformWebView::addToWindow()
{
    [m_window.rootViewController.view addSubview:m_view];
}

void PlatformWebView::removeFromWindow()
{
    [m_view removeFromSuperview];
}

void PlatformWebView::resizeTo(unsigned width, unsigned height, WebViewSizingMode viewSizingMode)
{
    WKRect frame = windowFrame();
    frame.size.width = width;
    frame.size.height = height;
    setWindowFrame(frame, viewSizingMode);
}

WKPageRef PlatformWebView::page()
{
    return [m_view _pageForTesting];
}

void PlatformWebView::focus()
{
    makeWebViewFirstResponder();
    setWindowIsKey(true);
}

WKRect PlatformWebView::windowFrame()
{
    CGRect frame = [m_window frameRespectingFakeOrigin];

    WKRect wkFrame;
    wkFrame.origin.x = frame.origin.x;
    wkFrame.origin.y = frame.origin.y;
    wkFrame.size.width = frame.size.width;
    wkFrame.size.height = frame.size.height;
    return wkFrame;
}

void PlatformWebView::setWindowFrame(WKRect frame, WebViewSizingMode viewSizingMode)
{
    [m_window setFrame:CGRectMake(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height)];
    [platformView() setFrame:viewRectForWindowRect(CGRectMake(0, 0, frame.size.width, frame.size.height), viewSizingMode)];
}

void PlatformWebView::didInitializeClients()
{
    // Set a temporary 1x1 window frame to force a WindowAndViewFramesChanged notification. <rdar://problem/13380145>
    WKRect wkFrame = windowFrame();
    [m_window setFrame:CGRectMake(0, 0, 1, 1)];
    setWindowFrame(wkFrame);
}

void PlatformWebView::addChromeInputField()
{
    UITextField* textField = [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 100, 20)];
    textField.tag = 1;
    [m_window addSubview:textField];
    [textField release];
}

void PlatformWebView::removeChromeInputField()
{
    UITextField* textField = (UITextField*)[m_window viewWithTag:1];
    if (textField) {
        [textField removeFromSuperview];
        makeWebViewFirstResponder();
        [textField release];
    }
}

void PlatformWebView::makeWebViewFirstResponder()
{
    [m_view becomeFirstResponder];
}

void PlatformWebView::changeWindowScaleIfNeeded(float)
{
    // Retina only surface.
}

void PlatformWebView::setEditable(bool editable)
{
    m_view._editable = editable;
}

bool PlatformWebView::drawsBackground() const
{
    return false;
}

void PlatformWebView::setDrawsBackground(bool)
{
}

#if !HAVE(IOSURFACE)
static void releaseDataProviderData(void* info, const void*, size_t)
{
    CARenderServerDestroyBuffer(static_cast<CARenderServerBufferRef>(info));
}
#endif

RetainPtr<CGImageRef> PlatformWebView::windowSnapshotImage()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    CGSize viewSize = m_view.bounds.size;
    RELEASE_ASSERT(viewSize.width);
    RELEASE_ASSERT(viewSize.height);

#if HAVE(IOSURFACE)
    __block bool isDone = false;
    __block RetainPtr<CGImageRef> result;
    
    RetainPtr<WKSnapshotConfiguration> snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setRect:CGRectMake(0, 0, viewSize.width, viewSize.height)];
    [snapshotConfiguration setSnapshotWidth:@(viewSize.width)];
    
    [m_view takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(UIImage *snapshotImage, NSError *error) {
        RELEASE_ASSERT(!error);
        RELEASE_ASSERT(snapshotImage);
        RELEASE_ASSERT(snapshotImage.size.width);
        RELEASE_ASSERT(snapshotImage.size.height);
        if (!error)
            result = [snapshotImage CGImage];
        isDone = true;
    }];
    while (!isDone)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
    return result;

#else
    CGFloat deviceScaleFactor = 2; // FIXME: hardcode 2x for now. In future we could respect 1x and 3x as we do on Mac.
    CATransform3D transform = CATransform3DMakeScale(deviceScaleFactor, deviceScaleFactor, 1);
    
    int bufferWidth = ceil(viewSize.width * deviceScaleFactor);
    int bufferHeight = ceil(viewSize.height * deviceScaleFactor);

    CARenderServerBufferRef buffer = CARenderServerCreateBuffer(bufferWidth, bufferHeight);
    RELEASE_ASSERT(buffer);

    CARenderServerRenderLayerWithTransform(MACH_PORT_NULL, m_view.layer.context.contextId, reinterpret_cast<uint64_t>(m_view.layer), buffer, 0, 0, &transform);

    uint8_t* data = CARenderServerGetBufferData(buffer);
    size_t rowBytes = CARenderServerGetBufferRowBytes(buffer);

    static CGColorSpaceRef sRGBSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateWithData(buffer, data, CARenderServerGetBufferDataSize(buffer), releaseDataProviderData));
    
    RetainPtr<CGImageRef> cgImage = adoptCF(CGImageCreate(bufferWidth, bufferHeight, 8, 32, rowBytes, sRGBSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host, provider.get(), 0, false, kCGRenderingIntentDefault));
    RELEASE_ASSERT(cgImage);

    return cgImage;
#endif
    END_BLOCK_OBJC_EXCEPTIONS;
}

void PlatformWebView::setNavigationGesturesEnabled(bool enabled)
{
    [platformView() setAllowsBackForwardNavigationGestures:enabled];
}

} // namespace WTR
