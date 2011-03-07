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

#include "CurrentTime.h"
#include "Font.h"
#include "FontDescription.h"
#include "GraphicsContext3D.h"
#include "LayerChromium.h"
#include "LayerTexture.h"
#include "TextRun.h"
#include "TextStream.h"
#include "TextureManager.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace std;

CCHeadsUpDisplay::CCHeadsUpDisplay(LayerRendererChromium* owner)
    : m_currentFrameNumber(0)
    , m_layerRenderer(owner)
    , m_showFPSCounter(false)
    , m_showPlatformLayerTree(false)
{
    m_presentTimeHistoryInSec[0] = currentTime();
    m_presentTimeHistoryInSec[1] = m_presentTimeHistoryInSec[0];
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
        hudSize.setWidth(min(2048, m_layerRenderer->visibleRectSize().width()));
        hudSize.setHeight(min(2048, m_layerRenderer->visibleRectSize().height()));
    } else {
        hudSize.setWidth(512);
        hudSize.setHeight(128);
    }

    m_hudTexture->reserve(hudSize, GraphicsContext3D::RGBA);

    // Render pixels into the texture.
    PlatformCanvas canvas;
    canvas.resize(hudSize);
    {
        PlatformCanvas::Painter painter(&canvas);
        drawHudContents(painter.context(), hudSize);
    }

    // Upload to GL.
    {
        PlatformCanvas::AutoLocker locker(&canvas);

        m_hudTexture->bindTexture();
        GLC(context.get(), context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, canvas.size().width(), canvas.size().height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, locker.pixels()));
    }

    // Draw the HUD onto the default render surface.
    const ContentLayerChromium::Program* program = m_layerRenderer->contentLayerProgram();
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

    m_hudTexture->unreserve();
}

void CCHeadsUpDisplay::drawHudContents(GraphicsContext* ctx, const IntSize& hudSize)
{
    FontDescription mediumFontDesc;
    mediumFontDesc.setGenericFamily(FontDescription::MonospaceFamily);
    mediumFontDesc.setComputedSize(12);
    Font mediumFont(mediumFontDesc, 0, 0);
    mediumFont.update(0);

    FontDescription smallFontDesc;
    smallFontDesc.setGenericFamily(FontDescription::MonospaceFamily);
    smallFontDesc.setComputedSize(10);
    Font smallFont(smallFontDesc, 0, 0);
    smallFont.update(0);

    // We haven't finished rendering yet, so we don't now the "current" present time.
    // So, consider the *last two* present times and use those as our present time.
    double secForLastFrame = m_presentTimeHistoryInSec[(m_currentFrameNumber - 1) % 2] - m_presentTimeHistoryInSec[m_currentFrameNumber % 2];

    int y = 14;

    if (m_showPlatformLayerTree) {
        ctx->setFillColor(Color(0, 0, 0, 192), ColorSpaceDeviceRGB);
        ctx->fillRect(FloatRect(0, 0, hudSize.width(), hudSize.height()));
    }

    // Draw fps.
    String topLine = "";
    if (secForLastFrame > 0 && m_showFPSCounter) {
        double fps = 1.0 / secForLastFrame;
        topLine += String::format("FPS: %3.1f", fps);
    }
    if (topLine.length()) {
        ctx->setFillColor(Color(0, 0, 0, 255), ColorSpaceDeviceRGB);
        TextRun run(topLine);
        ctx->fillRect(FloatRect(2, 2, mediumFont.width(run) + 2.0f, 15));
        ctx->setFillColor(Color(255, 0, 0), ColorSpaceDeviceRGB);
        ctx->drawText(mediumFont, run, IntPoint(3, y));
        y = 26;
    }

    // Draw layer tree, if enabled.
    if (m_showPlatformLayerTree) {
        ctx->setFillColor(Color(255, 0, 0), ColorSpaceDeviceRGB);
        Vector<String> lines;
        m_layerRenderer->layerTreeAsText().split('\n', lines);
        for (size_t i = 0; i < lines.size(); ++i) {
            ctx->drawText(smallFont, TextRun(lines[i]), IntPoint(2, y));
            y += 12;
        }
    }
}

void CCHeadsUpDisplay::onPresent()
{
    m_presentTimeHistoryInSec[m_currentFrameNumber % 2] = currentTime();
    m_currentFrameNumber += 1;
}

}

#endif // USE(ACCELERATED_COMPOSITING)
