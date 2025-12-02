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
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WorkerPool.h>

namespace WebCore {

class BitmapTexturePool;
class CoordinatedTileBuffer;
class GraphicsContext;
class GraphicsLayer;
class GraphicsLayerCoordinated;
class IntRect;
class IntSize;
class SkiaRecordingResult;
enum class RenderingMode : uint8_t;

class SkiaPaintingEngine {
    WTF_MAKE_TZONE_ALLOCATED(SkiaPaintingEngine);
    WTF_MAKE_NONCOPYABLE(SkiaPaintingEngine);
public:
    SkiaPaintingEngine();
    ~SkiaPaintingEngine();

    static std::unique_ptr<SkiaPaintingEngine> create();

    static unsigned numberOfCPUPaintingThreads();
    static unsigned numberOfGPUPaintingThreads();
    static bool shouldUseLinearTileTextures();

    bool useThreadedRendering() const { return m_workerPool; }

    Ref<CoordinatedTileBuffer> paint(const GraphicsLayerCoordinated&, const IntRect& dirtyRect, bool contentsOpaque, float contentsScale);
    Ref<SkiaRecordingResult> record(const GraphicsLayerCoordinated&, const IntRect& recordRect, bool contentsOpaque, float contentsScale);
    Ref<CoordinatedTileBuffer> replay(const GraphicsLayerCoordinated&, const RefPtr<SkiaRecordingResult>&, const IntRect& dirtyRect);

private:
    Ref<CoordinatedTileBuffer> createBuffer(RenderingMode, const IntSize&, bool contentsOpaque) const;
    void paintIntoGraphicsContext(const GraphicsLayer&, GraphicsContext&, const IntRect&, bool contentsOpaque, float contentsScale) const;

    RefPtr<WorkerPool> m_workerPool;
    std::unique_ptr<BitmapTexturePool> m_texturePool;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
