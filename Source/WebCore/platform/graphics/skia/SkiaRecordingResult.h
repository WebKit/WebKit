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
#include "GraphicsContextSkia.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPicture.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

class SkImage;

namespace WebCore {

class SkiaRecordingResult final : public ThreadSafeRefCounted<SkiaRecordingResult, WTF::DestructionThread::Main> {
public:
    ~SkiaRecordingResult();
    static Ref<SkiaRecordingResult> create(sk_sp<SkPicture>&&, SkiaImageToFenceMap&&, const IntRect& recordRect, RenderingMode, bool contentsOpaque, float contentsScale);

    void waitForFenceIfNeeded(const SkImage&);
    bool hasFences();

    const sk_sp<SkPicture>& picture() const { return m_picture; }
    const IntRect& recordRect() const { return m_recordRect; }
    RenderingMode renderingMode() const { return m_renderingMode; }
    bool contentsOpaque() const { return m_contentsOpaque; }
    float contentsScale() const { return m_contentsScale; }

private:
    SkiaRecordingResult(sk_sp<SkPicture>&&, SkiaImageToFenceMap&&, const IntRect& recordRect, RenderingMode, bool contentsOpaque, float contentsScale);

    sk_sp<SkPicture> m_picture;
    SkiaImageToFenceMap m_imageToFenceMap WTF_GUARDED_BY_LOCK(m_imageToFenceMapLock);
    Lock m_imageToFenceMapLock;
    IntRect m_recordRect;
    RenderingMode m_renderingMode { RenderingMode::Unaccelerated };
    bool m_contentsOpaque : 1 { true };
    float m_contentsScale { 0 };
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
