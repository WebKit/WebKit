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

#pragma once

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrContextThreadSafeProxy.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WorkQueue.h>
#include <wtf/WorkerPool.h>

namespace WebCore {

class AtlasUploadCondition;
class CoordinatedTileBuffer;
class GraphicsContext;
class GraphicsLayer;
class GraphicsLayerCoordinated;
class IntRect;
class IntSize;
class SkiaGPUAtlas;
class SkiaImageAtlasLayout;
class SkiaRecordingResult;
enum class RenderingMode : uint8_t;

class SkiaPaintingEngine {
    WTF_MAKE_TZONE_ALLOCATED(SkiaPaintingEngine);
    WTF_MAKE_NONCOPYABLE(SkiaPaintingEngine);
public:
    explicit SkiaPaintingEngine(sk_sp<GrContextThreadSafeProxy>&&);
    ~SkiaPaintingEngine();

    static std::unique_ptr<SkiaPaintingEngine> create(sk_sp<GrContextThreadSafeProxy>&&);

    static unsigned numberOfCPUPaintingThreads();
    static unsigned numberOfGPUPaintingThreads();
    static bool shouldUseDMABufAtlasTextures();
    static bool shouldUseLinearTileTextures();
    static bool shouldUseVivanteSuperTiledTileTextures();
    static bool isDDLEnabled();

    bool useThreadedRendering() const { return m_paintingWorkerPool; }
    const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext() const { return m_threadSafeGrContext; }

    Ref<CoordinatedTileBuffer> paint(const GraphicsLayerCoordinated&, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale);
    Ref<SkiaRecordingResult> record(const GraphicsLayerCoordinated&, const IntRect& recordRect, bool contentsOpaque, float contentsScale);
    Ref<CoordinatedTileBuffer> replay(const GraphicsLayerCoordinated&, Ref<SkiaRecordingResult>&&, const IntRect& tileRect, const IntRect& dirtyRect);

private:
    Ref<CoordinatedTileBuffer> createBuffer(RenderingMode, const IntSize&, bool contentsOpaque) const;
    void paintIntoGraphicsContext(const GraphicsLayer&, GraphicsContext&, const IntRect&, bool contentsOpaque, float contentsScale) const;
    RefPtr<SkiaGPUAtlas> createAtlas(const SkiaImageAtlasLayout&, AtlasUploadCondition&);
    bool tryReuseCachedAtlases(SkiaRecordingResult&, unsigned fingerprint);
    bool canUseDDL() const;

    sk_sp<GrContextThreadSafeProxy> m_threadSafeGrContext;
    RefPtr<WorkerPool> m_paintingWorkerPool;
    RefPtr<WorkQueue> m_uploadWorkQueue;
    unsigned m_cachedImageFingerprint { 0 };
    Vector<Ref<SkiaGPUAtlas>> m_cachedGPUAtlases;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
