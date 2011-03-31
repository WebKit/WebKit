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
#include "RenderLayerBacking.h"
#include "VideoFrameChromium.h"
#include "VideoFrameProvider.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCVideoLayerImpl.h"

namespace WebCore {

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
    deleteTexturesInUse();
}

PassRefPtr<CCLayerImpl> VideoLayerChromium::createCCLayerImpl()
{
    return CCVideoLayerImpl::create(this);
}

void VideoLayerChromium::deleteTexturesInUse()
{
    if (!layerRenderer())
        return;

    GraphicsContext3D* context = layerRendererContext();
    for (unsigned plane = 0; plane < VideoFrameChromium::maxPlanes; plane++) {
        Texture texture = m_textures[plane];
        if (!texture.isEmpty && texture.ownedByLayerRenderer)
            GLC(context, context->deleteTexture(texture.id));
    }
}

void VideoLayerChromium::cleanupResources()
{
    LayerChromium::cleanupResources();
    if (m_currentFrame)
        releaseCurrentFrame();
    else
        resetFrameParameters();
}

void VideoLayerChromium::updateCompositorResources()
{
    if (!m_contentsDirty || !m_owner)
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

    // If the incoming frame is backed by a texture (i.e. decoded in hardware),
    // then we do not need to allocate a texture via the layer renderer. Instead
    // we save the texture data then exit.
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
        Texture texture = m_textures[plane];
        ASSERT(frame->requiredTextureSize(plane) == texture.size);
        updateTexture(context, texture.id, texture.size, textureFormat, frame->data(plane));
    }

    m_dirtyRect.setSize(FloatSize());
    m_contentsDirty = false;

    m_provider->putCurrentFrame(frame);
}

void VideoLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCVideoLayerImpl* videoLayer = static_cast<CCVideoLayerImpl*>(layer);
    videoLayer->setSkipsDraw(m_skipsDraw);
    videoLayer->setFrameFormat(m_frameFormat);
    for (size_t i = 0; i < 3; ++i)
        videoLayer->setTexture(i, m_textures[i]);
}


unsigned VideoLayerChromium::determineTextureFormat(const VideoFrameChromium* frame)
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

bool VideoLayerChromium::allocateTexturesIfNeeded(GraphicsContext3D* context, const VideoFrameChromium* frame, unsigned textureFormat)
{
    ASSERT(context);
    ASSERT(frame);

    for (unsigned plane = 0; plane < frame->planes(); plane++) {
        IntSize requiredTextureSize = frame->requiredTextureSize(plane);
        Texture texture = m_textures[plane];

        // If the renderer cannot handle this large of a texture, return false.
        // FIXME: Remove this test when tiled layers are implemented.
        if (!layerRenderer()->checkTextureSize(requiredTextureSize))
            return false;

        if (texture.isEmpty) {
            texture.id = layerRenderer()->createLayerTexture();
            texture.ownedByLayerRenderer = true;
            texture.isEmpty = false;
        }

        if (!requiredTextureSize.isZero() && requiredTextureSize != texture.size) {
            allocateTexture(context, texture.id, requiredTextureSize, textureFormat);
            texture.size = requiredTextureSize;
            texture.visibleSize = computeVisibleSize(frame, plane);
        }
        m_textures[plane] = texture;
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

void VideoLayerChromium::allocateTexture(GraphicsContext3D* context, unsigned textureId, const IntSize& dimensions, unsigned textureFormat) const
{
    GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    GLC(context, context->texImage2DResourceSafe(GraphicsContext3D::TEXTURE_2D, 0, textureFormat, dimensions.width(), dimensions.height(), 0, textureFormat, GraphicsContext3D::UNSIGNED_BYTE));
}

void VideoLayerChromium::updateTexture(GraphicsContext3D* context, unsigned textureId, const IntSize& dimensions, unsigned format, const void* data) const
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

void VideoLayerChromium::releaseCurrentFrame()
{
    if (!m_currentFrame)
        return;

    m_provider->putCurrentFrame(m_currentFrame);
    m_currentFrame = 0;
    resetFrameParameters();
}

void VideoLayerChromium::resetFrameParameters()
{
    deleteTexturesInUse();
    for (unsigned plane = 0; plane < VideoFrameChromium::maxPlanes; plane++) {
        m_textures[plane].id = 0;
        m_textures[plane].size = IntSize();
        m_textures[plane].visibleSize = IntSize();
        m_textures[plane].ownedByLayerRenderer = false;
        m_textures[plane].isEmpty = true;
    }
}

void VideoLayerChromium::saveCurrentFrame(VideoFrameChromium* frame)
{
    ASSERT(!m_currentFrame);
    deleteTexturesInUse();
    m_currentFrame = frame;
    for (unsigned plane = 0; plane < frame->planes(); plane++) {
        m_textures[plane].id = frame->texture(plane);
        m_textures[plane].size = frame->requiredTextureSize(plane);
        m_textures[plane].visibleSize = computeVisibleSize(frame, plane);
        m_textures[plane].ownedByLayerRenderer = false;
        m_textures[plane].isEmpty = false;
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
