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
#include "FontDescription.h"
#include "GraphicsContext3D.h"
#include "LayerChromium.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"
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
    , m_showFPSCounter(false)
    , m_showPlatformLayerTree(false)
    , m_useMapSubForUploads(owner->contextSupportsMapSub())
{
    m_beginTimeHistoryInSec[0] = currentTime();
    m_beginTimeHistoryInSec[1] = m_beginTimeHistoryInSec[0];
    for (int i = 2; i < kBeginFrameHistorySize; i++)
        m_beginTimeHistoryInSec[i] = 0;

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

CCHeadsUpDisplay::~CCHeadsUpDisplay()
{
}

void CCHeadsUpDisplay::draw()
{
    GraphicsContext3D* context = m_layerRenderer->context();
    if (!m_hudTexture)
        m_hudTexture = LayerTexture::create(context, m_layerRenderer->textureManager());

    // Use a fullscreen texture only if we need to...
    IntSize hudSize;
    if (m_showPlatformLayerTree) {
        hudSize.setWidth(min(2048, m_layerRenderer->viewportSize().width()));
        hudSize.setHeight(min(2048, m_layerRenderer->viewportSize().height()));
    } else {
        hudSize.setWidth(512);
        hudSize.setHeight(128);
    }

    m_hudTexture->reserve(hudSize, GraphicsContext3D::RGBA);

    // Render pixels into the texture.
    PlatformCanvas canvas;
    canvas.resize(hudSize);
    {
        PlatformCanvas::Painter painter(&canvas, PlatformCanvas::Painter::GrayscaleText);
        drawHudContents(painter.context(), hudSize);
    }

    // Upload to GL.
    {
        PlatformCanvas::AutoLocker locker(&canvas);

        m_hudTexture->bindTexture();
        if (m_useMapSubForUploads) {
            Extensions3DChromium* extensions = static_cast<Extensions3DChromium*>(context->getExtensions());
            uint8_t* pixelDest = static_cast<uint8_t*>(extensions->mapTexSubImage2DCHROMIUM(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, hudSize.width(), hudSize.height(), GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, Extensions3DChromium::WRITE_ONLY));
            memcpy(pixelDest, locker.pixels(), hudSize.width() * hudSize.height() * 4);
            extensions->unmapTexSubImage2DCHROMIUM(pixelDest);
        } else
            GLC(context.get(), context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, canvas.size().width(), canvas.size().height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, locker.pixels()));
    }

    // Draw the HUD onto the default render surface.
    const Program* program = m_layerRenderer->headsUpDisplayProgram();
    ASSERT(program && program->initialized());
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    m_hudTexture->bindTexture();
    m_layerRenderer->useShader(program->program());
    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));

    TransformationMatrix matrix;
    matrix.translate3d(hudSize.width() * 0.5, hudSize.height() * 0.5, 0);
    LayerChromium::drawTexturedQuad(context, m_layerRenderer->projectionMatrix(),
                                    matrix, hudSize.width(), hudSize.height(),
                                    1.0f, program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation());
}

void CCHeadsUpDisplay::drawHudContents(GraphicsContext* ctx, const IntSize& hudSize)
{
    if (m_showPlatformLayerTree) {
        ctx->setFillColor(Color(0, 0, 0, 192), ColorSpaceDeviceRGB);
        ctx->fillRect(FloatRect(0, 0, hudSize.width(), hudSize.height()));
    }

    int fpsCounterHeight = m_mediumFont->fontMetrics().floatHeight() + 2;
    int fpsCounterTop = 2;
    int platformLayerTreeTop;
    if (m_showFPSCounter)
        platformLayerTreeTop = fpsCounterTop + fpsCounterHeight + 2;
    else
        platformLayerTreeTop = 0;

    if (m_showFPSCounter)
        drawFPSCounter(ctx, fpsCounterTop, fpsCounterHeight);

    if (m_showPlatformLayerTree)
        drawPlatformLayerTree(ctx, platformLayerTreeTop);
}

void CCHeadsUpDisplay::drawFPSCounter(GraphicsContext* ctx, int top, int height)
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

    // Create & measure FPS text.
    String text(String::format("FPS: %5.1f", 1.0 / m_filteredFrameTime));
    TextRun run(text);
    float textWidth = m_mediumFont->width(run) + 2.0f;
    float graphWidth = kBeginFrameHistorySize;

    // Draw background.
    ctx->setFillColor(Color(0, 0, 0, 255), ColorSpaceDeviceRGB);
    ctx->fillRect(FloatRect(2, top, textWidth + graphWidth, height));

    // Draw FPS text.
    if (m_filteredFrameTime) {
        ctx->setFillColor(Color(255, 0, 0), ColorSpaceDeviceRGB);
        ctx->drawText(*m_mediumFont, run, IntPoint(3, top + height - 6));
    }

    // Draw FPS graph.
    const double loFPS = 0.0;
    const double hiFPS = 120.0;
    ctx->setStrokeStyle(SolidStroke);
    ctx->setStrokeColor(Color(255, 0, 0), ColorSpaceDeviceRGB);
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
            ctx->drawLine(prev, cur);
        prev = cur;
        x += 1;
    }
}

void CCHeadsUpDisplay::drawPlatformLayerTree(GraphicsContext* ctx, int top)
{
    float smallFontHeight = m_smallFont->fontMetrics().floatHeight();
    int y = top + smallFontHeight - 4;
    ctx->setFillColor(Color(255, 0, 0), ColorSpaceDeviceRGB);
    Vector<String> lines;
    m_layerRenderer->layerTreeAsText().split('\n', lines);
    for (size_t i = 0; i < lines.size(); ++i) {
        ctx->drawText(*m_smallFont, TextRun(lines[i]), IntPoint(2, y));
        y += smallFontHeight;
    }
}

void CCHeadsUpDisplay::onFrameBegin(double timestamp)
{
    m_beginTimeHistoryInSec[m_currentFrameNumber % kBeginFrameHistorySize] = timestamp;
}

void CCHeadsUpDisplay::onPresent()
{
    m_currentFrameNumber += 1;
}

}

#endif // USE(ACCELERATED_COMPOSITING)
