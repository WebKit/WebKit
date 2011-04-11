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

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCVideoLayerImpl.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "NotImplemented.h"
#include "VideoLayerChromium.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

// These values are magic numbers that are used in the transformation
// from YUV to RGB color values.
// They are taken from the following webpage:
// http://www.fourcc.org/fccyvrgb.php
const float CCVideoLayerImpl::yuv2RGB[9] = {
    1.164f, 1.164f, 1.164f,
    0.f, -.391f, 2.018f,
    1.596f, -.813f, 0.f,
};

// These values map to 16, 128, and 128 respectively, and are computed
// as a fraction over 256 (e.g. 16 / 256 = 0.0625).
// They are used in the YUV to RGBA conversion formula:
//   Y - 16   : Gives 16 values of head and footroom for overshooting
//   U - 128  : Turns unsigned U into signed U [-128,127]
//   V - 128  : Turns unsigned V into signed V [-128,127]
const float CCVideoLayerImpl::yuvAdjust[3] = {
    -0.0625f,
    -0.5f,
    -0.5f,
};

CCVideoLayerImpl::CCVideoLayerImpl(LayerChromium* owner)
    : CCLayerImpl(owner)
{
}

CCVideoLayerImpl::~CCVideoLayerImpl()
{
    cleanupResources();
}

void CCVideoLayerImpl::setTexture(size_t i, VideoLayerChromium::Texture texture)
{
    ASSERT(i < 3);
    m_textures[i] = texture;
}

void CCVideoLayerImpl::draw(const IntRect&)
{
    if (m_skipsDraw)
        return;

    ASSERT(layerRenderer());
    const RGBAProgram* rgbaProgram = layerRenderer()->videoLayerRGBAProgram();
    ASSERT(rgbaProgram && rgbaProgram->initialized());
    const YUVProgram* yuvProgram = layerRenderer()->videoLayerYUVProgram();
    ASSERT(yuvProgram && yuvProgram->initialized());

    switch (m_frameFormat) {
    case VideoFrameChromium::YV12:
    case VideoFrameChromium::YV16:
        drawYUV(yuvProgram);
        break;
    case VideoFrameChromium::RGBA:
        drawRGBA(rgbaProgram);
        break;
    default:
        // FIXME: Implement other paths.
        notImplemented();
        break;
    }
}

void CCVideoLayerImpl::drawYUV(const CCVideoLayerImpl::YUVProgram* program) const
{
    GraphicsContext3D* context = layerRenderer()->context();
    VideoLayerChromium::Texture yTexture = m_textures[VideoFrameChromium::yPlane];
    VideoLayerChromium::Texture uTexture = m_textures[VideoFrameChromium::uPlane];
    VideoLayerChromium::Texture vTexture = m_textures[VideoFrameChromium::vPlane];

    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE1));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, yTexture.id));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE2));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, uTexture.id));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE3));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, vTexture.id));

    layerRenderer()->useShader(program->program());

    float yWidthScaleFactor = static_cast<float>(yTexture.visibleSize.width()) / yTexture.size.width();
    // Arbitrarily take the u sizes because u and v dimensions are identical.
    float uvWidthScaleFactor = static_cast<float>(uTexture.visibleSize.width()) / uTexture.size.width();
    GLC(context, context->uniform1f(program->vertexShader().yWidthScaleFactorLocation(), yWidthScaleFactor));
    GLC(context, context->uniform1f(program->vertexShader().uvWidthScaleFactorLocation(), uvWidthScaleFactor));

    GLC(context, context->uniform1i(program->fragmentShader().yTextureLocation(), 1));
    GLC(context, context->uniform1i(program->fragmentShader().uTextureLocation(), 2));
    GLC(context, context->uniform1i(program->fragmentShader().vTextureLocation(), 3));

    GLC(context, context->uniformMatrix3fv(program->fragmentShader().ccMatrixLocation(), 0, const_cast<float*>(yuv2RGB), 1));
    GLC(context, context->uniform3fv(program->fragmentShader().yuvAdjLocation(), const_cast<float*>(yuvAdjust), 1));

    LayerChromium::drawTexturedQuad(context, layerRenderer()->projectionMatrix(), drawTransform(),
                                    bounds().width(), bounds().height(), drawOpacity(),
                                    program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation());

    // Reset active texture back to texture 0.
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
}

void CCVideoLayerImpl::drawRGBA(const CCVideoLayerImpl::RGBAProgram* program) const
{
    GraphicsContext3D* context = layerRenderer()->context();
    VideoLayerChromium::Texture texture = m_textures[VideoFrameChromium::rgbPlane];

    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, texture.id));

    layerRenderer()->useShader(program->program());
    float widthScaleFactor = static_cast<float>(texture.visibleSize.width()) / texture.size.width();
    GLC(context, context->uniform4f(program->vertexShader().texTransformLocation(), 0, 0, widthScaleFactor, 1));

    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));

    LayerChromium::drawTexturedQuad(context, layerRenderer()->projectionMatrix(), drawTransform(),
                                    bounds().width(), bounds().height(), drawOpacity(),
                                    program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation());
}


void CCVideoLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "video layer\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
