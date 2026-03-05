/*
 * Copyright (C) 2024, 2025 Igalia S.L.
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
#include "GraphicsContextSkia.h"
#include "IntRect.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "PixelBufferConversion.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
#include "SkiaRecordingResult.h"
#include "SkiaReplayCanvas.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPixmap.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/utils/SkNWayCanvas.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/TZoneMallocInlines.h>

#if USE(COORDINATED_GRAPHICS)
#include "BitmapTexture.h"
#include "CoordinatedPlatformLayerBufferNativeImage.h"
#include "CoordinatedPlatformLayerBufferRGB.h"
#include "GraphicsLayerContentsDisplayDelegateCoordinated.h"
#include "TextureMapperFlags.h"
#endif

namespace WebCore {

// A canvas proxy that delegates all drawing operations to a single target canvas,
// which can be dynamically switched. This allows GraphicsContextSkia to hold a
// reference to this canvas while the actual target (recording vs surface) changes.
class SkiaSwitchableCanvas final : public SkNWayCanvas {
WTF_MAKE_TZONE_ALLOCATED(SkiaSwitchableCanvas);
public:
    explicit SkiaSwitchableCanvas(const IntSize& size)
        : SkNWayCanvas(size.width(), size.height())
    {
    }

    void switchToCanvas(SkCanvas* canvas)
    {
        SkNWayCanvas::removeAll();
        if (canvas)
            SkNWayCanvas::addCanvas(canvas);
    }
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageBufferSkiaAcceleratedBackend);

static inline bool shouldEnableDynamicMSAA()
{
    static std::once_flag onceFlag;
    static bool enableDynamicMSAA = false;
    std::call_once(onceFlag, [] {
        if (const char* enableDynamicMSAAEnv = getenv("WEBKIT_SKIA_ENABLE_DYNAMIC_MSAA")) {
            enableDynamicMSAA = *enableDynamicMSAAEnv != '0';
            return;
        }

#if PLATFORM(GTK)
        enableDynamicMSAA = true;
#else
        enableDynamicMSAA = false;
#endif
    });
    return enableDynamicMSAA;
}

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
    auto msaaSampleCount = PlatformDisplay::sharedDisplay().msaaSampleCount();
    uint32_t flags = 0;
    if (parameters.purpose == RenderingPurpose::Canvas && msaaSampleCount && shouldEnableDynamicMSAA()) {
        flags |= SkSurfaceProps::kDynamicMSAA_Flag;
        msaaSampleCount = 1;
    }
    SkSurfaceProps properties = FontRenderOptions::singleton().createSurfaceProps(flags);
    auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, msaaSampleCount, kTopLeft_GrSurfaceOrigin, &properties);
    if (!surface || !surface->getCanvas())
        return nullptr;

    return create(parameters, creationContext, WTF::move(surface));
}

std::unique_ptr<ImageBufferSkiaAcceleratedBackend> ImageBufferSkiaAcceleratedBackend::create(const Parameters& parameters, const ImageBufferCreationContext&, sk_sp<SkSurface>&& surface)
{
    ASSERT(surface);
    ASSERT(surface->getCanvas());
    return std::unique_ptr<ImageBufferSkiaAcceleratedBackend>(new ImageBufferSkiaAcceleratedBackend(parameters, WTF::move(surface)));
}

ImageBufferSkiaAcceleratedBackend::ImageBufferSkiaAcceleratedBackend(const Parameters& parameters, sk_sp<SkSurface>&& surface)
    : ImageBufferSkiaSurfaceBackend(parameters, WTF::move(surface), RenderingMode::Accelerated)
{
#if USE(COORDINATED_GRAPHICS)
    // Use a content layer for canvas.
    if (parameters.purpose == RenderingPurpose::Canvas)
        m_layerContentsDisplayDelegate = GraphicsLayerContentsDisplayDelegateCoordinated::create();
#endif
}

ImageBufferSkiaAcceleratedBackend::~ImageBufferSkiaAcceleratedBackend()
{
    // Unwind the surface context's save/restore stack before destruction
    if (m_canvasRecordingContext)
        m_canvasRecordingContext->unwindStateStack();
}

GraphicsContext& ImageBufferSkiaAcceleratedBackend::context()
{
    if (parameters().purpose != RenderingPurpose::Canvas)
        return ImageBufferSkiaSurfaceBackend::context();

    ensureCanvasRecordingContext();
    return *m_canvasRecordingContext;
}

void ImageBufferSkiaAcceleratedBackend::ensureCanvasRecordingContext()
{
    if (m_canvasRecordingContext)
        return;

    // Create a switchable canvas that will delegate to either recording or surface canvas.
    // GraphicsContextSkia holds a reference to this canvas, which never changes - only the
    // target canvas it delegates to changes.
    m_switchableCanvas = makeUnique<SkiaSwitchableCanvas>(size());

    auto* recordingCanvas = m_pictureRecorder.beginRecording(size().width(), size().height());
    m_switchableCanvas->switchToCanvas(recordingCanvas);

    // Dont' use Canvas purpose: SkPictureRecorder is CPU-side, doesn't need GL context.
    m_canvasRecordingContext = makeUnique<GraphicsContextSkia>(static_cast<SkCanvas&>(*m_switchableCanvas), RenderingMode::Accelerated, RenderingPurpose::LayerBacking);
    m_canvasRecordingContext->applyDeviceScaleFactor(resolutionScale());
    m_canvasRecordingContext->enableStateReplayTracking();
    m_canvasRecordingContext->beginRecording();
    m_hasActiveRecording = true;
}

std::unique_ptr<GLFence> ImageBufferSkiaAcceleratedBackend::flushCanvasRecordingContextIfNeeded()
{
    // Only flush if we have an active recording (not already flushed).
    if (!m_canvasRecordingContext || !m_hasActiveRecording)
        return nullptr;

    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return nullptr;

    IntRect recordRect(IntPoint(), size());
    auto recordingData = m_canvasRecordingContext->endRecording();
    auto picture = m_pictureRecorder.finishRecordingAsPicture();

    RefPtr<SkiaRecordingResult> recording = SkiaRecordingResult::create(WTF::move(picture), WTF::move(recordingData), recordRect, RenderingMode::Accelerated, false, 1.0);

    // Save the surface canvas save count before playback so we can undo any
    // unbalanced saves that the picture introduces.
    auto* surfaceCanvas = m_surface->getCanvas();
    auto surfaceSaveCount = surfaceCanvas->getSaveCount();

    if (recording->hasFences()) {
        auto replayCanvas = SkiaReplayCanvas::create(size(), recording);
        replayCanvas->addCanvas(surfaceCanvas);
        replayCanvas->picture()->playback(&replayCanvas.get());
        replayCanvas->removeCanvas(surfaceCanvas);
    } else
        recording->picture()->playback(surfaceCanvas);

    // Undo unbalanced saves from the picture playback on the surface canvas.
    surfaceCanvas->restoreToCount(surfaceSaveCount);

    // Switch the switchable canvas to target the surface canvas, then replay
    // state-replay state to bring the surface into the correct save/clip/CTM nesting.
    m_switchableCanvas->switchToCanvas(surfaceCanvas);
    m_canvasRecordingContext->replayStateOnCanvas(*surfaceCanvas);

    m_hasActiveRecording = false;

    auto* recordingContext = m_surface->recordingContext();
    auto* grContext = recordingContext ? recordingContext->asDirectContext() : nullptr;

    auto& glDisplay = PlatformDisplay::sharedDisplay().glDisplay();
    if (GLFence::isSupported(glDisplay)) {
        grContext->flushAndSubmit(m_surface.get(), GrSyncCpu::kNo);
        if (auto fence = GLFence::create(glDisplay))
            return fence;
        grContext->submit(GrSyncCpu::kYes);
        return nullptr;
    }

    grContext->flushAndSubmit(m_surface.get(), GrSyncCpu::kYes);
    return nullptr;
}

void ImageBufferSkiaAcceleratedBackend::flushContext()
{
    // For canvas recording, flush the recording and wait for GPU completion.
    if (auto fence = flushCanvasRecordingContextIfNeeded()) {
        fence->serverWait();
        return;
    }

    // Normal surface flush.
    if (!m_surface)
        return;

    if (auto fence = GraphicsContextSkia::createAcceleratedRenderingFence(m_surface.get()))
        fence->serverWait();
}

void ImageBufferSkiaAcceleratedBackend::prepareForDisplay()
{
#if USE(COORDINATED_GRAPHICS)
    if (!m_layerContentsDisplayDelegate)
        return;

    // Flush and get fence for async GPU→display synchronization
    auto fence = flushCanvasRecordingContextIfNeeded();

    // If not using canvas recording (or recording already flushed), create a fence the traditional way
    if (!fence)
        fence = GLFence::create(PlatformDisplay::sharedDisplay().glDisplay());

    auto image = createNativeImageReference();
    if (!image)
        return;

    m_layerContentsDisplayDelegate->setDisplayBuffer(CoordinatedPlatformLayerBufferNativeImage::create(image.releaseNonNull(), WTF::move(fence)));

    // Re-enable recording mode for subsequent drawing operations.
    // This allows batching to occur again after each prepareForDisplay() cycle.
    if (m_canvasRecordingContext) {
        // Clean up state-replayed saves on the surface canvas before switching away.
        m_surface->getCanvas()->restoreToCount(1);

        auto* recordingCanvas = m_pictureRecorder.beginRecording(size().width(), size().height());
        m_switchableCanvas->switchToCanvas(recordingCanvas);

        // Replay state onto the new recording canvas to give it the exact same save/clip/CTM nesting.
        m_canvasRecordingContext->replayStateOnCanvas(*recordingCanvas);
        m_canvasRecordingContext->beginRecording();
        m_hasActiveRecording = true;
    }
#endif
}

RefPtr<NativeImage> ImageBufferSkiaAcceleratedBackend::copyNativeImage()
{
    // SkSurface uses a copy-on-write mechanism for makeImageSnapshot(), so it's
    // always safe to return the SkImage without copying.
    return createNativeImageReference();
}

RefPtr<NativeImage> ImageBufferSkiaAcceleratedBackend::createNativeImageReference()
{
    flushCanvasRecordingContextIfNeeded();

    auto* recordingContext = m_surface->recordingContext();
    auto* grContext = recordingContext ? recordingContext->asDirectContext() : nullptr;

    // If we're using MSAA, we need to flush the surface before calling makeImageSnapshot(),
    // because that call doesn't force the MSAA resolution, which can produce outdated results
    // in the resulting SkImage.
    auto& display = PlatformDisplay::sharedDisplay();
    if (grContext && display.msaaSampleCount() > 0 && display.skiaGLContext()->makeContextCurrent())
        grContext->flush(m_surface.get());

    return NativeImage::create(m_surface->makeImageSnapshot(), grContext);
}

void ImageBufferSkiaAcceleratedBackend::getPixelBuffer(const IntRect& srcRect, PixelBuffer& destination)
{
    if (!PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent())
        return;

    // CPU needs to read pixels now, wait for GPU completion.
    if (auto fence = flushCanvasRecordingContextIfNeeded())
        fence->serverWait();

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
    if (!pixmap.extractSubset(&dstPixmap, destinationRect)) [[unlikely]]
        return;

    m_surface->readPixels(dstPixmap, sourceRectClipped.x(), sourceRectClipped.y());
}

static std::span<uint8_t> mutableSpan(SkData* data)
{
    return unsafeMakeSpan(static_cast<uint8_t*>(data->writable_data()), data->size());
}

void ImageBufferSkiaAcceleratedBackend::putPixelBuffer(const PixelBufferSourceView& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    // CPU needs to write pixels now, wait for GPU completion.
    if (auto fence = flushCanvasRecordingContextIfNeeded())
        fence->serverWait();

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
    if (!pixmap.extractSubset(&srcPixmap, sourceRectClipped)) [[unlikely]]
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
    ImageBufferBackend::putPixelBuffer(pixelBuffer, sourceRectClipped, IntPoint::zero(), destFormat, mutableSpan(data.get()));
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
