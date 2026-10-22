/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CoordinatedTileManager.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayer.h"
#include "CoordinatedTileBuffer.h"
#include "GraphicsLayerCoordinated.h"
#include "IntRect.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
#include "RenderingMode.h"
#include <wtf/NumberOfCores.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

#if USE(SKIA)
#include "SkiaPaintingEngine.h"
#include "SkiaRecordingResult.h"
#elif USE(CAIRO)
#include "CairoPaintingContext.h"
#include "CairoPaintingEngine.h"
#include "FloatRect.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CoordinatedTileManager);

#if USE(SKIA)

static bool canPerformAcceleratedRendering()
{
    return ProcessCapabilities::canUseAcceleratedBuffers() && PlatformDisplay::sharedDisplay().skiaGLContext();
}

unsigned CoordinatedTileManager::numberOfCPUPaintingThreads()
{
    static std::once_flag onceFlag;
    static unsigned numberOfThreads = 0;

    std::call_once(onceFlag, [] {
        numberOfThreads = std::max(1, std::min(8, WTF::numberOfProcessorCores() / 2)); // By default, use half the CPU cores, capped at 8.

        if (const char* envString = getenv("WEBKIT_SKIA_CPU_PAINTING_THREADS")) {
            auto newValue = parseInteger<unsigned>(StringView::fromLatin1(envString));
            if (newValue && *newValue >= 1 && *newValue <= 8)
                numberOfThreads = *newValue;
            else
                WTFLogAlways("The number of Skia painting threads is not between 1 and 8. Using the default value %u\n", numberOfThreads);
        }
    });

    return numberOfThreads;
}

#if USE(TEXTURE_MAPPER)
unsigned CoordinatedTileManager::numberOfGPUPaintingThreads()
{
    static std::once_flag onceFlag;
    static unsigned numberOfThreads = 0;

    std::call_once(onceFlag, [] {
        // By default, use 2 GPU worker threads if there are four or more CPU cores, otherwise use 1 thread only.
        numberOfThreads = WTF::numberOfProcessorCores() >= 4 ? 2 : 1;

        if (const char* envString = getenv("WEBKIT_SKIA_GPU_PAINTING_THREADS")) {
            auto newValue = parseInteger<unsigned>(StringView::fromLatin1(envString));
            if (newValue && *newValue >= 1 && *newValue <= 4)
                numberOfThreads = *newValue;
            else
                WTFLogAlways("The number of Skia/GPU painting threads is not between 1 and 4. Using the default value %u\n", numberOfThreads);
        }
    });

    return numberOfThreads;
}
#endif

static Ref<WorkerPool> createWorkerPool()
{
#if USE(TEXTURE_MAPPER)
    if (canPerformAcceleratedRendering())
        return WorkerPool::create("SkiaGPUWorker"_s, CoordinatedTileManager::numberOfGPUPaintingThreads());
#endif

    return WorkerPool::create("SkiaCPUWorker"_s, CoordinatedTileManager::numberOfCPUPaintingThreads());
}

std::unique_ptr<CoordinatedTileManager> CoordinatedTileManager::create(sk_sp<GrContextThreadSafeProxy>&& threadSafeGrContext)
{
    return makeUnique<CoordinatedTileManager>(WTF::move(threadSafeGrContext));
}

CoordinatedTileManager::CoordinatedTileManager(sk_sp<GrContextThreadSafeProxy>&& threadSafeGrContext)
    : m_paintingEngine(SkiaPaintingEngine::create(WTF::move(threadSafeGrContext)))
    , m_paintingWorkerPool(createWorkerPool())
{
}

const sk_sp<GrContextThreadSafeProxy>& CoordinatedTileManager::threadSafeGrContext() const
{
    return m_paintingEngine->threadSafeGrContext();
}

Ref<SkiaRecordingResult> CoordinatedTileManager::record(const GraphicsLayerCoordinated& layer, const IntRect& recordRect, bool contentsOpaque, float contentsScale, unsigned dirtyTilesCount)
{
    return m_paintingEngine->record(layer, recordRect, canPerformAcceleratedRendering() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, contentsOpaque, contentsScale, dirtyTilesCount);
}

Ref<CoordinatedTileBuffer> CoordinatedTileManager::scheduleReplay(const GraphicsLayerCoordinated& layer, const Ref<SkiaRecordingResult>& recording, const IntRect& tileRect, const IntRect& dirtyRect)
{
    Ref buffer = [&] -> Ref<CoordinatedTileBuffer> {
        auto flags = recording->contentsOpaque() ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha;
        if (recording->renderingMode() == RenderingMode::Unaccelerated)
            return CoordinatedUnacceleratedTileBuffer::create(dirtyRect.size(), flags);

#if USE(TEXTURE_MAPPER)
        return CoordinatedAcceleratedTileBuffer::create(dirtyRect.size(), flags);
#else
        return CoordinatedAcceleratedTileBuffer::create(threadSafeGrContext(), tileRect.size(), flags);
#endif
    }();

    Ref platformLayer = layer.coordinatedPlatformLayer();
    platformLayer->willPaintTile();

    m_paintingWorkerPool->postTask([this, layer = WTF::move(platformLayer), buffer = Ref { buffer }, recording = Ref { recording }, tileRect, dirtyRect] {
        if (buffer->beginPainting()) {
            m_paintingEngine->replay(buffer, recording, tileRect, dirtyRect);
            buffer->completePainting();
        }
        layer->didPaintTile();
    });
    return buffer;
}

#elif USE(CAIRO)

std::unique_ptr<CoordinatedTileManager> CoordinatedTileManager::create()
{
    return makeUnique<CoordinatedTileManager>();
}

CoordinatedTileManager::CoordinatedTileManager()
    : m_paintingEngine(Cairo::PaintingEngine::create())
{
}

Ref<CoordinatedTileBuffer> CoordinatedTileManager::schedulePaint(GraphicsLayerCoordinated& layer, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale)
{
    FloatRect scaledDirtyRect(dirtyRect);
    scaledDirtyRect.scale(1 / contentsScale);

    auto buffer = CoordinatedUnacceleratedTileBuffer::create(dirtyRect.size(), contentsOpaque ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha);
    m_paintingEngine->paint(layer, buffer.get(), dirtyRect, enclosingIntRect(scaledDirtyRect), IntRect { { }, dirtyRect.size() }, contentsScale);
    return buffer;
}

#endif

CoordinatedTileManager::~CoordinatedTileManager() = default;

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
