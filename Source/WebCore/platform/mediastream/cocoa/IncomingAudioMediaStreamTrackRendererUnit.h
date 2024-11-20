/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)

#include "BaseAudioMediaStreamTrackRendererUnit.h"
#include "CAAudioStreamDescription.h"
#include "LibWebRTCAudioModule.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/LoggerHelper.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class AudioMediaStreamTrackRendererInternalUnit;
class AudioSampleDataSource;
class AudioSampleBufferList;
class CAAudioStreamDescription;
class WebAudioBufferList;

class IncomingAudioMediaStreamTrackRendererUnit : public BaseAudioMediaStreamTrackRendererUnit
#if !RELEASE_LOG_DISABLED
    , public LoggerHelper
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(IncomingAudioMediaStreamTrackRendererUnit);
public:
    explicit IncomingAudioMediaStreamTrackRendererUnit(LibWebRTCAudioModule&);
    ~IncomingAudioMediaStreamTrackRendererUnit();

    void newAudioChunkPushed(uint64_t);

    void ref() { m_audioModule.get()->ref(); };
    void deref() { m_audioModule.get()->deref(); };

private:
    struct Mixer;
    void start(Mixer&);
    void stop(Mixer&);
    void postTask(Function<void()>&&);
    void renderAudioChunk(uint64_t currentAudioSampleCount);

    // BaseAudioMediaStreamTrackRendererUnit
    void addResetObserver(const String&, ResetObserver&) final;
    void addSource(const String&, Ref<AudioSampleDataSource>&&) final;
    void removeSource(const String&, AudioSampleDataSource&) final;

    std::pair<bool, Vector<Ref<AudioSampleDataSource>>> addSourceToMixer(const String&, Ref<AudioSampleDataSource>&&);
    std::pair<bool, Vector<Ref<AudioSampleDataSource>>> removeSourceFromMixer(const String&, AudioSampleDataSource&);

#if !RELEASE_LOG_DISABLED
    // LoggerHelper.
    const Logger& logger() const final;
    ASCIILiteral logClassName() const final { return "IncomingAudioMediaStreamTrackRendererUnit"_s; }
    WTFLogChannel& logChannel() const final;
    uint64_t logIdentifier() const final;
#endif

    const ThreadSafeWeakPtr<LibWebRTCAudioModule> m_audioModule;
    const Ref<WTF::WorkQueue> m_queue;

    struct Mixer {
        HashSet<Ref<AudioSampleDataSource>> sources;
        RefPtr<AudioSampleDataSource> registeredMixedSource;
        String deviceID;
    };

    struct RenderMixer {
        Vector<Ref<AudioSampleDataSource>> inputSources;
        RefPtr<AudioSampleDataSource> mixedSource;
        size_t writeCount { 0 };
    };

    HashMap<String, Mixer> m_mixers WTF_GUARDED_BY_CAPABILITY(mainThread);

    // Background thread variables.
    HashMap<String, RenderMixer> m_renderMixers WTF_GUARDED_BY_CAPABILITY(m_queue.get());

    std::optional<CAAudioStreamDescription> m_outputStreamDescription WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    std::unique_ptr<WebAudioBufferList> m_audioBufferList WTF_GUARDED_BY_CAPABILITY(m_queue.get());
    size_t m_sampleCount { 0 };

#if !RELEASE_LOG_DISABLED
    RefPtr<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

}

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)
