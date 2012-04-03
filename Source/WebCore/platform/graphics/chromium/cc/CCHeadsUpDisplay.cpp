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
#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
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

CCHeadsUpDisplay::CCHeadsUpDisplay(LayerRendererChromium* owner)
    : m_currentFrameNumber(1)
    , m_filteredFrameTime(0)
    , m_layerRenderer(owner)
    , m_useMapSubForUploads(owner->contextSupportsMapSub())
{
    m_beginTimeHistoryInSec[0] = currentTime();
    m_beginTimeHistoryInSec[1] = m_beginTimeHistoryInSec[0];
    for (int i = 2; i < kBeginFrameHistorySize; i++)
        m_beginTimeHistoryInSec[i] = 0;

    // We can't draw text in threaded mode with the current mechanism.
    // FIXME: Figure out a way to draw text in threaded mode.
    if (!CCProxy::implThread())
        initializeFonts();
}

CCHeadsUpDisplay::~CCHeadsUpDisplay()
{
}

void CCHeadsUpDisplay::initializeFonts()
{
    ASSERT(!CCProxy::implThread());
    FontDescription mediumFontDesc;
    mediumFontDesc.setGenericFamily(FontDescription::MonospaceFamily);
    mediumFontDesc.setComputedSize(20);

    m_mediumFont = adoptPtr(new Font(mediumFontDesc, 0, 0));
    m_mediumFont->update(0);

    FontDescription smallFontDesc;
    smallFontDesc.setGenericFamily(FontDescription::MonospaceFamily);
    smallFontDesc.setComputedSize(10);

    m_smallFont = adoptPtr(new Font(smallFontDesc, 0, 0));
    m_smallFont->update(0);
}

void CCHeadsUpDisplay::onFrameBegin(double timestamp)
{
    m_beginTimeHistoryInSec[m_currentFrameNumber % kBeginFrameHistorySize] = timestamp;
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
    return settings().showPlatformLayerTree && !CCProxy::implThread();
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

void CCHeadsUpDisplay::drawHudContents(GraphicsContext* context, const IntSize& hudSize)
{
    if (showPlatformLayerTree()) {
        context->setFillColor(Color(0, 0, 0, 192), ColorSpaceDeviceRGB);
        context->fillRect(FloatRect(0, 0, hudSize.width(), hudSize.height()));
    }

    int fpsCounterHeight = 25;
    int fpsCounterTop = 2;
    int platformLayerTreeTop;
    if (settings().showFPSCounter)
        platformLayerTreeTop = fpsCounterTop + fpsCounterHeight + 2;
    else
        platformLayerTreeTop = 0;

    if (settings().showFPSCounter)
        drawFPSCounter(context, fpsCounterTop, fpsCounterHeight);

    if (showPlatformLayerTree())
        drawPlatformLayerTree(context, platformLayerTreeTop);
}

void CCHeadsUpDisplay::drawFPSCounter(GraphicsContext* context, int top, int height)
{
    // Note that since we haven't finished the current frame, the FPS counter
    // actually reports the last frame's time.
    double secForLastFrame = m_beginTimeHistoryInSec[(m_currentFrameNumber + kBeginFrameHistorySize - 1) % kBeginFrameHistorySize] -
                             m_beginTimeHistoryInSec[(m_currentFrameNumber + kBeginFrameHistorySize - 2) % kBeginFrameHistorySize];

    // Filter the frame times to avoid spikes.
    const float alpha = 0.1;
    if (!m_filteredFrameTime) {
        if (m_currentFrameNumber == 2)
            m_filteredFrameTime = secForLastFrame;
    } else
        m_filteredFrameTime = ((1.0 - alpha) * m_filteredFrameTime) + (alpha * secForLastFrame);

    float textWidth = 0;
    if (!CCProxy::implThread())
        textWidth = drawFPSCounterText(context, top, height);

    float graphWidth = kBeginFrameHistorySize;

    // Draw background for graph
    context->setFillColor(Color(0, 0, 0, 255), ColorSpaceDeviceRGB);
    context->fillRect(FloatRect(2 + textWidth, top, graphWidth, height));

    // Draw FPS graph.
    const double loFPS = 0.0;
    const double hiFPS = 120.0;
    context->setStrokeStyle(SolidStroke);
    context->setStrokeColor(Color(255, 0, 0), ColorSpaceDeviceRGB);
    int graphLeft = static_cast<int>(textWidth + 3);
    IntPoint prev(-1, 0);
    int x = 0;
    double h = static_cast<double>(height - 2);
    for (int i = m_currentFrameNumber % kBeginFrameHistorySize; i != (m_currentFrameNumber - 1) % kBeginFrameHistorySize; i = (i + 1) % kBeginFrameHistorySize) {
        int j = (i + 1) % kBeginFrameHistorySize;
        double fps = 1.0 / (m_beginTimeHistoryInSec[j] - m_beginTimeHistoryInSec[i]);
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

float CCHeadsUpDisplay::drawFPSCounterText(GraphicsContext* context, int top, int height)
{
    ASSERT(!CCProxy::implThread());

    FontCachePurgePreventer fontCachePurgePreventer;

    // Create & measure FPS text.
    String text(String::format("FPS: %5.1f", 1.0 / m_filteredFrameTime));
    TextRun run(text);
    float textWidth = m_mediumFont->width(run) + 2.0f;

    // Draw background.
    context->setFillColor(Color(0, 0, 0, 255), ColorSpaceDeviceRGB);
    context->fillRect(FloatRect(2, top, textWidth, height));

    // Draw FPS text.
    if (m_filteredFrameTime) {
        context->setFillColor(Color(255, 0, 0), ColorSpaceDeviceRGB);
        context->drawText(*m_mediumFont, run, IntPoint(3, top + height - 6));
    }

    return textWidth;
}

void CCHeadsUpDisplay::drawPlatformLayerTree(GraphicsContext* context, int top)
{
    ASSERT(!CCProxy::implThread());

    FontCachePurgePreventer fontCachePurgePreventer;

    float smallFontHeight = m_smallFont->fontMetrics().floatHeight();
    int y = top + smallFontHeight - 4;
    context->setFillColor(Color(255, 0, 0), ColorSpaceDeviceRGB);
    Vector<String> lines;
    m_layerRenderer->layerTreeAsText().split('\n', lines);
    for (size_t i = 0; i < lines.size(); ++i) {
        context->drawText(*m_smallFont, TextRun(lines[i]), IntPoint(2, y));
        y += smallFontHeight;
    }
}

const CCSettings& CCHeadsUpDisplay::settings() const
{
    return m_layerRenderer->settings();
}

}

#endif // USE(ACCELERATED_COMPOSITING)
