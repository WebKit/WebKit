/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ScrollingCoordinator.h"

#include "CompositorFakeWebGraphicsContext3D.h"
#include "FrameTestHelpers.h"
#include "GraphicsLayerChromium.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "URLTestHelpers.h"
#include "WebCompositorInitializer.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebSettings.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include <gtest/gtest.h>
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebLayer.h>
#include <public/WebUnitTestSupport.h>

using namespace WebCore;
using namespace WebKit;

namespace {

class FakeWebViewClient : public WebViewClient {
public:
    virtual WebCompositorOutputSurface* createOutputSurface() OVERRIDE
    {
        return Platform::current()->compositorSupport()->createOutputSurfaceFor3D(CompositorFakeWebGraphicsContext3D::create(WebGraphicsContext3D::Attributes()).leakPtr());
    }

    virtual void initializeLayerTreeView(WebLayerTreeViewClient* client, const WebLayer& rootLayer, const WebLayerTreeView::Settings& settings)
    {
        m_layerTreeView = adoptPtr(Platform::current()->unitTestSupport()->createLayerTreeViewForTesting(WebUnitTestSupport::TestViewTypeUnitTest));
        ASSERT(m_layerTreeView);
        m_layerTreeView->setRootLayer(rootLayer);
    }

    virtual WebLayerTreeView* layerTreeView()
    {
        return m_layerTreeView.get();
    }

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
};

class MockWebFrameClient : public WebFrameClient {
};

class ScrollingCoordinatorChromiumTest : public testing::Test {
public:
    ScrollingCoordinatorChromiumTest()
        : m_baseURL("http://www.test.com/")
    {
        Platform::current()->compositorSupport()->initialize(0);

        // We cannot reuse FrameTestHelpers::createWebViewAndLoad here because the compositing
        // settings need to be set before the page is loaded.
        m_webViewImpl = static_cast<WebViewImpl*>(WebView::create(&m_mockWebViewClient));
        m_webViewImpl->settings()->setJavaScriptEnabled(true);
        m_webViewImpl->settings()->setForceCompositingMode(true);
        m_webViewImpl->settings()->setAcceleratedCompositingEnabled(true);
        m_webViewImpl->settings()->setAcceleratedCompositingForFixedPositionEnabled(true);
        m_webViewImpl->settings()->setAcceleratedCompositingForOverflowScrollEnabled(true);
        m_webViewImpl->settings()->setAcceleratedCompositingForScrollableFramesEnabled(true);
        m_webViewImpl->settings()->setCompositedScrollingForFramesEnabled(true);
        m_webViewImpl->settings()->setFixedPositionCreatesStackingContext(true);
        m_webViewImpl->initializeMainFrame(&m_mockWebFrameClient);
        m_webViewImpl->resize(IntSize(320, 240));
    }

    virtual ~ScrollingCoordinatorChromiumTest()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
        m_webViewImpl->close();

        Platform::current()->compositorSupport()->shutdown();
    }

    void navigateTo(const std::string& url)
    {
        FrameTestHelpers::loadFrame(m_webViewImpl->mainFrame(), url);
        Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

    WebLayer* getRootScrollLayer()
    {
        RenderLayerCompositor* compositor = m_webViewImpl->mainFrameImpl()->frame()->contentRenderer()->compositor();
        ASSERT(compositor);
        ASSERT(compositor->scrollLayer());

        WebLayer* webScrollLayer = static_cast<WebLayer*>(compositor->scrollLayer()->platformLayer());
        return webScrollLayer;
    }

protected:
    std::string m_baseURL;
    MockWebFrameClient m_mockWebFrameClient;
    FakeWebViewClient m_mockWebViewClient;
    WebViewImpl* m_webViewImpl;
};

TEST_F(ScrollingCoordinatorChromiumTest, fastScrollingByDefault)
{
    navigateTo("about:blank");

    // Make sure the scrolling coordinator is active.
    FrameView* frameView = m_webViewImpl->mainFrameImpl()->frameView();
    Page* page = m_webViewImpl->mainFrameImpl()->frame()->page();
    ASSERT_TRUE(page->scrollingCoordinator());
    ASSERT_TRUE(page->scrollingCoordinator()->coordinatesScrollingForFrameView(frameView));

    // Fast scrolling should be enabled by default.
    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_TRUE(rootScrollLayer->scrollable());
    ASSERT_FALSE(rootScrollLayer->shouldScrollOnMainThread());
    ASSERT_FALSE(rootScrollLayer->haveWheelEventHandlers());
}

TEST_F(ScrollingCoordinatorChromiumTest, fastScrollingForFixedPosition)
{
    registerMockedHttpURLLoad("fixed-position.html");
    navigateTo(m_baseURL + "fixed-position.html");

    Page* page = m_webViewImpl->mainFrameImpl()->frame()->page();
    ASSERT_TRUE(page->scrollingCoordinator()->supportsFixedPositionLayers());

    // Fixed position should not fall back to main thread scrolling.
    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_FALSE(rootScrollLayer->shouldScrollOnMainThread());

    // Verify the properties of the fixed position element starting from the RenderObject all the
    // way to the WebLayer.
    Element* fixedElement = m_webViewImpl->mainFrameImpl()->frame()->document()->getElementById("fixed");
    ASSERT(fixedElement);

    RenderObject* renderer = fixedElement->renderer();
    ASSERT_TRUE(renderer->isBoxModelObject());
    ASSERT_TRUE(renderer->hasLayer());

    RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();
    ASSERT_TRUE(layer->isComposited());

    RenderLayerBacking* layerBacking = layer->backing();
    WebLayer* webLayer = static_cast<WebLayer*>(layerBacking->graphicsLayer()->platformLayer());
    ASSERT_TRUE(webLayer->fixedToContainerLayer());
}

TEST_F(ScrollingCoordinatorChromiumTest, nonFastScrollableRegion)
{
    registerMockedHttpURLLoad("non-fast-scrollable.html");
    navigateTo(m_baseURL + "non-fast-scrollable.html");

    WebLayer* rootScrollLayer = getRootScrollLayer();
    WebVector<WebRect> nonFastScrollableRegion = rootScrollLayer->nonFastScrollableRegion();

    ASSERT_EQ(1u, nonFastScrollableRegion.size());
    ASSERT_EQ(WebRect(8, 8, 10, 10), nonFastScrollableRegion[0]);
}

TEST_F(ScrollingCoordinatorChromiumTest, wheelEventHandler)
{
    registerMockedHttpURLLoad("wheel-event-handler.html");
    navigateTo(m_baseURL + "wheel-event-handler.html");

    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_TRUE(rootScrollLayer->haveWheelEventHandlers());
}

TEST_F(ScrollingCoordinatorChromiumTest, clippedBodyTest)
{
    registerMockedHttpURLLoad("clipped-body.html");
    navigateTo(m_baseURL + "clipped-body.html");

    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_EQ(0u, rootScrollLayer->nonFastScrollableRegion().size());
}

TEST_F(ScrollingCoordinatorChromiumTest, overflowScrolling)
{
    registerMockedHttpURLLoad("overflow-scrolling.html");
    navigateTo(m_baseURL + "overflow-scrolling.html");

    // Verify the properties of the accelerated scrolling element starting from the RenderObject
    // all the way to the WebLayer.
    Element* scrollableElement = m_webViewImpl->mainFrameImpl()->frame()->document()->getElementById("scrollable");
    ASSERT(scrollableElement);

    RenderObject* renderer = scrollableElement->renderer();
    ASSERT_TRUE(renderer->isBoxModelObject());
    ASSERT_TRUE(renderer->hasLayer());

    RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();
    ASSERT_TRUE(layer->usesCompositedScrolling());
    ASSERT_TRUE(layer->isComposited());

    RenderLayerBacking* layerBacking = layer->backing();
    ASSERT_TRUE(layerBacking->hasScrollingLayer());
    ASSERT(layerBacking->scrollingContentsLayer());

    GraphicsLayerChromium* graphicsLayerChromium = static_cast<GraphicsLayerChromium*>(layerBacking->scrollingContentsLayer());
    ASSERT_EQ(layer, graphicsLayerChromium->scrollableArea());

    WebLayer* webScrollLayer = static_cast<WebLayer*>(layerBacking->scrollingContentsLayer()->platformLayer());
    ASSERT_TRUE(webScrollLayer->scrollable());

#if !OS(DARWIN) && !OS(WINDOWS)
    // Now verify we've attached impl-side scrollbars onto the scrollbar layers
    ASSERT_TRUE(layerBacking->layerForHorizontalScrollbar());
    ASSERT_TRUE(layerBacking->layerForHorizontalScrollbar()->hasContentsLayer());
    ASSERT_TRUE(layerBacking->layerForVerticalScrollbar());
    ASSERT_TRUE(layerBacking->layerForVerticalScrollbar()->hasContentsLayer());
#endif
}

TEST_F(ScrollingCoordinatorChromiumTest, iframeScrolling)
{
    registerMockedHttpURLLoad("iframe-scrolling.html");
    registerMockedHttpURLLoad("iframe-scrolling-inner.html");
    navigateTo(m_baseURL + "iframe-scrolling.html");

    // Verify the properties of the accelerated scrolling element starting from the RenderObject
    // all the way to the WebLayer.
    Element* scrollableFrame = m_webViewImpl->mainFrameImpl()->frame()->document()->getElementById("scrollable");
    ASSERT_TRUE(scrollableFrame);

    RenderObject* renderer = scrollableFrame->renderer();
    ASSERT_TRUE(renderer);
    ASSERT_TRUE(renderer->isWidget());

    RenderWidget* renderWidget = toRenderWidget(renderer);
    ASSERT_TRUE(renderWidget);
    ASSERT_TRUE(renderWidget->widget());
    ASSERT_TRUE(renderWidget->widget()->isFrameView());

    FrameView* innerFrameView = static_cast<FrameView*>(renderWidget->widget());
    RenderView* innerRenderView = innerFrameView->renderView();
    ASSERT_TRUE(innerRenderView);

    RenderLayerCompositor* innerCompositor = innerRenderView->compositor();
    ASSERT_TRUE(innerCompositor->inCompositingMode());
    ASSERT_TRUE(innerCompositor->scrollLayer());

    GraphicsLayerChromium* scrollLayer = static_cast<GraphicsLayerChromium*>(innerCompositor->scrollLayer());
    ASSERT_EQ(innerFrameView, scrollLayer->scrollableArea());

    WebLayer* webScrollLayer = static_cast<WebLayer*>(scrollLayer->platformLayer());
    ASSERT_TRUE(webScrollLayer->scrollable());

#if !OS(DARWIN) && !OS(WINDOWS)
    // Now verify we've attached impl-side scrollbars onto the scrollbar layers
    ASSERT_TRUE(innerCompositor->layerForHorizontalScrollbar());
    ASSERT_TRUE(innerCompositor->layerForHorizontalScrollbar()->hasContentsLayer());
    ASSERT_TRUE(innerCompositor->layerForVerticalScrollbar());
    ASSERT_TRUE(innerCompositor->layerForVerticalScrollbar()->hasContentsLayer());
#endif
}

} // namespace
