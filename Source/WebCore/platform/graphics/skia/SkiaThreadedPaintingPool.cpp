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
#include "SkiaThreadedPaintingPool.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "CoordinatedGraphicsLayer.h"
#include "CoordinatedTileBuffer.h"
#include "DisplayListRecorderImpl.h"
#include "GraphicsContextSkia.h"
#include <wtf/NumberOfCores.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaThreadedPaintingPool);

SkiaThreadedPaintingPool::SkiaThreadedPaintingPool(unsigned numberOfThreads)
    : m_workerPool(WorkerPool::create("SkiaPaintingThread"_s, numberOfThreads))
{
}

std::unique_ptr<SkiaThreadedPaintingPool> SkiaThreadedPaintingPool::create()
{
    if (auto numberOfThreads = numberOfPaintingThreads(); numberOfThreads > 0)
        return makeUnique<SkiaThreadedPaintingPool>(numberOfThreads);

    return nullptr;
}

std::unique_ptr<DisplayList::DisplayList> SkiaThreadedPaintingPool::recordDisplayList(const CoordinatedGraphicsLayer& layer, const IntRect& dirtyRect) const
{
    auto displayList = makeUnique<DisplayList::DisplayList>(DisplayList::ReplayOption::FlushImagesAndWaitForCompletion);
    DisplayList::RecorderImpl recordingContext(*displayList, GraphicsContextState(), FloatRect({ }, dirtyRect.size()), AffineTransform());
    layer.paintIntoGraphicsContext(recordingContext, dirtyRect);
    return displayList;
}

void SkiaThreadedPaintingPool::postPaintingTask(Ref<CoordinatedTileBuffer>& buffer, const CoordinatedGraphicsLayer& layer, const IntRect& dirtyRect)
{
    WTFBeginSignpost(this, RecordTile);
    auto displayList = recordDisplayList(layer, dirtyRect);
    WTFEndSignpost(this, RecordTile);

    buffer->beginPainting();

    m_workerPool->postTask([buffer = Ref { buffer }, displayList = WTFMove(displayList), dirtyRect]() mutable {
        if (auto* canvas = buffer->canvas()) {
            canvas->save();
            canvas->clear(SkColors::kTransparent);

            static thread_local RefPtr<ControlFactory> s_controlFactory;
            if (!s_controlFactory)
                s_controlFactory = ControlFactory::create();

            WTFBeginSignpost(canvas, PaintTile, "Skia unaccelerated multithread, dirty region %ix%i+%i+%i", dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height());

            GraphicsContextSkia context(*canvas, RenderingMode::Unaccelerated, RenderingPurpose::LayerBacking);
            context.drawDisplayListItems(displayList->items(), displayList->resourceHeap(), *s_controlFactory, FloatPoint::zero());

            WTFEndSignpost(canvas, PaintTile);
            canvas->restore();
        }

        buffer->completePainting();

        // Destruct display list on main thread.
        ensureOnMainThread([displayList = WTFMove(displayList)]() mutable {
            displayList = nullptr;
        });
    });
}

unsigned SkiaThreadedPaintingPool::numberOfPaintingThreads()
{
    static std::once_flag onceFlag;
    static unsigned numberOfThreads = 0;

    std::call_once(onceFlag, [] {
        numberOfThreads = std::max(1, std::min(8, WTF::numberOfProcessorCores() / 2)); // By default, use half the CPU cores, capped at 8.

        if (const char* envString = getenv("WEBKIT_SKIA_PAINTING_THREADS")) {
            auto newValue = parseInteger<unsigned>(StringView::fromLatin1(envString));
            if (newValue && *newValue <= 8)
                numberOfThreads = *newValue;
            else
                WTFLogAlways("The number of Skia painting threads is not between 0 and 8. Using the default value %u\n", numberOfThreads);
        }
    });

    return numberOfThreads;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
