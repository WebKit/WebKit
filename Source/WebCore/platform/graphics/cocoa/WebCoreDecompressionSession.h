/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "MediaPromiseTypes.h"

#include "ProcessIdentity.h"
#include <CoreMedia/CMTime.h>
#include <atomic>
#include <wtf/Expected.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/MediaTime.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer *CVPixelBufferRef;
typedef struct __CVBuffer *CVImageBufferRef;
typedef struct OpaqueCMTaggedBufferGroup *CMTaggedBufferGroupRef;
typedef UInt32 VTDecodeInfoFlags;
typedef struct OpaqueVTDecompressionSession*  VTDecompressionSessionRef;

namespace WebCore {

class VideoDecoder;
struct PlatformVideoColorSpace;

class WebCoreDecompressionSession : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<WebCoreDecompressionSession> {
public:
    WEBCORE_EXPORT static Ref<WebCoreDecompressionSession> createOpenGL();
    WEBCORE_EXPORT static Ref<WebCoreDecompressionSession> createRGB();
    static Ref<WebCoreDecompressionSession> create(NSDictionary *pixelBufferAttributes) { return adoptRef(*new WebCoreDecompressionSession(pixelBufferAttributes)); }

    WEBCORE_EXPORT ~WebCoreDecompressionSession();
    WEBCORE_EXPORT void invalidate();

    WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> decodeSampleSync(CMSampleBufferRef);

    using DecodingPromise = NativePromise<Vector<RetainPtr<CMSampleBufferRef>>, OSStatus>;
    enum class DecodingFlag : uint8_t {
        NonDisplaying = 1 << 0,
        RealTime = 1 << 1,
        EnableStereo = 1 << 2,
    };
    using DecodingFlags = OptionSet<DecodingFlag>;

    WEBCORE_EXPORT Ref<DecodingPromise> decodeSample(CMSampleBufferRef, DecodingFlags);
    WEBCORE_EXPORT void flush();

    void setResourceOwner(const ProcessIdentity& resourceOwner) { m_resourceOwner = resourceOwner; }
    bool isHardwareAccelerated() const;

private:
    WEBCORE_EXPORT WebCoreDecompressionSession(NSDictionary *);
    static NSDictionary *defaultPixelBufferAttributes();

    Expected<RetainPtr<VTDecompressionSessionRef>, OSStatus> ensureDecompressionSessionForSample(CMSampleBufferRef);

    Ref<DecodingPromise> decodeSampleInternal(CMSampleBufferRef, DecodingFlags);
    void assignResourceOwner(CVImageBufferRef);

    Ref<MediaPromise> initializeVideoDecoder(FourCharCode, std::span<const uint8_t>, const std::optional<PlatformVideoColorSpace>&);
    bool isInvalidated() const { return m_invalidated; }

    static WorkQueue& queueSingleton();
    const RetainPtr<NSDictionary> m_pixelBufferAttributes;

    mutable Lock m_lock;
    RetainPtr<VTDecompressionSessionRef> m_decompressionSession WTF_GUARDED_BY_LOCK(m_lock);
    mutable std::optional<bool> m_isHardwareAccelerated WTF_GUARDED_BY_LOCK(m_lock);

    std::atomic<uint32_t> m_flushId { 0 };
    RefPtr<VideoDecoder> m_videoDecoder WTF_GUARDED_BY_LOCK(m_lock);
    bool m_videoDecoderCreationFailed { false };
    struct PendingDecodeData {
        DecodingFlags flags;
    };
    std::optional<PendingDecodeData> m_pendingDecodeData WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    Vector<RetainPtr<CMSampleBufferRef>> m_lastDecodedSamples WTF_GUARDED_BY_CAPABILITY(queueSingleton());
    OSStatus m_lastDecodingError WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { noErr };
    RetainPtr<CMFormatDescriptionRef> m_currentImageDescription WTF_GUARDED_BY_CAPABILITY(queueSingleton());

    // Stereo playback support
    const bool m_stereoSupported { false };
    bool m_stereoConfigured WTF_GUARDED_BY_CAPABILITY(queueSingleton()) { false };
    RetainPtr<CFArrayRef> m_tagCollections WTF_GUARDED_BY_CAPABILITY(queueSingleton());

    std::atomic<bool> m_invalidated { false };

    ProcessIdentity m_resourceOwner;
};

}
