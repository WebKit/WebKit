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
#include "SkiaPaintingEngine.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "BitmapTexturePool.h"
#include "CoordinatedGraphicsLayer.h"
#include "CoordinatedTileBuffer.h"
#include "DisplayListRecorderImpl.h"
#include "DisplayListReplayer.h"
#include "GLContext.h"
#include "GraphicsContextSkia.h"
#include "PlatformDisplay.h"
#include "ProcessCapabilities.h"
#include "RenderingMode.h"
#include <wtf/NumberOfCores.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaPaintingEngine);

// Note:
// If WEBKIT_SKIA_ENABLE_CPU_RENDERING is unset, we will allocate a GPU-only worker pool with WEBKIT_SKIA_GPU_PAINTING_THREADS threads (default: 1).
// If WEBKIT_SKIA_ENABLE_CPU_RENDERING is unset, and WEBKIT_SKIA_GPU_PAINTING_THREADS is set to 0, we will use GPU rendering on main thread.
//
// If WEBKIT_SKIA_ENABLE_CPU_RENDERING=1 is set, we will allocate a CPU-only worker pool with WEBKIT_SKIA_CPU_PAINTING_THREADS threads (default: nCores/2).
// if WEBKIT_SKIA_ENABLE_CPU_RENDERING=1 is set, and WEBKIT_SKIA_CPU_PAINTING_THREADS is set to 0, we will use CPU rendering on main thread.

SkiaPaintingEngine::SkiaPaintingEngine(unsigned numberOfCPUThreads, unsigned numberOfGPUThreads)
{
    // By default, GPU rendering, if activated, takes precedence over CPU rendering.
    if (ProcessCapabilities::canUseAcceleratedBuffers()) {
        m_texturePool = makeUnique<BitmapTexturePool>();

        if (numberOfGPUThreads)
            m_gpuWorkerPool = WorkerPool::create("SkiaGPUWorker"_s, numberOfGPUThreads);
    } else if (numberOfCPUThreads)
        m_cpuWorkerPool = WorkerPool::create("SkiaCPUWorker"_s, numberOfCPUThreads);
}

std::unique_ptr<SkiaPaintingEngine> SkiaPaintingEngine::create()
{
    return makeUnique<SkiaPaintingEngine>(numberOfCPUPaintingThreads(), numberOfGPUPaintingThreads());
}

std::unique_ptr<DisplayList::DisplayList> SkiaPaintingEngine::recordDisplayList(const CoordinatedGraphicsLayer& layer, const IntRect& dirtyRect) const
{
    auto displayList = makeUnique<DisplayList::DisplayList>(DisplayList::ReplayOption::FlushImagesAndWaitForCompletion);
    DisplayList::RecorderImpl recordingContext(*displayList, GraphicsContextState(), FloatRect({ }, dirtyRect.size()), AffineTransform());
    paintIntoGraphicsContext(layer, recordingContext, dirtyRect);
    return displayList;
}

void SkiaPaintingEngine::paintIntoGraphicsContext(const CoordinatedGraphicsLayer& layer, GraphicsContext& context, const IntRect& dirtyRect) const
{
    IntRect initialClip(IntPoint::zero(), dirtyRect.size());
    context.clip(initialClip);

    if (!layer.contentsOpaque()) {
        context.setCompositeOperation(CompositeOperator::Copy);
        context.fillRect(initialClip, Color::transparentBlack);
        context.setCompositeOperation(CompositeOperator::SourceOver);
    }

    auto scale = layer.effectiveContentsScale();
    FloatRect clipRect(dirtyRect);
    clipRect.scale(1 / scale);

    context.translate(-dirtyRect.x(), -dirtyRect.y());
    context.scale(scale);
    layer.paintGraphicsLayerContents(context, clipRect);
}

bool SkiaPaintingEngine::paintDisplayListIntoBuffer(Ref<CoordinatedTileBuffer>& buffer, DisplayList::DisplayList& displayList)
{
    auto* canvas = buffer->canvas();
    if (!canvas)
        return false;

    static thread_local RefPtr<ControlFactory> s_controlFactory;
    if (!s_controlFactory)
        s_controlFactory = ControlFactory::create();

    canvas->save();
    canvas->clear(SkColors::kTransparent);

    GraphicsContextSkia context(*canvas, buffer->isBackedByOpenGL() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
    DisplayList::Replayer(context, displayList.items(), displayList.resourceHeap(), *s_controlFactory, displayList.replayOptions()).replay();

    canvas->restore();
    return true;
}

bool SkiaPaintingEngine::paintGraphicsLayerIntoBuffer(Ref<CoordinatedTileBuffer>& buffer, const CoordinatedGraphicsLayer& layer, const IntRect& dirtyRect) const
{
    auto* canvas = buffer->canvas();
    if (!canvas)
        return false;

    canvas->save();
    canvas->clear(SkColors::kTransparent);

    GraphicsContextSkia context(*canvas, buffer->isBackedByOpenGL() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
    paintIntoGraphicsContext(layer, context, dirtyRect);

    canvas->restore();
    return true;
}

static bool canPerformAcceleratedRendering()
{
    return ProcessCapabilities::canUseAcceleratedBuffers() && PlatformDisplay::sharedDisplay().skiaGLContext();
}

RenderingMode SkiaPaintingEngine::renderingMode() const
{
    if (canPerformAcceleratedRendering())
        return RenderingMode::Accelerated;

    return RenderingMode::Unaccelerated;
}

std::optional<RenderingMode> SkiaPaintingEngine::threadedRenderingMode() const
{
    if (m_gpuWorkerPool && canPerformAcceleratedRendering())
        return RenderingMode::Accelerated;

    if (m_cpuWorkerPool)
        return RenderingMode::Unaccelerated;

    return std::nullopt;
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::createBuffer(RenderingMode renderingMode, const CoordinatedGraphicsLayer& layer, const IntSize& size) const
{
    if (renderingMode == RenderingMode::Accelerated) {
        PlatformDisplay::sharedDisplay().skiaGLContext()->makeContextCurrent();

        OptionSet<BitmapTexture::Flags> textureFlags;
        if (!layer.contentsOpaque())
            textureFlags.add(BitmapTexture::Flags::SupportsAlpha);

        ASSERT(m_texturePool);
        return CoordinatedAcceleratedTileBuffer::create(m_texturePool->acquireTexture(size, textureFlags));
    }

    return CoordinatedUnacceleratedTileBuffer::create(size, layer.contentsOpaque() ? CoordinatedTileBuffer::NoFlags : CoordinatedTileBuffer::SupportsAlpha);
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::paintLayer(const CoordinatedGraphicsLayer& layer, const IntRect& dirtyRect)
{
    // ### Asynchronous rendering on worker threads ###
    if (auto renderingMode = SkiaPaintingEngine::threadedRenderingMode())
        return postPaintingTask(renderingMode.value(), layer, dirtyRect);

    // ### Synchronous rendering on main thread ###
    return performPaintingTask(renderingMode(), layer, dirtyRect);
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::postPaintingTask(RenderingMode renderingMode, const CoordinatedGraphicsLayer& layer, const IntRect& dirtyRect)
{
    WTFBeginSignpost(this, RecordTile);
    auto displayList = recordDisplayList(layer, dirtyRect);
    WTFEndSignpost(this, RecordTile);

    auto buffer = createBuffer(renderingMode, layer, dirtyRect.size());
    buffer->beginPainting();

    auto& workerPool = renderingMode == RenderingMode::Accelerated ? *m_gpuWorkerPool.get() : *m_cpuWorkerPool.get();
    workerPool.postTask([buffer = Ref { buffer }, displayList = WTFMove(displayList), dirtyRect]() mutable {
        if (auto* canvas = buffer->canvas()) {
            WTFBeginSignpost(canvas, PaintTile, "Skia/%s threaded, dirty region %ix%i+%i+%i", buffer->isBackedByOpenGL() ? "GPU" : "CPU", dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height());
            paintDisplayListIntoBuffer(buffer, *displayList.get());
            WTFEndSignpost(canvas, PaintTile);
        }

        buffer->completePainting();

        // Destruct display list on main thread.
        ensureOnMainThread([displayList = WTFMove(displayList)]() mutable {
            displayList = nullptr;
        });
    });

    return buffer;
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::performPaintingTask(RenderingMode renderingMode, const CoordinatedGraphicsLayer& layer, const IntRect& dirtyRect)
{
    auto buffer = createBuffer(renderingMode, layer, dirtyRect.size());
    buffer->beginPainting();

    if (auto* canvas = buffer->canvas()) {
        WTFBeginSignpost(canvas, PaintTile, "Skia/%s, dirty region %ix%i+%i+%i", buffer->isBackedByOpenGL() ? "GPU" : "CPU", dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height());
        paintGraphicsLayerIntoBuffer(buffer, layer, dirtyRect);
        WTFEndSignpost(canvas, PaintTile);
    }

    buffer->completePainting();
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
        // If WEBKIT_SKIA_ENABLE_CPU_RENDERING=1 is set in the environment, no GPU painting is used.
        if (!ProcessCapabilities::canUseAcceleratedBuffers())
            return;

        numberOfThreads = 1; // By default, use 1 GPU worker thread, if GPU painting is active.

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

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
