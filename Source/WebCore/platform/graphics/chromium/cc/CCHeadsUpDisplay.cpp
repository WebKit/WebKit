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

#include "CCFontAtlas.h"
#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "InspectorController.h"
#include "LayerChromium.h"
#include "LayerRendererChromium.h"
#include "ManagedTexture.h"
#include "PlatformCanvas.h"
#include "TextRun.h"
#include "TextStream.h"
#include "TextureManager.h"
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace std;

CCHeadsUpDisplay::CCHeadsUpDisplay(LayerRendererChromium* owner, CCFontAtlas* headsUpDisplayFontAtlas)
    : m_currentFrameNumber(1)
    , m_layerRenderer(owner)
    , m_useMapSubForUploads(owner->contextSupportsMapSub())
    , m_fontAtlas(headsUpDisplayFontAtlas)
{
    m_beginTimeHistoryInSec[0] = currentTime();
    m_beginTimeHistoryInSec[1] = m_beginTimeHistoryInSec[0];
    for (int i = 2; i < kBeginFrameHistorySize; i++)
        m_beginTimeHistoryInSec[i] = 0;
}

CCHeadsUpDisplay::~CCHeadsUpDisplay()
{
}

const double CCHeadsUpDisplay::kIdleSecondsTriggersReset = 0.5;
const double CCHeadsUpDisplay::kFrameTooFast = 1.0 / 70;

// safeMod works on -1, returning m-1 in that case.
static inline int safeMod(int number, int modulus)
{
    return (number + modulus) % modulus;
}

inline int CCHeadsUpDisplay::frameIndex(int frame) const
{
    return safeMod(frame, kBeginFrameHistorySize);
}

void CCHeadsUpDisplay::onFrameBegin(double timestamp)
{
    m_beginTimeHistoryInSec[frameIndex(m_currentFrameNumber)] = timestamp;
}

void CCHeadsUpDisplay::onSwapBuffers()
{
    m_currentFrameNumber += 1;
}

bool CCHeadsUpDisplay::enabled() const
{
    return showPlatformLayerTree() || settings().showFPSCounter;
}

bool CCHeadsUpDisplay::showPlatformLayerTree() const
{
    return settings().showPlatformLayerTree;
}

void CCHeadsUpDisplay::draw()
{
    GraphicsContext3D* context = m_layerRenderer->context();
    if (!m_hudTexture)
        m_hudTexture = ManagedTexture::create(m_layerRenderer->renderSurfaceTextureManager());

    // Use a fullscreen texture only if we need to...
    IntSize hudSize;
    if (showPlatformLayerTree()) {
        hudSize.setWidth(min(2048, m_layerRenderer->viewportWidth()));
        hudSize.setHeight(min(2048, m_layerRenderer->viewportHeight()));
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
        drawHudContents(painter.context(), hudSize);
    }

    // Upload to GL.
    {
        PlatformCanvas::AutoLocker locker(&canvas);

        m_hudTexture->bindTexture(context, m_layerRenderer->renderSurfaceTextureAllocator());
        bool uploadedViaMap = false;
        if (m_useMapSubForUploads) {
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

    // Draw the HUD onto the default render surface.
    const Program* program = m_layerRenderer->headsUpDisplayProgram();
    ASSERT(program && program->initialized());
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    m_hudTexture->bindTexture(context, m_layerRenderer->renderSurfaceTextureAllocator());
    GLC(context, context->useProgram(program->program()));
    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));

    TransformationMatrix matrix;
    matrix.translate3d(hudSize.width() * 0.5, hudSize.height() * 0.5, 0);
    m_layerRenderer->drawTexturedQuad(matrix, hudSize.width(), hudSize.height(),
                                      1.0f, m_layerRenderer->sharedGeometryQuad(), program->vertexShader().matrixLocation(),
                                      program->fragmentShader().alphaLocation(),
                                      -1);
    m_hudTexture->unreserve();
}

bool CCHeadsUpDisplay::isBadFrame(int frameNumber) const
{
    double delta = m_beginTimeHistoryInSec[frameIndex(frameNumber)] -
                   m_beginTimeHistoryInSec[frameIndex(frameNumber - 1)];
    bool tooSlow = delta > (kNumMissedFramesForReset / 60.0);
    bool schedulerAllowsDoubleFrames = !CCProxy::hasImplThread();
    return (schedulerAllowsDoubleFrames && delta < kFrameTooFast) || tooSlow;
}

void CCHeadsUpDisplay::drawHudContents(GraphicsContext* context, const IntSize& hudSize)
{
    if (showPlatformLayerTree()) {
        context->setFillColor(Color(0, 0, 0, 192), ColorSpaceDeviceRGB);
        context->fillRect(FloatRect(0, 0, hudSize.width(), hudSize.height()));
    }

    int fpsCounterHeight = 40;
    int fpsCounterTop = 2;
    int platformLayerTreeTop;
    if (settings().showFPSCounter)
        platformLayerTreeTop = fpsCounterTop + fpsCounterHeight;
    else
        platformLayerTreeTop = 0;

    if (settings().showFPSCounter)
        drawFPSCounter(context, fpsCounterTop, fpsCounterHeight);

    if (showPlatformLayerTree())
        drawPlatformLayerTree(context, hudSize, platformLayerTreeTop);
}

void CCHeadsUpDisplay::getAverageFPSAndStandardDeviation(double *average, double *standardDeviation) const
{
    double& averageFPS = *average;

    int frame = m_currentFrameNumber - 1; 
    averageFPS = 0;
    int averageFPSCount = 0;
    double fpsVarianceNumerator = 0;

    // Walk backwards through the samples looking for a run of good frame
    // timings from which to compute the mean and standard deviation.
    //
    // Slow frames occur just because the user is inactive, and should be
    // ignored. Fast frames are ignored if the scheduler is in single-thread
    // mode in order to represent the true frame rate in spite of the fact that
    // the first few swapbuffers happen instantly which skews the statistics
    // too much for short lived animations.
    //
    // isBadFrame encapsulates the frame too slow/frame too fast logic.
    while (1) {
        if (!isBadFrame(frame)) {
            averageFPSCount++;
            double secForLastFrame = m_beginTimeHistoryInSec[frameIndex(frame)] -
                                     m_beginTimeHistoryInSec[frameIndex(frame - 1)];
            double x = 1.0 / secForLastFrame;
            double deltaFromAverage = x - averageFPS;
            // Change with caution - numerics. http://en.wikipedia.org/wiki/Standard_deviation
            averageFPS = averageFPS + deltaFromAverage / averageFPSCount;
            fpsVarianceNumerator = fpsVarianceNumerator + deltaFromAverage * (x - averageFPS);
        }
        if (averageFPSCount && isBadFrame(frame)) {
            // We've gathered a run of good samples, so stop.
            break;
        }
        --frame;
        if (frameIndex(frame) == frameIndex(m_currentFrameNumber) || frame < 0) {
            // We've gone through all available historical data, so stop.
            break;
        }
    }

    *standardDeviation = sqrt(fpsVarianceNumerator / averageFPSCount);
}

void CCHeadsUpDisplay::drawFPSCounter(GraphicsContext* context, int top, int height)
{
    float textWidth = 170; // so text fits on linux.
    float graphWidth = kBeginFrameHistorySize;

    // Draw the FPS text.
    drawFPSCounterText(context, top, textWidth, height);

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
    for (int i = m_currentFrameNumber % kBeginFrameHistorySize; i != (m_currentFrameNumber - 1) % kBeginFrameHistorySize; i = (i + 1) % kBeginFrameHistorySize) {
        int j = (i + 1) % kBeginFrameHistorySize;
        double fps = 1.0 / (m_beginTimeHistoryInSec[j] - m_beginTimeHistoryInSec[i]);
        if (isBadFrame(j)) {
            x += 1;
            continue;
        }
        double p = 1 - ((fps - loFPS) / (hiFPS - loFPS));
        if (p < 0)
            p = 0;
        if (p > 1)
            p = 1;
        IntPoint cur(graphLeft + x, 1 + top + p*h);
        if (prev.x() != -1)
            context->drawLine(prev, cur);
        prev = cur;
        x += 1;
    }
}

void CCHeadsUpDisplay::drawFPSCounterText(GraphicsContext* context, int top, int width, int height)
{
    double averageFPS, stdDeviation;
    getAverageFPSAndStandardDeviation(&averageFPS, &stdDeviation);

    // Draw background.
    context->setFillColor(Color(0, 0, 0, 255), ColorSpaceDeviceRGB);
    context->fillRect(FloatRect(2, top, width, height));

    // Draw FPS text.
    if (m_fontAtlas)
        m_fontAtlas->drawText(context, String::format("FPS: %4.1f +/- %3.1f", averageFPS, stdDeviation), IntPoint(10, height / 3), IntSize(width, height));
}

void CCHeadsUpDisplay::drawPlatformLayerTree(GraphicsContext* context, const IntSize hudSize, int top)
{
    if (m_fontAtlas)
        m_fontAtlas->drawText(context, m_layerRenderer->layerTreeAsText(), IntPoint(2, top), hudSize);
}

const CCSettings& CCHeadsUpDisplay::settings() const
{
    return m_layerRenderer->settings();
}

}

#endif // USE(ACCELERATED_COMPOSITING)
