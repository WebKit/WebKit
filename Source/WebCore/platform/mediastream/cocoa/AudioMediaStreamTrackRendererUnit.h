/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "AudioMediaStreamTrackRendererInternalUnit.h"
#include "BaseAudioMediaStreamTrackRendererUnit.h"
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class AudioSampleDataSource;
class AudioSampleBufferList;
class CAAudioStreamDescription;
class AudioMediaStreamTrackRendererInternalUnit;

class AudioMediaStreamTrackRendererUnit : public BaseAudioMediaStreamTrackRendererUnit, public CanMakeWeakPtr<AudioMediaStreamTrackRendererUnit, WeakPtrFactoryInitialization::Eager> {
public:
    WEBCORE_EXPORT static AudioMediaStreamTrackRendererUnit& singleton();

    AudioMediaStreamTrackRendererUnit();
    ~AudioMediaStreamTrackRendererUnit();

    using CreateInternalUnitFunction = Function<UniqueRef<AudioMediaStreamTrackRendererInternalUnit>(AudioMediaStreamTrackRendererInternalUnit::RenderCallback&&, AudioMediaStreamTrackRendererInternalUnit::ResetCallback&&)>;
    WEBCORE_EXPORT static void setCreateInternalUnitFunction(CreateInternalUnitFunction&&);

    WEBCORE_EXPORT void render(size_t sampleCount, AudioBufferList&, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags&);
    void reset();

    void retrieveFormatDescription(CompletionHandler<void(const CAAudioStreamDescription*)>&&);

    // BaseAudioMediaStreamTrackRendererUnit
    void setAudioOutputDevice(const String&) final;
    void addSource(Ref<AudioSampleDataSource>&&) final;
    void removeSource(AudioSampleDataSource&) final;
    void addResetObserver(ResetObserver& observer) final { m_resetObservers.add(observer); }

private:
    void start();
    void stop();

    void createAudioUnitIfNeeded();
    void updateRenderSourcesIfNecessary();

    HashSet<Ref<AudioSampleDataSource>> m_sources;
    Vector<Ref<AudioSampleDataSource>> m_pendingRenderSources WTF_GUARDED_BY_LOCK(m_pendingRenderSourcesLock);
    Vector<Ref<AudioSampleDataSource>> m_renderSources;
    bool m_hasPendingRenderSources WTF_GUARDED_BY_LOCK(m_pendingRenderSourcesLock) { false };
    Lock m_pendingRenderSourcesLock;
    UniqueRef<AudioMediaStreamTrackRendererInternalUnit> m_internalUnit;
    WeakHashSet<ResetObserver> m_resetObservers;
};

}

#endif // ENABLE(MEDIA_STREAM)
