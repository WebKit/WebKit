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
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/MediaTime.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer *CVPixelBufferRef;
typedef struct __CVBuffer *CVImageBufferRef;
typedef UInt32 VTDecodeInfoFlags;
typedef struct OpaqueVTDecompressionSession*  VTDecompressionSessionRef;

namespace WebCore {

class VideoDecoder;
struct PlatformVideoColorSpace;

class WebCoreDecompressionSession : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<WebCoreDecompressionSession> {
public:
    static Ref<WebCoreDecompressionSession> createOpenGL() { return adoptRef(*new WebCoreDecompressionSession(OpenGL)); }
    static Ref<WebCoreDecompressionSession> createRGB() { return adoptRef(*new WebCoreDecompressionSession(RGB)); }

    WEBCORE_EXPORT ~WebCoreDecompressionSession();
    WEBCORE_EXPORT void invalidate();

    WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> decodeSampleSync(CMSampleBufferRef);

    using DecodingPromise = NativePromise<RetainPtr<CMSampleBufferRef>, OSStatus>;
    WEBCORE_EXPORT Ref<DecodingPromise> decodeSample(CMSampleBufferRef, bool displaying);
    WEBCORE_EXPORT void flush();

    void setResourceOwner(const ProcessIdentity& resourceOwner) { m_resourceOwner = resourceOwner; }

private:
    enum Mode {
        OpenGL,
        RGB,
    };
    WEBCORE_EXPORT explicit WebCoreDecompressionSession(Mode);

    RetainPtr<VTDecompressionSessionRef> ensureDecompressionSessionForSample(CMSampleBufferRef);

    Ref<DecodingPromise> decodeSampleInternal(CMSampleBufferRef, bool displaying);
    void handleDecompressionOutput(bool displaying, OSStatus, VTDecodeInfoFlags, CVImageBufferRef, CMTime presentationTimeStamp, CMTime presentationDuration);
    void assignResourceOwner(CVImageBufferRef);

    Ref<MediaPromise> initializeVideoDecoder(FourCharCode, std::span<const uint8_t>, const std::optional<PlatformVideoColorSpace>&);
    bool isInvalidated() const { return m_invalidated; }

    Mode m_mode;
    mutable Lock m_lock;
    RetainPtr<VTDecompressionSessionRef> m_decompressionSession WTF_GUARDED_BY_LOCK(m_lock);
    const Ref<WorkQueue> m_decompressionQueue;

    std::atomic<uint32_t> m_flushId { 0 };
    RefPtr<VideoDecoder> m_videoDecoder WTF_GUARDED_BY_LOCK(m_lock);
    bool m_videoDecoderCreationFailed { false };
    struct PendingDecodeData {
        bool displaying { false };
    };
    std::optional<PendingDecodeData> m_pendingDecodeData WTF_GUARDED_BY_CAPABILITY(m_decompressionQueue.get());
    RetainPtr<CMSampleBufferRef> m_lastDecodedSample WTF_GUARDED_BY_CAPABILITY(m_decompressionQueue.get());
    OSStatus m_lastDecodingError WTF_GUARDED_BY_CAPABILITY(m_decompressionQueue.get()) { noErr };

    std::atomic<bool> m_invalidated { false };

    ProcessIdentity m_resourceOwner;
};

}
