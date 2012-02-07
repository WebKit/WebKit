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

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "NotImplemented.h"
#include "ProgramBinding.h"
#include "cc/CCProxy.h"
#include "cc/CCVideoDrawQuad.h"
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

CCVideoLayerImpl::CCVideoLayerImpl(int id, VideoFrameProvider* provider)
    : CCLayerImpl(id)
    , m_provider(provider)
{
    provider->setVideoFrameProviderClient(this);
}

CCVideoLayerImpl::~CCVideoLayerImpl()
{
    MutexLocker locker(m_providerMutex);
    if (m_provider) {
        m_provider->setVideoFrameProviderClient(0);
        m_provider = 0;
    }
    for (unsigned i = 0; i < MaxPlanes; ++i)
        m_textures[i].m_texture.clear();
    cleanupResources();
}

void CCVideoLayerImpl::stopUsingProvider()
{
    MutexLocker locker(m_providerMutex);
    m_provider = 0;
}

// Convert VideoFrameChromium::Format to GraphicsContext3D's format enum values.
static GC3Denum convertVFCFormatToGC3DFormat(VideoFrameChromium::Format format)
{
    switch (format) {
    case VideoFrameChromium::YV12:
    case VideoFrameChromium::YV16:
        return GraphicsContext3D::LUMINANCE;
    case VideoFrameChromium::RGBA:
        return GraphicsContext3D::RGBA;
    case VideoFrameChromium::NativeTexture:
        return GraphicsContext3D::TEXTURE_2D;
    case VideoFrameChromium::Invalid:
    case VideoFrameChromium::RGB555:
    case VideoFrameChromium::RGB565:
    case VideoFrameChromium::RGB24:
    case VideoFrameChromium::RGB32:
    case VideoFrameChromium::NV12:
    case VideoFrameChromium::Empty:
    case VideoFrameChromium::ASCII:
    case VideoFrameChromium::I420:
        notImplemented();
    }
    return GraphicsContext3D::INVALID_VALUE;
}

void CCVideoLayerImpl::draw(LayerRendererChromium* layerRenderer)
{
    ASSERT(CCProxy::isImplThread());

    MutexLocker locker(m_providerMutex);

    if (!m_provider)
        return;

    VideoFrameChromium* frame = m_provider->getCurrentFrame();
    if (!frame)
        return;
    GC3Denum format = convertVFCFormatToGC3DFormat(frame->format());

    if (format == GraphicsContext3D::INVALID_VALUE) {
        m_provider->putCurrentFrame(frame);
        return;
    }

    if (!copyFrameToTextures(frame, format, layerRenderer)) {
        m_provider->putCurrentFrame(frame);
        return;
    }

    switch (format) {
    case GraphicsContext3D::LUMINANCE:
        drawYUV(layerRenderer);
        break;
    case GraphicsContext3D::RGBA:
        drawRGBA(layerRenderer);
        break;
    case GraphicsContext3D::TEXTURE_2D:
        drawNativeTexture(layerRenderer);
        break;
    default:
        CRASH(); // Someone updated convertVFCFormatToGC3DFormat above but update this!
    }

    for (unsigned plane = 0; plane < frame->planes(); ++plane)
        m_textures[plane].m_texture->unreserve();
    m_provider->putCurrentFrame(frame);
}

void CCVideoLayerImpl::appendQuads(CCQuadList& quadList, const CCSharedQuadState* sharedQuadState)
{
    IntRect quadRect(IntPoint(), bounds());
    quadList.append(CCVideoDrawQuad::create(sharedQuadState, quadRect, this));
}

bool CCVideoLayerImpl::copyFrameToTextures(const VideoFrameChromium* frame, GC3Denum format, LayerRendererChromium* layerRenderer)
{
    if (frame->format() == VideoFrameChromium::NativeTexture) {
        m_nativeTextureId = frame->textureId();
        m_nativeTextureSize = IntSize(frame->width(), frame->height());
        return true;
    }

    if (!reserveTextures(frame, format, layerRenderer))
        return false;

    for (unsigned plane = 0; plane < frame->planes(); ++plane) {
        ASSERT(frame->requiredTextureSize(plane) == m_textures[plane].m_texture->size());
        copyPlaneToTexture(layerRenderer, frame->data(plane), plane);
    }
    for (unsigned plane = frame->planes(); plane < MaxPlanes; ++plane) {
        Texture& texture = m_textures[plane];
        texture.m_texture.clear();
        texture.m_visibleSize = IntSize();
    }
    m_planes = frame->planes();
    return true;
}

void CCVideoLayerImpl::copyPlaneToTexture(LayerRendererChromium* layerRenderer, const void* plane, int index)
{
    Texture& texture = m_textures[index];
    GraphicsContext3D* context = layerRenderer->context();
    TextureAllocator* allocator = layerRenderer->renderSurfaceTextureAllocator();
    texture.m_texture->bindTexture(context, allocator);
    GC3Denum format = texture.m_texture->format();
    IntSize dimensions = texture.m_texture->size();

    void* mem = static_cast<Extensions3DChromium*>(context->getExtensions())->mapTexSubImage2DCHROMIUM(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, dimensions.width(), dimensions.height(), format, GraphicsContext3D::UNSIGNED_BYTE, Extensions3DChromium::WRITE_ONLY);
    if (mem) {
        memcpy(mem, plane, dimensions.width() * dimensions.height());
        GLC(context, static_cast<Extensions3DChromium*>(context->getExtensions())->unmapTexSubImage2DCHROMIUM(mem));
    } else {
        // If mapTexSubImage2DCHROMIUM fails, then do the slower texSubImage2D
        // upload. This does twice the copies as mapTexSubImage2DCHROMIUM, one
        // in the command buffer and another to the texture.
        GLC(context, context->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, dimensions.width(), dimensions.height(), format, GraphicsContext3D::UNSIGNED_BYTE, plane));
    }
}

static IntSize computeVisibleSize(const VideoFrameChromium* frame, unsigned plane)
{
    int visibleWidth = frame->width(plane);
    int visibleHeight = frame->height(plane);
    // When there are dead pixels at the edge of the texture, decrease
    // the frame width by 1 to prevent the rightmost pixels from
    // interpolating with the dead pixels.
    if (frame->hasPaddingBytes(plane))
        --visibleWidth;

    // In YV12, every 2x2 square of Y values corresponds to one U and
    // one V value. If we decrease the width of the UV plane, we must decrease the
    // width of the Y texture by 2 for proper alignment. This must happen
    // always, even if Y's texture does not have padding bytes.
    if (plane == VideoFrameChromium::yPlane && frame->format() == VideoFrameChromium::YV12) {
        if (frame->hasPaddingBytes(VideoFrameChromium::uPlane)) {
            int originalWidth = frame->width(plane);
            visibleWidth = originalWidth - 2;
        }
    }

    return IntSize(visibleWidth, visibleHeight);
}

bool CCVideoLayerImpl::reserveTextures(const VideoFrameChromium* frame, GC3Denum format, LayerRendererChromium* layerRenderer)
{
    if (frame->planes() > MaxPlanes)
        return false;
    int maxTextureSize = layerRenderer->capabilities().maxTextureSize;
    for (unsigned plane = 0; plane < frame->planes(); ++plane) {
        IntSize requiredTextureSize = frame->requiredTextureSize(plane);
        // If the renderer cannot handle this large of a texture, return false.
        // FIXME: Remove this test when tiled layers are implemented.
        if (requiredTextureSize.isZero() || requiredTextureSize.width() > maxTextureSize || requiredTextureSize.height() > maxTextureSize)
            return false;
        if (!m_textures[plane].m_texture) {
            m_textures[plane].m_texture = ManagedTexture::create(layerRenderer->renderSurfaceTextureManager());
            if (!m_textures[plane].m_texture)
                return false;
            m_textures[plane].m_visibleSize = IntSize();
        } else {
            // The renderSurfaceTextureManager may have been destroyed and recreated since the last frame, so pass the new one.
            // This is a no-op if the TextureManager is still around.
            m_textures[plane].m_texture->setTextureManager(layerRenderer->renderSurfaceTextureManager());
        }
        if (m_textures[plane].m_texture->size() != requiredTextureSize)
            m_textures[plane].m_visibleSize = computeVisibleSize(frame, plane);
        if (!m_textures[plane].m_texture->reserve(requiredTextureSize, format))
            return false;
    }
    return true;
}

void CCVideoLayerImpl::drawYUV(LayerRendererChromium* layerRenderer) const
{
    const YUVProgram* program = layerRenderer->videoLayerYUVProgram();
    ASSERT(program && program->initialized());

    GraphicsContext3D* context = layerRenderer->context();
    const Texture& yTexture = m_textures[VideoFrameChromium::yPlane];
    const Texture& uTexture = m_textures[VideoFrameChromium::uPlane];
    const Texture& vTexture = m_textures[VideoFrameChromium::vPlane];

    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE1));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, yTexture.m_texture->textureId()));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE2));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, uTexture.m_texture->textureId()));
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE3));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, vTexture.m_texture->textureId()));

    GLC(context, context->useProgram(program->program()));

    float yWidthScaleFactor = static_cast<float>(yTexture.m_visibleSize.width()) / yTexture.m_texture->size().width();
    // Arbitrarily take the u sizes because u and v dimensions are identical.
    float uvWidthScaleFactor = static_cast<float>(uTexture.m_visibleSize.width()) / uTexture.m_texture->size().width();
    GLC(context, context->uniform1f(program->vertexShader().yWidthScaleFactorLocation(), yWidthScaleFactor));
    GLC(context, context->uniform1f(program->vertexShader().uvWidthScaleFactorLocation(), uvWidthScaleFactor));

    GLC(context, context->uniform1i(program->fragmentShader().yTextureLocation(), 1));
    GLC(context, context->uniform1i(program->fragmentShader().uTextureLocation(), 2));
    GLC(context, context->uniform1i(program->fragmentShader().vTextureLocation(), 3));

    GLC(context, context->uniformMatrix3fv(program->fragmentShader().ccMatrixLocation(), 0, const_cast<float*>(yuv2RGB), 1));
    GLC(context, context->uniform3fv(program->fragmentShader().yuvAdjLocation(), const_cast<float*>(yuvAdjust), 1));

    layerRenderer->drawTexturedQuad(drawTransform(), bounds().width(), bounds().height(), drawOpacity(), FloatQuad(),
                                    program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation(),
                                    -1);

    // Reset active texture back to texture 0.
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
}

template<class Program>
void CCVideoLayerImpl::drawCommon(LayerRendererChromium* layerRenderer, Program* program, float widthScaleFactor, Platform3DObject textureId) const
{
    ASSERT(program && program->initialized());

    GraphicsContext3D* context = layerRenderer->context();

    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));

    GLC(context, context->useProgram(program->program()));
    GLC(context, context->uniform4f(program->vertexShader().texTransformLocation(), 0, 0, widthScaleFactor, 1));
    GLC(context, context->uniform1i(program->fragmentShader().samplerLocation(), 0));

    layerRenderer->drawTexturedQuad(drawTransform(), bounds().width(), bounds().height(), drawOpacity(), layerRenderer->sharedGeometryQuad(),
                                    program->vertexShader().matrixLocation(),
                                    program->fragmentShader().alphaLocation(),
                                    -1);
}

void CCVideoLayerImpl::drawRGBA(LayerRendererChromium* layerRenderer) const
{
    const RGBAProgram* program = layerRenderer->videoLayerRGBAProgram();
    const Texture& texture = m_textures[VideoFrameChromium::rgbPlane];
    float widthScaleFactor = static_cast<float>(texture.m_visibleSize.width()) / texture.m_texture->size().width();
    drawCommon(layerRenderer, program, widthScaleFactor, texture.m_texture->textureId());
}

void CCVideoLayerImpl::drawNativeTexture(LayerRendererChromium* layerRenderer) const
{
    const NativeTextureProgram* program = layerRenderer->videoLayerNativeTextureProgram();
    drawCommon(layerRenderer, program, 1, m_nativeTextureId);
}

void CCVideoLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "video layer\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
