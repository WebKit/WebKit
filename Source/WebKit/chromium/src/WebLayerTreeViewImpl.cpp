/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebLayerTreeViewImpl.h"

#include "CCFontAtlas.h"
#include "CCInputHandler.h"
#include "CCLayerTreeHost.h"
#include "LayerChromium.h"
#include "WebLayerImpl.h"
#include "WebToCCInputHandlerAdapter.h"
#include <public/WebGraphicsContext3D.h>
#include <public/WebInputHandler.h>
#include <public/WebLayer.h>
#include <public/WebLayerTreeView.h>
#include <public/WebLayerTreeViewClient.h>
#include <public/WebRenderingStats.h>
#include <public/WebSize.h>

using namespace WebCore;

namespace WebKit {

WebLayerTreeView* WebLayerTreeView::create(WebLayerTreeViewClient* client, const WebLayer& root, const WebLayerTreeView::Settings& settings)
{
    OwnPtr<WebLayerTreeViewImpl> layerTreeViewImpl = adoptPtr(new WebLayerTreeViewImpl(client));
    if (!layerTreeViewImpl->initialize(settings))
        return 0;
    layerTreeViewImpl->setRootLayer(root);
    return layerTreeViewImpl.leakPtr();
}

WebLayerTreeViewImpl::WebLayerTreeViewImpl(WebLayerTreeViewClient* client)
    : m_client(client)
{
}

WebLayerTreeViewImpl::~WebLayerTreeViewImpl()
{
}

bool WebLayerTreeViewImpl::initialize(const WebLayerTreeView::Settings& webSettings)
{
    CCLayerTreeSettings settings;
    settings.acceleratePainting = webSettings.acceleratePainting;
    settings.showFPSCounter = webSettings.showFPSCounter;
    settings.showPlatformLayerTree = webSettings.showPlatformLayerTree;
    settings.showPaintRects = webSettings.showPaintRects;
    settings.renderVSyncEnabled = webSettings.renderVSyncEnabled;
    settings.refreshRate = webSettings.refreshRate;
    settings.defaultTileSize = webSettings.defaultTileSize;
    settings.maxUntiledLayerSize = webSettings.maxUntiledLayerSize;
    m_layerTreeHost = CCLayerTreeHost::create(this, settings);
    if (!m_layerTreeHost)
        return false;
    return true;
}

void WebLayerTreeViewImpl::setSurfaceReady()
{
    m_layerTreeHost->setSurfaceReady();
}

void WebLayerTreeViewImpl::setRootLayer(const WebLayer& root)
{
    m_layerTreeHost->setRootLayer(static_cast<const WebLayerImpl*>(&root)->layer());
}

void WebLayerTreeViewImpl::clearRootLayer()
{
    m_layerTreeHost->setRootLayer(PassRefPtr<LayerChromium>());
}

void WebLayerTreeViewImpl::setViewportSize(const WebSize& layoutViewportSize, const WebSize& deviceViewportSize)
{
    if (!deviceViewportSize.isEmpty())
        m_layerTreeHost->setViewportSize(layoutViewportSize, deviceViewportSize);
    else
        m_layerTreeHost->setViewportSize(layoutViewportSize, layoutViewportSize);
}

WebSize WebLayerTreeViewImpl::layoutViewportSize() const
{
    return WebSize(m_layerTreeHost->layoutViewportSize());
}

WebSize WebLayerTreeViewImpl::deviceViewportSize() const
{
    return WebSize(m_layerTreeHost->deviceViewportSize());
}

void WebLayerTreeViewImpl::setDeviceScaleFactor(const float deviceScaleFactor)
{
    m_layerTreeHost->setDeviceScaleFactor(deviceScaleFactor);
}

float WebLayerTreeViewImpl::deviceScaleFactor() const
{
    return m_layerTreeHost->deviceScaleFactor();
}

void WebLayerTreeViewImpl::setBackgroundColor(WebColor color)
{
    m_layerTreeHost->setBackgroundColor(color);
}

void WebLayerTreeViewImpl::setHasTransparentBackground(bool transparent)
{
    m_layerTreeHost->setHasTransparentBackground(transparent);
}

void WebLayerTreeViewImpl::setVisible(bool visible)
{
    m_layerTreeHost->setVisible(visible);
}

void WebLayerTreeViewImpl::setPageScaleFactorAndLimits(float pageScaleFactor, float minimum, float maximum)
{
    m_layerTreeHost->setPageScaleFactorAndLimits(pageScaleFactor, minimum, maximum);
}

void WebLayerTreeViewImpl::startPageScaleAnimation(const WebPoint& scroll, bool useAnchor, float newPageScale, double durationSec)
{
    m_layerTreeHost->startPageScaleAnimation(IntSize(scroll.x, scroll.y), useAnchor, newPageScale, durationSec);
}

void WebLayerTreeViewImpl::setNeedsAnimate()
{
    m_layerTreeHost->setNeedsAnimate();
}

void WebLayerTreeViewImpl::setNeedsRedraw()
{
    m_layerTreeHost->setNeedsRedraw();
}

bool WebLayerTreeViewImpl::commitRequested() const
{
    return m_layerTreeHost->commitRequested();
}

void WebLayerTreeViewImpl::composite()
{
    if (CCProxy::hasImplThread())
        m_layerTreeHost->setNeedsCommit();
    else
        m_layerTreeHost->composite();
}

void WebLayerTreeViewImpl::updateAnimations(double frameBeginTime)
{
    m_layerTreeHost->updateAnimations(frameBeginTime);
}

bool WebLayerTreeViewImpl::compositeAndReadback(void *pixels, const WebRect& rect)
{
    return m_layerTreeHost->compositeAndReadback(pixels, rect);
}

void WebLayerTreeViewImpl::finishAllRendering()
{
    m_layerTreeHost->finishAllRendering();
}

void WebLayerTreeViewImpl::renderingStats(WebRenderingStats& stats) const
{
    CCRenderingStats ccStats;
    m_layerTreeHost->renderingStats(ccStats);

    stats.numAnimationFrames = ccStats.numAnimationFrames;
    stats.numFramesSentToScreen = ccStats.numFramesSentToScreen;
    stats.droppedFrameCount = ccStats.droppedFrameCount;
    stats.totalPaintTimeInSeconds = ccStats.totalPaintTimeInSeconds;
    stats.totalRasterizeTimeInSeconds = ccStats.totalRasterizeTimeInSeconds;
}

void WebLayerTreeViewImpl::setFontAtlas(SkBitmap bitmap, WebRect asciiToWebRectTable[128], int fontHeight)
{
    IntRect asciiToRectTable[128];
    for (int i = 0; i < 128; ++i)
        asciiToRectTable[i] = asciiToWebRectTable[i];
    OwnPtr<CCFontAtlas> fontAtlas = CCFontAtlas::create(bitmap, asciiToRectTable, fontHeight);
    m_layerTreeHost->setFontAtlas(fontAtlas.release());
}

void WebLayerTreeViewImpl::loseCompositorContext(int numTimes)
{
    m_layerTreeHost->loseContext(numTimes);
}

void WebLayerTreeViewImpl::willBeginFrame()
{
    m_client->willBeginFrame();
}

void WebLayerTreeViewImpl::didBeginFrame()
{
    m_client->didBeginFrame();
}

void WebLayerTreeViewImpl::animate(double monotonicFrameBeginTime)
{
    m_client->updateAnimations(monotonicFrameBeginTime);
}

void WebLayerTreeViewImpl::layout()
{
    m_client->layout();
}

void WebLayerTreeViewImpl::applyScrollAndScale(const WebCore::IntSize& scrollDelta, float pageScale)
{
    m_client->applyScrollAndScale(scrollDelta, pageScale);
}

PassOwnPtr<WebCompositorOutputSurface> WebLayerTreeViewImpl::createOutputSurface()
{
    return adoptPtr(m_client->createOutputSurface());
}

void WebLayerTreeViewImpl::didRecreateOutputSurface(bool success)
{
    m_client->didRecreateOutputSurface(success);
}

PassOwnPtr<CCInputHandler> WebLayerTreeViewImpl::createInputHandler()
{
    OwnPtr<WebInputHandler> handler = adoptPtr(m_client->createInputHandler());
    if (handler)
        return WebToCCInputHandlerAdapter::create(handler.release());
    return nullptr;
}

void WebLayerTreeViewImpl::willCommit()
{
    m_client->willCommit();
}

void WebLayerTreeViewImpl::didCommit()
{
    m_client->didCommit();
}

void WebLayerTreeViewImpl::didCommitAndDrawFrame()
{
    m_client->didCommitAndDrawFrame();
}

void WebLayerTreeViewImpl::didCompleteSwapBuffers()
{
    m_client->didCompleteSwapBuffers();
}

void WebLayerTreeViewImpl::scheduleComposite()
{
    m_client->scheduleComposite();
}

} // namespace WebKit
