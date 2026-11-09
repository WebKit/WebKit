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

#if PLATFORM(MAC)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static constexpr _WKCALayerTreeAsTextOptions mirrorComparisonOptions = _WKCALayerTreeAsTextOptionsRemoteLayersOnly
    | _WKCALayerTreeAsTextOptionsIncludeLayerIdentifiers
    | _WKCALayerTreeAsTextOptionsIncludeLayerClasses
    | _WKCALayerTreeAsTextOptionsIncludeNames
    | _WKCALayerTreeAsTextOptionsIncludeTransforms
    | _WKCALayerTreeAsTextOptionsIncludeVisibility
    | _WKCALayerTreeAsTextOptionsIncludeContents
    | _WKCALayerTreeAsTextOptionsIncludeAnimations;

// The primary video layer is created and named by the video presentation manager, which only the primary tree
// registers with, so video layers are compared by the context they host rather than by class or name.
static constexpr _WKCALayerTreeAsTextOptions videoComparisonOptions = mirrorComparisonOptions & ~(_WKCALayerTreeAsTextOptionsIncludeLayerClasses | _WKCALayerTreeAsTextOptionsIncludeNames);

class CaptureMirror {
public:
    explicit CaptureMirror(TestWKWebView *webView)
        : m_webView(webView)
        // Like for tab capture, the mirror is shown in another window than the web view.
        , m_window(adoptNS([[NSWindow alloc] initWithContentRect:webView.bounds styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]))
    {
        [m_window contentView].wantsLayer = YES;
        enable();
    }

    ~CaptureMirror()
    {
        [m_webView _disableCaptureMirrorLayerTreeForTesting];
    }

    void enable()
    {
        __block bool done = false;
        __block RetainPtr<CALayer> containerLayer;
        [m_webView _enableCaptureMirrorLayerTreeForTesting:^(CALayer *layer) {
            containerLayer = layer;
            done = true;
        }];
        Util::run(&done);

        m_containerLayer = WTF::move(containerLayer);
        if (m_containerLayer)
            [[[m_window contentView] layer] addSublayer:m_containerLayer.get()];
    }

    void disable()
    {
        [m_webView _disableCaptureMirrorLayerTreeForTesting];
        [m_containerLayer removeFromSuperlayer];
    }

    CALayer *containerLayer() const { return m_containerLayer.get(); }

    NSString *mirrorTreeAsText(_WKCALayerTreeAsTextOptions options = mirrorComparisonOptions) const
    {
        return [m_webView _caLayerTreeAsTextForLayer:m_containerLayer.get() options:options];
    }

    NSString *primaryTreeAsText(_WKCALayerTreeAsTextOptions options = mirrorComparisonOptions) const
    {
        return [m_webView _caLayerTreeAsTextWithOptions:options];
    }

    void expectMatchesPrimaryTree(_WKCALayerTreeAsTextOptions options = mirrorComparisonOptions) const
    {
        [m_webView waitForNextPresentationUpdate];
        EXPECT_WK_STREQ(primaryTreeAsText(options), mirrorTreeAsText(options));
    }

    // Threaded animations update both trees on every display refresh, so a refresh can happen between the two dumps.
    void waitUntilMatchesPrimaryTree(_WKCALayerTreeAsTextOptions options = mirrorComparisonOptions) const
    {
        [m_webView waitForNextPresentationUpdate];
        RetainPtr<NSString> primaryTree;
        RetainPtr<NSString> mirrorTree;
        bool matches = Util::waitFor([&] {
            primaryTree = primaryTreeAsText(options);
            mirrorTree = mirrorTreeAsText(options);
            return [primaryTree isEqualToString:mirrorTree.get()];
        }, 50);
        EXPECT_TRUE(matches);
        if (!matches)
            EXPECT_WK_STREQ(primaryTree.get(), mirrorTree.get());
    }

private:
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<NSWindow> m_window;
    RetainPtr<CALayer> m_containerLayer;
};

static RetainPtr<TestWKWebView> createWebView(void (^configure)(WKWebViewConfiguration *) = nil)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    if (configure)
        configure(configuration.get());
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
}

static NSString *const staticContent = @""
    "<style>"
    "body { margin: 0; height: 4000px; font: 20px sans-serif; }"
    "div { position: absolute; width: 100px; height: 100px; }"
    "#solid { left: 10px; top: 10px; background: rgb(0, 128, 255); will-change: transform; }"
    "#painted { left: 120px; top: 10px; width: 200px; background: linear-gradient(red, blue); border-radius: 12px; will-change: transform; }"
    "#text { left: 330px; top: 10px; width: 200px; will-change: transform; }"
    "#rotated { left: 10px; top: 120px; background: green; transform: rotate3d(1, 1, 0, 30deg); }"
    "#preserve3d { left: 120px; top: 120px; transform-style: preserve-3d; transform: rotateY(20deg); }"
    "#preserve3d > div { left: 0; top: 0; background: orange; transform: translateZ(30px); }"
    "#backdrop { left: 230px; top: 120px; backdrop-filter: blur(4px); }"
    "#clip { left: 340px; top: 120px; overflow: hidden; border-radius: 20px; }"
    "#clip > div { left: 0; top: 0; width: 200px; height: 200px; background: purple; will-change: transform; }"
    "#masked { left: 450px; top: 120px; background: teal; -webkit-mask-image: linear-gradient(black, transparent); will-change: transform; }"
    "#filtered { left: 10px; top: 230px; background: yellow; filter: blur(2px) drop-shadow(4px 4px 4px black); will-change: transform; }"
    "#translucent { left: 120px; top: 230px; background: red; opacity: 0.5; will-change: opacity; }"
    "#scroller { left: 230px; top: 230px; width: 200px; overflow: scroll; }"
    "#scroller > div { position: static; height: 1000px; background: linear-gradient(white, black); }"
    "#fixed { position: fixed; left: 0; bottom: 0; width: 100%; height: 40px; background: gray; }"
    "canvas { position: absolute; left: 440px; top: 230px; will-change: transform; }"
    "</style>"
    "<div id='solid'></div>"
    "<div id='painted'></div>"
    "<div id='text'>Mirrored text</div>"
    "<div id='rotated'></div>"
    "<div id='preserve3d'><div></div></div>"
    "<div id='backdrop'></div>"
    "<div id='clip'><div></div></div>"
    "<div id='masked'></div>"
    "<div id='filtered'></div>"
    "<div id='translucent'></div>"
    "<div id='scroller'><div></div></div>"
    "<div id='fixed'></div>"
    "<canvas id='canvas2d' width='100' height='100'></canvas>"
    "<script>"
    "const context = document.getElementById('canvas2d').getContext('2d');"
    "context.fillStyle = 'rgb(255, 0, 128)';"
    "context.fillRect(10, 10, 80, 80);"
    "</script>";

TEST(CaptureMirrorLayerTree, StaticContent)
{
    RetainPtr webView = createWebView();
    [webView synchronouslyLoadHTMLString:staticContent];
    [webView waitForNextPresentationUpdate];

    CaptureMirror mirror(webView.get());
    ASSERT_NOT_NULL(mirror.containerLayer());
    mirror.expectMatchesPrimaryTree();

    // The mirror must have content, not just matching geometry.
    RetainPtr mirrorTree = mirror.mirrorTreeAsText();
    EXPECT_TRUE([mirrorTree containsString:@"layer contents"]);
    EXPECT_TRUE([mirrorTree containsString:@"CATransformLayer"]);
    EXPECT_TRUE([mirrorTree containsString:@"CABackdropLayer"]);
}

TEST(CaptureMirrorLayerTree, AttachingDoesNotChangePrimaryTree)
{
    RetainPtr webView = createWebView();
    [webView synchronouslyLoadHTMLString:staticContent];
    [webView waitForNextPresentationUpdate];

    RetainPtr primaryTreeBefore = [webView _caLayerTreeAsTextWithOptions:mirrorComparisonOptions];

    CaptureMirror mirror(webView.get());
    ASSERT_NOT_NULL(mirror.containerLayer());
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ(primaryTreeBefore.get(), [webView _caLayerTreeAsTextWithOptions:mirrorComparisonOptions]);
}

TEST(CaptureMirrorLayerTree, UpdatesAfterAttaching)
{
    RetainPtr webView = createWebView();
    [webView synchronouslyLoadHTMLString:@"<style>div { width: 100px; height: 100px; will-change: transform; }</style><div id='first' style='background: red'></div>"];
    [webView waitForNextPresentationUpdate];

    CaptureMirror mirror(webView.get());
    ASSERT_NOT_NULL(mirror.containerLayer());
    mirror.expectMatchesPrimaryTree();

    [webView objectByEvaluatingJavaScript:@"let second = document.createElement('div'); second.style.background = 'linear-gradient(blue, green)'; document.body.appendChild(second); 1"];
    mirror.expectMatchesPrimaryTree();

    [webView objectByEvaluatingJavaScript:@"document.getElementById('first').style.background = 'blue'; 1"];
    mirror.expectMatchesPrimaryTree();

    [webView objectByEvaluatingJavaScript:@"document.getElementById('first').textContent = 'Repainted'; 1"];
    mirror.expectMatchesPrimaryTree();

    [webView objectByEvaluatingJavaScript:@"document.getElementById('first').remove(); 1"];
    mirror.expectMatchesPrimaryTree();
}

TEST(CaptureMirrorLayerTree, WebGL)
{
    RetainPtr webView = createWebView();
    [webView synchronouslyLoadHTMLString:@""
        "<canvas id='canvas' width='200' height='200'></canvas>"
        "<script>"
        "const gl = document.getElementById('canvas').getContext('webgl', { preserveDrawingBuffer: true });"
        "function draw(r, g, b) { gl.clearColor(r, g, b, 1); gl.clear(gl.COLOR_BUFFER_BIT); }"
        "draw(1, 0, 0);"
        "</script>"];
    [webView waitForNextPresentationUpdate];

    CaptureMirror mirror(webView.get());
    ASSERT_NOT_NULL(mirror.containerLayer());
    mirror.expectMatchesPrimaryTree();
    EXPECT_TRUE([mirror.mirrorTreeAsText() containsString:@"layer contents"]);

    // New WebGL frames are displayed in new buffers.
    [webView objectByEvaluatingJavaScript:@"draw(0, 1, 0); 1"];
    mirror.expectMatchesPrimaryTree();

    [webView objectByEvaluatingJavaScript:@"draw(0, 0, 1); 1"];
    mirror.expectMatchesPrimaryTree();
}

TEST(CaptureMirrorLayerTree, Video)
{
    RetainPtr webView = createWebView();
    // This page starts playing on load.
    [webView synchronouslyLoadTestPageNamed:@"video-without-audio"];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.querySelector('video').currentTime > 0"] boolValue];
    }));
    [webView waitForNextPresentationUpdate];

    CaptureMirror mirror(webView.get());
    ASSERT_NOT_NULL(mirror.containerLayer());
    mirror.expectMatchesPrimaryTree(videoComparisonOptions);

    // Both trees host the GPU process video layer.
    EXPECT_TRUE([mirror.mirrorTreeAsText(videoComparisonOptions) containsString:@"layer hosted context"]);

    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').style.width = '480px'; 1"];
    mirror.expectMatchesPrimaryTree(videoComparisonOptions);
}

TEST(CaptureMirrorLayerTree, SiteIsolatedIframe)
{
    HTTPServer server({
        { "/main"_s, { "<style>iframe { width: 400px; height: 300px; }</style><iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<div style='width: 137px; height: 59px; background: linear-gradient(red, blue); will-change: transform'></div>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Wait for the cross-origin iframe, composited in its own process, to be hosted in the main frame's layer tree.
    EXPECT_TRUE(Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return [[webView _caLayerTreeAsText] containsString:@"width: 137 height: 59"];
    }));

    // Seeding re-sends the layers of every process, which must not reparent the iframe content in the primary tree.
    RetainPtr primaryTreeBefore = [webView _caLayerTreeAsTextWithOptions:mirrorComparisonOptions];

    CaptureMirror mirror(webView.get());
    ASSERT_NOT_NULL(mirror.containerLayer());
    mirror.expectMatchesPrimaryTree();
    EXPECT_WK_STREQ(primaryTreeBefore.get(), [webView _caLayerTreeAsTextWithOptions:mirrorComparisonOptions]);
    EXPECT_TRUE([mirror.mirrorTreeAsText() containsString:@"width: 137 height: 59"]);

    [webView evaluateJavaScript:@"document.querySelector('div').style.width = '200px'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return [[webView _caLayerTreeAsText] containsString:@"width: 200 height: 59"];
    }));
    mirror.expectMatchesPrimaryTree();
}

TEST(CaptureMirrorLayerTree, DisableAndEnableAgain)
{
    RetainPtr webView = createWebView();
    [webView synchronouslyLoadHTMLString:staticContent];
    [webView waitForNextPresentationUpdate];

    CaptureMirror mirror(webView.get());
    ASSERT_NOT_NULL(mirror.containerLayer());
    mirror.expectMatchesPrimaryTree();

    RetainPtr previousContainerLayer = mirror.containerLayer();
    mirror.disable();
    EXPECT_EQ([previousContainerLayer sublayers].count, 0u);

    [webView objectByEvaluatingJavaScript:@"document.getElementById('solid').style.background = 'black'; 1"];
    [webView waitForNextPresentationUpdate];

    mirror.enable();
    ASSERT_NOT_NULL(mirror.containerLayer());
    mirror.expectMatchesPrimaryTree();
}

TEST(CaptureMirrorLayerTree, EnableTwice)
{
    RetainPtr webView = createWebView();
    [webView synchronouslyLoadHTMLString:staticContent];

    // A second request while the first one waits for seeds gets the same mirror.
    __block unsigned completedCount = 0;
    __block bool done = false;
    __block RetainPtr<CALayer> firstContainerLayer;
    __block RetainPtr<CALayer> secondContainerLayer;
    [webView _enableCaptureMirrorLayerTreeForTesting:^(CALayer *layer) {
        firstContainerLayer = layer;
        done = ++completedCount == 2;
    }];
    [webView _enableCaptureMirrorLayerTreeForTesting:^(CALayer *layer) {
        secondContainerLayer = layer;
        done = ++completedCount == 2;
    }];
    Util::run(&done);
    EXPECT_NOT_NULL(firstContainerLayer.get());
    EXPECT_EQ(firstContainerLayer.get(), secondContainerLayer.get());

    [webView _disableCaptureMirrorLayerTreeForTesting];
}

TEST(CaptureMirrorLayerTree, CrossSiteNavigationInvalidatesMirror)
{
    HTTPServer server({
        { "/first"_s, { "<div style='width: 100px; height: 100px; background: red; will-change: transform'></div>"_s } },
        { "/second"_s, { "<div style='width: 50px; height: 50px; background: blue; will-change: transform'></div>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/first"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    CaptureMirror mirror(webView.get());
    ASSERT_NOT_NULL(mirror.containerLayer());
    mirror.expectMatchesPrimaryTree();

    // Swapping processes replaces the drawing area, and its mirror with it. The mirror must not keep stale content.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/second"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(Util::waitFor([&] {
        return ![mirror.containerLayer() sublayers].count;
    }));
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
