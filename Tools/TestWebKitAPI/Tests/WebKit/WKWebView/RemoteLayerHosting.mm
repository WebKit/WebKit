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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/UISideCompositingScope.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if USE(EXTENSIONKIT)
#import <BrowserEngineKit/BELayerHierarchyHostingView.h>
#endif

namespace TestWebKitAPI {

// WKVideoLayerHost is internal to WebKit, so it is identified by name here rather than by
// importing its header. It derives from CALayerHost, so a search for CALayerHost finds the
// video host layer on both the new and the legacy hosting path.
static NSString * const videoLayerHostClassName = @"WKVideoLayerHost";

template<typename Functor> static void forEachLayer(CALayer *layer, const Functor& functor)
{
    functor(layer);
    for (CALayer *sublayer in layer.sublayers)
        forEachLayer(sublayer, functor);
}

static void appendLayerDescription(NSMutableString *description, CALayer *layer, unsigned depth)
{
    [description appendString:[@"" stringByPaddingToLength:depth * 2 withString:@" " startingAtIndex:0]];
    [description appendString:NSStringFromClass([layer class])];
    if (layer.name.length)
        [description appendFormat:@" name=\"%@\"", layer.name];
    [description appendFormat:@" bounds=(%g, %g)", layer.bounds.size.width, layer.bounds.size.height];
    // -frame accounts for -bounds, -position and -affineTransform, so it can differ from
    // -bounds when WebAVPlayerLayer has scaled a layer rather than resizing it.
    [description appendFormat:@" frame=(%g, %g)", layer.frame.size.width, layer.frame.size.height];
    if (!CGAffineTransformIsIdentity(layer.affineTransform))
        [description appendFormat:@" scale=(%g, %g)", layer.affineTransform.a, layer.affineTransform.d];
    if ([layer isKindOfClass:[CALayerHost class]])
        [description appendFormat:@" contextId=%u", static_cast<CALayerHost *>(layer).contextId];
    [description appendString:@"\n"];

    for (CALayer *sublayer in layer.sublayers)
        appendLayerDescription(description, sublayer, depth + 1);
}

static NSString *layerTreeDescription(CALayer *layer)
{
    RetainPtr description = adoptNS([[NSMutableString alloc] init]);
    appendLayerDescription(description.get(), layer, 0);
    return description.autorelease();
}

static RetainPtr<CALayer> findLayerOfClassNamed(CALayer *root, NSString *className)
{
    Class layerClass = NSClassFromString(className);
    if (!layerClass)
        return nil;

    RetainPtr<CALayer> result;
    forEachLayer(root, [&](CALayer *layer) {
        if (!result && [layer isKindOfClass:layerClass])
            result = layer;
    });
    return result;
}

// The layer hosting the GPU process video layer, whichever hosting path created it: it is
// the CALayerHost parented into the WebAVPlayerLayer that represents the video element.
static RetainPtr<CALayerHost> findVideoHostLayer(CALayer *root)
{
    RetainPtr playerLayer = findLayerOfClassNamed(root, @"WebAVPlayerLayer");
    if (!playerLayer)
        return nil;

    for (CALayer *sublayer in [playerLayer sublayers]) {
        if ([sublayer isKindOfClass:[CALayerHost class]])
            return static_cast<CALayerHost *>(sublayer);
    }
    return nil;
}

#if USE(EXTENSIONKIT)
// Under BrowserEngineKit the remote hierarchy is adopted by a BELayerHierarchyHostingView
// rather than by setting a context ID, so that is where hosting has to be observed. The layer
// host view is never added to a view hierarchy — only its layer is parented — so it is
// reached through its backing layer's delegate.
static RetainPtr<BELayerHierarchyHostingView> findLayerHierarchyHostingView(CALayer *videoHostLayer)
{
    RetainPtr view = dynamic_objc_cast<UIView>([videoHostLayer delegate]);
    for (UIView *subview in [view subviews]) {
        if ([subview isKindOfClass:[BELayerHierarchyHostingView class]])
            return static_cast<BELayerHierarchyHostingView *>(subview);
    }
    return nil;
}
#endif

#if PLATFORM(MAC)
static RetainPtr<CALayer> findCaptionsLayer(CALayer *root)
{
    RetainPtr<CALayer> result;
    forEachLayer(root, [&](CALayer *layer) {
        if (!result && [layer.name isEqualToString:@"Captions layer"])
            result = layer;
    });
    return result;
}
#endif

static RetainPtr<TestWKWebView> createWebViewAndStartPlayback(bool remoteLayerHostingBypassesWebContentProcess)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
#if PLATFORM(IOS_FAMILY)
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
#endif

    // Navigating away must actually destroy the media elements, rather than parking the page
    // in the back/forward cache with its players still alive.
    [[configuration preferences] _setUsesPageCache:NO];

    WKPreferencesRef preferences = (__bridge WKPreferencesRef)[configuration preferences];
    WKPreferencesSetBoolValueForKeyForTesting(preferences, true, adoptWK(WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled")).get());
    WKPreferencesSetBoolValueForKeyForTesting(preferences, remoteLayerHostingBypassesWebContentProcess, adoptWK(WKStringCreateWithUTF8CString("RemoteLayerHostingBypassesWebContentProcess")).get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get() addToWindow:YES]);

    __block bool isPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ isPlaying = true; }];
    [webView synchronouslyLoadTestPageNamed:@"video-with-audio"];
    Util::run(&isPlaying);

    // The video layer is created in response to the media player reporting that it has
    // begun rendering, which happens after playback starts. Wait for the layer tree
    // containing it to be committed to this process.
    [webView waitForNextPresentationUpdate];

    return webView;
}

// WebAVPlayerLayer resolves the video sublayer's geometry asynchronously: -layoutSublayers
// applies a transform and schedules -resolveBounds, which does the final -setFrame:. Poll
// until the host layer's frame stops changing so that geometry comparisons are not made
// against an intermediate state.
//
// The frame, rather than the bounds, is the comparable quantity across the two hosting
// paths: the legacy path leaves a scale transform on the layer, so its bounds remain the
// element's default size while its frame reflects the size the video is presented at.
static CGSize waitForStableVideoHostLayerFrameSize(TestWKWebView *webView)
{
    constexpr unsigned requiredStableSamples = 3;
    constexpr unsigned maximumSamples = 100;

    CGSize previousSize = CGSizeMake(-1, -1);
    unsigned stableSamples = 0;

    for (unsigned sample = 0; sample < maximumSamples; ++sample) {
        [webView waitForNextPresentationUpdate];

        RetainPtr hostLayer = findVideoHostLayer([webView layer]);
        CGSize currentSize = hostLayer ? [hostLayer frame].size : CGSizeZero;

        if (!CGSizeEqualToSize(currentSize, CGSizeZero) && CGSizeEqualToSize(currentSize, previousSize)) {
            if (++stableSamples >= requiredStableSamples)
                return currentSize;
        } else
            stableSamples = 0;

        previousSize = currentSize;
        Util::runFor(0.05_s);
    }

    return previousSize;
}

#pragma mark - Layer hosting path

// Verifies that starting video playback with RemoteLayerHostingBypassesWebContentProcess
// enabled produces a WKVideoLayerHost in the UI process layer tree, hosting a context
// supplied by the GPU process.
TEST(RemoteLayerHosting, VideoLayerIsHostedDirectlyFromGPUProcess)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    ASSERT_TRUE(!!NSClassFromString(videoLayerHostClassName));

    RetainPtr webView = createWebViewAndStartPlayback(true);
    RetainPtr rootLayer = [webView layer];

    NSLog(@"Layer tree with remote layer hosting enabled:\n%@", layerTreeDescription(rootLayer.get()));

    RetainPtr videoLayerHost = findLayerOfClassNamed(rootLayer.get(), videoLayerHostClassName);
    EXPECT_NOT_NULL(videoLayerHost.get());
    if (!videoLayerHost)
        return;

    // Hosting must actually have been established, rather than the view merely having been
    // created: DidCreateRemoteLayer has to have arrived and been applied. How that shows up
    // differs by platform, because BrowserEngineKit adopts a layer hierarchy from a handle
    // instead of exposing a usable CA context ID.
#if USE(EXTENSIONKIT)
    RetainPtr hostingView = findLayerHierarchyHostingView(videoLayerHost.get());
    EXPECT_NOT_NULL(hostingView.get());
    EXPECT_NOT_NULL([hostingView handle]);
#else
    EXPECT_NE(0u, static_cast<CALayerHost *>(videoLayerHost.get()).contextId);
#endif
}

// The counterpart of the test above: with the setting disabled, the video layer must still
// be hosted the old way, through a plain CALayerHost created by VideoPresentationManagerProxy.
// Without this, the test above could pass for the wrong reason.
TEST(RemoteLayerHosting, VideoLayerIsHostedViaWebContentProcessWhenDisabled)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    RetainPtr webView = createWebViewAndStartPlayback(false);
    RetainPtr rootLayer = [webView layer];

    NSLog(@"Layer tree with remote layer hosting disabled:\n%@", layerTreeDescription(rootLayer.get()));

    EXPECT_NULL(findLayerOfClassNamed(rootLayer.get(), videoLayerHostClassName).get());

    RetainPtr layerHost = findLayerOfClassNamed(rootLayer.get(), @"CALayerHost");
    EXPECT_NOT_NULL(layerHost.get());
}

#pragma mark - Geometry

// The video element in video-with-audio.html has no CSS size, so it is laid out at the
// video's intrinsic size and the aspect ratios match: the video should exactly fill its
// WebAVPlayerLayer.
//
// Note that there is deliberately no comparison against the legacy path here. The host layer
// does not mean the same thing on both: on the legacy path it is a viewport onto the
// WebContent process's transport context, which letterboxes the video inside itself, so its
// geometry legitimately differs from the video's presented size.
TEST(RemoteLayerHosting, VideoLayerFillsPlayerLayer)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    RetainPtr webView = createWebViewAndStartPlayback(true);
    CGSize hostSize = waitForStableVideoHostLayerFrameSize(webView.get());

    RetainPtr playerLayer = findLayerOfClassNamed([webView layer], @"WebAVPlayerLayer");
    EXPECT_NOT_NULL(playerLayer.get());
    if (!playerLayer)
        return;

    CGSize playerSize = [playerLayer bounds].size;
    NSLog(@"Player layer = (%g, %g), video host layer = (%g, %g)", playerSize.width, playerSize.height, hostSize.width, hostSize.height);

    // WebAVPlayerLayer may reach the target size by scaling rather than resizing, so the
    // frame is the product of a floating point transform.
    constexpr CGFloat tolerance = 0.01;
    EXPECT_NEAR(playerSize.width, hostSize.width, tolerance);
    EXPECT_NEAR(playerSize.height, hostSize.height, tolerance);

#if USE(EXTENSIONKIT)
    // The hosted hierarchy is adopted by a subview of the layer host view rather than by its
    // backing layer, so that view has to be sized along with the video layer as well.
    RetainPtr hostLayer = findVideoHostLayer([webView layer]);
    RetainPtr hostingView = hostLayer ? findLayerHierarchyHostingView(hostLayer.get()) : nil;
    EXPECT_NOT_NULL(hostingView.get());
    if (!hostingView)
        return;

    CGSize hostingViewSize = [hostingView bounds].size;
    NSLog(@"Layer hierarchy hosting view = (%g, %g)", hostingViewSize.width, hostingViewSize.height);

    EXPECT_NEAR(hostSize.width, hostingViewSize.width, tolerance);
    EXPECT_NEAR(hostSize.height, hostingViewSize.height, tolerance);
#endif
}

// A resize is normally previewed by scaling the video layer and resolved to the real size once
// the process owning that layer catches up. On this path the owning process is the GPU process,
// which is close enough at hand to resize synchronously, so WebAVPlayerLayer is told not to
// preview at all. That matters beyond tidiness: -[WKVideoLayerHost setFrame:] is what tells the
// GPU process how large to make its layer, and the preview path does not go through -setFrame:.
// While a preview transform is in effect the host layer's bounds, the size the GPU process was
// given, and the size on screen all disagree.
TEST(RemoteLayerHosting, VideoLayerResizeIsNotPreviewedWithTransform)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    RetainPtr webView = createWebViewAndStartPlayback(true);
    waitForStableVideoHostLayerFrameSize(webView.get());

    RetainPtr hostLayer = findVideoHostLayer([webView layer]);
    EXPECT_NOT_NULL(hostLayer.get());
    if (!hostLayer)
        return;

    CGSize bounds = [hostLayer bounds].size;
    CGSize frame = [hostLayer frame].size;
    CGAffineTransform transform = [hostLayer affineTransform];
    NSLog(@"Video host layer bounds = (%g, %g), frame = (%g, %g), transform scale = (%g, %g)", bounds.width, bounds.height, frame.width, frame.height, transform.a, transform.d);

    EXPECT_TRUE(CGAffineTransformIsIdentity(transform));
    EXPECT_EQ(bounds.width, frame.width);
    EXPECT_EQ(bounds.height, frame.height);
}

#pragma mark - Captions

// Captions are only overlaid on the hosted video layer where the interface parents a captions
// layer into it. VideoPresentationInterfaceAVKitLegacy, the interface used for inline video on
// iOS, overrides setupCaptionsLayer() to ignore the parent it is handed and attach the captions
// layer to the fullscreen player layer instead, which does not exist inline — so there is no
// captions layer in the inline layer tree there to make an assertion about. That is true of
// both hosting paths.
#if PLATFORM(MAC)

// The captions layer is parented into the video host layer and sized to cover it, so that
// text tracks are laid out over the video. On the new path
// VideoPresentationManagerProxy::setVideoLayerFrame no longer messages the WebContent
// process, so this verifies it still drives setCaptionsFrame().
static void testCaptionsLayerCoversVideoLayer(bool remoteLayerHostingBypassesWebContentProcess)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    RetainPtr webView = createWebViewAndStartPlayback(remoteLayerHostingBypassesWebContentProcess);
    waitForStableVideoHostLayerFrameSize(webView.get());

    RetainPtr rootLayer = [webView layer];
    NSLog(@"Layer tree when checking captions (bypass = %d):\n%@", remoteLayerHostingBypassesWebContentProcess, layerTreeDescription(rootLayer.get()));

    RetainPtr hostLayer = findVideoHostLayer(rootLayer.get());
    RetainPtr captionsLayer = findCaptionsLayer(rootLayer.get());
    EXPECT_NOT_NULL(hostLayer.get());
    EXPECT_NOT_NULL(captionsLayer.get());
    if (!hostLayer || !captionsLayer)
        return;

    // The captions layer is a child of the host layer, so it is sized in the host layer's
    // own coordinate space: it should cover the host layer's bounds.
    CGSize hostBounds = [hostLayer bounds].size;
    CGSize captionsFrame = [captionsLayer frame].size;
    NSLog(@"Host bounds = (%g, %g), captions frame = (%g, %g)", hostBounds.width, hostBounds.height, captionsFrame.width, captionsFrame.height);

    EXPECT_FALSE(CGSizeEqualToSize(captionsFrame, CGSizeZero));
    EXPECT_EQ(hostBounds.width, captionsFrame.width);
    EXPECT_EQ(hostBounds.height, captionsFrame.height);
}

TEST(RemoteLayerHosting, CaptionsLayerCoversVideoLayer)
{
    testCaptionsLayerCoversVideoLayer(true);
}

TEST(RemoteLayerHosting, CaptionsLayerCoversVideoLayerWhenDisabled)
{
    testCaptionsLayerCoversVideoLayer(false);
}

#endif // PLATFORM(MAC)

#pragma mark - GPU process side of the hosting boundary

// A UI process layer tree walk stops at the CALayerHost. This exercises the synchronous
// debugging message that dumps the hierarchy on the other side of the boundary.
TEST(RemoteLayerHosting, GPUProcessLayerTreeIsReachableForTesting)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    RetainPtr webView = createWebViewAndStartPlayback(true);
    CGSize hostSize = waitForStableVideoHostLayerFrameSize(webView.get());

    NSString *gpuLayerTree = [webView _gpuProcessRemoteLayerTreeAsTextForTesting];
    NSLog(@"GPU process layer tree:\n%@", gpuLayerTree);

    EXPECT_GT(gpuLayerTree.length, 0u);
    EXPECT_TRUE([gpuLayerTree containsString:@"remote layer "]);
    EXPECT_FALSE([gpuLayerTree containsString:@"<no content layer>"]);

    // The hosted content layer must sit at the origin of its hosting context, since its
    // position within the UI process layer tree is a property of the layer host there. The
    // UI process cannot see this, so it is only checkable through this dump.
    EXPECT_TRUE([gpuLayerTree containsString:@"position=(0, 0)"]);

    // The size the GPU process was told to use must equal the size the layer host presents
    // at, or the video is rendered at one size and scaled to another. This holds because
    // WebAVPlayerLayer resizes the sublayer directly on this path instead of previewing the
    // resize with a transform, which -[WKVideoLayerHost setFrame:] cannot observe.
    NSString *expectedSize = [NSString stringWithFormat:@"size=(%g, %g)", hostSize.width, hostSize.height];
    NSLog(@"UI process host layer presents at (%g, %g), looking for \"%@\"", hostSize.width, hostSize.height, expectedSize);
    EXPECT_TRUE([gpuLayerTree containsString:expectedSize]);
}

// With the setting disabled nothing is hosted through this mechanism, so the dump is empty.
TEST(RemoteLayerHosting, GPUProcessLayerTreeIsEmptyWhenDisabled)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    RetainPtr webView = createWebViewAndStartPlayback(false);
    waitForStableVideoHostLayerFrameSize(webView.get());

    EXPECT_EQ(0u, [webView _gpuProcessRemoteLayerTreeAsTextForTesting].length);
}

#pragma mark - Lifetime

static uint64_t gpuProcessHostedVideoLayerCount(TestWKWebView *webView)
{
    __block bool done = false;
    __block NSUInteger result = 0;
    [webView _gpuProcessHostedVideoLayerCountForTesting:^(NSUInteger count) {
        result = count;
        done = true;
    }];
    Util::run(&done);
    return result;
}

// Navigating away destroys the media players, which must discard their hosting contexts in
// the GPU process and the corresponding layer hosts in this process. A layer tree walk
// cannot see this: the layer host is unparented by remote layer tree teardown regardless,
// while the bookkeeping on both sides is what leaks.
TEST(RemoteLayerHosting, HostedLayersAreReleasedAfterNavigation)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    RetainPtr webView = createWebViewAndStartPlayback(true);
    waitForStableVideoHostLayerFrameSize(webView.get());

    EXPECT_GT([webView _hostedVideoLayerCountForTesting], 0u);
    EXPECT_GT(gpuProcessHostedVideoLayerCount(webView.get()), 0u);

    [webView synchronouslyLoadHTMLString:@"<body>no video here</body>"];
    [webView waitForNextPresentationUpdate];

    // Teardown is driven by the GPU process reporting that each player's remote layer is
    // gone, so allow for the round trip.
    unsigned timeout = 0;
    while (([webView _hostedVideoLayerCountForTesting] || gpuProcessHostedVideoLayerCount(webView.get())) && timeout++ < 100)
        Util::runFor(0.05_s);

    EXPECT_EQ(0u, [webView _hostedVideoLayerCountForTesting]);
    EXPECT_EQ(0u, gpuProcessHostedVideoLayerCount(webView.get()));
    EXPECT_NULL(findLayerOfClassNamed([webView layer], videoLayerHostClassName).get());
}

// The two processes must agree on how many layers are hosted; a mismatch means one side
// created or dropped an entry the other did not.
TEST(RemoteLayerHosting, HostedLayerCountsAgreeAcrossProcesses)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    RetainPtr webView = createWebViewAndStartPlayback(true);
    waitForStableVideoHostLayerFrameSize(webView.get());

    NSUInteger uiProcessCount = [webView _hostedVideoLayerCountForTesting];
    uint64_t gpuProcessCount = gpuProcessHostedVideoLayerCount(webView.get());
    NSLog(@"Hosted video layers: UI process = %lu, GPU process = %llu", static_cast<unsigned long>(uiProcessCount), gpuProcessCount);

    EXPECT_GT(uiProcessCount, 0u);
    EXPECT_EQ(static_cast<uint64_t>(uiProcessCount), gpuProcessCount);
}

// With the setting disabled nothing should be hosted through this mechanism at all.
TEST(RemoteLayerHosting, NoLayersAreHostedWhenDisabled)
{
    UISideCompositingScope scope { UISideCompositingState::Enabled };

    RetainPtr webView = createWebViewAndStartPlayback(false);
    waitForStableVideoHostLayerFrameSize(webView.get());

    EXPECT_EQ(0u, [webView _hostedVideoLayerCountForTesting]);
    EXPECT_EQ(0u, gpuProcessHostedVideoLayerCount(webView.get()));
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && ENABLE(GPU_PROCESS)
