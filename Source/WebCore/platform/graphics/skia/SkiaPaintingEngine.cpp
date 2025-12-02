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
#include "GraphicsLayerCoordinated.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
#include "RenderingMode.h"
#include "SkiaRecordingResult.h"
#include "SkiaReplayCanvas.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPictureRecorder.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
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
        m_texturePool = makeUnique<BitmapTexturePool>();

        if (auto numberOfGPUThreads = numberOfGPUPaintingThreads())
            m_workerPool = WorkerPool::create("SkiaGPUWorker"_s, numberOfGPUThreads);

        return;
    }

    if (auto numberOfCPUThreads = numberOfCPUPaintingThreads())
        m_workerPool = WorkerPool::create("SkiaCPUWorker"_s, numberOfCPUThreads);
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

        ASSERT(m_texturePool);
        return CoordinatedAcceleratedTileBuffer::create(m_texturePool->acquireTexture(size, textureFlags));
    }

    return CoordinatedUnacceleratedTileBuffer::create(size, contentsOpaque ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha);
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
    ASSERT(m_workerPool);

    auto renderingMode = canPerformAcceleratedRendering() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;

    WTFBeginSignpost(this, RecordTile);
    SkPictureRecorder pictureRecorder;
    auto* recordingCanvas = pictureRecorder.beginRecording(recordRect.width(), recordRect.height());
    GraphicsContextSkia recordingContext(*recordingCanvas, renderingMode, RenderingPurpose::LayerBacking);
    recordingContext.beginRecording();
    paintIntoGraphicsContext(layer, recordingContext, recordRect, contentsOpaque, contentsScale);
    auto imageToFenceMap = recordingContext.endRecording();
    auto picture = pictureRecorder.finishRecordingAsPicture();
    WTFEndSignpost(this, RecordTile);

    return SkiaRecordingResult::create(WTFMove(picture), WTFMove(imageToFenceMap), recordRect, renderingMode, contentsOpaque, contentsScale);
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

    m_workerPool->postTask([platformLayer = WTFMove(platformLayer), buffer = Ref { buffer }, dirtyRect, recording = RefPtr { recording }]() mutable {
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
            if (recording->hasFences()) {
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

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
