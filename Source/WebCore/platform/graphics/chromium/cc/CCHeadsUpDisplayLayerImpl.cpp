/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "cc/CCHeadsUpDisplayLayerImpl.h"

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "PlatformCanvas.h"
#include "cc/CCDebugRectHistory.h"
#include "cc/CCFontAtlas.h"
#include "cc/CCFrameRateCounter.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCQuadSink.h"
#include <public/WebCompositorTextureQuad.h>
#include <wtf/text/WTFString.h>

using WebKit::WebCompositorTextureQuad;

namespace WebCore {

CCHeadsUpDisplayLayerImpl::CCHeadsUpDisplayLayerImpl(int id, PassOwnPtr<CCFontAtlas> fontAtlas)
    : CCLayerImpl(id),
      m_fontAtlas(fontAtlas)
{
}

CCHeadsUpDisplayLayerImpl::~CCHeadsUpDisplayLayerImpl()
{
}

void CCHeadsUpDisplayLayerImpl::willDraw(CCResourceProvider* resourceProvider)
{
    CCLayerImpl::willDraw(resourceProvider);

    if (!m_hudTexture)
        m_hudTexture = CCScopedTexture::create(resourceProvider);

    // FIXME: Scale the HUD by deviceScale to make it more friendly under high DPI.

    if (m_hudTexture->size() != bounds())
        m_hudTexture->free();

    if (!m_hudTexture->id() && !m_hudTexture->allocate(CCRenderer::ImplPool, bounds(), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny))
        return;

    // Render pixels into the texture.
    PlatformCanvas canvas;
    canvas.resize(bounds());
    {
        PlatformCanvas::Painter painter(&canvas, PlatformCanvas::Painter::GrayscaleText);
        painter.context()->clearRect(FloatRect(0, 0, bounds().width(), bounds().height()));
        drawHudContents(painter.context());
    }

    {
        PlatformCanvas::AutoLocker locker(&canvas);
        IntRect layerRect(IntPoint(), bounds());
        resourceProvider->upload(m_hudTexture->id(), locker.pixels(), layerRect, layerRect, layerRect);
    }
}

void CCHeadsUpDisplayLayerImpl::appendQuads(CCQuadSink& quadList, const CCSharedQuadState* sharedQuadState, bool&)
{
    if (!m_hudTexture->id())
        return;

    IntRect quadRect(IntPoint(), bounds());
    bool premultipliedAlpha = true;
    FloatRect uvRect(0, 0, 1, 1);
    bool flipped = false;
    quadList.append(WebCompositorTextureQuad::create(sharedQuadState, quadRect, m_hudTexture->id(), premultipliedAlpha, uvRect, flipped));
}

void CCHeadsUpDisplayLayerImpl::didDraw(CCResourceProvider* resourceProvider)
{
    CCLayerImpl::didDraw(resourceProvider);

    if (!m_hudTexture->id())
        return;

    // FIXME: the following assert will not be true when sending resources to a
    // parent compositor. We will probably need to hold on to m_hudTexture for
    // longer, and have several HUD textures in the pipeline.
    ASSERT(!resourceProvider->inUseByConsumer(m_hudTexture->id()));
}

void CCHeadsUpDisplayLayerImpl::didLoseContext()
{
    m_hudTexture->leak();
}

void CCHeadsUpDisplayLayerImpl::drawHudContents(GraphicsContext* context)
{
    const CCLayerTreeSettings& settings = layerTreeHostImpl()->settings();

    if (settings.showPlatformLayerTree) {
        context->setFillColor(Color(0, 0, 0, 192), ColorSpaceDeviceRGB);
        context->fillRect(FloatRect(0, 0, bounds().width(), bounds().height()));
    }

    int fpsCounterHeight = 40;
    int fpsCounterTop = 2;
    int platformLayerTreeTop;

    if (settings.showFPSCounter)
        platformLayerTreeTop = fpsCounterTop + fpsCounterHeight;
    else
        platformLayerTreeTop = 0;

    if (settings.showFPSCounter)
        drawFPSCounter(context, layerTreeHostImpl()->fpsCounter(), fpsCounterTop, fpsCounterHeight);

    if (settings.showPlatformLayerTree && m_fontAtlas) {
        String layerTree = layerTreeHostImpl()->layerTreeAsText();
        m_fontAtlas->drawText(context, layerTree, IntPoint(2, platformLayerTreeTop), bounds());
    }

    if (settings.showDebugRects())
        drawDebugRects(context, layerTreeHostImpl()->debugRectHistory());
}

void CCHeadsUpDisplayLayerImpl::drawFPSCounter(GraphicsContext* context, CCFrameRateCounter* fpsCounter, int top, int height)
{
    float textWidth = 170; // so text fits on linux.
    float graphWidth = fpsCounter->timeStampHistorySize();

    // Draw the FPS text.
    drawFPSCounterText(context, fpsCounter, top, textWidth, height);

    // Draw FPS graph.
    const double loFPS = 0;
    const double hiFPS = 80;
    context->setStrokeStyle(SolidStroke);
    context->setFillColor(Color(154, 205, 50), ColorSpaceDeviceRGB);
    context->fillRect(FloatRect(2 + textWidth, top, graphWidth, height / 2));
    context->setFillColor(Color(255, 250, 205), ColorSpaceDeviceRGB);
    context->fillRect(FloatRect(2 + textWidth, top + height / 2, graphWidth, height / 2));
    context->setStrokeColor(Color(255, 0, 0), ColorSpaceDeviceRGB);
    context->setFillColor(Color(255, 0, 0), ColorSpaceDeviceRGB);

    int graphLeft = static_cast<int>(textWidth + 3);
    IntPoint prev(-1, 0);
    int x = 0;
    double h = static_cast<double>(height - 2);
    for (int i = 0; i < fpsCounter->timeStampHistorySize() - 1; ++i) {
        int j = i + 1;
        double delta = fpsCounter->timeStampOfRecentFrame(j) - fpsCounter->timeStampOfRecentFrame(i);

        // Skip plotting this particular instantaneous frame rate if it is not likely to have been valid.
        if (fpsCounter->isBadFrameInterval(delta)) {
            x += 1;
            continue;
        }

        double fps = 1.0 / delta;

        // Clamp the FPS to the range we want to plot visually.
        double p = 1 - ((fps - loFPS) / (hiFPS - loFPS));
        if (p < 0)
            p = 0;
        if (p > 1)
            p = 1;

        // Plot this data point.
        IntPoint cur(graphLeft + x, 1 + top + p*h);
        if (prev.x() != -1)
            context->drawLine(prev, cur);
        prev = cur;
        x += 1;
    }
}

void CCHeadsUpDisplayLayerImpl::drawFPSCounterText(GraphicsContext* context, CCFrameRateCounter* fpsCounter, int top, int width, int height)
{
    double averageFPS, stdDeviation;
    fpsCounter->getAverageFPSAndStandardDeviation(averageFPS, stdDeviation);

    // Draw background.
    context->setFillColor(Color(0, 0, 0, 255), ColorSpaceDeviceRGB);
    context->fillRect(FloatRect(2, top, width, height));

    // Draw FPS text.
    if (m_fontAtlas)
        m_fontAtlas->drawText(context, String::format("FPS: %4.1f +/- %3.1f", averageFPS, stdDeviation), IntPoint(10, height / 3), IntSize(width, height));
}

void CCHeadsUpDisplayLayerImpl::drawDebugRects(GraphicsContext* context, CCDebugRectHistory* debugRectHistory)
{
    const Vector<CCDebugRect>& debugRects = debugRectHistory->debugRects();
    for (size_t i = 0; i < debugRects.size(); ++i) {

        if (debugRects[i].type == PaintRectType) {
            // Paint rects in red
            context->setStrokeColor(Color(255, 0, 0, 255), ColorSpaceDeviceRGB);
            context->fillRect(debugRects[i].rect, Color(255, 0, 0, 30), ColorSpaceDeviceRGB);
            context->strokeRect(debugRects[i].rect, 2.0);
        }

        if (debugRects[i].type == PropertyChangedRectType) {
            // Property-changed rects in blue
            context->setStrokeColor(Color(0, 0, 255, 255), ColorSpaceDeviceRGB);
            context->fillRect(debugRects[i].rect, Color(0, 0, 255, 30), ColorSpaceDeviceRGB);
            context->strokeRect(debugRects[i].rect, 2.0);
        }

        if (debugRects[i].type == SurfaceDamageRectType) {
            // Surface damage rects in yellow-orange
            context->setStrokeColor(Color(200, 100, 0, 255), ColorSpaceDeviceRGB);
            context->fillRect(debugRects[i].rect, Color(200, 100, 0, 30), ColorSpaceDeviceRGB);
            context->strokeRect(debugRects[i].rect, 2.0);
        }

        if (debugRects[i].type == ReplicaScreenSpaceRectType) {
            // Screen space rects in green.
            context->setStrokeColor(Color(100, 200, 0, 255), ColorSpaceDeviceRGB);
            context->fillRect(debugRects[i].rect, Color(100, 200, 0, 30), ColorSpaceDeviceRGB);
            context->strokeRect(debugRects[i].rect, 2.0);
        }

        if (debugRects[i].type == ScreenSpaceRectType) {
            // Screen space rects in purple.
            context->setStrokeColor(Color(100, 0, 200, 255), ColorSpaceDeviceRGB);
            context->fillRect(debugRects[i].rect, Color(100, 0, 200, 10), ColorSpaceDeviceRGB);
            context->strokeRect(debugRects[i].rect, 2.0);
        }

        if (debugRects[i].type == OccludingRectType) {
            // Occluding rects in a reddish color.
            context->setStrokeColor(Color(200, 0, 100, 255), ColorSpaceDeviceRGB);
            context->fillRect(debugRects[i].rect, Color(200, 0, 100, 10), ColorSpaceDeviceRGB);
            context->strokeRect(debugRects[i].rect, 2.0);
        }
    }
}

}
