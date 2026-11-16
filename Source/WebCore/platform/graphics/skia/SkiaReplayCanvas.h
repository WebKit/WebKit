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

#if USE(SKIA)
#include "SkiaRecordingResult.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrContextThreadSafeProxy.h>
#include <skia/utils/SkNWayCanvas.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/Assertions.h>
#include <wtf/Function.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

class SkImage;

namespace WebCore {
class IntSize;
class SkiaReplayAtlas;

class SkiaReplayCanvas final : public SkNWayCanvas, public RefCounted<SkiaReplayCanvas> {
public:
    ~SkiaReplayCanvas() override;
    static Ref<SkiaReplayCanvas> create(const IntSize&, const Ref<SkiaRecordingResult>&, const sk_sp<GrContextThreadSafeProxy>& = nullptr);

    const sk_sp<SkPicture>& picture() const { return m_recording->picture(); }

private:
    SkiaReplayCanvas(const IntSize&, const Ref<SkiaRecordingResult>&, const sk_sp<GrContextThreadSafeProxy>&);

    // SkNWayCanvas overrides
    void onDrawImage2(const SkImage*, SkScalar, SkScalar, const SkSamplingOptions&, const SkPaint*) override;
    void onDrawImageRect2(const SkImage*, const SkRect&, const SkRect&, const SkSamplingOptions&, const SkPaint*, SrcRectConstraint) override;

    const Ref<SkiaRecordingResult> m_recording;
    sk_sp<GrContextThreadSafeProxy> m_threadSafeGrContext;
    Vector<std::unique_ptr<SkiaReplayAtlas>> m_atlases;
};

} // namespace WebCore

#endif // USE(SKIA)
