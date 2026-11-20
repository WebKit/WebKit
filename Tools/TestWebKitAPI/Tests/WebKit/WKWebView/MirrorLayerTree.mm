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
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

namespace TestWebKitAPI {

static constexpr _WKCALayerTreeAsTextOptions mirrorComparisonOptions = _WKCALayerTreeAsTextOptionsRemoteLayersOnly
    | _WKCALayerTreeAsTextOptionsIncludeLayerIdentifiers
    | _WKCALayerTreeAsTextOptionsIncludeLayerClasses
    | _WKCALayerTreeAsTextOptionsIncludeNames
    | _WKCALayerTreeAsTextOptionsIncludeTransforms
    | _WKCALayerTreeAsTextOptionsIncludeVisibility
    | _WKCALayerTreeAsTextOptionsIncludeContents
    | _WKCALayerTreeAsTextOptionsIncludeAnimations;

// Video layers are compared by the context they host rather than by class or name.
static constexpr _WKCALayerTreeAsTextOptions videoComparisonOptions = mirrorComparisonOptions & ~(_WKCALayerTreeAsTextOptionsIncludeLayerClasses | _WKCALayerTreeAsTextOptionsIncludeNames);

class LayerTreeMirror {
public:
    explicit LayerTreeMirror(TestWKWebView *webView)
        : m_webView(webView)
        , m_window(adoptNS([[NSWindow alloc] initWithContentRect:webView.bounds styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]))
    {
        [m_window contentView].wantsLayer = YES;
        enable();
    }

    ~LayerTreeMirror()
    {
        [m_webView _disableMirrorLayerTreeForTesting];
    }

    void enable()
    {
        __block bool done = false;
        __block RetainPtr<CALayer> layer;
        [m_webView _enableMirrorLayerTreeForTesting:^(CALayer *mirrorLayer) {
            layer = mirrorLayer;
            done = true;
        }];
        Util::run(&done);

        m_layer = WTF::move(layer);
        if (m_layer)
            [[[m_window contentView] layer] addSublayer:m_layer];
    }

    void disable()
    {
        [m_webView _disableMirrorLayerTreeForTesting];
        [m_layer removeFromSuperlayer];
    }

    CALayer *layer() const { return m_layer; }

    NSString *mirrorTreeAsText(_WKCALayerTreeAsTextOptions options = mirrorComparisonOptions) const
    {
        return [m_webView _caLayerTreeAsTextForLayer:m_layer options:options];
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

private:
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<NSWindow> m_window;
    RetainPtr<CALayer> m_layer;
};

static RetainPtr<TestWKWebView> createMirrorWebView(void (^configure)(WKWebViewConfiguration *) = nil)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    if (configure)
        configure(configuration);
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
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

TEST(MirrorLayerTree, StaticContent)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:staticContent];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();

    RetainPtr mirrorTree = mirror.mirrorTreeAsText();
    EXPECT_TRUE([mirrorTree containsString:@"layer contents"]);
    EXPECT_TRUE([mirrorTree containsString:@"CATransformLayer"]);
    EXPECT_TRUE([mirrorTree containsString:@"CABackdropLayer"]);

    EXPECT_TRUE(CGSizeEqualToSize([mirror.layer() bounds].size, [webView bounds].size));
    EXPECT_EQ([mirror.layer() contentsAreFlipped], [[webView layer] contentsAreFlipped]);
    EXPECT_TRUE(CATransform3DIsIdentity([mirror.layer() sublayerTransform]));
    EXPECT_TRUE([mirror.layer() masksToBounds]);
}

TEST(MirrorLayerTree, FollowsWebViewSize)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:staticContent];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    EXPECT_TRUE(CGSizeEqualToSize([mirror.layer() bounds].size, CGSizeMake(800, 600)));

    [webView setFrame:NSMakeRect(0, 0, 640, 480)];
    EXPECT_TRUE(Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return CGSizeEqualToSize([mirror.layer() bounds].size, CGSizeMake(640, 480));
    }));
    mirror.expectMatchesPrimaryTree();
}

TEST(MirrorLayerTree, AttachingDoesNotChangePrimaryTree)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:staticContent];
    [webView waitForNextPresentationUpdate];

    RetainPtr primaryTreeBefore = [webView _caLayerTreeAsTextWithOptions:mirrorComparisonOptions];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ(primaryTreeBefore, [webView _caLayerTreeAsTextWithOptions:mirrorComparisonOptions]);
}

TEST(MirrorLayerTree, UpdatesAfterAttaching)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:@"<style>div { width: 100px; height: 100px; will-change: transform; }</style><div id='first' style='background: red'></div>"];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
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

TEST(MirrorLayerTree, FlippedSuperlayer)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:staticContent];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());

    // Like the layer of a flipped view, a flipped superlayer must not flip the mirror again.
    RetainPtr flippedLayer = adoptNS([[CALayer alloc] init]);
    [flippedLayer setGeometryFlipped:YES];
    [[mirror.layer() superlayer] addSublayer:flippedLayer.get()];
    [flippedLayer addSublayer:mirror.layer()];

    // The mirror layer geometry is updated with the next commit.
    [webView objectByEvaluatingJavaScript:@"document.getElementById('solid').style.background = 'red'; 1"];
    mirror.expectMatchesPrimaryTree();
    EXPECT_EQ([mirror.layer() contentsAreFlipped], [[webView layer] contentsAreFlipped]);
}

struct Pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;

    friend bool operator==(const Pixel&, const Pixel&) = default;
};

static Pixel pixelAtCenter(CALayer *layer)
{
    auto width = static_cast<size_t>(CGRectGetWidth([layer bounds]));
    auto height = static_cast<size_t>(CGRectGetHeight([layer bounds]));
    Vector<uint8_t> pixels(FillWith { }, width * height * 4, 0);
    RetainPtr colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    RetainPtr context = adoptCF(CGBitmapContextCreate(pixels.mutableSpan().data(), width, height, 8, 4 * width, colorSpace.get(), static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big)));
    [layer renderInContext:context.get()];

    auto offset = 4 * ((height / 2) * width + width / 2);
    return { pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3] };
}

TEST(MirrorLayerTree, RepaintsInPlace)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:@""
        "<style>"
        "body { margin: 0; }"
        "#target { width: 100vw; height: 100vh; will-change: transform; }"
        "#target > div { width: 100%; height: 100%; background: rgb(255, 0, 0); }"
        "</style>"
        "<div id='target'><div></div></div>"];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());

    // Repainting a non-composited child repaints the backing store of #target without changing its layer.
    // After a few repaints, the primary layer is given buffers it already displayed, with a dirty rect.
    for (NSString *color : @[ @"rgb(0, 255, 0)", @"rgb(0, 0, 255)", @"rgb(255, 0, 0)", @"rgb(0, 255, 0)", @"rgb(0, 0, 255)" ]) {
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('#target > div').style.background = '%@'; 1", color]];
        [webView waitForNextPresentationUpdate];

        auto primaryPixel = pixelAtCenter([webView layer]);
        auto mirrorPixel = pixelAtCenter(mirror.layer());
        EXPECT_EQ(primaryPixel, mirrorPixel);
        EXPECT_EQ(mirrorPixel.red, [color isEqualToString:@"rgb(255, 0, 0)"] ? 255 : 0);
        EXPECT_EQ(mirrorPixel.green, [color isEqualToString:@"rgb(0, 255, 0)"] ? 255 : 0);
        EXPECT_EQ(mirrorPixel.blue, [color isEqualToString:@"rgb(0, 0, 255)"] ? 255 : 0);
    }
}

TEST(MirrorLayerTree, WebGL)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:@""
        "<canvas id='canvas' width='200' height='200'></canvas>"
        "<script>"
        "const gl = document.getElementById('canvas').getContext('webgl', { preserveDrawingBuffer: true });"
        "function draw(r, g, b) { gl.clearColor(r, g, b, 1); gl.clear(gl.COLOR_BUFFER_BIT); }"
        "draw(1, 0, 0);"
        "</script>"];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();
    EXPECT_TRUE([mirror.mirrorTreeAsText() containsString:@"layer contents"]);

    // New WebGL frames are displayed in new buffers.
    [webView objectByEvaluatingJavaScript:@"draw(0, 1, 0); 1"];
    mirror.expectMatchesPrimaryTree();

    [webView objectByEvaluatingJavaScript:@"draw(0, 0, 1); 1"];
    mirror.expectMatchesPrimaryTree();
}

TEST(MirrorLayerTree, Video)
{
    RetainPtr webView = createMirrorWebView();
    __block bool isPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{
        isPlaying = true;
    }];
    [webView synchronouslyLoadTestPageNamed:@"video-without-audio"];
    Util::run(&isPlaying);
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.querySelector('video').currentTime > 0"] boolValue];
    }));
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree(videoComparisonOptions);

    // Both trees host the GPU process video layer.
    EXPECT_TRUE([mirror.mirrorTreeAsText(videoComparisonOptions) containsString:@"layer hosted context"]);

    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').style.width = '480px'; 1"];
    mirror.expectMatchesPrimaryTree(videoComparisonOptions);
}

TEST(MirrorLayerTree, SiteIsolatedIframe)
{
    HTTPServer server({
        { "/main"_s, { "<style>iframe { width: 400px; height: 300px; }</style><iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<div style='width: 137px; height: 59px; background: linear-gradient(red, blue); will-change: transform'></div>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // Wait for the cross-origin iframe, composited in its own process.
    EXPECT_TRUE(Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return [[webView _caLayerTreeAsText] containsString:@"width: 137 height: 59"];
    }));

    RetainPtr primaryTreeBefore = [webView _caLayerTreeAsTextWithOptions:mirrorComparisonOptions];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();
    EXPECT_WK_STREQ(primaryTreeBefore, [webView _caLayerTreeAsTextWithOptions:mirrorComparisonOptions]);
    EXPECT_TRUE([mirror.mirrorTreeAsText() containsString:@"width: 137 height: 59"]);

    [webView evaluateJavaScript:@"document.querySelector('div').style.width = '200px'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return [[webView _caLayerTreeAsText] containsString:@"width: 200 height: 59"];
    }));
    mirror.expectMatchesPrimaryTree();
}

static bool waitForPrimaryTreeToContain(TestWKWebView *webView, NSString *layerDescription)
{
    return Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return [[webView _caLayerTreeAsText] containsString:layerDescription];
    });
}

TEST(MirrorLayerTree, ThirdPartyIframesFromSeveralSites)
{
    HTTPServer server({
        { "/main"_s, { "<style>iframe { width: 300px; height: 200px; }</style><iframe src='https://webkit.org/first'></iframe><iframe src='https://apple.com/second'></iframe>"_s } },
        { "/first"_s, { "<div style='width: 137px; height: 59px; background: red; will-change: transform'></div>"_s } },
        { "/second"_s, { "<div style='width: 61px; height: 43px; background: blue; will-change: transform'></div>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE(waitForPrimaryTreeToContain(webView, @"width: 137 height: 59"));
    EXPECT_TRUE(waitForPrimaryTreeToContain(webView, @"width: 61 height: 43"));

    // Each iframe process sends its own full state.
    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();
    EXPECT_TRUE([mirror.mirrorTreeAsText() containsString:@"width: 137 height: 59"]);
    EXPECT_TRUE([mirror.mirrorTreeAsText() containsString:@"width: 61 height: 43"]);

    [webView evaluateJavaScript:@"document.querySelector('div').style.width = '200px'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_TRUE(waitForPrimaryTreeToContain(webView, @"width: 200 height: 59"));
    mirror.expectMatchesPrimaryTree();
}

TEST(MirrorLayerTree, ThirdPartyIframeAddedAfterAttaching)
{
    HTTPServer server({
        { "/main"_s, { "<div style='width: 100px; height: 100px; background: green; will-change: transform'></div>"_s } },
        { "/iframe"_s, { "<div style='width: 137px; height: 59px; background: red; will-change: transform'></div>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();

    // The iframe process joins the page after the mirror was created, so its commits send all its layers.
    [webView evaluateJavaScript:@"let iframe = document.createElement('iframe'); iframe.style.width = '300px'; iframe.style.height = '200px'; iframe.src = 'https://webkit.org/iframe'; document.body.appendChild(iframe)" completionHandler:nil];
    EXPECT_TRUE(waitForPrimaryTreeToContain(webView, @"width: 137 height: 59"));
    mirror.expectMatchesPrimaryTree();
}

TEST(MirrorLayerTree, HiddenThirdPartyIframe)
{
    HTTPServer server({
        { "/main"_s, { "<style>iframe { width: 300px; height: 200px; display: none; }</style><iframe src='https://webkit.org/iframe'></iframe><div style='width: 100px; height: 100px; background: green; will-change: transform'></div>"_s } },
        { "/iframe"_s, { "<div style='width: 137px; height: 59px; background: red; will-change: transform'></div>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    // The process of a hidden iframe may never render, so the mirror must not wait for its full state.
    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();

    [webView evaluateJavaScript:@"document.querySelector('iframe').style.display = 'block'" completionHandler:nil];
    EXPECT_TRUE(waitForPrimaryTreeToContain(webView, @"width: 137 height: 59"));
    mirror.expectMatchesPrimaryTree();
}

TEST(MirrorLayerTree, ThirdPartyIframeRemovedWhileAttaching)
{
    HTTPServer server({
        { "/main"_s, { "<style>iframe { width: 300px; height: 200px; }</style><iframe src='https://webkit.org/iframe'></iframe><div style='width: 100px; height: 100px; background: green; will-change: transform'></div>"_s } },
        { "/iframe"_s, { "<div style='width: 137px; height: 59px; background: red; will-change: transform'></div>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_TRUE(waitForPrimaryTreeToContain(webView, @"width: 137 height: 59"));

    // The iframe process can outlive its part in the page, and then never sends its full state, which must not keep the mirror waiting.
    __block bool done = false;
    __block RetainPtr<CALayer> layer;
    [webView _enableMirrorLayerTreeForTesting:^(CALayer *mirrorLayer) {
        layer = mirrorLayer;
        done = true;
    }];
    [webView evaluateJavaScript:@"document.querySelector('iframe').remove()" completionHandler:nil];
    Util::run(&done);
    ASSERT_NOT_NULL(layer);

    EXPECT_TRUE(Util::waitFor([&] {
        [webView waitForNextPresentationUpdate];
        return ![[webView _caLayerTreeAsText] containsString:@"width: 137 height: 59"];
    }));
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ([webView _caLayerTreeAsTextWithOptions:mirrorComparisonOptions], [webView _caLayerTreeAsTextForLayer:layer options:mirrorComparisonOptions]);

    [webView _disableMirrorLayerTreeForTesting];
}

TEST(MirrorLayerTree, DisableAndEnableAgain)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:staticContent];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();

    RetainPtr previousLayer = mirror.layer();
    mirror.disable();
    EXPECT_EQ([previousLayer sublayers].count, 0u);

    [webView objectByEvaluatingJavaScript:@"document.getElementById('solid').style.background = 'black'; 1"];
    [webView waitForNextPresentationUpdate];

    mirror.enable();
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();
}

TEST(MirrorLayerTree, DisableWhileAttachingAndEnableAgain)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:staticContent];
    [webView waitForNextPresentationUpdate];

    __block bool done = false;
    __block RetainPtr<CALayer> layer;
    [webView _enableMirrorLayerTreeForTesting:^(CALayer *mirrorLayer) {
        layer = mirrorLayer;
        done = true;
    }];
    [webView _disableMirrorLayerTreeForTesting];
    Util::run(&done);
    EXPECT_NULL(layer);

    // The full state requested for the disabled mirror arrives with no mirror to apply it to.
    // It must not keep the next mirror from requesting its own.
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();
}

TEST(MirrorLayerTree, EnableTwiceWhileAttaching)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:staticContent];

    // There is only one mirror, so a second request while the first one waits for full states fails.
    __block bool done = false;
    __block bool secondRequestDone = false;
    __block RetainPtr<CALayer> firstLayer;
    __block RetainPtr<CALayer> secondLayer;
    [webView _enableMirrorLayerTreeForTesting:^(CALayer *layer) {
        firstLayer = layer;
        done = true;
    }];
    [webView _enableMirrorLayerTreeForTesting:^(CALayer *layer) {
        secondLayer = layer;
        secondRequestDone = true;
    }];
    Util::run(&secondRequestDone);
    EXPECT_NULL(secondLayer);
    EXPECT_FALSE(done);

    Util::run(&done);
    EXPECT_NOT_NULL(firstLayer);

    [webView _disableMirrorLayerTreeForTesting];
}

TEST(MirrorLayerTree, EnableTwiceWhenAttached)
{
    RetainPtr webView = createMirrorWebView();
    [webView synchronouslyLoadHTMLString:@"<style>div { width: 100px; height: 100px; will-change: transform; }</style><div id='first' style='background: red'></div>"];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());

    __block bool done = false;
    __block RetainPtr<CALayer> secondLayer;
    [webView _enableMirrorLayerTreeForTesting:^(CALayer *layer) {
        secondLayer = layer;
        done = true;
    }];
    Util::run(&done);
    EXPECT_NULL(secondLayer);

    // The failed request leaves the first mirror in place.
    [webView objectByEvaluatingJavaScript:@"document.getElementById('first').style.background = 'blue'; 1"];
    mirror.expectMatchesPrimaryTree();
}

TEST(MirrorLayerTree, CrossSiteNavigationInvalidatesMirror)
{
    HTTPServer server({
        { "/first"_s, { "<div style='width: 100px; height: 100px; background: red; will-change: transform'></div>"_s } },
        { "/second"_s, { "<div style='width: 50px; height: 50px; background: blue; will-change: transform'></div>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView setNavigationDelegate:navigationDelegate];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/first"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    LayerTreeMirror mirror(webView);
    ASSERT_NOT_NULL(mirror.layer());
    mirror.expectMatchesPrimaryTree();

    // Swapping processes replaces the drawing area, and its mirror with it. The mirror must not keep stale content.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/second"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(Util::waitFor([&] {
        return ![mirror.layer() sublayers].count;
    }));
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
