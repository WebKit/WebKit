/*
 * Copyright (C) 2017 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(LIBWEBRTC)

#include "AudioSampleDataSource.h"
#include "RealtimeOutgoingAudioSource.h"
#include <wtf/CheckedRef.h>

namespace webrtc {
class AudioTrackInterface;
class AudioTrackSinkInterface;
}

namespace WebCore {

class RealtimeOutgoingAudioSourceCocoa final : public RealtimeOutgoingAudioSource, public CanMakeCheckedPtr<RealtimeOutgoingAudioSourceCocoa> {
public:
    static Ref<RealtimeOutgoingAudioSourceCocoa> create(Ref<MediaStreamTrackPrivate>&& audioSource) { return adoptRef(*new RealtimeOutgoingAudioSourceCocoa(WTFMove(audioSource))); }

private:
    explicit RealtimeOutgoingAudioSourceCocoa(Ref<MediaStreamTrackPrivate>&&);
    ~RealtimeOutgoingAudioSourceCocoa();

    // CheckedPtr interface
    uint32_t ptrCount() const final { return CanMakeCheckedPtr::ptrCount(); }
    void incrementPtrCount() const final { CanMakeCheckedPtr::incrementPtrCount(); }
    void decrementPtrCount() const final { CanMakeCheckedPtr::decrementPtrCount(); }
#if CHECKED_POINTER_DEBUG
    void registerCheckedPtr(const void* pointer) const final { CanMakeCheckedPtr::registerCheckedPtr(pointer); };
    void copyCheckedPtr(const void* source, const void* destination) const final { CanMakeCheckedPtr::copyCheckedPtr(source, destination); }
    void moveCheckedPtr(const void* source, const void* destination) const final { CanMakeCheckedPtr::moveCheckedPtr(source, destination); }
    void unregisterCheckedPtr(const void* pointer) const final { CanMakeCheckedPtr::unregisterCheckedPtr(pointer); }
#endif // CHECKED_POINTER_DEBUG

    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;

    bool isReachingBufferedAudioDataHighLimit() final;
    bool isReachingBufferedAudioDataLowLimit() final;
    bool hasBufferedEnoughData() final;
    void sourceUpdated() final;

    void pullAudioData();

    Ref<AudioSampleDataSource> m_sampleConverter;
    std::optional<CAAudioStreamDescription> m_inputStreamDescription;
    std::optional<CAAudioStreamDescription> m_outputStreamDescription;

    Vector<uint8_t> m_audioBuffer;
    uint64_t m_readCount { 0 };
    uint64_t m_writeCount { 0 };
    bool m_skippingAudioData { false };
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
