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

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "NotImplemented.h"
#include "VideoFrameChromium.h"
#include "VideoFrameProvider.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCTextureUpdater.h"
#include "cc/CCVideoLayerImpl.h"

namespace WebCore {

PassRefPtr<VideoLayerChromium> VideoLayerChromium::create(CCLayerDelegate* delegate,
                                                          VideoFrameProvider* provider)
{
    return adoptRef(new VideoLayerChromium(delegate, provider));
}

VideoLayerChromium::VideoLayerChromium(CCLayerDelegate* delegate, VideoFrameProvider* provider)
    : LayerChromium(delegate)
    , m_skipsDraw(true)
    , m_frameFormat(VideoFrameChromium::Invalid)
    , m_provider(provider)
    , m_planes(0)
    , m_nativeTextureId(0)
{
}

VideoLayerChromium::~VideoLayerChromium()
{
    cleanupResources();
}

PassRefPtr<CCLayerImpl> VideoLayerChromium::createCCLayerImpl()
{
    return CCVideoLayerImpl::create(m_layerId);
}

void VideoLayerChromium::cleanupResources()
{
    LayerChromium::cleanupResources();
    for (unsigned i = 0; i < MaxPlanes; ++i)
        m_textures[i].m_texture.clear();
}

void VideoLayerChromium::updateCompositorResources(GraphicsContext3D* context, CCTextureUpdater& updater)
{
    if (!m_delegate || !m_provider || !drawsContent())
        return;

    if (!m_needsDisplay && texturesValid()) {
        for (unsigned plane = 0; plane < m_planes; plane++) {
            ManagedTexture* tex = m_textures[plane].m_texture.get();
            if (!tex->reserve(tex->size(), tex->format())) {
                m_skipsDraw = true;
                break;
            }
        }
        return;
    }

    m_planes = 0;
    m_skipsDraw = false;

    VideoFrameChromium* frame = m_provider->getCurrentFrame();
    if (!frame) {
        m_skipsDraw = true;
        m_provider->putCurrentFrame(frame);
        return;
    }

    m_frameFormat = frame->format();
    GC3Denum textureFormat = determineTextureFormat(frame);
    if (textureFormat == GraphicsContext3D::INVALID_VALUE) {
        // FIXME: Implement other paths.
        notImplemented();
        m_skipsDraw = true;
        m_provider->putCurrentFrame(frame);
        return;
    }

    if (textureFormat == GraphicsContext3D::TEXTURE_2D) {
        m_nativeTextureId = frame->textureId();
        m_nativeTextureSize = IntSize(frame->width(), frame->height());
        m_nativeTextureVisibleSize = IntSize(frame->width(), frame->height());
        m_needsDisplay = false;
        m_provider->putCurrentFrame(frame);
        return;
    }

    // Allocate textures for planes if they are not allocated already, or
    // reallocate textures that are the wrong size for the frame.
    bool texturesReserved = reserveTextures(frame, textureFormat);
    if (!texturesReserved) {
        m_skipsDraw = true;
        m_provider->putCurrentFrame(frame);
        return;
    }

    // Update texture planes.
    for (unsigned plane = 0; plane < frame->planes(); plane++) {
        Texture& texture = m_textures[plane];
        ASSERT(texture.m_texture);
        ASSERT(frame->requiredTextureSize(plane) == texture.m_texture->size());
        updateTexture(context, updater.allocator(), texture, frame->data(plane));
    }

    m_planes = frame->planes();
    ASSERT(m_planes <= MaxPlanes);

    m_updateRect = FloatRect(FloatPoint(), bounds());
    m_needsDisplay = false;

    m_provider->putCurrentFrame(frame);
}

void VideoLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCVideoLayerImpl* videoLayer = static_cast<CCVideoLayerImpl*>(layer);
    videoLayer->setSkipsDraw(m_skipsDraw);
    videoLayer->setFrameFormat(m_frameFormat);
    for (unsigned i = 0; i < m_planes; ++i) {
        if (!m_textures[i].m_texture) {
            videoLayer->setSkipsDraw(true);
            break;
        }
        videoLayer->setTexture(i, m_textures[i].m_texture->textureId(), m_textures[i].m_texture->size(), m_textures[i].m_visibleSize);
    }
    for (unsigned i = m_planes; i < MaxPlanes; ++i)
        videoLayer->setTexture(i, 0, IntSize(), IntSize());
    if (m_frameFormat == VideoFrameChromium::NativeTexture)
        videoLayer->setNativeTexture(m_nativeTextureId, m_nativeTextureSize, m_nativeTextureVisibleSize);
}

void VideoLayerChromium::setLayerTreeHost(CCLayerTreeHost* host)
{
    if (host && layerTreeHost() != host) {
        for (unsigned i = 0; i < MaxPlanes; ++i) {
            m_textures[i].m_visibleSize = IntSize();
            m_textures[i].m_texture = ManagedTexture::create(host->contentsTextureManager());
        }
    }

    LayerChromium::setLayerTreeHost(host);
}

GC3Denum VideoLayerChromium::determineTextureFormat(const VideoFrameChromium* frame)
{
    switch (frame->format()) {
    case VideoFrameChromium::YV12:
    case VideoFrameChromium::YV16:
        return GraphicsContext3D::LUMINANCE;
    case VideoFrameChromium::RGBA:
        return GraphicsContext3D::RGBA;
    case VideoFrameChromium::NativeTexture:
        return GraphicsContext3D::TEXTURE_2D;
    default:
        break;
    }
    return GraphicsContext3D::INVALID_VALUE;
}

bool VideoLayerChromium::texturesValid()
{
    for (unsigned plane = 0; plane < m_planes; plane++) {
        ManagedTexture* tex = m_textures[plane].m_texture.get();
        if (!tex->isValid(tex->size(), tex->format()))
            return false;
    }
    return true;
}

bool VideoLayerChromium::reserveTextures(const VideoFrameChromium* frame, GC3Denum textureFormat)
{
    ASSERT(layerTreeHost());
    ASSERT(frame);
    ASSERT(textureFormat != GraphicsContext3D::INVALID_VALUE);

    int maxTextureSize = layerTreeHost()->layerRendererCapabilities().maxTextureSize;

    for (unsigned plane = 0; plane < frame->planes(); plane++) {
        IntSize requiredTextureSize = frame->requiredTextureSize(plane);

        // If the renderer cannot handle this large of a texture, return false.
        // FIXME: Remove this test when tiled layers are implemented.
        if (requiredTextureSize.isZero() || requiredTextureSize.width() > maxTextureSize || requiredTextureSize.height() > maxTextureSize)
            return false;

        if (!m_textures[plane].m_texture)
            return false;

        if (m_textures[plane].m_texture->size() != requiredTextureSize)
            m_textures[plane].m_visibleSize = computeVisibleSize(frame, plane);

        m_textures[plane].m_texture->reserve(requiredTextureSize, textureFormat);
    }

    return true;
}

IntSize VideoLayerChromium::computeVisibleSize(const VideoFrameChromium* frame, unsigned plane)
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

void VideoLayerChromium::updateTexture(GraphicsContext3D* context, TextureAllocator* allocator, Texture& texture, const void* data) const
{
    ASSERT(context);
    ASSERT(texture.m_texture);

    texture.m_texture->bindTexture(context, allocator);

    GC3Denum format = texture.m_texture->format();
    IntSize dimensions = texture.m_texture->size();

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

void VideoLayerChromium::releaseProvider()
{
    m_provider = 0;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
