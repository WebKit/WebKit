/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageBufferSkiaAcceleratedBackend.h"

#if USE(SKIA)
#include "FontRenderOptions.h"
#include "GLContext.h"
#include "IntRect.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
#include <skia/core/SkBitmap.h>
#include <skia/core/SkPixmap.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/gl/GrGLTypes.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(NICOSIA)
#include "BitmapTexture.h"
#include "GLFence.h"
#include "PlatformLayerDisplayDelegate.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxyGL.h"
#include <skia/gpu/GrBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ImageBufferSkiaAcceleratedBackend);

std::unique_ptr<ImageBufferSkiaAcceleratedBackend> ImageBufferSkiaAcceleratedBackend::create(const Parameters& parameters, const ImageBufferCreationContext&)
{
    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    // We always want to accelerate the canvas when Accelerated2DCanvas setting is true, even if skia CPU is enabled.
    if (parameters.purpose != RenderingPurpose::Canvas && !ProcessCapabilities::canUseAcceleratedBuffers())
        return nullptr;

    auto* glContext = PlatformDisplay::sharedDisplay().skiaGLContext();
    if (!glContext || !glContext->makeContextCurrent())
        return nullptr;

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    RELEASE_ASSERT(grContext);
    auto imageInfo = SkImageInfo::Make(backendSize.width(), backendSize.height(), kRGBA_8888_SkColorType, kPremul_SkAlphaType, parameters.colorSpace.platformColorSpace());
    SkSurfaceProps properties = { 0, FontRenderOptions::singleton().subpixelOrder() };
    auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, PlatformDisplay::sharedDisplay().msaaSampleCount(), kTopLeft_GrSurfaceOrigin, &properties);
    if (!surface || !surface->getCanvas())
        return nullptr;

    return std::unique_ptr<ImageBufferSkiaAcceleratedBackend>(new ImageBufferSkiaAcceleratedBackend(parameters, WTFMove(surface)));
}

ImageBufferSkiaAcceleratedBackend::ImageBufferSkiaAcceleratedBackend(const Parameters& parameters, sk_sp<SkSurface>&& surface)
    : ImageBufferSkiaSurfaceBackend(parameters, WTFMove(surface), RenderingMode::Accelerated)
{
#if USE(NICOSIA)
    // Use a content layer for canvas.
    if (parameters.purpose == RenderingPurpose::Canvas) {
        m_contentLayer = Nicosia::ContentLayer::create(*this, adoptRef(*new TextureMapperPlatformLayerProxyGL(TextureMapperPlatformLayerProxy::ContentType::Canvas)));
        m_layerContentsDisplayDelegate = PlatformLayerDisplayDelegate::create(m_contentLayer.get());
    }
#endif
}

ImageBufferSkiaAcceleratedBackend::~ImageBufferSkiaAcceleratedBackend()
{
#if USE(NICOSIA)
    if (m_texture.back || m_texture.front) {
        GLContext::ScopedGLContextCurrent scopedContext(*PlatformDisplay::sharedDisplay().sharingGLContext());
        m_texture.back = nullptr;
        m_texture.front = nullptr;
    }

    if (m_contentLayer)
        m_contentLayer->invalidateClient();
#endif
}

void flushSurfaceIfNeeded(SkSurface* surface)
{
    // If we're using MSAA, we need to flush the surface before calling makeImageSnapshot(),
    // because that call doesn't force the MSAA resolution, which can produce outdated results
    // in the resulting SkImage.
    auto& display = PlatformDisplay::sharedDisplay();
    if (display.msaaSampleCount() > 0) {
        auto* glContext = display.skiaGLContext();
        if (!glContext || !glContext->makeContextCurrent())
            return;
        display.skiaGrContext()->flush(surface);
    }
}

RefPtr<NativeImage> ImageBufferSkiaAcceleratedBackend::copyNativeImage()
{
    // FIXME: do we have to do a explicit copy here?
    flushSurfaceIfNeeded(m_surface.get());
    return NativeImage::create(m_surface->makeImageSnapshot());
}

RefPtr<NativeImage> ImageBufferSkiaAcceleratedBackend::createNativeImageReference()
{
    flushSurfaceIfNeeded(m_surface.get());
    return NativeImage::create(m_surface->makeImageSnapshot());
}

void ImageBufferSkiaAcceleratedBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return;

    auto info = m_surface->imageInfo();
    auto data = SkData::MakeUninitialized(info.computeMinByteSize());
    auto* pixels = static_cast<uint8_t*>(data->writable_data());
    size_t rowBytes = static_cast<size_t>(info.minRowBytes64());

    // Create a SkImageInfo so the readPixels call will convert the RGBA pixels from the surface into BGRA.
    SkImageInfo imageInfo = SkImageInfo::Make(info.width(), info.height(), SkColorType::kBGRA_8888_SkColorType, SkAlphaType::kPremul_SkAlphaType, colorSpace().platformColorSpace());
    if (m_surface->readPixels(imageInfo, pixels, rowBytes, 0, 0))
        ImageBufferBackend::getPixelBuffer(srcRect, pixels, destination);
}

void ImageBufferSkiaAcceleratedBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return;

    auto info = m_surface->imageInfo();
    auto data = SkData::MakeUninitialized(info.computeMinByteSize());
    auto* pixels = static_cast<uint8_t*>(data->writable_data());
    ImageBufferBackend::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat, pixels);

    // Create a SkImageInfo so the writePixels call will transform the pixels from BGRA to RGBA when painting.
    SkImageInfo imageInfo = SkImageInfo::Make(info.width(), info.height(), SkColorType::kBGRA_8888_SkColorType, SkAlphaType::kPremul_SkAlphaType, colorSpace().platformColorSpace());
    SkPixmap pixmap(imageInfo, pixels, info.minRowBytes64());
    m_surface->writePixels(pixmap, 0, 0);
}

#if USE(NICOSIA)
RefPtr<GraphicsLayerContentsDisplayDelegate> ImageBufferSkiaAcceleratedBackend::layerContentsDisplayDelegate() const
{
    return m_layerContentsDisplayDelegate;
}

void ImageBufferSkiaAcceleratedBackend::swapBuffersIfNeeded()
{
    auto& display = PlatformDisplay::sharedDisplay();
    if (!display.skiaGLContext()->makeContextCurrent())
        return;

    RELEASE_ASSERT(m_contentLayer);

    auto* grContext = display.skiaGrContext();
    RELEASE_ASSERT(grContext);
    grContext->flushAndSubmit(m_surface.get(), GLFence::isSupported() ? GrSyncCpu::kNo : GrSyncCpu::kYes);

    auto texture = SkSurfaces::GetBackendTexture(m_surface.get(), SkSurface::BackendHandleAccess::kFlushRead);
    ASSERT(texture.isValid());
    GrGLTextureInfo textureInfo;
    bool retrievedTextureInfo = GrBackendTextures::GetGLTextureInfo(texture, &textureInfo);
    ASSERT_UNUSED(retrievedTextureInfo, retrievedTextureInfo);
    std::unique_ptr<GLFence> fence = GLFence::create();

    // Switch to the sharing context for the texture copy.
    if (!display.sharingGLContext()->makeContextCurrent())
        return;

    auto info = m_surface->imageInfo();
    IntSize textureSize(info.width(), info.height());
    if (!m_texture.back)
        m_texture.back = BitmapTexture::create(textureSize, BitmapTexture::Flags::SupportsAlpha);
    fence->serverWait();
    m_texture.back->copyFromExternalTexture(textureInfo.fID);
    fence = GLFence::create();
    std::swap(m_texture.back, m_texture.front);

    if (!display.skiaGLContext()->makeContextCurrent())
        return;

    auto& proxy = m_contentLayer->proxy();
    Locker locker { proxy.lock() };
    auto layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(m_texture.front->id(), textureSize, TextureMapperFlags::ShouldBlend, GL_DONT_CARE);
#if PLATFORM(GTK) || PLATFORM(WPE)
    layerBuffer->setFence(WTFMove(fence));
#endif
    downcast<TextureMapperPlatformLayerProxyGL>(proxy).pushNextBuffer(WTFMove(layerBuffer));
}
#endif

} // namespace WebCore

#endif // USE(SKIA)
