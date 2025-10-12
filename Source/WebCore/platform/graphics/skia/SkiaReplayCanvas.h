/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "IntSize.h"
#include "SkiaRecordingResult.h"
#include <wtf/Assertions.h>
#include <wtf/Function.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/utils/SkNWayCanvas.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

class SkImage;

namespace WebCore {

class SkiaReplayCanvas final : public SkNWayCanvas, public RefCounted<SkiaReplayCanvas> {
public:
    ~SkiaReplayCanvas() override;
    static Ref<SkiaReplayCanvas> create(const IntSize&, const RefPtr<SkiaRecordingResult>&);

    const sk_sp<SkPicture>& picture() const { return m_recording->picture(); }

private:
    SkiaReplayCanvas(const IntSize&, const RefPtr<SkiaRecordingResult>&);

    sk_sp<SkImage> waitForRenderingCompletionAndRewrapImageIfNeeded(const SkImage*);

    void invokeDrawFunctionWithImage(const SkImage*, Function<void(const SkImage*)>&&);
    void invokeDrawFunctionWithPaint(const SkPaint&, Function<void(const SkPaint&)>&&);
    void invokeDrawFunctionWithShader(const SkShader*, Function<void(const SkShader*)>&&);

    // SkNWayCanvas overrides
    void onClipShader(sk_sp<SkShader>, SkClipOp) override;
    void onDrawArc(const SkRect&, SkScalar, SkScalar, bool, const SkPaint&) override;
    void onDrawAtlas2(const SkImage*, const SkRSXform[], const SkRect[], const SkColor[], int, SkBlendMode, const SkSamplingOptions&, const SkRect*, const SkPaint*) override;
    void onDrawBehind(const SkPaint&) override;
    void onDrawDRRect(const SkRRect&, const SkRRect&, const SkPaint&) override;
    void onDrawEdgeAAImageSet2(const ImageSetEntry[], int, const SkPoint[], const SkMatrix[], const SkSamplingOptions&, const SkPaint*, SrcRectConstraint) override;
    void onDrawGlyphRunList(const sktext::GlyphRunList&, const SkPaint&) override;
    void onDrawImage2(const SkImage*, SkScalar, SkScalar, const SkSamplingOptions&, const SkPaint*) override;
    void onDrawImageLattice2(const SkImage*, const Lattice&, const SkRect&, SkFilterMode, const SkPaint*) override;
    void onDrawImageRect2(const SkImage*, const SkRect&, const SkRect&, const SkSamplingOptions&, const SkPaint*, SrcRectConstraint) override;
    void onDrawOval(const SkRect&, const SkPaint&) override;
    void onDrawPaint(const SkPaint&) override;
    void onDrawPatch(const SkPoint[12], const SkColor[4], const SkPoint[4], SkBlendMode, const SkPaint&) override;
    void onDrawPath(const SkPath&, const SkPaint&) override;
    void onDrawPoints(PointMode, size_t, const SkPoint[], const SkPaint&) override;
    void onDrawRRect(const SkRRect&, const SkPaint&) override;
    void onDrawRect(const SkRect&, const SkPaint&) override;
    void onDrawRegion(const SkRegion&, const SkPaint&) override;
    void onDrawSlug(const sktext::gpu::Slug*, const SkPaint&) override;
    void onDrawTextBlob(const SkTextBlob*, SkScalar x, SkScalar y, const SkPaint&) override;
    void onDrawVerticesObject(const SkVertices*, SkBlendMode, const SkPaint&) override;

    RefPtr<SkiaRecordingResult> m_recording;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
