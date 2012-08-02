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
#include "SkBitmap.h"
#include "SkPaint.h"
#include "cc/CCDebugRectHistory.h"
#include "cc/CCFontAtlas.h"
#include "cc/CCFrameRateCounter.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCQuadSink.h"
#include "skia/ext/platform_canvas.h"
#include <public/WebCompositorTextureQuad.h>
#include <wtf/text/WTFString.h>

using WebKit::WebCompositorTextureQuad;

namespace WebCore {

CCHeadsUpDisplayLayerImpl::CCHeadsUpDisplayLayerImpl(int id)
    : CCLayerImpl(id)
{
}

CCHeadsUpDisplayLayerImpl::~CCHeadsUpDisplayLayerImpl()
{
}

void CCHeadsUpDisplayLayerImpl::setFontAtlas(PassOwnPtr<CCFontAtlas> fontAtlas)
{
    m_fontAtlas = fontAtlas;
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
    OwnPtr<SkCanvas> canvas = adoptPtr(skia::CreateBitmapCanvas(bounds().width(), bounds().height(), false /* opaque */));
    drawHudContents(canvas.get());

    const SkBitmap* bitmap = &canvas->getDevice()->accessBitmap(false);
    SkAutoLockPixels locker(*bitmap);

    IntRect layerRect(IntPoint(), bounds());
    ASSERT(bitmap->config() == SkBitmap::kARGB_8888_Config);
    resourceProvider->upload(m_hudTexture->id(), static_cast<const uint8_t*>(bitmap->getPixels()), layerRect, layerRect, layerRect);
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
    m_hudTexture.clear();
}

void CCHeadsUpDisplayLayerImpl::drawHudContents(SkCanvas* canvas)
{
    const CCLayerTreeSettings& settings = layerTreeHostImpl()->settings();

    if (settings.showPlatformLayerTree) {
        SkPaint paint;
        paint.setColor(SkColorSetARGB(192, 0, 0, 0));
        canvas->drawRect(SkRect::MakeXYWH(0, 0, bounds().width(), bounds().height()), paint);
    }

    int fpsCounterHeight = 40;
    int fpsCounterTop = 2;
    int platformLayerTreeTop;

    if (settings.showFPSCounter)
        platformLayerTreeTop = fpsCounterTop + fpsCounterHeight;
    else
        platformLayerTreeTop = 0;

    if (settings.showFPSCounter)
        drawFPSCounter(canvas, layerTreeHostImpl()->fpsCounter(), fpsCounterTop, fpsCounterHeight);

    if (settings.showPlatformLayerTree && m_fontAtlas) {
        String layerTree = layerTreeHostImpl()->layerTreeAsText();
        m_fontAtlas->drawText(canvas, layerTree, IntPoint(2, platformLayerTreeTop), bounds());
    }

    if (settings.showDebugRects())
        drawDebugRects(canvas, layerTreeHostImpl()->debugRectHistory());
}

void CCHeadsUpDisplayLayerImpl::drawFPSCounter(SkCanvas* canvas, CCFrameRateCounter* fpsCounter, int top, int height)
{
    float textWidth = 170; // so text fits on linux.
    float graphWidth = fpsCounter->timeStampHistorySize();

    // Draw the FPS text.
    drawFPSCounterText(canvas, fpsCounter, top, textWidth, height);

    // Draw FPS graph.
    const double loFPS = 0;
    const double hiFPS = 80;
    SkPaint paint;
    paint.setColor(SkColorSetRGB(154, 205, 50));
    canvas->drawRect(SkRect::MakeXYWH(2 + textWidth, top, graphWidth, height / 2), paint);

    paint.setColor(SkColorSetRGB(255, 250, 205));
    canvas->drawRect(SkRect::MakeXYWH(2 + textWidth, top + height / 2, graphWidth, height / 2), paint);

    int graphLeft = static_cast<int>(textWidth + 3);
    int x = 0;
    double h = static_cast<double>(height - 2);
    SkPath path;
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
        if (path.isEmpty())
            path.moveTo(cur);
        else
            path.lineTo(cur);
        x += 1;
    }
    paint.setColor(SK_ColorRED);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(1);
    paint.setAntiAlias(true);
    canvas->drawPath(path, paint);
}

void CCHeadsUpDisplayLayerImpl::drawFPSCounterText(SkCanvas* canvas, CCFrameRateCounter* fpsCounter, int top, int width, int height)
{
    double averageFPS, stdDeviation;
    fpsCounter->getAverageFPSAndStandardDeviation(averageFPS, stdDeviation);

    // Draw background.
    SkPaint paint;
    paint.setColor(SK_ColorBLACK);
    canvas->drawRect(SkRect::MakeXYWH(2, top, width, height), paint);

    // Draw FPS text.
    if (m_fontAtlas)
        m_fontAtlas->drawText(canvas, String::format("FPS: %4.1f +/- %3.1f", averageFPS, stdDeviation), IntPoint(10, height / 3), IntSize(width, height));
}

void CCHeadsUpDisplayLayerImpl::drawDebugRects(SkCanvas* canvas, CCDebugRectHistory* debugRectHistory)
{
    const Vector<CCDebugRect>& debugRects = debugRectHistory->debugRects();

    for (size_t i = 0; i < debugRects.size(); ++i) {
        SkColor strokeColor = 0;
        SkColor fillColor = 0;

        switch (debugRects[i].type) {
        case PaintRectType:
            // Paint rects in red
            strokeColor = SkColorSetARGB(255, 255, 0, 0);
            fillColor = SkColorSetARGB(30, 255, 0, 0);
            break;
        case PropertyChangedRectType:
            // Property-changed rects in blue
            strokeColor = SkColorSetARGB(255, 255, 0, 0);
            fillColor = SkColorSetARGB(30, 0, 0, 255);
            break;
        case SurfaceDamageRectType:
            // Surface damage rects in yellow-orange
            strokeColor = SkColorSetARGB(255, 200, 100, 0);
            fillColor = SkColorSetARGB(30, 200, 100, 0);
            break;
        case ReplicaScreenSpaceRectType:
            // Screen space rects in green.
            strokeColor = SkColorSetARGB(255, 100, 200, 0);
            fillColor = SkColorSetARGB(30, 100, 200, 0);
            break;
        case ScreenSpaceRectType:
            // Screen space rects in purple.
            strokeColor = SkColorSetARGB(255, 100, 0, 200);
            fillColor = SkColorSetARGB(10, 100, 0, 200);
            break;
        case OccludingRectType:
            // Occluding rects in a reddish color.
            strokeColor = SkColorSetARGB(255, 200, 0, 100);
            fillColor = SkColorSetARGB(10, 200, 0, 100);
            break;
        }

        SkRect rect = debugRects[i].rect;
        SkPaint paint;
        paint.setColor(fillColor);
        canvas->drawRect(rect, paint);

        paint.setColor(strokeColor);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(2);
        canvas->drawRect(rect, paint);
    }
}

}
