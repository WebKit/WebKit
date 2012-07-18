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
#include "LayerRendererChromium.h" // For GLC macro
#include "LayerTextureSubImage.h"
#include "NotImplemented.h"
#include "TextStream.h"
#include "TextureAllocator.h"
#include "cc/CCGraphicsContext.h"
#include "cc/CCIOSurfaceDrawQuad.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCProxy.h"
#include "cc/CCQuadCuller.h"
#include "cc/CCStreamVideoDrawQuad.h"
#include "cc/CCTextureDrawQuad.h"
#include "cc/CCYUVVideoDrawQuad.h"
#include <public/WebVideoFrame.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

CCVideoLayerImpl::CCVideoLayerImpl(int id, WebKit::WebVideoFrameProvider* provider)
    : CCLayerImpl(id)
    , m_provider(provider)
    , m_frame(0)
{
    // This matrix is the default transformation for stream textures, and flips on the Y axis.
    m_streamTextureMatrix = WebKit::WebTransformationMatrix(
        1, 0, 0, 0,
        0, -1, 0, 0,
        0, 0, 1, 0,
        0, 1, 0, 1);

    // This only happens during a commit on the compositor thread while the main
    // thread is blocked. That makes this a thread-safe call to set the video
    // frame provider client that does not require a lock. The same is true of
    // the call in the destructor.
    ASSERT(CCProxy::isMainThreadBlocked());
    m_provider->setVideoFrameProviderClient(this);
}

CCVideoLayerImpl::~CCVideoLayerImpl()
{
    // See comment in constructor for why this doesn't need a lock.
    ASSERT(CCProxy::isMainThreadBlocked());
    if (m_provider) {
        m_provider->setVideoFrameProviderClient(0);
        m_provider = 0;
    }
    freePlaneData(layerTreeHostImpl()->context());

#if !ASSERT_DISABLED
    for (unsigned i = 0; i < WebKit::WebVideoFrame::maxPlanes; ++i)
        ASSERT(!m_framePlanes[i].textureId);
#endif
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

void CCVideoLayerImpl::willDraw(CCRenderer* layerRenderer, CCGraphicsContext* context)
{
    ASSERT(CCProxy::isImplThread());
    CCLayerImpl::willDraw(layerRenderer, context);

    // Explicitly lock and unlock the provider mutex so it can be held from
    // willDraw to didDraw. Since the compositor thread is in the middle of
    // drawing, the layer will not be destroyed before didDraw is called.
    // Therefore, the only thing that will prevent this lock from being released
    // is the GPU process locking it. As the GPU process can't cause the
    // destruction of the provider (calling stopUsingProvider), holding this
    // lock should not cause a deadlock.
    m_providerMutex.lock();

    willDrawInternal(layerRenderer, context);
    freeUnusedPlaneData(context);

    if (!m_frame)
        m_providerMutex.unlock();
}

void CCVideoLayerImpl::willDrawInternal(CCRenderer* layerRenderer, CCGraphicsContext* context)
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

    if (m_frame->planes() > WebKit::WebVideoFrame::maxPlanes) {
        m_provider->putCurrentFrame(m_frame);
        m_frame = 0;
        return;
    }

    if (!allocatePlaneData(layerRenderer, context)) {
        m_provider->putCurrentFrame(m_frame);
        m_frame = 0;
        return;
    }

    if (!copyPlaneData(layerRenderer, context)) {
        m_provider->putCurrentFrame(m_frame);
        m_frame = 0;
        return;
    }
}

void CCVideoLayerImpl::appendQuads(CCQuadCuller& quadList, const CCSharedQuadState* sharedQuadState, bool&)
{
    ASSERT(CCProxy::isImplThread());

    if (!m_frame)
        return;

    // FIXME: When we pass quads out of process, we need to double-buffer, or
    // otherwise synchonize use of all textures in the quad.

    IntRect quadRect(IntPoint(), contentBounds());

    switch (m_format) {
    case GraphicsContext3D::LUMINANCE: {
        // YUV software decoder.
        const FramePlane& yPlane = m_framePlanes[WebKit::WebVideoFrame::yPlane];
        const FramePlane& uPlane = m_framePlanes[WebKit::WebVideoFrame::uPlane];
        const FramePlane& vPlane = m_framePlanes[WebKit::WebVideoFrame::vPlane];
        OwnPtr<CCYUVVideoDrawQuad> yuvVideoQuad = CCYUVVideoDrawQuad::create(sharedQuadState, quadRect, yPlane, uPlane, vPlane);
        quadList.append(yuvVideoQuad.release());
        break;
    }
    case GraphicsContext3D::RGBA: {
        // RGBA software decoder.
        const FramePlane& plane = m_framePlanes[WebKit::WebVideoFrame::rgbPlane];
        float widthScaleFactor = static_cast<float>(plane.visibleSize.width()) / plane.size.width();

        bool premultipliedAlpha = true;
        FloatRect uvRect(0, 0, widthScaleFactor, 1);
        bool flipped = false;
        OwnPtr<CCTextureDrawQuad> textureQuad = CCTextureDrawQuad::create(sharedQuadState, quadRect, plane.textureId, premultipliedAlpha, uvRect, flipped);
        quadList.append(textureQuad.release());
        break;
    }
    case GraphicsContext3D::TEXTURE_2D: {
        // NativeTexture hardware decoder.
        bool premultipliedAlpha = true;
        FloatRect uvRect(0, 0, 1, 1);
#if defined(OS_CHROMEOS) && defined(__ARMEL__)
        bool flipped = true; // Under the covers, implemented by OpenMAX IL.
#elif OS(WINDOWS)
        bool flipped = false; // Under the covers, implemented by DXVA.
#else
        bool flipped = false; // LibVA (cros/intel), MacOS.
#endif
        OwnPtr<CCTextureDrawQuad> textureQuad = CCTextureDrawQuad::create(sharedQuadState, quadRect, m_frame->textureId(), premultipliedAlpha, uvRect, flipped);
        quadList.append(textureQuad.release());
        break;
    }
    case Extensions3D::TEXTURE_RECTANGLE_ARB: {
        IntSize textureSize(m_frame->width(), m_frame->height()); 
        OwnPtr<CCIOSurfaceDrawQuad> ioSurfaceQuad = CCIOSurfaceDrawQuad::create(sharedQuadState, quadRect, textureSize, m_frame->textureId(), CCIOSurfaceDrawQuad::Unflipped);
        quadList.append(ioSurfaceQuad.release());
        break;
    }
    case Extensions3DChromium::GL_TEXTURE_EXTERNAL_OES: {
        // StreamTexture hardware decoder.
        OwnPtr<CCStreamVideoDrawQuad> streamVideoQuad = CCStreamVideoDrawQuad::create(sharedQuadState, quadRect, m_frame->textureId(), m_streamTextureMatrix);
        quadList.append(streamVideoQuad.release());
        break;
    }
    default:
        CRASH(); // Someone updated convertVFCFormatToGC3DFormat above but update this!
    }
}

void CCVideoLayerImpl::didDraw()
{
    ASSERT(CCProxy::isImplThread());
    CCLayerImpl::didDraw();

    if (!m_frame)
        return;

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

bool CCVideoLayerImpl::FramePlane::allocateData(CCGraphicsContext* context)
{
    if (textureId)
        return true;

    WebKit::WebGraphicsContext3D* context3D = context->context3D();
    if (!context3D)
        return false;

    GLC(context3D, textureId = context3D->createTexture());
    GLC(context3D, context3D->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    // Do basic linear filtering on resize.
    GLC(context3D, context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(context3D, context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    // NPOT textures in GL ES only work when the wrap mode is set to GraphicsContext3D::CLAMP_TO_EDGE.
    GLC(context3D, context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(context3D, context3D->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));

    GLC(context3D, context3D->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, format, size.width(), size.height(), 0, format, GraphicsContext3D::UNSIGNED_BYTE, 0));

    return textureId;
}

void CCVideoLayerImpl::FramePlane::freeData(CCGraphicsContext* context)
{
    if (!textureId)
        return;

    WebKit::WebGraphicsContext3D* context3D = context->context3D();
    if (!context3D)
        return;

    GLC(context3D, context3D->deleteTexture(textureId));
    textureId = 0;
}

bool CCVideoLayerImpl::allocatePlaneData(CCRenderer* layerRenderer, CCGraphicsContext* context)
{
    int maxTextureSize = layerRenderer->capabilities().maxTextureSize;
    for (unsigned planeIndex = 0; planeIndex < m_frame->planes(); ++planeIndex) {
        CCVideoLayerImpl::FramePlane& plane = m_framePlanes[planeIndex];

        IntSize requiredTextureSize(m_frame->stride(planeIndex), videoFrameDimension(m_frame->height(), planeIndex, m_frame->format()));
        // FIXME: Remove the test against maxTextureSize when tiled layers are implemented.
        if (requiredTextureSize.isZero() || requiredTextureSize.width() > maxTextureSize || requiredTextureSize.height() > maxTextureSize)
            return false;

        if (plane.size != requiredTextureSize || plane.format != m_format) {
            plane.freeData(context);
            plane.size = requiredTextureSize;
            plane.format = m_format;
        }

        if (!plane.textureId) {
            if (!plane.allocateData(context))
                return false;
            plane.visibleSize = computeVisibleSize(*m_frame, planeIndex);
        }
    }
    return true;
}

bool CCVideoLayerImpl::copyPlaneData(CCRenderer* layerRenderer, CCGraphicsContext* context)
{
    size_t softwarePlaneCount = m_frame->planes();
    if (!softwarePlaneCount)
        return true;

    WebKit::WebGraphicsContext3D* context3d = context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return false;
    }

    LayerTextureSubImage uploader(true);
    for (size_t softwarePlaneIndex = 0; softwarePlaneIndex < softwarePlaneCount; ++softwarePlaneIndex) {
        CCVideoLayerImpl::FramePlane& plane = m_framePlanes[softwarePlaneIndex];
        const uint8_t* softwarePlanePixels = static_cast<const uint8_t*>(m_frame->data(softwarePlaneIndex));
        IntRect planeRect(IntPoint(), plane.size);

        context3d->bindTexture(GraphicsContext3D::TEXTURE_2D, plane.textureId);
        uploader.setSubImageSize(plane.size);
        uploader.upload(softwarePlanePixels, planeRect, planeRect, planeRect, plane.format, context);
    }
    return true;
}

void CCVideoLayerImpl::freePlaneData(CCGraphicsContext* context)
{
    for (unsigned i = 0; i < WebKit::WebVideoFrame::maxPlanes; ++i)
        m_framePlanes[i].freeData(context);
}

void CCVideoLayerImpl::freeUnusedPlaneData(CCGraphicsContext* context)
{
    unsigned firstUnusedPlane = m_frame ? m_frame->planes() : 0;
    for (unsigned i = firstUnusedPlane; i < WebKit::WebVideoFrame::maxPlanes; ++i)
        m_framePlanes[i].freeData(context);
}

void CCVideoLayerImpl::didReceiveFrame()
{
    setNeedsRedraw();
}

void CCVideoLayerImpl::didUpdateMatrix(const float matrix[16])
{
    m_streamTextureMatrix = WebKit::WebTransformationMatrix(
        matrix[0], matrix[1], matrix[2], matrix[3],
        matrix[4], matrix[5], matrix[6], matrix[7],
        matrix[8], matrix[9], matrix[10], matrix[11],
        matrix[12], matrix[13], matrix[14], matrix[15]);
    setNeedsRedraw();
}

void CCVideoLayerImpl::didLoseContext()
{
    freePlaneData(layerTreeHostImpl()->context());
}

void CCVideoLayerImpl::setNeedsRedraw()
{
    layerTreeHostImpl()->setNeedsRedraw();
}

void CCVideoLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "video layer\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
