/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if USE(ACCELERATED_COMPOSITING)
#include "CCHeadsUpDisplay.h"

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "ManagedTexture.h"
#include "PlatformCanvas.h"
#include "TextureManager.h"
#include "cc/CCDebugRectHistory.h"
#include "cc/CCFontAtlas.h"
#include "cc/CCFrameRateCounter.h"
#include "cc/CCLayerTreeHostImpl.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace std;

CCHeadsUpDisplay::~CCHeadsUpDisplay()
{
}

void CCHeadsUpDisplay::setFontAtlas(PassOwnPtr<CCFontAtlas> fontAtlas)
{
    m_fontAtlas = fontAtlas;
}

bool CCHeadsUpDisplay::enabled(const CCSettings& settings) const
{
    return showPlatformLayerTree(settings) || settings.showFPSCounter || showDebugRects(settings);
}

bool CCHeadsUpDisplay::showPlatformLayerTree(const CCSettings& settings) const
{
    return settings.showPlatformLayerTree;
}

bool CCHeadsUpDisplay::showDebugRects(const CCSettings& settings) const
{
    return settings.showPaintRects || settings.showPropertyChangedRects || settings.showSurfaceDamageRects;
}

void CCHeadsUpDisplay::draw(CCLayerTreeHostImpl* layerTreeHostImpl)
{
    LayerRendererChromium* layerRenderer = layerTreeHostImpl->layerRenderer();
    GraphicsContext3D* context = layerRenderer->context();
    if (!m_hudTexture)
        m_hudTexture = ManagedTexture::create(layerRenderer->renderSurfaceTextureManager());

    const CCSettings& settings = layerTreeHostImpl->settings();
    // Use a fullscreen texture only if we need to...
    IntSize hudSize;
    if (showPlatformLayerTree(settings) || showDebugRects(settings)) {
        hudSize.setWidth(min(2048, layerTreeHostImpl->viewportSize().width()));
        hudSize.setHeight(min(2048, layerTreeHostImpl->viewportSize().height()));
    } else {
        hudSize.setWidth(512);
        hudSize.setHeight(128);
    }

    if (!m_hudTexture->reserve(hudSize, GraphicsContext3D::RGBA))
        return;

    // Render pixels into the texture.
    PlatformCanvas canvas;
    canvas.resize(hudSize);
    {
        PlatformCanvas::Painter painter(&canvas, PlatformCanvas::Painter::GrayscaleText);
        painter.context()->clearRect(FloatRect(0, 0, hudSize.width(), hudSize.height()));
        drawHudContents(painter.context(), layerTreeHostImpl, settings, hudSize);
    }

    // Upload to GL.
    {
        PlatformCanvas::AutoLocker locker(&canvas);

        m_hudTexture->bindTexture(context, layerRenderer->renderSurfaceTextureAllocator());
        bool uploadedViaMap = false;
        if (layerRenderer->contextSupportsMapSub()) {
            Extensions3DChromium* extensions = static_cast<Extensions3DChromium*>(context->getExtensions());
            uint8_t* pixelDest = static_cast<uint8_t*>(extensions->mapTexSubImage2DCHROMIUM(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, hudSize.width(), hudSize.height(), GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, Extensions3DChromium::WRITE_ONLY));

            if (pixelDest) {
                uploadedViaMap = true;
                memcpy(pixelDest, locker.pixels(), hudSize.width() * hudSize.height() * 4);
                extensions->unmapTexSubImage2DCHROMIUM(pixelDest);
            }
        }

        if (!uploadedViaMap) {
            GLC(context, context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, canvas.size().width(), canvas.size().height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, locker.pixels()));
        }
    }

    layerRenderer->drawHeadsUpDisplay(m_hudTexture.get(), hudSize);

    m_hudTexture->unreserve();
}

void CCHeadsUpDisplay::drawHudContents(GraphicsContext* context, CCLayerTreeHostImpl* layerTreeHostImpl, const CCSettings& settings, const IntSize& hudSize)
{
    if (showPlatformLayerTree(settings)) {
        context->setFillColor(Color(0, 0, 0, 192), ColorSpaceDeviceRGB);
        context->fillRect(FloatRect(0, 0, hudSize.width(), hudSize.height()));
    }

    int fpsCounterHeight = 40;
    int fpsCounterTop = 2;
    int platformLayerTreeTop;

    if (settings.showFPSCounter)
        platformLayerTreeTop = fpsCounterTop + fpsCounterHeight;
    else
        platformLayerTreeTop = 0;

    if (settings.showFPSCounter)
        drawFPSCounter(context, layerTreeHostImpl->fpsCounter(), fpsCounterTop, fpsCounterHeight);

    if (showPlatformLayerTree(settings) && m_fontAtlas) {
        String layerTree = layerTreeHostImpl->layerTreeAsText();
        m_fontAtlas->drawText(context, layerTree, IntPoint(2, platformLayerTreeTop), hudSize);
    }

    if (showDebugRects(settings))
        drawDebugRects(context, layerTreeHostImpl->debugRectHistory(), settings);
}

void CCHeadsUpDisplay::drawFPSCounter(GraphicsContext* context, CCFrameRateCounter* fpsCounter, int top, int height)
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

void CCHeadsUpDisplay::drawFPSCounterText(GraphicsContext* context, CCFrameRateCounter* fpsCounter, int top, int width, int height)
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

void CCHeadsUpDisplay::drawDebugRects(GraphicsContext* context, CCDebugRectHistory* debugRectHistory, const CCSettings& settings)
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
    }
}

}

#endif // USE(ACCELERATED_COMPOSITING)
