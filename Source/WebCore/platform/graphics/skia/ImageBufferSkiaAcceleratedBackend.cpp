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
#include "GLFence.h"
#include "IntRect.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkBitmap.h>
#include <skia/core/SkPixmap.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/TZoneMallocInlines.h>

#if USE(COORDINATED_GRAPHICS)
#include "BitmapTexture.h"
#include "CoordinatedPlatformLayerBufferNativeImage.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "GraphicsLayerContentsDisplayDelegateTextureMapper.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayerProxy.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ImageBufferSkiaAcceleratedBackend);

std::unique_ptr<ImageBufferSkiaAcceleratedBackend> ImageBufferSkiaAcceleratedBackend::create(const Parameters& parameters, const ImageBufferCreationContext& creationContext)
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
    SkSurfaceProps properties { 0, FontRenderOptions::singleton().subpixelOrder() };
    auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, PlatformDisplay::sharedDisplay().msaaSampleCount(), kTopLeft_GrSurfaceOrigin, &properties);
    if (!surface || !surface->getCanvas())
        return nullptr;

    return create(parameters, creationContext, WTFMove(surface));
}

std::unique_ptr<ImageBufferSkiaAcceleratedBackend> ImageBufferSkiaAcceleratedBackend::create(const Parameters& parameters, const ImageBufferCreationContext&, sk_sp<SkSurface>&& surface)
{
    ASSERT(surface);
    ASSERT(surface->getCanvas());
    return std::unique_ptr<ImageBufferSkiaAcceleratedBackend>(new ImageBufferSkiaAcceleratedBackend(parameters, WTFMove(surface)));
}

ImageBufferSkiaAcceleratedBackend::ImageBufferSkiaAcceleratedBackend(const Parameters& parameters, sk_sp<SkSurface>&& surface)
    : ImageBufferSkiaSurfaceBackend(parameters, WTFMove(surface), RenderingMode::Accelerated)
    , m_skiaGrContext(PlatformDisplay::sharedDisplay().skiaGrContext())
{
    ASSERT(m_skiaGrContext);

#if USE(COORDINATED_GRAPHICS)
    // Use a content layer for canvas.
    if (parameters.purpose == RenderingPurpose::Canvas) {
        auto proxy = TextureMapperPlatformLayerProxy::create(TextureMapperPlatformLayerProxy::ContentType::Canvas);
        proxy->setSwapBuffersFunction([this](TextureMapperPlatformLayerProxy& proxy) {
            auto image = createNativeImageReference();
            if (!image)
                return;

            proxy.pushNextBuffer(CoordinatedPlatformLayerBufferNativeImage::create(image.releaseNonNull(), GLFence::create()));
        });
        m_layerContentsDisplayDelegate = GraphicsLayerContentsDisplayDelegateTextureMapper::create(WTFMove(proxy));
    }
#endif
}

ImageBufferSkiaAcceleratedBackend::~ImageBufferSkiaAcceleratedBackend()
{
#if USE(COORDINATED_GRAPHICS)
    if (m_layerContentsDisplayDelegate)
        static_cast<GraphicsLayerContentsDisplayDelegateTextureMapper*>(m_layerContentsDisplayDelegate.get())->proxy().setSwapBuffersFunction(nullptr);
#endif
}

void ImageBufferSkiaAcceleratedBackend::finishAcceleratedRenderingAndCreateFence()
{
    Locker locker { m_fenceLock };
    if (m_fence)
        return;

    auto* glContext = PlatformDisplay::sharedDisplay().skiaGLContext();
    if (!glContext || !glContext->makeContextCurrent())
        return;

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    RELEASE_ASSERT(grContext);

    if (GLFence::isSupported()) {
        grContext->flushAndSubmit(m_surface.get(), GrSyncCpu::kNo);
        m_fence = GLFence::create();
        if (!m_fence)
            grContext->submit(GrSyncCpu::kYes);
    } else
        grContext->flushAndSubmit(m_surface.get(), GrSyncCpu::kYes);
}

void ImageBufferSkiaAcceleratedBackend::waitForAcceleratedRenderingFenceCompletion()
{
    Locker locker { m_fenceLock };
    if (!m_fence)
        return;

    m_fence->serverWait();
    m_fence = nullptr;
}

RefPtr<ImageBuffer> ImageBufferSkiaAcceleratedBackend::copyAcceleratedImageBufferBorrowingBackendRenderTarget(const ImageBuffer& imageBuffer) const
{
    auto* glContext = PlatformDisplay::sharedDisplay().skiaGLContext();
    if (!glContext || !glContext->makeContextCurrent())
        return nullptr;

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    RELEASE_ASSERT(grContext);

    auto backendRenderTarget = SkSurfaces::GetBackendRenderTarget(m_surface.get(), SkSurfaces::BackendHandleAccess::kFlushRead);

    const auto& imageInfo = m_surface->imageInfo();
    auto surface = SkSurfaces::WrapBackendRenderTarget(grContext, backendRenderTarget, kTopLeft_GrSurfaceOrigin, imageInfo.colorType(), imageInfo.refColorSpace(), &m_surface->props());
    if (!surface || !surface->getCanvas())
        return nullptr;

    auto backend = ImageBufferSkiaAcceleratedBackend::create(parameters(), { }, WTFMove(surface));
    return ImageBuffer::create<ImageBuffer>(imageBuffer.parameters(), imageBuffer.backendInfo(), { }, WTFMove(backend));
}

RefPtr<NativeImage> ImageBufferSkiaAcceleratedBackend::copyNativeImage()
{
    // SkSurface uses a copy-on-write mechanism for makeImageSnapshot(), so it's
    // always safe to return the SkImage without copying.
    return createNativeImageReference();
}

RefPtr<NativeImage> ImageBufferSkiaAcceleratedBackend::createNativeImageReference()
{
    // If we're using MSAA, we need to flush the surface before calling makeImageSnapshot(),
    // because that call doesn't force the MSAA resolution, which can produce outdated results
    // in the resulting SkImage.
    auto& display = PlatformDisplay::sharedDisplay();
    if (display.msaaSampleCount() > 0) {
        if (display.skiaGLContext()->makeContextCurrent())
            display.skiaGrContext()->flush(m_surface.get());
    }
    return NativeImage::create(m_surface->makeImageSnapshot());
}

void ImageBufferSkiaAcceleratedBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return;

    const IntRect backendRect { { }, size() };
    const auto sourceRectClipped = intersection(backendRect, srcRect);
    IntRect destinationRect { IntPoint::zero(), sourceRectClipped.size() };

    if (srcRect.x() < 0)
        destinationRect.setX(destinationRect.x() - srcRect.x());
    if (srcRect.y() < 0)
        destinationRect.setY(destinationRect.y() - srcRect.y());

    if (destination.size() != sourceRectClipped.size())
        destination.zeroFill();

    const auto destinationColorType = (destination.format().pixelFormat == PixelFormat::RGBA8)
        ? SkColorType::kRGBA_8888_SkColorType : SkColorType::kBGRA_8888_SkColorType;

    const auto destinationAlphaType = (destination.format().alphaFormat == AlphaPremultiplication::Premultiplied)
        ? SkAlphaType::kPremul_SkAlphaType : SkAlphaType::kUnpremul_SkAlphaType;

    auto destinationInfo = SkImageInfo::Make(destination.size().width(), destination.size().height(),
        destinationColorType, destinationAlphaType, destination.format().colorSpace.platformColorSpace());
    SkPixmap pixmap(destinationInfo, destination.bytes().data(), destination.size().width() * 4);

    SkPixmap dstPixmap;
    if (UNLIKELY(!pixmap.extractSubset(&dstPixmap, destinationRect)))
        return;

    m_surface->readPixels(dstPixmap, sourceRectClipped.x(), sourceRectClipped.y());
}

void ImageBufferSkiaAcceleratedBackend::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    UNUSED_PARAM(destFormat);

    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return;

    ASSERT(IntRect({ 0, 0 }, pixelBuffer.size()).contains(srcRect));
    ASSERT(pixelBuffer.format().pixelFormat == PixelFormat::RGBA8 || pixelBuffer.format().pixelFormat == PixelFormat::BGRA8);
    ASSERT(pixelBuffer.format().alphaFormat == AlphaPremultiplication::Premultiplied || pixelBuffer.format().alphaFormat == AlphaPremultiplication::Unpremultiplied);

    const auto colorType = (pixelBuffer.format().pixelFormat == PixelFormat::RGBA8)
        ? SkColorType::kRGBA_8888_SkColorType : SkColorType::kBGRA_8888_SkColorType;

    const auto alphaType = (pixelBuffer.format().alphaFormat == AlphaPremultiplication::Premultiplied)
        ? SkAlphaType::kPremul_SkAlphaType : SkAlphaType::kUnpremul_SkAlphaType;

    const IntRect backendRect { { }, size() };
    auto sourceRectClipped = intersection({ IntPoint::zero(), pixelBuffer.size() }, srcRect);
    auto destinationRect = sourceRectClipped;
    destinationRect.moveBy(destPoint);

    if (srcRect.x() < 0)
        destinationRect.setX(destinationRect.x() - srcRect.x());
    if (srcRect.y() < 0)
        destinationRect.setY(destinationRect.y() - srcRect.y());

    destinationRect.intersect(backendRect);
    sourceRectClipped.setSize(destinationRect.size());

    auto pixelBufferInfo = SkImageInfo::Make(pixelBuffer.size().width(), pixelBuffer.size().height(),
        colorType, alphaType, pixelBuffer.format().colorSpace.platformColorSpace());
    SkPixmap pixmap(pixelBufferInfo, pixelBuffer.bytes().data(), pixelBuffer.size().width() * 4);

    SkPixmap srcPixmap;
    if (UNLIKELY(!pixmap.extractSubset(&srcPixmap, sourceRectClipped)))
        return;

    const auto destAlphaType = (destFormat == AlphaPremultiplication::Premultiplied)
        ? SkAlphaType::kPremul_SkAlphaType : SkAlphaType::kUnpremul_SkAlphaType;

    // If all the pixels in the source rectangle are opaque, it does not matter which kind
    // of alpha is involved: the destination pixels will be replaced by the source ones.
    if (m_surface->imageInfo().alphaType() == destAlphaType || srcPixmap.computeIsOpaque()) {
        m_surface->writePixels(srcPixmap, destinationRect.x(), destinationRect.y());
        return;
    }

    // Fall back to converting, but only the part covered by sourceRectClipped/srcPixmap.
    auto data = SkData::MakeUninitialized(srcPixmap.computeByteSize());
    ImageBufferBackend::putPixelBuffer(pixelBuffer, sourceRectClipped, IntPoint::zero(), destFormat,
        static_cast<uint8_t*>(data->writable_data()));
    auto convertedSrcInfo = SkImageInfo::Make(srcPixmap.dimensions(), SkColorType::kBGRA_8888_SkColorType,
        SkAlphaType::kPremul_SkAlphaType, colorSpace().platformColorSpace());
    SkPixmap convertedSrcPixmap(convertedSrcInfo, data->writable_data(), convertedSrcInfo.minRowBytes64());
    m_surface->writePixels(convertedSrcPixmap, destinationRect.x(), destinationRect.y());
}

#if USE(COORDINATED_GRAPHICS)
RefPtr<GraphicsLayerContentsDisplayDelegate> ImageBufferSkiaAcceleratedBackend::layerContentsDisplayDelegate() const
{
    return m_layerContentsDisplayDelegate;
}
#endif

} // namespace WebCore

#endif // USE(SKIA)
