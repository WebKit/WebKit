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
#include "SkiaPaintingEngine.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "BitmapTexturePool.h"
#include "CoordinatedBackingStoreProxy.h"
#include "CoordinatedPlatformLayer.h"
#include "CoordinatedTileBuffer.h"
#include "GLContext.h"
#include "GraphicsContextSkia.h"
#include "GraphicsLayerCoordinated.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
#include "RenderingMode.h"
#include "SkiaGPUAtlas.h"
#include "SkiaImageAtlasLayout.h"
#include "SkiaRecordingResult.h"
#include "SkiaReplayCanvas.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkPictureRecorder.h>
#include <skia/core/SkSurface.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/GrDirectContext.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/NumberOfCores.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaPaintingEngine);

// Note:
// If WEBKIT_SKIA_ENABLE_CPU_RENDERING is unset, we will allocate a GPU-only worker pool with WEBKIT_SKIA_GPU_PAINTING_THREADS threads.
// If WEBKIT_SKIA_ENABLE_CPU_RENDERING is unset, and WEBKIT_SKIA_GPU_PAINTING_THREADS is set to 0, we will use GPU rendering on main thread.
//
// If WEBKIT_SKIA_ENABLE_CPU_RENDERING=1 is set, we will allocate a CPU-only worker pool with WEBKIT_SKIA_CPU_PAINTING_THREADS threads.
// if WEBKIT_SKIA_ENABLE_CPU_RENDERING=1 is set, and WEBKIT_SKIA_CPU_PAINTING_THREADS is set to 0, we will use CPU rendering on main thread.

static bool canPerformAcceleratedRendering()
{
    return ProcessCapabilities::canUseAcceleratedBuffers() && PlatformDisplay::sharedDisplay().skiaGLContext();
}

SkiaPaintingEngine::SkiaPaintingEngine()
{
    if (canPerformAcceleratedRendering()) {
        if (auto numberOfGPUThreads = numberOfGPUPaintingThreads())
            m_paintingWorkerPool = WorkerPool::create("SkiaGPUWorker"_s, numberOfGPUThreads);

        return;
    }

    if (auto numberOfCPUThreads = numberOfCPUPaintingThreads())
        m_paintingWorkerPool = WorkerPool::create("SkiaCPUWorker"_s, numberOfCPUThreads);
}

SkiaPaintingEngine::~SkiaPaintingEngine() = default;

std::unique_ptr<SkiaPaintingEngine> SkiaPaintingEngine::create()
{
    return makeUnique<SkiaPaintingEngine>();
}

void SkiaPaintingEngine::paintIntoGraphicsContext(const GraphicsLayer& layer, GraphicsContext& context, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale) const
{
    IntRect initialClip(IntPoint::zero(), dirtyRect.size());
    context.clip(initialClip);

    if (!contentsOpaque) {
        context.setCompositeOperation(CompositeOperator::Copy);
        context.fillRect(initialClip, Color::transparentBlack);
        context.setCompositeOperation(CompositeOperator::SourceOver);
    }

    FloatRect clipRect(dirtyRect);
    clipRect.scale(1 / contentsScale);

    context.translate(-dirtyRect.x(), -dirtyRect.y());
    context.scale(contentsScale);
    layer.paintGraphicsLayerContents(context, clipRect);
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::createBuffer(RenderingMode renderingMode, const IntSize& size, bool contentsOpaque) const
{
    if (renderingMode == RenderingMode::Accelerated) {
        PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent();

        OptionSet<BitmapTexture::Flags> textureFlags;
        if (!contentsOpaque)
            textureFlags.add(BitmapTexture::Flags::SupportsAlpha);

        return CoordinatedAcceleratedTileBuffer::create(BitmapTexturePool::singleton().acquireTexture(size, textureFlags));
    }

    return CoordinatedUnacceleratedTileBuffer::create(size, contentsOpaque ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha);
}

RefPtr<SkiaGPUAtlas> SkiaPaintingEngine::createAtlas(const SkiaImageAtlasLayout& layout, AtlasUploadCondition& uploadCondition)
{
    const auto& atlasSize = layout.atlasSize();

    OptionSet<BitmapTexture::Flags> textureFlags { BitmapTexture::Flags::UseBGRALayout, BitmapTexture::Flags::NearestFiltering, BitmapTexture::Flags::SupportsAlpha };
    bool isDMABufBackedTexture = false;
#if USE(GBM)
    if (shouldUseDMABufAtlasTextures()) {
        isDMABufBackedTexture = true;
        textureFlags.add({ BitmapTexture::Flags::BackedByDMABuf, BitmapTexture::Flags::ForceLinearBuffer });
    }
#endif

    // Verify the texture actually has DMA-buf backing. BitmapTexture silently
    // falls back to GL if DMA-buf allocation fails, but we must not dispatch
    // GL operations to the upload worker thread (which has no GL context).
    auto texture = BitmapTexturePool::singleton().acquireTexture(atlasSize, textureFlags);
    if (!texture->memoryMappedGPUBuffer())
        isDMABufBackedTexture = false;

    auto atlas = SkiaGPUAtlas::create(layout, WTF::move(texture));
    if (!atlas)
        return nullptr;

    // GL path: upload synchronously.
    if (!isDMABufBackedTexture) [[unlikely]] {
        atlas->uploadImages();
        return atlas;
    }

    // DMA-buf path: create atlas without uploading, dispatch pixel writes to worker.
    if (!m_uploadWorkQueue)
        m_uploadWorkQueue = WorkQueue::create("AtlasUpload"_s);
    uploadCondition.addPending();
    m_uploadWorkQueue->dispatch([atlas = Ref { *atlas }, condition = Ref { uploadCondition }]() mutable {
        atlas->uploadImages();
        condition->signal();
    });
    return atlas;
}

bool SkiaPaintingEngine::tryReuseCachedAtlases(SkiaRecordingResult& result, unsigned fingerprint)
{
    if (m_cachedGPUAtlases.isEmpty() || fingerprint != m_cachedImageFingerprint)
        return false;

    // Cache hit — reuse GPU atlases.
    result.setGPUAtlases(copyToVectorOf<Ref<SkiaGPUAtlas>>(m_cachedGPUAtlases));
    return true;
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::paint(const GraphicsLayerCoordinated& layer, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale)
{
    // ### Synchronous rendering on main thread ###
    ASSERT(!useThreadedRendering());

    Ref platformLayer = layer.coordinatedPlatformLayer();
    platformLayer->willPaintTile();

    auto renderingMode = canPerformAcceleratedRendering() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;
    auto buffer = createBuffer(renderingMode, dirtyRect.size(), contentsOpaque);
    buffer->beginPainting();

    if (auto* canvas = buffer->canvas()) {
        WTFBeginSignpost(canvas, PaintTile, "Skia/%s, dirty region %ix%i+%i+%i", buffer->isBackedByOpenGL() ? "GPU" : "CPU", dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height());
        canvas->save();
        canvas->clear(SkColors::kTransparent);

        GraphicsContextSkia context(*canvas, renderingMode, RenderingPurpose::LayerBacking);
        paintIntoGraphicsContext(layer, context, dirtyRect, contentsOpaque, contentsScale);

        canvas->restore();
        WTFEndSignpost(canvas, PaintTile);
    }

    buffer->completePainting();
    platformLayer->didPaintTile();

    return buffer;
}

Ref<SkiaRecordingResult> SkiaPaintingEngine::record(const GraphicsLayerCoordinated& layer, const IntRect& recordRect, bool contentsOpaque, float contentsScale)
{
    // ### Asynchronous rendering on worker threads ###
    ASSERT(useThreadedRendering());
    ASSERT(m_paintingWorkerPool);

    auto renderingMode = canPerformAcceleratedRendering() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;

    WTFBeginSignpost(this, RecordTile);
    SkPictureRecorder pictureRecorder;
    auto* recordingCanvas = pictureRecorder.beginRecording(recordRect.width(), recordRect.height());
    GraphicsContextSkia recordingContext(*recordingCanvas, renderingMode, RenderingPurpose::LayerBacking);
    recordingContext.beginRecording();
    paintIntoGraphicsContext(layer, recordingContext, recordRect, contentsOpaque, contentsScale);
    auto recordingData = recordingContext.endRecording();

    auto picture = pictureRecorder.finishRecordingAsPicture();
    WTFEndSignpost(this, RecordTile);

    auto result = SkiaRecordingResult::create(WTF::move(picture), WTF::move(recordingData), recordRect, renderingMode, contentsOpaque, contentsScale);

    // Prepare GPU atlases on main thread before dispatching to workers.
    if (result->hasAtlasLayouts()) {
        auto fingerprint = result->imageSetFingerprint();

        // Fast path: reuse cached atlases if the image set is unchanged.
        if (!tryReuseCachedAtlases(result.get(), fingerprint)) {
            PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent();

            auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
            RELEASE_ASSERT(grContext);

            Vector<Ref<SkiaGPUAtlas>> gpuAtlases;
            auto uploadCondition = AtlasUploadCondition::create();
            gpuAtlases.reserveInitialCapacity(result->atlasLayouts().size());

            for (const auto& layout : result->atlasLayouts()) {
                if (auto atlas = createAtlas(layout.get(), uploadCondition.get()))
                    gpuAtlases.append(atlas.releaseNonNull());
            }

            if (!gpuAtlases.isEmpty()) {
                // Only populate atlas cache when we see the same fingerprint twice in a row.
                // This avoids holding the BitmapTextures by an extra frame, if not needed.
                if (fingerprint == m_cachedImageFingerprint) {
                    m_cachedGPUAtlases = WTF::move(gpuAtlases);
                    result->setGPUAtlases(copyToVectorOf<Ref<SkiaGPUAtlas>>(m_cachedGPUAtlases), WTF::move(uploadCondition));
                } else {
                    m_cachedImageFingerprint = fingerprint;
                    m_cachedGPUAtlases.clear();
                    result->setGPUAtlases(WTF::move(gpuAtlases), WTF::move(uploadCondition));
                }

                // Flush and fence for the GL upload path, where
                // BitmapTexture::updateContents() issues GL upload commands.
                // On the DMA-buf path, uploading is CPU-side (memory-mapped),
                // so this is a no-op flush but harmless.
                auto& glDisplay = PlatformDisplay::sharedDisplay().glDisplay();
                if (GLFence::isSupported(glDisplay)) {
                    grContext->flushAndSubmit(GrSyncCpu::kNo);
                    result->setUploadFence(GLFence::create(glDisplay));
                } else
                    grContext->flushAndSubmit(GrSyncCpu::kYes);
            }
        }
    } else {
        // No atlas layouts — clear cache to release GPU memory.
        m_cachedImageFingerprint = 0;
        m_cachedGPUAtlases.clear();
    }

    return result;
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::replay(const GraphicsLayerCoordinated& layer, const RefPtr<SkiaRecordingResult>& recording, const IntRect& dirtyRect)
{
    // ### Asynchronous rendering on worker threads ###
    ASSERT(useThreadedRendering());

    Ref platformLayer = layer.coordinatedPlatformLayer();
    platformLayer->willPaintTile();

    auto renderingMode = recording->renderingMode();
    auto buffer = createBuffer(renderingMode, dirtyRect.size(), recording->contentsOpaque());
    buffer->beginPainting();

    m_paintingWorkerPool->postTask([platformLayer = WTF::move(platformLayer), buffer = Ref { buffer }, dirtyRect, recording = RefPtr { recording }]() mutable {
        if (auto* canvas = buffer->canvas()) {
            auto replayPicture = [](const sk_sp<SkPicture>& picture, SkCanvas* canvas, const IntRect& recordRect, const IntRect& paintRect) {
                canvas->save();
                canvas->clear(SkColors::kTransparent);
                canvas->clipRect(SkRect::MakeXYWH(0, 0, paintRect.width(), paintRect.height()));
                canvas->translate(recordRect.x() - paintRect.x(), recordRect.y() - paintRect.y());
                picture->playback(canvas);
                canvas->restore();
            };

            WTFBeginSignpost(canvas, PaintTile, "Skia/%s threaded, dirty region %ix%i+%i+%i", buffer->isBackedByOpenGL() ? "GPU" : "CPU", dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height());
            // Use SkiaReplayCanvas if there are GPU fences or GPU atlases to handle.
            if (recording->hasFences() || recording->hasGPUAtlases()) {
                auto replayCanvas = SkiaReplayCanvas::create(dirtyRect.size(), recording);
                replayCanvas->addCanvas(canvas);
                replayPicture(replayCanvas->picture(), &replayCanvas.get(), recording->recordRect(), dirtyRect);
                replayCanvas->removeCanvas(canvas);
            } else
                replayPicture(recording->picture(), canvas, recording->recordRect(), dirtyRect);
            WTFEndSignpost(canvas, PaintTile);
        }

        buffer->completePainting();
        platformLayer->didPaintTile();
    });

    return buffer;
}

unsigned SkiaPaintingEngine::numberOfCPUPaintingThreads()
{
    static std::once_flag onceFlag;
    static unsigned numberOfThreads = 0;

    std::call_once(onceFlag, [] {
        numberOfThreads = std::max(1, std::min(8, WTF::numberOfProcessorCores() / 2)); // By default, use half the CPU cores, capped at 8.

        if (const char* envString = getenv("WEBKIT_SKIA_CPU_PAINTING_THREADS")) {
            auto newValue = parseInteger<unsigned>(StringView::fromLatin1(envString));
            if (newValue && *newValue <= 8)
                numberOfThreads = *newValue;
            else
                WTFLogAlways("The number of Skia painting threads is not between 0 and 8. Using the default value %u\n", numberOfThreads);
        }
    });

    return numberOfThreads;
}

unsigned SkiaPaintingEngine::numberOfGPUPaintingThreads()
{
    static std::once_flag onceFlag;
    static unsigned numberOfThreads = 0;

    std::call_once(onceFlag, [] {
        // By default, use 2 GPU worker threads if there are four or more CPU cores, otherwise use 1 thread only.
        numberOfThreads = WTF::numberOfProcessorCores() >= 4 ? 2 : 1;

        if (const char* envString = getenv("WEBKIT_SKIA_GPU_PAINTING_THREADS")) {
            auto newValue = parseInteger<unsigned>(StringView::fromLatin1(envString));
            if (newValue && *newValue <= 4)
                numberOfThreads = *newValue;
            else
                WTFLogAlways("The number of Skia/GPU painting threads is not between 0 and 4. Using the default value %u\n", numberOfThreads);
        }
    });

    return numberOfThreads;
}

bool SkiaPaintingEngine::shouldUseDMABufAtlasTextures()
{
#if USE(GBM)
    static std::once_flag onceFlag;
    static bool shouldUseDMABufAtlas = true;

    std::call_once(onceFlag, [] {
        if (const char* envString = getenv("WEBKIT_DISABLE_DMABUF_ATLAS")) {
            auto envStringView = StringView::fromLatin1(envString);
            if (envStringView == "1"_s)
                shouldUseDMABufAtlas = false;
        }
    });

    return shouldUseDMABufAtlas;
#else
    return false;
#endif
}

bool SkiaPaintingEngine::shouldUseLinearTileTextures()
{
    static std::once_flag onceFlag;
    static bool shouldUseLinearTextures = false;

    std::call_once(onceFlag, [] {
        if (const char* envString = getenv("WEBKIT_SKIA_USE_LINEAR_TILE_TEXTURES")) {
            auto envStringView = StringView::fromLatin1(envString);
            if (envStringView == "1"_s)
                shouldUseLinearTextures = true;
        }
    });

    return shouldUseLinearTextures;
}

bool SkiaPaintingEngine::shouldUseVivanteSuperTiledTileTextures()
{
    static std::once_flag onceFlag;
    static bool shouldUseVivanteSuperTiledTextures = false;

    std::call_once(onceFlag, [] {
        if (const char* envString = getenv("WEBKIT_SKIA_USE_VIVANTE_SUPER_TILED_TILE_TEXTURES")) {
            auto envStringView = StringView::fromLatin1(envString);
            if (envStringView == "1"_s)
                shouldUseVivanteSuperTiledTextures = true;
        }
    });

    return shouldUseVivanteSuperTiledTextures;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
