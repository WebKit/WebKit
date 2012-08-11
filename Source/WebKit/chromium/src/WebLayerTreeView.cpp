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
#include <public/WebLayerTreeView.h>

#include "LayerChromium.h"
#include "WebLayerTreeViewImpl.h"
#include "cc/CCFontAtlas.h"
#include "cc/CCGraphicsContext.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCRenderingStats.h"
#include <public/WebLayer.h>
#include <public/WebPoint.h>
#include <public/WebRect.h>
#include <public/WebRenderingStats.h>
#include <public/WebSize.h>

using namespace WebCore;

namespace WebKit {

WebLayerTreeView::Settings::operator CCLayerTreeSettings() const
{
    CCLayerTreeSettings settings;
    settings.showFPSCounter = showFPSCounter;
    settings.showPlatformLayerTree = showPlatformLayerTree;
    settings.showPaintRects = showPaintRects;
    settings.renderVSyncEnabled = renderVSyncEnabled;
    settings.refreshRate = refreshRate;
    settings.defaultTileSize = defaultTileSize;
    settings.maxUntiledLayerSize = maxUntiledLayerSize;
    settings.acceleratePainting = acceleratePainting;

    // FIXME: showFPSCounter / showPlatformLayerTree / maxPartialTextureUpdates aren't supported currently.
    return settings;
}

void WebLayerTreeView::reset()
{
    m_private.reset(0);
}

bool WebLayerTreeView::isNull() const
{
    return !m_private.get();
}

bool WebLayerTreeView::initialize(WebLayerTreeViewClient* client, const WebLayer& root, const WebLayerTreeView::Settings& settings)
{
    // We have to leak the pointer here into a WebPrivateOwnPtr. We free this object in reset().
    m_private.reset(WebLayerTreeViewImpl::create(client, root, settings).leakPtr());
    return !isNull();
}

void WebLayerTreeView::setSurfaceReady()
{
    m_private->layerTreeHost()->setSurfaceReady();
}

void WebLayerTreeView::setRootLayer(WebLayer *root)
{
    if (root)
        m_private->layerTreeHost()->setRootLayer(*root);
    else
        m_private->layerTreeHost()->setRootLayer(PassRefPtr<LayerChromium>());
}

int WebLayerTreeView::compositorIdentifier()
{
    return m_private->layerTreeHost()->compositorIdentifier();
}

void WebLayerTreeView::setViewportSize(const WebSize& layoutViewportSize, const WebSize& deviceViewportSize)
{
    if (!deviceViewportSize.isEmpty())
        m_private->layerTreeHost()->setViewportSize(layoutViewportSize, deviceViewportSize);
    else
        m_private->layerTreeHost()->setViewportSize(layoutViewportSize, layoutViewportSize);
}

WebSize WebLayerTreeView::layoutViewportSize() const
{
    return WebSize(m_private->layerTreeHost()->layoutViewportSize());
}

WebSize WebLayerTreeView::deviceViewportSize() const
{
    return WebSize(m_private->layerTreeHost()->deviceViewportSize());
}

void WebLayerTreeView::setDeviceScaleFactor(const float deviceScaleFactor)
{
    m_private->layerTreeHost()->setDeviceScaleFactor(deviceScaleFactor);
}

float WebLayerTreeView::deviceScaleFactor() const
{
    return m_private->layerTreeHost()->deviceScaleFactor();
}

void WebLayerTreeView::setBackgroundColor(WebColor color)
{
    m_private->layerTreeHost()->setBackgroundColor(color);
}

void WebLayerTreeView::setHasTransparentBackground(bool transparent)
{
    m_private->layerTreeHost()->setHasTransparentBackground(transparent);
}

void WebLayerTreeView::setVisible(bool visible)
{
    m_private->layerTreeHost()->setVisible(visible);
}

void WebLayerTreeView::setPageScaleFactorAndLimits(float pageScaleFactor, float minimum, float maximum)
{
    m_private->layerTreeHost()->setPageScaleFactorAndLimits(pageScaleFactor, minimum, maximum);
}

void WebLayerTreeView::startPageScaleAnimation(const WebPoint& scroll, bool useAnchor, float newPageScale, double durationSec)
{
    m_private->layerTreeHost()->startPageScaleAnimation(IntSize(scroll.x, scroll.y), useAnchor, newPageScale, durationSec);
}

void WebLayerTreeView::setNeedsAnimate()
{
    m_private->layerTreeHost()->setNeedsAnimate();
}

void WebLayerTreeView::setNeedsRedraw()
{
    m_private->layerTreeHost()->setNeedsRedraw();
}

bool WebLayerTreeView::commitRequested() const
{
    return m_private->layerTreeHost()->commitRequested();
}

void WebLayerTreeView::composite()
{
    if (CCProxy::hasImplThread())
        m_private->layerTreeHost()->setNeedsCommit();
    else
        m_private->layerTreeHost()->composite();
}

void WebLayerTreeView::updateAnimations(double frameBeginTime)
{
    m_private->layerTreeHost()->updateAnimations(frameBeginTime);
}

bool WebLayerTreeView::compositeAndReadback(void *pixels, const WebRect& rect)
{
    return m_private->layerTreeHost()->compositeAndReadback(pixels, rect);
}

void WebLayerTreeView::finishAllRendering()
{
    m_private->layerTreeHost()->finishAllRendering();
}

void WebLayerTreeView::renderingStats(WebRenderingStats& stats) const
{
    CCRenderingStats ccStats;
    m_private->layerTreeHost()->renderingStats(ccStats);

    stats.numAnimationFrames = ccStats.numAnimationFrames;
    stats.numFramesSentToScreen = ccStats.numFramesSentToScreen;
    stats.droppedFrameCount = ccStats.droppedFrameCount;
    stats.totalPaintTimeInSeconds = ccStats.totalPaintTimeInSeconds;
    stats.totalRasterizeTimeInSeconds = ccStats.totalRasterizeTimeInSeconds;
}

void WebLayerTreeView::setFontAtlas(SkBitmap bitmap, WebRect asciiToWebRectTable[128], int fontHeight)
{
    IntRect asciiToRectTable[128];
    for (int i = 0; i < 128; ++i)
        asciiToRectTable[i] = asciiToWebRectTable[i];
    OwnPtr<CCFontAtlas> fontAtlas = CCFontAtlas::create(bitmap, asciiToRectTable, fontHeight);
    m_private->layerTreeHost()->setFontAtlas(fontAtlas.release());
}

void WebLayerTreeView::loseCompositorContext(int numTimes)
{
    m_private->layerTreeHost()->loseContext(numTimes);
}

} // namespace WebKit
