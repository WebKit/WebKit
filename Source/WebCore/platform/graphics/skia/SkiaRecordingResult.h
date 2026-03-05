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
#include "GLFence.h"
#include "IntRect.h"
#include "RenderingMode.h"
#include "SkiaGPUAtlas.h"
#include "SkiaImageAtlasLayout.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
#include <skia/core/SkPicture.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/Condition.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

using SkiaImageToFenceMap = HashMap<const SkImage*, std::unique_ptr<GLFence>>;

class AtlasUploadCondition : public ThreadSafeRefCounted<AtlasUploadCondition> {
public:
    static Ref<AtlasUploadCondition> create() { return adoptRef(*new AtlasUploadCondition); }

    void addPending()
    {
        Locker locker { m_lock };
        ++m_pendingCount;
    }

    void signal()
    {
        Locker locker { m_lock };
        ASSERT(m_pendingCount);
        if (!--m_pendingCount)
            m_condition.notifyAll();
    }

    void wait()
    {
        Locker locker { m_lock };
        m_condition.wait(m_lock, [this]() WTF_REQUIRES_LOCK(m_lock) {
            return !m_pendingCount;
        });
    }

private:
    AtlasUploadCondition() = default;

    Lock m_lock;
    Condition m_condition;
    unsigned m_pendingCount WTF_GUARDED_BY_LOCK(m_lock) { 0 };
};

struct SkiaRecordingData {
    SkiaImageToFenceMap imageToFenceMap;
    Vector<Ref<SkiaImageAtlasLayout>> atlasLayouts;
    unsigned imageSetFingerprint { 0 };
};

class SkiaRecordingResult final : public ThreadSafeRefCounted<SkiaRecordingResult, WTF::DestructionThread::Main> {
public:
    ~SkiaRecordingResult();
    static Ref<SkiaRecordingResult> create(sk_sp<SkPicture>&&, SkiaRecordingData&&, const IntRect& recordRect, RenderingMode, bool contentsOpaque, float contentsScale);

    void waitForFenceIfNeeded(const SkImage&);
    bool hasFences();

    const sk_sp<SkPicture>& picture() const { return m_picture; }
    const IntRect& recordRect() const { return m_recordRect; }
    RenderingMode renderingMode() const { return m_renderingMode; }
    bool contentsOpaque() const { return m_contentsOpaque; }
    float contentsScale() const { return m_contentsScale; }

    // Atlas layouts for batched raster image uploads.
    bool hasAtlasLayouts() const { return !m_atlasLayouts.isEmpty(); }
    const Vector<Ref<SkiaImageAtlasLayout>>& atlasLayouts() const { return m_atlasLayouts; }
    unsigned imageSetFingerprint() const { return m_imageSetFingerprint; }

    // GPU atlases prepared on main thread for worker threads to rewrap.
    void setGPUAtlases(Vector<Ref<SkiaGPUAtlas>>&& atlases, RefPtr<AtlasUploadCondition>&& condition = nullptr)
    {
        m_gpuAtlases = WTF::move(atlases);
        m_uploadCondition = WTF::move(condition);
    }

    // Set the upload fence for async GPU operations (created by SkiaPaintingEngine).
    void setUploadFence(std::unique_ptr<GLFence>&& fence) { m_uploadFence = WTF::move(fence); }

    // Check if GPU atlases are ready.
    bool hasGPUAtlases() const { return !m_gpuAtlases.isEmpty(); }

    // Get prepared GPU atlases (for worker threads to rewrap).
    const Vector<Ref<SkiaGPUAtlas>>& gpuAtlases() const { return m_gpuAtlases; }

    // Wait for GPU upload fence (call from worker threads before using atlases).
    void waitForUploadFence();

    // Wait for async DMA-buf atlas upload to complete.
    void waitForUploadCondition();

private:
    SkiaRecordingResult(sk_sp<SkPicture>&&, SkiaRecordingData&&, const IntRect& recordRect, RenderingMode, bool contentsOpaque, float contentsScale);

    sk_sp<SkPicture> m_picture;
    SkiaImageToFenceMap m_imageToFenceMap WTF_GUARDED_BY_LOCK(m_imageToFenceMapLock);
    Lock m_imageToFenceMapLock;
    Vector<Ref<SkiaImageAtlasLayout>> m_atlasLayouts;
    unsigned m_imageSetFingerprint { 0 };
    Vector<Ref<SkiaGPUAtlas>> m_gpuAtlases;
    std::unique_ptr<GLFence> m_uploadFence; // Fence for async GPU upload
    RefPtr<AtlasUploadCondition> m_uploadCondition;
    IntRect m_recordRect;
    RenderingMode m_renderingMode { RenderingMode::Unaccelerated };
    bool m_contentsOpaque : 1 { true };
    float m_contentsScale { 0 };
};

} // namespace WebCore

#endif // USE(SKIA)
