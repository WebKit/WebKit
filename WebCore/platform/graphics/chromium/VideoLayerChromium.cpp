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

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
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

VideoLayerChromium::SharedValues::SharedValues(GraphicsContext3D* context)
    : m_context(context)
    , m_yuvShaderProgram(0)
    , m_rgbaShaderProgram(0)
    , m_yuvShaderMatrixLocation(0)
    , m_yuvWidthScaleFactorLocation(0)
    , m_rgbaShaderMatrixLocation(0)
    , m_rgbaWidthScaleFactorLocation(0)
    , m_ccMatrixLocation(0)
    , m_yTextureLocation(0)
    , m_uTextureLocation(0)
    , m_vTextureLocation(0)
    , m_rgbaTextureLocation(0)
    , m_yuvAlphaLocation(0)
    , m_rgbaAlphaLocation(0)
    , m_initialized(false)
{
    // Frame textures are allocated based on stride width, not visible frame
    // width, such that there is a guarantee that the frame rows line up
    // properly and are not shifted by (stride - width) pixels. To hide the
    // "padding" pixels between the edge of the visible frame width and the end
    // of the stride, we give the shader a widthScaleFactor (<=1.0) of how much
    // of the width of the texture should be shown when drawing the texture onto
    // the vertices.
    char vertexShaderString[] =
        "precision mediump float;     \n"
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "uniform mat4 matrix;         \n"
        "varying vec2 v_texCoord;     \n"
        "uniform float widthScaleFactor;  \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = matrix * a_position; \n"
        "  v_texCoord = vec2(widthScaleFactor * a_texCoord.x, a_texCoord.y); \n"
        "}                            \n";

    char yuvFragmentShaderString[] =
        "precision mediump float;                             \n"
        "precision mediump int;                               \n"
        "varying vec2 v_texCoord;                             \n"
        "uniform sampler2D y_texture;                         \n"
        "uniform sampler2D u_texture;                         \n"
        "uniform sampler2D v_texture;                         \n"
        "uniform float alpha;                                 \n"
        "uniform mat3 cc_matrix;                              \n"
        "void main()                                          \n"
        "{                                                    \n"
        "  float y = texture2D(y_texture, v_texCoord).x;      \n"
        "  float u = texture2D(u_texture, v_texCoord).r - .5; \n"
        "  float v = texture2D(v_texture, v_texCoord).r - .5; \n"
        "  vec3 rgb = cc_matrix * vec3(y, u, v);              \n"
        "  gl_FragColor = vec4(rgb.x, rgb.y, rgb.z, 1.0) * alpha; \n"
        "}                                                    \n";

    char rgbaFragmentShaderString[] =
        "precision mediump float;                             \n"
        "varying vec2 v_texCoord;                             \n"
        "uniform sampler2D rgba_texture;                      \n"
        "uniform float alpha;                                 \n"
        "void main()                                          \n"
        "{                                                    \n"
        "  vec4 texColor = texture2D(rgba_texture, vec2(v_texCoord.x, 1.0 - v_texCoord.y)); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha; \n"
        "}                                                    \n";

    m_rgbaShaderProgram = createShaderProgram(m_context, vertexShaderString, rgbaFragmentShaderString);
    if (!m_rgbaShaderProgram) {
        LOG_ERROR("VideoLayerChromium: Failed to create rgba shader program");
        return;
    }

    m_yuvShaderProgram = createShaderProgram(m_context, vertexShaderString, yuvFragmentShaderString);
    if (!m_yuvShaderProgram) {
        LOG_ERROR("VideoLayerChromium: Failed to create yuv shader program");
        return;
    }

    m_yuvShaderMatrixLocation = m_context->getUniformLocation(m_yuvShaderProgram, "matrix");
    m_yuvWidthScaleFactorLocation = m_context->getUniformLocation(m_yuvShaderProgram, "widthScaleFactor");
    m_yTextureLocation = m_context->getUniformLocation(m_yuvShaderProgram, "y_texture");
    m_uTextureLocation = m_context->getUniformLocation(m_yuvShaderProgram, "u_texture");
    m_vTextureLocation = m_context->getUniformLocation(m_yuvShaderProgram, "v_texture");
    m_ccMatrixLocation = m_context->getUniformLocation(m_yuvShaderProgram, "cc_matrix");
    m_yuvAlphaLocation = m_context->getUniformLocation(m_yuvShaderProgram, "alpha");

    ASSERT(m_yuvShaderMatrixLocation != -1);
    ASSERT(m_yuvWidthScaleFactorLocation != -1);
    ASSERT(m_yTextureLocation != -1);
    ASSERT(m_uTextureLocation != -1);
    ASSERT(m_vTextureLocation != -1);
    ASSERT(m_ccMatrixLocation != -1);
    ASSERT(m_yuvAlphaLocation != -1);

    m_rgbaShaderMatrixLocation = m_context->getUniformLocation(m_rgbaShaderProgram, "matrix");
    m_rgbaTextureLocation = m_context->getUniformLocation(m_rgbaShaderProgram, "rgba_texture");
    m_rgbaWidthScaleFactorLocation = m_context->getUniformLocation(m_rgbaShaderProgram, "widthScaleFactor");
    m_rgbaAlphaLocation = m_context->getUniformLocation(m_rgbaShaderProgram, "alpha");

    ASSERT(m_rgbaShaderMatrixLocation != -1);
    ASSERT(m_rgbaTextureLocation != -1);
    ASSERT(m_rgbaWidthScaleFactorLocation != -1);
    ASSERT(m_rgbaAlphaLocation != -1);

    m_initialized = true;
}

VideoLayerChromium::SharedValues::~SharedValues()
{
    if (m_yuvShaderProgram)
        GLC(m_context, m_context->deleteProgram(m_yuvShaderProgram));
    if (m_rgbaShaderProgram)
        GLC(m_context, m_context->deleteProgram(m_rgbaShaderProgram));
}

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
{
    for (unsigned plane = 0; plane < VideoFrameChromium::maxPlanes; plane++) {
        m_textures[plane] = 0;
        m_textureSizes[plane] = IntSize();
        m_frameSizes[plane] = IntSize();
    }
}

VideoLayerChromium::~VideoLayerChromium()
{
    if (!layerRenderer())
        return;

    GraphicsContext3D* context = layerRendererContext();
    for (unsigned plane = 0; plane < VideoFrameChromium::maxPlanes; plane++) {
        if (m_textures[plane])
            GLC(context, context->deleteTexture(m_textures[plane]));
    }
}

void VideoLayerChromium::updateContents()
{
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
            m_frameSizes[plane] = IntSize(frame->width(), frame->height());
        }
    }
    return true;
}

void VideoLayerChromium::allocateTexture(GraphicsContext3D* context, unsigned textureId, const IntSize& dimensions, unsigned textureFormat)
{
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    GLC(context, context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, textureFormat, dimensions.width(), dimensions.height(), 0, textureFormat, GraphicsContext3D::UNSIGNED_BYTE, 0));
}

void VideoLayerChromium::updateTexture(GraphicsContext3D* context, unsigned textureId, const IntSize& dimensions, unsigned format, const void* data)
{
    ASSERT(context);
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    void* mem = context->mapTexSubImage2DCHROMIUM(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, dimensions.width(), dimensions.height(), format, GraphicsContext3D::UNSIGNED_BYTE, GraphicsContext3D::WRITE_ONLY);
    if (mem) {
        memcpy(mem, data, dimensions.width() * dimensions.height());
        GLC(context, context->unmapTexSubImage2DCHROMIUM(mem));
    } else {
        // FIXME: We should have some sort of code to handle the case when
        // mapTexSubImage2D fails.
        m_skipsDraw = true;
    }
}

void VideoLayerChromium::draw()
{
    if (m_skipsDraw)
        return;

    ASSERT(layerRenderer());
    const VideoLayerChromium::SharedValues* sv = layerRenderer()->videoLayerSharedValues();
    ASSERT(sv && sv->initialized());

    switch (m_frameFormat) {
    case VideoFrameChromium::YV12:
        drawYUV(sv);
        break;
    case VideoFrameChromium::RGBA:
        drawRGBA(sv);
        break;
    default:
        // FIXME: Implement other paths.
        notImplemented();
        break;
    }
}

void VideoLayerChromium::drawYUV(const SharedValues* sv)
{
    GraphicsContext3D* context = layerRendererContext();
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE1));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textures[VideoFrameChromium::yPlane]));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE2));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textures[VideoFrameChromium::uPlane]));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE3));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textures[VideoFrameChromium::vPlane]));

    layerRenderer()->useShader(sv->yuvShaderProgram());
    unsigned frameWidth = m_frameSizes[VideoFrameChromium::yPlane].width();
    unsigned textureWidth = m_textureSizes[VideoFrameChromium::yPlane].width();
    float widthScaleFactor = static_cast<float>(frameWidth) / textureWidth;
    GLC(context, context->uniform1f(sv->yuvWidthScaleFactorLocation(), widthScaleFactor));

    GLC(context, context->uniform1i(sv->yTextureLocation(), 1));
    GLC(context, context->uniform1i(sv->uTextureLocation(), 2));
    GLC(context, context->uniform1i(sv->vTextureLocation(), 3));

    GLC(context, context->uniformMatrix3fv(sv->ccMatrixLocation(), 0, const_cast<float*>(yuv2RGB), 1));

    drawTexturedQuad(context, layerRenderer()->projectionMatrix(), drawTransform(),
                     bounds().width(), bounds().height(), drawOpacity(),
                     sv->yuvShaderMatrixLocation(), sv->yuvAlphaLocation());

    // Reset active texture back to texture 0.
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
}

void VideoLayerChromium::drawRGBA(const SharedValues* sv)
{
    GraphicsContext3D* context = layerRendererContext();
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textures[VideoFrameChromium::rgbPlane]));

    layerRenderer()->useShader(sv->rgbaShaderProgram());
    unsigned frameWidth = m_frameSizes[VideoFrameChromium::rgbPlane].width();
    unsigned textureWidth = m_textureSizes[VideoFrameChromium::rgbPlane].width();
    float widthScaleFactor = static_cast<float>(frameWidth) / textureWidth;
    GLC(context, context->uniform1f(sv->rgbaWidthScaleFactorLocation(), widthScaleFactor));

    GLC(context, context->uniform1i(sv->rgbaTextureLocation(), 0));

    drawTexturedQuad(context, layerRenderer()->projectionMatrix(), drawTransform(),
                     bounds().width(), bounds().height(), drawOpacity(),
                     sv->rgbaShaderMatrixLocation(), sv->rgbaAlphaLocation());
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
