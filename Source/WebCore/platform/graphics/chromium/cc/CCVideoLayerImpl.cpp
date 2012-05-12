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
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCProxy.h"
#include "cc/CCQuadCuller.h"
#include "cc/CCVideoDrawQuad.h"
#include <public/WebVideoFrame.h>
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

// This matrix is the default transformation for stream textures.
const float CCVideoLayerImpl::flipTransform[16] = {
    1, 0, 0, 0,
    0, -1, 0, 0,
    0, 0, 1, 0,
    0, 1, 0, 1,
};

CCVideoLayerImpl::CCVideoLayerImpl(int id, WebKit::WebVideoFrameProvider* provider)
    : CCLayerImpl(id)
    , m_provider(provider)
    , m_layerTreeHostImpl(0)
    , m_frame(0)
{
    memcpy(m_streamTextureMatrix, flipTransform, sizeof(m_streamTextureMatrix));

    // This only happens during a commit on the compositor thread while the main
    // thread is blocked. That makes this a thread-safe call to set the video
    // frame provider client that does not require a lock. The same is true of
    // the call in the destructor.
    m_provider->setVideoFrameProviderClient(this);
}

CCVideoLayerImpl::~CCVideoLayerImpl()
{
    // See comment in constructor for why this doesn't need a lock.
    if (m_provider) {
        m_provider->setVideoFrameProviderClient(0);
        m_provider = 0;
    }
    for (unsigned i = 0; i < MaxPlanes; ++i)
        m_textures[i].m_texture.clear();
}

void CCVideoLayerImpl::stopUsingProvider()
{
    // Block the provider from shutting down until this client is done
    // using the frame.
    MutexLocker locker(m_providerMutex);
    ASSERT(!m_frame);
    m_provider = 0;
}

// Convert WebKit::WebVideoFrame::Format to GraphicsContext3D's format enum values.
static GC3Denum convertVFCFormatToGC3DFormat(const WebKit::WebVideoFrame& frame)
{
    switch (frame.format()) {
    case WebKit::WebVideoFrame::FormatYV12:
    case WebKit::WebVideoFrame::FormatYV16:
        return GraphicsContext3D::LUMINANCE;
    case WebKit::WebVideoFrame::FormatNativeTexture:
        return frame.textureTarget();
    case WebKit::WebVideoFrame::FormatInvalid:
    case WebKit::WebVideoFrame::FormatRGB32:
    case WebKit::WebVideoFrame::FormatEmpty:
    case WebKit::WebVideoFrame::FormatI420:
        notImplemented();
    }
    return GraphicsContext3D::INVALID_VALUE;
}

void CCVideoLayerImpl::willDraw(LayerRendererChromium* layerRenderer)
{
    ASSERT(CCProxy::isImplThread());
    CCLayerImpl::willDraw(layerRenderer);

    // Explicitly lock and unlock the provider mutex so it can be held from
    // willDraw to didDraw. Since the compositor thread is in the middle of
    // drawing, the layer will not be destroyed before didDraw is called.
    // Therefore, the only thing that will prevent this lock from being released
    // is the GPU process locking it. As the GPU process can't cause the
    // destruction of the provider (calling stopUsingProvider), holding this
    // lock should not cause a deadlock.
    m_providerMutex.lock();

    willDrawInternal(layerRenderer);

    if (!m_frame)
        m_providerMutex.unlock();
}

void CCVideoLayerImpl::willDrawInternal(LayerRendererChromium* layerRenderer)
{
    ASSERT(CCProxy::isImplThread());

    if (!m_provider) {
        m_frame = 0;
        return;
    }

    m_frame = m_provider->getCurrentFrame();

    if (!m_frame)
        return;

    m_format = convertVFCFormatToGC3DFormat(*m_frame);

    if (m_format == GraphicsContext3D::INVALID_VALUE) {
        m_provider->putCurrentFrame(m_frame);
        m_frame = 0;
        return;
    }

    if (!reserveTextures(*m_frame, m_format, layerRenderer)) {
        m_provider->putCurrentFrame(m_frame);
        m_frame = 0;
    }
}

void CCVideoLayerImpl::appendQuads(CCQuadCuller& quadList, const CCSharedQuadState* sharedQuadState, bool&)
{
    ASSERT(CCProxy::isImplThread());

    if (!m_frame)
        return;

    IntRect quadRect(IntPoint(), bounds());
    OwnPtr<CCVideoDrawQuad> videoQuad = CCVideoDrawQuad::create(sharedQuadState, quadRect, m_textures, m_frame, m_format);

    if (m_format == Extensions3DChromium::GL_TEXTURE_EXTERNAL_OES)
        videoQuad->setMatrix(m_streamTextureMatrix);

    quadList.append(videoQuad.release());
}

void CCVideoLayerImpl::didDraw()
{
    ASSERT(CCProxy::isImplThread());
    CCLayerImpl::didDraw();

    if (!m_frame)
        return;

    for (unsigned plane = 0; plane < m_frame->planes(); ++plane)
        m_textures[plane].m_texture->unreserve();
    m_provider->putCurrentFrame(m_frame);
    m_frame = 0;

    m_providerMutex.unlock();
}

static int videoFrameDimension(int originalDimension, unsigned plane, int format)
{
    if (format == WebKit::WebVideoFrame::FormatYV12 && plane != WebKit::WebVideoFrame::yPlane)
        return originalDimension / 2;
    return originalDimension;
}

static bool hasPaddingBytes(const WebKit::WebVideoFrame& frame, unsigned plane)
{
    return frame.stride(plane) > videoFrameDimension(frame.width(), plane, frame.format());
}

IntSize CCVideoLayerImpl::computeVisibleSize(const WebKit::WebVideoFrame& frame, unsigned plane)
{
    int visibleWidth = videoFrameDimension(frame.width(), plane, frame.format());
    int originalWidth = visibleWidth;
    int visibleHeight = videoFrameDimension(frame.height(), plane, frame.format());

    // When there are dead pixels at the edge of the texture, decrease
    // the frame width by 1 to prevent the rightmost pixels from
    // interpolating with the dead pixels.
    if (hasPaddingBytes(frame, plane))
        --visibleWidth;

    // In YV12, every 2x2 square of Y values corresponds to one U and
    // one V value. If we decrease the width of the UV plane, we must decrease the
    // width of the Y texture by 2 for proper alignment. This must happen
    // always, even if Y's texture does not have padding bytes.
    if (plane == WebKit::WebVideoFrame::yPlane && frame.format() == WebKit::WebVideoFrame::FormatYV12) {
        if (hasPaddingBytes(frame, WebKit::WebVideoFrame::uPlane))
            visibleWidth = originalWidth - 2;
    }

    return IntSize(visibleWidth, visibleHeight);
}

bool CCVideoLayerImpl::reserveTextures(const WebKit::WebVideoFrame& frame, GC3Denum format, LayerRendererChromium* layerRenderer)
{
    if (frame.planes() > MaxPlanes)
        return false;
    int maxTextureSize = layerRenderer->capabilities().maxTextureSize;
    for (unsigned plane = 0; plane < frame.planes(); ++plane) {
        IntSize requiredTextureSize(frame.stride(plane), videoFrameDimension(frame.height(), plane, frame.format()));
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

void CCVideoLayerImpl::didReceiveFrame()
{
    setNeedsRedraw();
}

void CCVideoLayerImpl::didUpdateMatrix(const float matrix[16])
{
    memcpy(m_streamTextureMatrix, matrix, sizeof(m_streamTextureMatrix));
    setNeedsRedraw();
}

void CCVideoLayerImpl::didLoseContext()
{
    for (unsigned i = 0; i < MaxPlanes; ++i)
        m_textures[i].m_texture.clear();
}

void CCVideoLayerImpl::setNeedsRedraw()
{
    if (m_layerTreeHostImpl)
        m_layerTreeHostImpl->setNeedsRedraw();
}

void CCVideoLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "video layer\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
