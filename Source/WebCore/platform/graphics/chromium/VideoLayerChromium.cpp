/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "VideoLayerChromium.h"

#include "cc/CCLayerImpl.h"
#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "NotImplemented.h"
#include "RenderLayerBacking.h"
#include "VideoFrameChromium.h"
#include "VideoFrameProvider.h"

namespace WebCore {

// These values are magic numbers that are used in the transformation
// from YUV to RGB color values.
const float VideoLayerChromium::yuv2RGB[9] = {
    1.f, 1.f, 1.f,
    0.f, -.344f, 1.772f,
    1.403f, -.714f, 0.f,
};

PassRefPtr<VideoLayerChromium> VideoLayerChromium::create(GraphicsLayerChromium* owner,
                                                          VideoFrameProvider* provider)
{
    return adoptRef(new VideoLayerChromium(owner, provider));
}

VideoLayerChromium::VideoLayerChromium(GraphicsLayerChromium* owner, VideoFrameProvider* provider)
    : LayerChromium(owner)
    , m_skipsDraw(true)
    , m_frameFormat(VideoFrameChromium::Invalid)
    , m_provider(provider)
    , m_currentFrame(0)
{
    resetFrameParameters();
}

VideoLayerChromium::~VideoLayerChromium()
{
    cleanupResources();
}

void VideoLayerChromium::cleanupResources()
{
    LayerChromium::cleanupResources();
    releaseCurrentFrame();
    if (!layerRenderer())
        return;

    GraphicsContext3D* context = layerRendererContext();
    for (unsigned plane = 0; plane < VideoFrameChromium::maxPlanes; plane++) {
        if (m_textures[plane])
            GLC(context, context->deleteTexture(m_textures[plane]));
    }
}

void VideoLayerChromium::updateContentsIfDirty()
{
    if (!m_contentsDirty)
        return;

    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_owner->client());
    if (!backing || backing->paintingGoesToWindow())
        return;

    ASSERT(drawsContent());

    m_skipsDraw = false;
    VideoFrameChromium* frame = m_provider->getCurrentFrame();
    if (!frame) {
        m_skipsDraw = true;
        m_provider->putCurrentFrame(frame);
        return;
    }

    m_frameFormat = frame->format();
    unsigned textureFormat = determineTextureFormat(frame);
    if (textureFormat == GraphicsContext3D::INVALID_VALUE) {
        // FIXME: Implement other paths.
        notImplemented();
        m_skipsDraw = true;
        m_provider->putCurrentFrame(frame);
        return;
    }

    if (frame->surfaceType() == VideoFrameChromium::TypeTexture) {
        releaseCurrentFrame();
        saveCurrentFrame(frame);
        m_dirtyRect.setSize(FloatSize());
        m_contentsDirty = false;
        return;
    }

    // Allocate textures for planes if they are not allocated already, or
    // reallocate textures that are the wrong size for the frame.
    GraphicsContext3D* context = layerRendererContext();
    bool texturesAllocated = allocateTexturesIfNeeded(context, frame, textureFormat);
    if (!texturesAllocated) {
        m_skipsDraw = true;
        m_provider->putCurrentFrame(frame);
        return;
    }

    // Update texture planes.
    for (unsigned plane = 0; plane < frame->planes(); plane++) {
        ASSERT(frame->requiredTextureSize(plane) == m_textureSizes[plane]);
        updateTexture(context, m_textures[plane], frame->requiredTextureSize(plane), textureFormat, frame->data(plane));
    }

    m_dirtyRect.setSize(FloatSize());
    m_contentsDirty = false;

    m_provider->putCurrentFrame(frame);
}

unsigned VideoLayerChromium::determineTextureFormat(VideoFrameChromium* frame)
{
    switch (frame->format()) {
    case VideoFrameChromium::YV12:
    case VideoFrameChromium::YV16:
        return GraphicsContext3D::LUMINANCE;
    case VideoFrameChromium::RGBA:
        return GraphicsContext3D::RGBA;
    default:
        break;
    }
    return GraphicsContext3D::INVALID_VALUE;
}

bool VideoLayerChromium::allocateTexturesIfNeeded(GraphicsContext3D* context, VideoFrameChromium* frame, unsigned textureFormat)
{
    ASSERT(context);
    ASSERT(frame);

    for (unsigned plane = 0; plane < frame->planes(); plane++) {
        IntSize planeTextureSize = frame->requiredTextureSize(plane);

        // If the renderer cannot handle this large of a texture, return false.
        // FIXME: Remove this test when tiled layers are implemented.
        if (!layerRenderer()->checkTextureSize(planeTextureSize))
            return false;

        if (!m_textures[plane])
            m_textures[plane] = layerRenderer()->createLayerTexture();

        if (!planeTextureSize.isZero() && planeTextureSize != m_textureSizes[plane]) {
            allocateTexture(context, m_textures[plane], planeTextureSize, textureFormat);
            m_textureSizes[plane] = planeTextureSize;
            int frameWidth = frame->width(plane);
            int frameHeight = frame->height(plane);
            // When there are dead pixels at the edge of the texture, decrease
            // the frame width by 1 to prevent the rightmost pixels from
            // interpolating with the dead pixels.
            if (frame->hasPaddingBytes(plane))
                --frameWidth;
            m_frameSizes[plane] = IntSize(frameWidth, frameHeight);
        }
    }

    // In YV12, every 2x2 square of Y values corresponds to one U and
    // one V value. If we decrease the width of the UV plane, we must decrease the
    // width of the Y texture by 2 for proper alignment. This must happen
    // always, even if Y's texture does not have padding bytes.
    if (frame->format() == VideoFrameChromium::YV12) {
        int yPlaneOriginalWidth = frame->width(VideoFrameChromium::yPlane);
        if (frame->hasPaddingBytes(VideoFrameChromium::uPlane))
            m_frameSizes[VideoFrameChromium::yPlane].setWidth(yPlaneOriginalWidth - 2);
    }

    return true;
}

void VideoLayerChromium::allocateTexture(GraphicsContext3D* context, unsigned textureId, const IntSize& dimensions, unsigned textureFormat)
{
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    GLC(context, context->texImage2DResourceSafe(GraphicsContext3D::TEXTURE_2D, 0, textureFormat, dimensions.width(), dimensions.height(), 0, textureFormat, GraphicsContext3D::UNSIGNED_BYTE));
}

void VideoLayerChromium::updateTexture(GraphicsContext3D* context, unsigned textureId, const IntSize& dimensions, unsigned format, const void* data)
{
    ASSERT(context);
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    void* mem = static_cast<Extensions3DChromium*>(context->getExtensions())->mapTexSubImage2DCHROMIUM(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, dimensions.width(), dimensions.height(), format, GraphicsContext3D::UNSIGNED_BYTE, Extensions3DChromium::WRITE_ONLY);
    if (mem) {
        memcpy(mem, data, dimensions.width() * dimensions.height());
        GLC(context, static_cast<Extensions3DChromium*>(context->getExtensions())->unmapTexSubImage2DCHROMIUM(mem));
    } else {
        // If mapTexSubImage2DCHROMIUM fails, then do the slower texSubImage2D
        // upload. This does twice the copies as mapTexSubImage2DCHROMIUM, one
        // in the command buffer and another to the texture.
        GLC(context, context->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, dimensions.width(), dimensions.height(), format, GraphicsContext3D::UNSIGNED_BYTE, data));
    }
}

void VideoLayerChromium::draw()
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
    releaseCurrentFrame();
}

void VideoLayerChromium::releaseCurrentFrame()
{
    if (!m_currentFrame)
        return;

    m_provider->putCurrentFrame(m_currentFrame);
    m_currentFrame = 0;
    resetFrameParameters();
}

void VideoLayerChromium::drawYUV(const VideoLayerChromium::YUVProgram* program)
{
    GraphicsContext3D* context = layerRendererContext();
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE1));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textures[VideoFrameChromium::yPlane]));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE2));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textures[VideoFrameChromium::uPlane]));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE3));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textures[VideoFrameChromium::vPlane]));

    layerRenderer()->useShader(program->program());
    unsigned yFrameWidth = m_frameSizes[VideoFrameChromium::yPlane].width();
    unsigned yTextureWidth = m_textureSizes[VideoFrameChromium::yPlane].width();
    // Arbitrarily take the u sizes because u and v dimensions are identical.
    unsigned uvFrameWidth = m_frameSizes[VideoFrameChromium::uPlane].width();
    unsigned uvTextureWidth = m_textureSizes[VideoFrameChromium::uPlane].width();

    float yWidthScaleFactor = static_cast<float>(yFrameWidth) / yTextureWidth;
    float uvWidthScaleFactor = static_cast<float>(uvFrameWidth) / uvTextureWidth;
    GLC(context, context->uniform1f(program->vertexShader().yWidthScaleFactorLocation(), yWidthScaleFactor));
    GLC(context, context->uniform1f(program->vertexShader().uvWidthScaleFactorLocation(), uvWidthScaleFactor));

    GLC(context, context->uniform1i(program->fragmentShader().yTextureLocation(), 1));
    GLC(context, context->uniform1i(program->fragmentShader().uTextureLocation(), 2));
    GLC(context, context->uniform1i(program->fragmentShader().vTextureLocation(), 3));

    // This value of 0.5 maps to 128. It is used in the YUV to RGB conversion
    // formula to turn unsigned u and v values to signed u and v values.
    // This is loaded as a uniform because certain drivers have problems
    // reading literal float values.
    GLC(context, context->uniform1f(program->fragmentShader().signAdjLocation(), 0.5));

    GLC(context, context->uniformMatrix3fv(program->fragmentShader().ccMatrixLocation(), 0, const_cast<float*>(yuv2RGB), 1));

    drawTexturedQuad(context, layerRenderer()->projectionMatrix(), ccLayerImpl()->drawTransform(),
                     bounds().width(), bounds().height(), ccLayerImpl()->drawOpacity(),
                     program->vertexShader().matrixLocation(),
                     program->fragmentShader().alphaLocation());

    // Reset active texture back to texture 0.
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
}

void VideoLayerChromium::drawRGBA(const VideoLayerChromium::RGBAProgram* program)
{
    GraphicsContext3D* context = layerRendererContext();
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textures[VideoFrameChromium::rgbPlane]));

    layerRenderer()->useShader(program->program());
    unsigned frameWidth = m_frameSizes[VideoFrameChromium::rgbPlane].width();
    unsigned textureWidth = m_textureSizes[VideoFrameChromium::rgbPlane].width();
    float widthScaleFactor = static_cast<float>(frameWidth) / textureWidth;
    GLC(context, context->uniform4f(program->vertexShader().texTransformLocation(), 0, 0, widthScaleFactor, 1));

    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));

    drawTexturedQuad(context, layerRenderer()->projectionMatrix(), ccLayerImpl()->drawTransform(),
                     bounds().width(), bounds().height(), ccLayerImpl()->drawOpacity(),
                     program->vertexShader().matrixLocation(),
                     program->fragmentShader().alphaLocation());
}

void VideoLayerChromium::resetFrameParameters()
{
    for (unsigned plane = 0; plane < VideoFrameChromium::maxPlanes; plane++) {
        m_textures[plane] = 0;
        m_textureSizes[plane] = IntSize();
        m_frameSizes[plane] = IntSize();
    }
}

void VideoLayerChromium::saveCurrentFrame(VideoFrameChromium* frame)
{
    ASSERT(!m_currentFrame);
    m_currentFrame = frame;
    for (unsigned plane = 0; plane < frame->planes(); plane++) {
        m_textures[plane] = frame->texture(plane);
        m_textureSizes[plane] = frame->requiredTextureSize(plane);
        m_frameSizes[plane] = m_textureSizes[plane];
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
