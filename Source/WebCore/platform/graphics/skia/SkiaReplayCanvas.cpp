/*
 * Copyright (C) 2025, 2026 Igalia S.L.
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
#include "SkiaReplayCanvas.h"

#if USE(SKIA)
#include "IntSize.h"
#include "SkiaReplayAtlas.h"

namespace WebCore {

SkiaReplayCanvas::SkiaReplayCanvas(const IntSize& size, const Ref<SkiaRecordingResult>& recording, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
    : SkNWayCanvas(size.width(), size.height())
    , m_recording(recording)
    , m_threadSafeGrContext(threadSafeGrContext)
{
    // Create atlas wrappers from pre-prepared GPU atlases.
    // GPU atlases were uploaded on main thread; we just rewrap for this worker's context.
    if (m_recording->hasGPUAtlases()) {
        const auto& gpuAtlases = m_recording->gpuAtlases();
        m_atlases.reserveInitialCapacity(gpuAtlases.size());

        for (const auto& gpuAtlas : gpuAtlases)
            m_atlases.append(SkiaReplayAtlas::create(gpuAtlas));
    }
}

SkiaReplayCanvas::~SkiaReplayCanvas() = default;

Ref<SkiaReplayCanvas> SkiaReplayCanvas::create(const IntSize& size, const Ref<SkiaRecordingResult>& recordingResult, const sk_sp<GrContextThreadSafeProxy>& threadSafeGrContext)
{
    return adoptRef(*new SkiaReplayCanvas(size, recordingResult, threadSafeGrContext));
}

void SkiaReplayCanvas::onDrawImage2(const SkImage* image, SkScalar x, SkScalar y, const SkSamplingOptions& sampling, const SkPaint* paint)
{
    // Check for atlas substitution for raster images.
    if (!m_atlases.isEmpty() && image && !image->isTextureBacked()) {
        for (auto& atlas : m_atlases) {
            if (auto atlasRect = atlas->rectForImage(*image)) {
                // Draw from atlas instead of original image.
                const auto& atlasImage = atlas->atlasImage();
                ASSERT(atlasImage);
                auto dst = SkRect::MakeXYWH(x, y, atlasRect->width(), atlasRect->height());
                SkNWayCanvas::onDrawImageRect2(atlasImage.get(), *atlasRect, dst, sampling, paint, SkCanvas::kStrict_SrcRectConstraint);
                return;
            }
        }
    }

    SkNWayCanvas::onDrawImage2(image, x, y, sampling, paint);
}

void SkiaReplayCanvas::onDrawImageRect2(const SkImage* image, const SkRect& src, const SkRect& dst, const SkSamplingOptions& sampling, const SkPaint* paint, SrcRectConstraint constraint)
{
    // Check for atlas substitution for raster images.
    if (!m_atlases.isEmpty() && image && !image->isTextureBacked()) {
        for (auto& atlas : m_atlases) {
            if (auto atlasRect = atlas->rectForImage(*image)) {
                // Draw from atlas instead of original image.
                const auto& atlasImage = atlas->atlasImage();
                ASSERT(atlasImage);

                // Map src rect from image coordinates to atlas coordinates.
                SkScalar atlasX = atlasRect->x() + src.x();
                SkScalar atlasY = atlasRect->y() + src.y();
                auto atlasSrc = SkRect::MakeXYWH(atlasX, atlasY, src.width(), src.height());

                // Clamp to image bounds within atlas to prevent reading adjacent images.
                SkRect imageBoundsInAtlas = *atlasRect;
                if (!atlasSrc.intersect(imageBoundsInAtlas))
                    return;

                // Adjust destination proportionally if source was clamped.
                if (atlasSrc.width() != src.width() || atlasSrc.height() != src.height()) {
                    SkScalar leftClip = atlasSrc.x() - atlasX;
                    SkScalar topClip = atlasSrc.y() - atlasY;
                    SkScalar scaleX = dst.width() / src.width();
                    SkScalar scaleY = dst.height() / src.height();

                    auto adjustedDst = SkRect::MakeXYWH(dst.x() + leftClip * scaleX, dst.y() + topClip * scaleY, atlasSrc.width() * scaleX, atlasSrc.height() * scaleY);
                    SkNWayCanvas::onDrawImageRect2(atlasImage.get(), atlasSrc, adjustedDst, sampling, paint, SkCanvas::kStrict_SrcRectConstraint);
                    return;
                }

                SkNWayCanvas::onDrawImageRect2(atlasImage.get(), atlasSrc, dst, sampling, paint, SkCanvas::kStrict_SrcRectConstraint);
                return;
            }
        }
    }

    SkNWayCanvas::onDrawImageRect2(image, src, dst, sampling, paint, constraint);
}

} // namespace WebCore

#endif // USE(SKIA)
