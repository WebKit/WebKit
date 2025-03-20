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
#include "CoordinatedTileBuffer.h"
#include "DisplayListRecorderImpl.h"
#include "DisplayListReplayer.h"
#include "GLContext.h"
#include "GraphicsContextSkia.h"
#include "GraphicsLayer.h"
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
//
// By default we use the "hybrid" mode, utilizing both CPU & GPU.
// See below for WEBKIT_SKIA_HYBRID_PAINTING_MODE_STRATEGY.

SkiaPaintingEngine::SkiaPaintingEngine(unsigned numberOfCPUThreads, unsigned numberOfGPUThreads)
{
    if (ProcessCapabilities::canUseAcceleratedBuffers()) {
        m_texturePool = makeUnique<BitmapTexturePool>();

        if (numberOfGPUThreads)
            m_gpuWorkerPool = WorkerPool::create("SkiaGPUWorker"_s, numberOfGPUThreads);
    }

    if (numberOfCPUThreads)
        m_cpuWorkerPool = WorkerPool::create("SkiaCPUWorker"_s, numberOfCPUThreads);
}

SkiaPaintingEngine::~SkiaPaintingEngine() = default;

std::unique_ptr<SkiaPaintingEngine> SkiaPaintingEngine::create()
{
    return makeUnique<SkiaPaintingEngine>(numberOfCPUPaintingThreads(), numberOfGPUPaintingThreads());
}

static bool canPerformAcceleratedRendering()
{
    return ProcessCapabilities::canUseAcceleratedBuffers() && PlatformDisplay::sharedDisplay().skiaGLContext();
}

std::unique_ptr<DisplayList::DisplayList> SkiaPaintingEngine::recordDisplayList(RenderingMode& renderingMode, const GraphicsLayer& layer, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale) const
{
    OptionSet<DisplayList::ReplayOption> options;
    if (isHybridMode() || renderingMode == RenderingMode::Accelerated)
        options.add(DisplayList::ReplayOption::FlushAcceleratedImagesAndWaitForCompletion);

    auto displayList = makeUnique<DisplayList::DisplayList>(options);
    DisplayList::RecorderImpl recordingContext(*displayList, GraphicsContextState(), FloatRect({ }, dirtyRect.size()), AffineTransform());
    paintIntoGraphicsContext(layer, recordingContext, dirtyRect, contentsOpaque, contentsScale);

    // If we used accelerated ImageBuffers during recording, it's mandatory to replay in a GPU worker thread, with a GL context available.
    if (recordingContext.usedAcceleratedRendering()) {
        // Verify that we only used accelerated rendering if we're in either hybrid mode or GPU rendering mode (renderingMode == RenderingMode::Accelerated).
        // If isHybridMode() == true, we eventually decided to use the CPU before (indicated by renderingMode == RenderingMode::Unaccelerated), but are
        // forced to switch to use the GPU for replaying, because fences were created due the use of accelerated ImageBuffers, and thus we have to wait for
        // the fences to be signalled -- that requires a GL context, and thus it's mandatory to replay in a GPU worker thread.
        ASSERT(isHybridMode() || renderingMode == RenderingMode::Accelerated);
        ASSERT(canPerformAcceleratedRendering());
        renderingMode = RenderingMode::Accelerated;
    }

    return displayList;
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

bool SkiaPaintingEngine::paintGraphicsLayerIntoBuffer(Ref<CoordinatedTileBuffer>& buffer, const GraphicsLayer& layer, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale) const
{
    auto* canvas = buffer->canvas();
    if (!canvas)
        return false;

    canvas->save();
    canvas->clear(SkColors::kTransparent);

    GraphicsContextSkia context(*canvas, buffer->isBackedByOpenGL() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
    paintIntoGraphicsContext(layer, context, dirtyRect, contentsOpaque, contentsScale);

    canvas->restore();
    return true;
}

bool SkiaPaintingEngine::isHybridMode() const
{
    return m_cpuWorkerPool && m_gpuWorkerPool && canPerformAcceleratedRendering();
}

RenderingMode SkiaPaintingEngine::decideHybridRenderingMode(const IntRect& dirtyRect, float contentsScale) const
{
    // Single strategy: If CPU is idle, always use it.
    auto handlePreferCPUIfIdle = [&]() -> RenderingMode {
        if (m_cpuWorkerPool->numberOfTasks() < numberOfCPUPaintingThreads())
            return RenderingMode::Unaccelerated;
        return RenderingMode::Accelerated;
    };

    // Single strategy: If GPU is idle, always use it.
    auto handlePreferGPUIfIdle = [&]() -> RenderingMode {
        if (m_gpuWorkerPool->numberOfTasks() < numberOfGPUPaintingThreads())
            return RenderingMode::Accelerated;
        return RenderingMode::Unaccelerated;
    };

    // Single strategy: If painting area exceeds a threshold, always use GPU.
    auto handlePreferGPUAboveMinimumArea = [&]() -> RenderingMode {
        if (dirtyRect.area() >= minimumAreaForGPUPainting())
            return RenderingMode::Accelerated;
        return RenderingMode::Unaccelerated;
    };

    // Single strategy: Decide randomly whether to use GPU or not.
    auto handleMinimumFractionOfTasksUsingGPU = [&]() -> RenderingMode {
        auto randomFraction = static_cast<double>(weakRandomNumber<uint32_t>()) / static_cast<double>(UINT32_MAX);
        if (randomFraction <= minimumFractionOfTasksUsingGPUPainting())
            return RenderingMode::Accelerated;
        return RenderingMode::Unaccelerated;
    };

    // Combined strategy: default for WPE, "hybrid mode", saturates CPU painting, before using GPU.
    auto handleCPUAffineRendering = [&]() -> RenderingMode {
        // If there is a non-identity scaling applied, prefer GPU rendering.
        if (contentsScale != 1)
            return RenderingMode::Accelerated;

        // If the CPU worker pool has unused workers, use them.
        if (m_cpuWorkerPool->numberOfTasks() < numberOfCPUPaintingThreads())
            return RenderingMode::Unaccelerated;

        // If the GPU worker pool has unused workers, use them.
        if (m_gpuWorkerPool->numberOfTasks() < numberOfGPUPaintingThreads())
            return RenderingMode::Accelerated;

        return handleMinimumFractionOfTasksUsingGPU();
    };

    // Combined strategy: default for Gtk, useful mode for high-end GPUs, saturates GPU painting, before using CPU.
    auto handleGPUAffineRendering = [&]() -> RenderingMode {
        // If there is a non-identity scaling applied, prefer GPU rendering.
        if (contentsScale != 1)
            return RenderingMode::Accelerated;

        // If the GPU worker pool has unused workers, use them.
        if (m_gpuWorkerPool->numberOfTasks() < numberOfGPUPaintingThreads())
            return RenderingMode::Accelerated;

        // If the CPU worker pool has unused workers, use them.
        if (m_cpuWorkerPool->numberOfTasks() < numberOfCPUPaintingThreads())
            return RenderingMode::Unaccelerated;

        return handleMinimumFractionOfTasksUsingGPU();
    };

    switch (hybridPaintingStrategy()) {
    case HybridPaintingStrategy::PreferCPUIfIdle:
        return handlePreferCPUIfIdle();
    case HybridPaintingStrategy::PreferGPUIfIdle:
        return handlePreferGPUIfIdle();
    case HybridPaintingStrategy::PreferGPUAboveMinimumArea:
        return handlePreferGPUAboveMinimumArea();
    case HybridPaintingStrategy::MinimumFractionOfTasksUsingGPU:
        return handleMinimumFractionOfTasksUsingGPU();
    case HybridPaintingStrategy::CPUAffineRendering:
        return handleCPUAffineRendering();
    case HybridPaintingStrategy::GPUAffineRendering:
        return handleGPUAffineRendering();
    }

    ASSERT_NOT_REACHED();
    return RenderingMode::Unaccelerated;
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

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::paintLayer(const GraphicsLayer& layer, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale)
{
    // ### Synchronous rendering on main thread ###
    if (!m_cpuWorkerPool && !m_gpuWorkerPool) {
        auto renderingMode = canPerformAcceleratedRendering() ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;
        return performPaintingTask(layer, renderingMode, dirtyRect, contentsOpaque, contentsScale);
    }

    // ### Asynchronous rendering on worker threads ###

    // ### Hybrid CPU/GPU mode ###
    if (isHybridMode()) {
        auto renderingMode = decideHybridRenderingMode(dirtyRect, contentsScale);
        return postPaintingTask(layer, renderingMode, dirtyRect, contentsOpaque, contentsScale);
    }

    // ### CPU-only mode ###
    if (m_cpuWorkerPool)
        return postPaintingTask(layer, RenderingMode::Unaccelerated, dirtyRect, contentsOpaque, contentsScale);

    // ### GPU-only mode ###
    if (m_gpuWorkerPool && canPerformAcceleratedRendering())
        return postPaintingTask(layer, RenderingMode::Accelerated, dirtyRect, contentsOpaque, contentsScale);

    ASSERT_NOT_REACHED();
    return performPaintingTask(layer, RenderingMode::Unaccelerated, dirtyRect, contentsOpaque, contentsScale);
}

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::postPaintingTask(const GraphicsLayer& layer, RenderingMode renderingMode, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale)
{
    WTFBeginSignpost(this, RecordTile);
    auto displayList = recordDisplayList(renderingMode, layer, dirtyRect, contentsOpaque, contentsScale);
    WTFEndSignpost(this, RecordTile);

    auto buffer = createBuffer(renderingMode, dirtyRect.size(), contentsOpaque);
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

Ref<CoordinatedTileBuffer> SkiaPaintingEngine::performPaintingTask(const GraphicsLayer& layer, RenderingMode renderingMode, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale)
{
    auto buffer = createBuffer(renderingMode, dirtyRect.size(), contentsOpaque);
    buffer->beginPainting();

    if (auto* canvas = buffer->canvas()) {
        WTFBeginSignpost(canvas, PaintTile, "Skia/%s, dirty region %ix%i+%i+%i", buffer->isBackedByOpenGL() ? "GPU" : "CPU", dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height());
        paintGraphicsLayerIntoBuffer(buffer, layer, dirtyRect, contentsOpaque, contentsScale);
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

unsigned SkiaPaintingEngine::minimumAreaForGPUPainting()
{
    static std::once_flag onceFlag;
    static unsigned areaThreshold = 0;

    std::call_once(onceFlag, [] {
        areaThreshold = 256 * 256; // Prefer GPU rendering above an area of 256x256px (by default, a fourth of a 512x512 tile).

        if (const char* envString = getenv("WEBKIT_SKIA_GPU_PAINTING_MIN_AREA")) {
            if (auto newValue = parseInteger<unsigned>(StringView::fromLatin1(envString)))
                areaThreshold = *newValue;
        }
    });

    return areaThreshold;
}

float SkiaPaintingEngine::minimumFractionOfTasksUsingGPUPainting()
{
    static std::once_flag onceFlag;
    static unsigned gpuUsagePercentage = 0;

    std::call_once(onceFlag, [] {
        gpuUsagePercentage = 50; // Half of the tasks go to CPU, half to GPU.

        if (const char* envString = getenv("WEBKIT_SKIA_GPU_MIN_FRACTION_OF_TASKS_IN_PERCENT")) {
            if (auto newValue = parseInteger<unsigned>(StringView::fromLatin1(envString)))
                gpuUsagePercentage = *newValue;
        }
    });

    return float(gpuUsagePercentage) / 100.0f;
}

SkiaPaintingEngine::HybridPaintingStrategy SkiaPaintingEngine::hybridPaintingStrategy()
{
    static std::once_flag onceFlag;
    static HybridPaintingStrategy strategy;

    std::call_once(onceFlag, [] {
#if PLATFORM(WPE)
        strategy = HybridPaintingStrategy::CPUAffineRendering; // Saturate CPU, before using GPU.
#else
        strategy = HybridPaintingStrategy::GPUAffineRendering; // Saturate GPU, before using CPU.
#endif

        if (const char* envString = getenv("WEBKIT_SKIA_HYBRID_PAINTING_MODE_STRATEGY")) {
            auto envStringView = StringView::fromLatin1(envString);
            if (envStringView == "PreferCPUIfIdle"_s)
                strategy = HybridPaintingStrategy::PreferCPUIfIdle;
            else if (envStringView == "PreferGPUIfIdle"_s)
                strategy = HybridPaintingStrategy::PreferGPUIfIdle;
            else if (envStringView == "PreferGPUAboveMinimumArea"_s)
                strategy = HybridPaintingStrategy::PreferGPUAboveMinimumArea;
            else if (envStringView == "MinimumFractionOfTasksUsingGPU"_s)
                strategy = HybridPaintingStrategy::MinimumFractionOfTasksUsingGPU;
            else if (envStringView == "CPUAffineRendering"_s)
                strategy = HybridPaintingStrategy::CPUAffineRendering;
            else if (envStringView == "GPUAffineRendering"_s)
                strategy = HybridPaintingStrategy::GPUAffineRendering;
        }
    });

    return strategy;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
