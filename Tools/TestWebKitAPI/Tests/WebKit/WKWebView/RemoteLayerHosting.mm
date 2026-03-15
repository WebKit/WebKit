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
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#if ENABLE(MEDIA_STREAM)
#import "Helpers/cocoa/UserMediaCaptureUIDelegate.h"
#endif
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

static RetainPtr<NSString> gpuProcessRemoteLayerTree(TestWKWebView *webView)
{
    __block bool done = false;
    __block RetainPtr<NSString> result;
    [webView _gpuProcessRemoteLayerTreeAsTextForTesting:^(NSString *tree) {
        result = tree;
        done = true;
    }];
    Util::run(&done);
    return result;
}

// Runs a block that makes the test page begin playback, and waits for the page to report that
// playback began or failed.
static bool waitForPlayback(TestWKWebView *webView, NS_NOESCAPE dispatch_block_t startPlayback)
{
    __block bool done = false;
    __block bool isPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{
        isPlaying = true;
        done = true;
    }];
    for (NSString *failure in @[ @"not playing", @"media source unsupported", @"load failed", @"capture failed" ]) {
        [webView performAfterReceivingMessage:failure action:^{
            ADD_FAILURE() << "Playback did not begin: " << failure.UTF8String;
            done = true;
        }];
    }

    startPlayback();
    Util::run(&done);

    // The video layer is created in response to the media player reporting that it has
    // begun rendering, which happens after playback starts. Wait for the layer tree
    // containing it to be committed to this process.
    [webView waitForNextPresentationUpdate];

    return isPlaying;
}

static RetainPtr<TestWKWebView> createWebViewAndStartPlayback(bool remoteLayerHostingBypassesWebContentProcess, NSString *pageName = @"video-with-audio")
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

    // The switchable source test page fetches its media from a file URL, and captures from a
    // mock camera.
    [[configuration preferences] _setMediaSourceEnabled:YES];
    [[configuration preferences] _setAllowFileAccessFromFileURLs:YES];
#if ENABLE(MEDIA_STREAM)
    configuration.get()._mediaCaptureEnabled = YES;
    configuration.get().preferences._mediaCaptureRequiresSecureConnection = NO;
    configuration.get().preferences._mockCaptureDevicesEnabled = YES;
    configuration.get().preferences._getUserMediaRequiresFocus = NO;
#endif

    WKPreferencesRef preferences = (__bridge WKPreferencesRef)[configuration preferences];
    WKPreferencesSetBoolValueForKeyForTesting(preferences, true, adoptWK(WKStringCreateWithUTF8CString("UseGPUProcessForMediaEnabled")).get());
    WKPreferencesSetBoolValueForKeyForTesting(preferences, remoteLayerHostingBypassesWebContentProcess, adoptWK(WKStringCreateWithUTF8CString("RemoteLayerHostingBypassesWebContentProcess")).get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get() addToWindow:YES]);

    waitForPlayback(webView.get(), ^{
        [webView synchronouslyLoadTestPageNamed:pageName];
    });

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
//
// Pass the size before a resize as differentFrom to wait for the resize to take effect.
static CGSize waitForStableVideoHostLayerFrameSize(TestWKWebView *webView, std::optional<CGSize> differentFrom = std::nullopt)
{
    constexpr unsigned requiredStableSamples = 3;
    constexpr unsigned maximumSamples = 100;

    CGSize previousSize = CGSizeMake(-1, -1);
    unsigned stableSamples = 0;

    for (unsigned sample = 0; sample < maximumSamples; ++sample) {
        [webView waitForNextPresentationUpdate];

        RetainPtr hostLayer = findVideoHostLayer([webView layer]);
        CGSize currentSize = hostLayer ? [hostLayer frame].size : CGSizeZero;

        bool isCandidate = !CGSizeEqualToSize(currentSize, CGSizeZero) && !(differentFrom && CGSizeEqualToSize(currentSize, *differentFrom));
        if (isCandidate && CGSizeEqualToSize(currentSize, previousSize)) {
            if (++stableSamples >= requiredStableSamples)
                return currentSize;
        } else
            stableSamples = 0;

        previousSize = currentSize;
        Util::runFor(0.05_s);
    }

    ADD_FAILURE() << "Video host layer frame did not settle; last size (" << previousSize.width << ", " << previousSize.height << ")";
    return previousSize;
}

#pragma mark - Layer hosting path

// Verifies that starting video playback with RemoteLayerHostingBypassesWebContentProcess
// enabled produces a WKVideoLayerHost in the UI process layer tree, hosting a context
// supplied by the GPU process.
TEST(RemoteLayerHosting, VideoLayerIsHostedDirectlyFromGPUProcess)
{
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
    RetainPtr webView = createWebViewAndStartPlayback(true);
    CGSize hostSize = waitForStableVideoHostLayerFrameSize(webView.get());

    RetainPtr gpuLayerTree = gpuProcessRemoteLayerTree(webView.get());
    NSLog(@"GPU process layer tree:\n%@", gpuLayerTree.get());

    EXPECT_GT([gpuLayerTree.get() length], 0u);
    EXPECT_TRUE([gpuLayerTree.get() containsString:@"remote layer "]);
    EXPECT_FALSE([gpuLayerTree.get() containsString:@"<no content layer>"]);

    // The hosted content layer must sit at the origin of its hosting context, since its
    // position within the UI process layer tree is a property of the layer host there. The
    // UI process cannot see this, so it is only checkable through this dump.
    EXPECT_TRUE([gpuLayerTree.get() containsString:@"position=(0, 0)"]);

    // The size the GPU process was told to use must equal the size the layer host presents
    // at, or the video is rendered at one size and scaled to another. This holds because
    // WebAVPlayerLayer resizes the sublayer directly on this path instead of previewing the
    // resize with a transform, which -[WKVideoLayerHost setFrame:] cannot observe.
    NSString *expectedSize = [NSString stringWithFormat:@"size=(%g, %g)", hostSize.width, hostSize.height];
    NSLog(@"UI process host layer presents at (%g, %g), looking for \"%@\"", hostSize.width, hostSize.height, expectedSize);
    EXPECT_TRUE([gpuLayerTree.get() containsString:expectedSize]);
}

// With the setting disabled nothing is hosted through this mechanism, so the dump is empty.
TEST(RemoteLayerHosting, GPUProcessLayerTreeIsEmptyWhenDisabled)
{
    RetainPtr webView = createWebViewAndStartPlayback(false);
    waitForStableVideoHostLayerFrameSize(webView.get());

    EXPECT_EQ(0u, [gpuProcessRemoteLayerTree(webView.get()) length]);
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

static void expectHostedVideoLayersAreReleased(TestWKWebView *observer)
{
    unsigned timeout = 0;
    while (([observer _hostedVideoLayerCountForTesting] || gpuProcessHostedVideoLayerCount(observer)) && timeout++ < 100)
        Util::runFor(0.05_s);

    EXPECT_EQ(0u, [observer _hostedVideoLayerCountForTesting]);
    EXPECT_EQ(0u, gpuProcessHostedVideoLayerCount(observer));
}

TEST(RemoteLayerHosting, HostedLayersAreReleasedWhenWebViewIsClosed)
{
    RetainPtr webView = createWebViewAndStartPlayback(true);
    waitForStableVideoHostLayerFrameSize(webView.get());

    RetainPtr observer = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:[webView configuration]]);
    EXPECT_GT([observer _hostedVideoLayerCountForTesting], 0u);

    [webView _close];

    expectHostedVideoLayersAreReleased(observer.get());
}

TEST(RemoteLayerHosting, HostedLayersAreReleasedWhenWebContentProcessTerminates)
{
    RetainPtr webView = createWebViewAndStartPlayback(true);
    waitForStableVideoHostLayerFrameSize(webView.get());

    EXPECT_GT([webView _hostedVideoLayerCountForTesting], 0u);

    __block bool didTerminate = false;
    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *, _WKProcessTerminationReason) {
        didTerminate = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView _killWebContentProcess];
    Util::run(&didTerminate);

    expectHostedVideoLayersAreReleased(webView.get());
}

// The two processes must agree on how many layers are hosted; a mismatch means one side
// created or dropped an entry the other did not.
TEST(RemoteLayerHosting, HostedLayerCountsAgreeAcrossProcesses)
{
    RetainPtr webView = createWebViewAndStartPlayback(true);
    waitForStableVideoHostLayerFrameSize(webView.get());

    NSUInteger uiProcessCount = [webView _hostedVideoLayerCountForTesting];
    uint64_t gpuProcessCount = gpuProcessHostedVideoLayerCount(webView.get());
    NSLog(@"Hosted video layers: UI process = %lu, GPU process = %llu", static_cast<unsigned long>(uiProcessCount), gpuProcessCount);

    EXPECT_GT(uiProcessCount, 0u);
    EXPECT_EQ(static_cast<uint64_t>(uiProcessCount), gpuProcessCount);
}

TEST(RemoteLayerHosting, LayerHostViewsAreOnlyCreatedForDisplayedVideo)
{
    RetainPtr webView = createWebViewAndStartPlayback(true);
    waitForStableVideoHostLayerFrameSize(webView.get());

    EXPECT_EQ(2u, gpuProcessHostedVideoLayerCount(webView.get()));
    EXPECT_EQ(2u, [webView _hostedVideoLayerCountForTesting]);
    EXPECT_EQ(1u, [webView _hostedVideoLayerViewCountForTesting]);
}

// With the setting disabled nothing should be hosted through this mechanism at all.
TEST(RemoteLayerHosting, NoLayersAreHostedWhenDisabled)
{
    RetainPtr webView = createWebViewAndStartPlayback(false);
    waitForStableVideoHostLayerFrameSize(webView.get());

    EXPECT_EQ(0u, [webView _hostedVideoLayerCountForTesting]);
    EXPECT_EQ(0u, gpuProcessHostedVideoLayerCount(webView.get()));
}

#pragma mark - Media Source

// Hosting has been established from a GPU process context, rather than the layer host view
// merely having been created.
static void expectVideoLayerIsHostedFromGPUProcess(TestWKWebView *webView)
{
    RetainPtr videoLayerHost = findLayerOfClassNamed([webView layer], videoLayerHostClassName);
    EXPECT_NOT_NULL(videoLayerHost.get());
    if (!videoLayerHost)
        return;

#if USE(EXTENSIONKIT)
    RetainPtr hostingView = findLayerHierarchyHostingView(videoLayerHost.get());
    EXPECT_NOT_NULL(hostingView.get());
    EXPECT_NOT_NULL([hostingView handle]);
#else
    EXPECT_NE(0u, static_cast<CALayerHost *>(videoLayerHost.get()).contextId);
#endif
}

// Layers are torn down and created asynchronously in the GPU process when a player is
// replaced, so allow for the round trips before comparing.
static void expectHostedVideoLayerCounts(TestWKWebView *webView, uint64_t expectedCount)
{
    for (unsigned attempt = 0; attempt < 40; ++attempt) {
        if ([webView _hostedVideoLayerCountForTesting] == expectedCount && gpuProcessHostedVideoLayerCount(webView) == expectedCount)
            break;
        Util::runFor(0.05_s);
    }

    EXPECT_EQ(expectedCount, static_cast<uint64_t>([webView _hostedVideoLayerCountForTesting]));
    EXPECT_EQ(expectedCount, gpuProcessHostedVideoLayerCount(webView));
}

typedef BOOL (^LayerTreePredicate)(NSString *);

// Waits for the GPU process side of the hosting boundary to satisfy a predicate, and returns
// the last dump taken.
static RetainPtr<NSString> waitForGPUProcessLayerTree(TestWKWebView *webView, NS_NOESCAPE LayerTreePredicate predicate)
{
    RetainPtr<NSString> gpuLayerTree;
    for (unsigned attempt = 0; attempt < 40; ++attempt) {
        gpuLayerTree = gpuProcessRemoteLayerTree(webView);
        if (predicate(gpuLayerTree.get()))
            break;
        Util::runFor(0.05_s);
    }
    return gpuLayerTree;
}

// The size the GPU process uses for the hosted layer must equal the size the layer host in
// this process presents at, or the video is rendered at one size and scaled to another.
static void expectGPUProcessLayerSize(TestWKWebView *webView, CGSize hostSize)
{
    RetainPtr expectedSize = [NSString stringWithFormat:@"size=(%g, %g)", hostSize.width, hostSize.height];
    RetainPtr gpuLayerTree = waitForGPUProcessLayerTree(webView, ^BOOL (NSString *tree) {
        return [tree containsString:expectedSize.get()];
    });
    NSLog(@"GPU process layer tree:\n%@", gpuLayerTree.get());

    EXPECT_TRUE([gpuLayerTree containsString:expectedSize.get()]) << "Expected " << [expectedSize UTF8String];
    EXPECT_FALSE([gpuLayerTree containsString:@"<no content layer>"]);
}

// The video layer each kind of source puts into its hosting context: a remote MediaPlayer
// playing a file renders into an AVPlayerLayer, and a remote AudioVideoRenderer playing MSE
// renders into an AVSampleBufferDisplayLayer.
static NSString * const mediaPlayerVideoLayerClassName = @"AVPlayerLayer";
static NSString * const audioVideoRendererVideoLayerClassName = @"AVSampleBufferDisplayLayer";

// Identifies whose layer the GPU process is hosting, which a UI process layer tree walk cannot.
static void expectGPUProcessHostsVideoLayerOfClass(TestWKWebView *webView, NSString *expectedClassName, NSString *unexpectedClassName)
{
    auto containsLayerOfClass = ^(NSString *tree, NSString *className) {
        return [tree containsString:[NSString stringWithFormat:@" %@ name=", className]];
    };

    RetainPtr gpuLayerTree = waitForGPUProcessLayerTree(webView, ^BOOL (NSString *tree) {
        return containsLayerOfClass(tree, expectedClassName) && !containsLayerOfClass(tree, unexpectedClassName);
    });

    EXPECT_TRUE(containsLayerOfClass(gpuLayerTree.get(), expectedClassName)) << "Expected a " << expectedClassName.UTF8String << " in the GPU process layer tree";
    EXPECT_FALSE(containsLayerOfClass(gpuLayerTree.get(), unexpectedClassName)) << "Did not expect a " << unexpectedClassName.UTF8String << " in the GPU process layer tree";
}

// An MSE player stays in the WebContent process and renders through a remote
// AudioVideoRenderer, so this exercises RemoteAudioVideoRendererProxyManager as the source of
// the hosted layer rather than RemoteMediaPlayerProxy.
TEST(RemoteLayerHosting, MediaSourceVideoLayerIsHostedDirectlyFromGPUProcess)
{
    RetainPtr webView = createWebViewAndStartPlayback(true, @"switchable-video-source");
    CGSize hostSize = waitForStableVideoHostLayerFrameSize(webView.get());

    expectVideoLayerIsHostedFromGPUProcess(webView.get());
    expectHostedVideoLayerCounts(webView.get(), 1);
    expectGPUProcessHostsVideoLayerOfClass(webView.get(), audioVideoRendererVideoLayerClassName, mediaPlayerVideoLayerClassName);
    expectGPUProcessLayerSize(webView.get(), hostSize);
}

// A resize in this process is forwarded to the GPU process inside a fence, and must be applied
// to the renderer's layer there.
TEST(RemoteLayerHosting, MediaSourceVideoLayerFollowsResize)
{
    RetainPtr webView = createWebViewAndStartPlayback(true, @"switchable-video-source");
    CGSize initialSize = waitForStableVideoHostLayerFrameSize(webView.get());

    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"video().style.width = '%gpx'", std::round(initialSize.width / 2)]];
    CGSize resizedSize = waitForStableVideoHostLayerFrameSize(webView.get(), initialSize);
    NSLog(@"Video host layer resized from (%g, %g) to (%g, %g)", initialSize.width, initialSize.height, resizedSize.width, resizedSize.height);

    EXPECT_FALSE(CGSizeEqualToSize(initialSize, resizedSize));
    expectGPUProcessLayerSize(webView.get(), resizedSize);
}

// HTMLMediaElement replaces its player when its source changes. Switching between MSE and a
// file replaces one kind of source with the other for the same element: a remote
// AudioVideoRenderer and a remote MediaPlayer. The element's layer host in this process must
// survive each swap and end up hosting the new source's layer at the size it presents at, and
// the departing source must not tear down its successor's layer.
//
// The element is presented at other than the videos' natural size, so a new source's layer
// must come up at the element's size rather than the video's.
TEST(RemoteLayerHosting, VideoLayerSurvivesSwitchingBetweenMediaSourceAndFile)
{
    RetainPtr webView = createWebViewAndStartPlayback(true, @"switchable-video-source");
    CGSize naturalSize = waitForStableVideoHostLayerFrameSize(webView.get());

    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"video().style.width = '%gpx'", std::round(naturalSize.width / 2)]];
    waitForStableVideoHostLayerFrameSize(webView.get(), naturalSize);

    auto switchSourceAndExpectHostedLayer = [&](NSString *script, NSString *expectedClassName, NSString *unexpectedClassName) {
        SCOPED_TRACE(script.UTF8String);

        EXPECT_TRUE(waitForPlayback(webView.get(), ^{
            [webView objectByEvaluatingJavaScript:script];
        }));
        CGSize hostSize = waitForStableVideoHostLayerFrameSize(webView.get());

        expectVideoLayerIsHostedFromGPUProcess(webView.get());
        expectHostedVideoLayerCounts(webView.get(), 1);
        expectGPUProcessHostsVideoLayerOfClass(webView.get(), expectedClassName, unexpectedClassName);
        expectGPUProcessLayerSize(webView.get(), hostSize);
    };

    switchSourceAndExpectHostedLayer(@"loadFile()", mediaPlayerVideoLayerClassName, audioVideoRendererVideoLayerClassName);
    switchSourceAndExpectHostedLayer(@"loadMediaSource()", audioVideoRendererVideoLayerClassName, mediaPlayerVideoLayerClassName);
}

static RetainPtr<NSString> gpuProcessHostedLayerSummary(TestWKWebView *webView)
{
    return [[gpuProcessRemoteLayerTree(webView) componentsSeparatedByString:@"\n"] firstObject];
}

TEST(RemoteLayerHosting, SupersededSourceDoesNotTearDownItsSuccessorsLayer)
{
    RetainPtr webView = createWebViewAndStartPlayback(true, @"switchable-video-source");
    CGSize hostSize = waitForStableVideoHostLayerFrameSize(webView.get());

    RetainPtr summaryBefore = gpuProcessHostedLayerSummary(webView.get());
    EXPECT_TRUE([summaryBefore hasPrefix:@"remote layer "]);

    __block bool done = false;
    [webView _supersedeAndDetachHostedVideoLayerSourcesForTesting:^{
        done = true;
    }];
    Util::run(&done);
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ(summaryBefore.get(), gpuProcessHostedLayerSummary(webView.get()).get());
    expectVideoLayerIsHostedFromGPUProcess(webView.get());
    expectHostedVideoLayerCounts(webView.get(), 1);
    expectGPUProcessHostsVideoLayerOfClass(webView.get(), audioVideoRendererVideoLayerClassName, mediaPlayerVideoLayerClassName);
    expectGPUProcessLayerSize(webView.get(), hostSize);
}

#pragma mark - Players that are not hosted directly

#if ENABLE(MEDIA_STREAM)

// The video layer is hosted through a context relayed by the WebContent process: the host layer
// in the WebAVPlayerLayer is a plain CALayerHost hosting a nonzero context, not a
// WKVideoLayerHost. Waits for this, since the relayed context arrives after playback starts.
static void expectVideoLayerIsHostedViaWebContentProcess(TestWKWebView *webView)
{
    RetainPtr<CALayerHost> hostLayer;
    for (unsigned attempt = 0; attempt < 40; ++attempt) {
        [webView waitForNextPresentationUpdate];
        hostLayer = findVideoHostLayer([webView layer]);
        if (hostLayer && ![hostLayer isKindOfClass:NSClassFromString(videoLayerHostClassName)] && [hostLayer contextId])
            break;
        Util::runFor(0.05_s);
    }

    NSLog(@"Layer tree when expecting a relayed video layer:\n%@", layerTreeDescription([webView layer]));

    EXPECT_NOT_NULL(hostLayer.get());
    if (!hostLayer)
        return;
    EXPECT_FALSE([hostLayer isKindOfClass:NSClassFromString(videoLayerHostClassName)]);
    EXPECT_NE(0u, [hostLayer contextId]);
}

// A MediaStream player stays in the WebContent process and renders through a sample buffer
// display layer whose hosting context the WebContent process relays, so even with the setting
// enabled its video layer must not be hosted directly by the UI process, which would leave
// it blank. Switching to it and away again changes how the element's layer is hosted, and the
// layer host view in this process must be replaced each time.
TEST(RemoteLayerHosting, MediaStreamVideoLayerIsHostedViaWebContentProcess)
{
    RetainPtr webView = createWebViewAndStartPlayback(true, @"switchable-video-source");
    RetainPtr delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    waitForStableVideoHostLayerFrameSize(webView.get());
    expectVideoLayerIsHostedFromGPUProcess(webView.get());

    {
        SCOPED_TRACE("loadMediaStream()");
        EXPECT_TRUE(waitForPlayback(webView.get(), ^{
            [webView objectByEvaluatingJavaScript:@"loadMediaStream()"];
        }));
        expectVideoLayerIsHostedViaWebContentProcess(webView.get());

        // Neither process keeps hosting state for the element once it stops being hosted
        // directly.
        expectHostedVideoLayerCounts(webView.get(), 0);
    }

    {
        SCOPED_TRACE("loadFile()");
        EXPECT_TRUE(waitForPlayback(webView.get(), ^{
            [webView objectByEvaluatingJavaScript:@"loadFile()"];
        }));
        CGSize hostSize = waitForStableVideoHostLayerFrameSize(webView.get());

        expectVideoLayerIsHostedFromGPUProcess(webView.get());
        expectHostedVideoLayerCounts(webView.get(), 1);
        expectGPUProcessHostsVideoLayerOfClass(webView.get(), mediaPlayerVideoLayerClassName, audioVideoRendererVideoLayerClassName);
        expectGPUProcessLayerSize(webView.get(), hostSize);
    }
}

#endif // ENABLE(MEDIA_STREAM)

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS)
