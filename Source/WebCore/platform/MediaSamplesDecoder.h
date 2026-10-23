/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/MediaSamplesBlock.h>
#include <WebCore/PlatformMediaError.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/NativePromise.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

class AudioDecoder;
class AudioInfo;
class PlatformRawAudioData;
class TrackInfo;

class MediaSamplesDecoder final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaSamplesDecoder> {
    WTF_MAKE_TZONE_ALLOCATED(MediaSamplesDecoder);
public:
    enum class Filter : uint8_t {
        InProcess = 1 << 0,
        SoftwareAllowed = 1 << 1,
        HardwareAllowed = 1 << 2,
        UseSharedMemory = 1 << 3,
    };
    using Filters = OptionSet<Filter>;

    using CreatePromise = NativePromise<Ref<MediaSamplesDecoder>, PlatformMediaError>;
    using DecodePromise = NativePromise<Vector<MediaSamplesBlock>, PlatformMediaError>;

    WEBCORE_EXPORT static bool canDecode(const TrackInfo&, Filters);
    WEBCORE_EXPORT static Ref<CreatePromise> create(const TrackInfo&, Filters);
    WEBCORE_EXPORT ~MediaSamplesDecoder();

    WEBCORE_EXPORT bool canContinueWith(const TrackInfo&) const;

    WEBCORE_EXPORT Ref<DecodePromise> decode(MediaSamplesBlock&&);
    WEBCORE_EXPORT Ref<DecodePromise> drain();
    WEBCORE_EXPORT Ref<MediaPromise> flush();

private:
    MediaSamplesDecoder(Ref<const TrackInfo>&& inputInfo, Ref<const TrackInfo>&& outputInfo, Filters);

    template<typename PromiseType> Ref<PromiseType> enqueueOperation(Function<Ref<PromiseType>()>&&);
    static Ref<CreatePromise> createAudio(const AudioInfo&, Filters);

    Ref<DecodePromise> decodeOnQueue(MediaSamplesBlock&&);
    Ref<DecodePromise> drainOnQueue();
    Ref<MediaPromise> flushOnQueue();
    void didDecodeAudio(Ref<PlatformRawAudioData>&&);
    void didFailDecoding();
    Ref<DecodePromise> settleWithPendingOutput(bool succeeded, uint32_t generation);
    Ref<MediaPromise> finishFlush(bool succeeded);

    const Ref<WorkQueue> m_queue;
    const Ref<const TrackInfo> m_inputInfo;
    const Ref<const TrackInfo> m_outputInfo;
    const Filters m_filters;
    RefPtr<AudioDecoder> m_audioDecoder WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    MediaSamplesBlock::SamplesVector m_pendingOutput WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    Deque<std::pair<MediaTime, MediaTime>> m_trimmedRanges WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    RefPtr<GenericPromise> m_lastOperation WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    uint32_t m_generation WTF_GUARDED_BY_CAPABILITY(m_queue.get()) { 0 };
    std::optional<PlatformMediaError> m_error WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    bool m_needsSyncSample WTF_GUARDED_BY_CAPABILITY(m_queue.get()) { true };
    std::optional<bool> m_discontinuity WTF_GUARDED_BY_CAPABILITY(m_queue.get());
};

} // namespace WebCore
