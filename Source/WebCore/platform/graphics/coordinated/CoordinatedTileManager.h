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

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include <WebCore/IntRect.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WorkerPool.h>

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrContextThreadSafeProxy.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {
class CoordinatedPlatformLayer;
class CoordinatedTileBuffer;
class GraphicsLayerCoordinated;

#if USE(SKIA)
class SkiaPaintingEngine;
class SkiaRecordingResult;
#elif USE(CAIRO)
namespace Cairo {
class PaintingEngine;
}
#endif

class CoordinatedTileManager {
    WTF_MAKE_TZONE_ALLOCATED(CoordinatedTileManager);
    WTF_MAKE_NONCOPYABLE(CoordinatedTileManager);
public:
#if USE(SKIA)
    explicit CoordinatedTileManager(sk_sp<GrContextThreadSafeProxy>&&);
    static std::unique_ptr<CoordinatedTileManager> create(sk_sp<GrContextThreadSafeProxy>&&);
#elif USE(CAIRO)
    static std::unique_ptr<CoordinatedTileManager> create();
    CoordinatedTileManager();
#endif
    ~CoordinatedTileManager();

#if USE(SKIA)
    static unsigned numberOfPaintingThreads();

    const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext() const;
    Ref<SkiaRecordingResult> record(const GraphicsLayerCoordinated&, const IntRect& recordRect, bool contentsOpaque, float contentsScale, unsigned dirtyTilesCount);
    Ref<CoordinatedTileBuffer> scheduleReplay(const GraphicsLayerCoordinated&, const Ref<SkiaRecordingResult>&, const IntRect& tileRect, const IntRect& dirtyRect);
#elif USE(CAIRO)
    Ref<CoordinatedTileBuffer> schedulePaint(GraphicsLayerCoordinated&, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale);
#endif

private:
#if USE(SKIA)
    struct PaintingTask {
        Ref<CoordinatedPlatformLayer> layer;
        Ref<CoordinatedTileBuffer> buffer;
        Ref<SkiaRecordingResult> recording;
        IntRect tileRect;
        IntRect dirtyRect;
    };

    const std::unique_ptr<SkiaPaintingEngine> m_paintingEngine;
    Lock m_paintingTaskQueueLock;
    Vector<PaintingTask> m_paintingTaskQueue WTF_GUARDED_BY_LOCK(m_paintingTaskQueueLock);

    // Keep the worker pool as last member to ensure it's destroyed first.
    const Ref<WorkerPool> m_paintingWorkerPool;
#elif USE(CAIRO)
    const std::unique_ptr<Cairo::PaintingEngine> m_paintingEngine;
#endif
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
