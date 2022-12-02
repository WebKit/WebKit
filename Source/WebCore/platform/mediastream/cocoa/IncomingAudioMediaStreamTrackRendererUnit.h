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

#if ENABLE(MEDIA_STREAM)

#include "BaseAudioMediaStreamTrackRendererUnit.h"
#include "CAAudioStreamDescription.h"
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/LoggerHelper.h>
#include <wtf/Vector.h>

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class AudioMediaStreamTrackRendererInternalUnit;
class AudioSampleDataSource;
class AudioSampleBufferList;
class CAAudioStreamDescription;
class LibWebRTCAudioModule;
class WebAudioBufferList;

class IncomingAudioMediaStreamTrackRendererUnit : public BaseAudioMediaStreamTrackRendererUnit
#if !RELEASE_LOG_DISABLED
    , public LoggerHelper
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit IncomingAudioMediaStreamTrackRendererUnit(LibWebRTCAudioModule&);
    ~IncomingAudioMediaStreamTrackRendererUnit();

    void newAudioChunkPushed(uint64_t);

private:
    void start();
    void stop();
    void postTask(Function<void()>&&);
    void renderAudioChunk(uint64_t currentAudioSampleCount);

    // BaseAudioMediaStreamTrackRendererUnit
    void setAudioOutputDevice(const String&) final;
    void addResetObserver(ResetObserver&) final;
    void addSource(Ref<AudioSampleDataSource>&&) final;
    void removeSource(AudioSampleDataSource&) final;

#if !RELEASE_LOG_DISABLED
    // LoggerHelper.
    const Logger& logger() const final;
    const char* logClassName() const final { return "IncomingAudioMediaStreamTrackRendererUnit"; }
    WTFLogChannel& logChannel() const final;
    const void* logIdentifier() const final;
#endif

    // Main thread variables.
    LibWebRTCAudioModule& m_audioModule;
    Ref<WTF::WorkQueue> m_queue;
    HashSet<Ref<AudioSampleDataSource>> m_sources;
    RefPtr<AudioSampleDataSource> m_registeredMixedSource;

    // Background thread variables.
    Vector<Ref<AudioSampleDataSource>> m_renderSources;
    RefPtr<AudioSampleDataSource> m_mixedSource;
    std::optional<CAAudioStreamDescription> m_outputStreamDescription;
    std::unique_ptr<WebAudioBufferList> m_audioBufferList;
    size_t m_sampleCount { 0 };
    size_t m_writeCount { 0 };

#if !RELEASE_LOG_DISABLED
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

}

#endif // ENABLE(MEDIA_STREAM)
