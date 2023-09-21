/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "MediaCanStartListener.h"
#include "MediaProducer.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrack.h"
#include "ScriptWrappable.h"
#include "Timer.h"
#include "URLRegistry.h"
#include <wtf/LoggerHelper.h>
#include <wtf/RefPtr.h>
#include <wtf/RobinHoodHashMap.h>

namespace WebCore {

class Document;

class MediaStream final
    : public EventTarget
    , public ActiveDOMObject
    , public MediaStreamPrivate::Observer
    , private MediaCanStartListener
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
    , public RefCounted<MediaStream> {
    WTF_MAKE_ISO_ALLOCATED(MediaStream);
public:
    static Ref<MediaStream> create(Document&);
    static Ref<MediaStream> create(Document&, MediaStream&);
    static Ref<MediaStream> create(Document&, const Vector<RefPtr<MediaStreamTrack>>&);
    static Ref<MediaStream> create(Document&, Ref<MediaStreamPrivate>&&);
    virtual ~MediaStream();

    String id() const { return m_private->id(); }

    void addTrack(MediaStreamTrack&);
    void removeTrack(MediaStreamTrack&);
    MediaStreamTrack* getTrackById(String);

    MediaStreamTrack* getFirstAudioTrack() const;
    MediaStreamTrack* getFirstVideoTrack() const;

    MediaStreamTrackVector getAudioTracks() const;
    MediaStreamTrackVector getVideoTracks() const;
    MediaStreamTrackVector getTracks() const;

    RefPtr<MediaStream> clone();

    using MediaStreamPrivate::Observer::weakPtrFactory;
    using MediaStreamPrivate::Observer::WeakValueType;
    using MediaStreamPrivate::Observer::WeakPtrImplType;

    bool active() const { return m_isActive; }
    bool muted() const { return m_private->muted(); }

    template<typename Function> bool hasMatchingTrack(Function&& function) const { return anyOf(m_trackMap.values(), std::forward<Function>(function)); }

    MediaStreamPrivate& privateStream() { return m_private.get(); }

    void startProducingData();
    void stopProducingData();

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return MediaStreamEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    using RefCounted<MediaStream>::ref;
    using RefCounted<MediaStream>::deref;

    void addTrackFromPlatform(Ref<MediaStreamTrack>&&);

#if !RELEASE_LOG_DISABLED
    const void* logIdentifier() const final { return m_private->logIdentifier(); }
#endif

protected:
    MediaStream(Document&, const Vector<Ref<MediaStreamTrack>>&);
    MediaStream(Document&, Ref<MediaStreamPrivate>&&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_private->logger(); }
    WTFLogChannel& logChannel() const final;
    const char* logClassName() const final { return "MediaStream"; }
#endif

private:
    void internalAddTrack(Ref<MediaStreamTrack>&&);
    WEBCORE_EXPORT RefPtr<MediaStreamTrack> internalTakeTrack(const String&);

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // MediaStreamPrivate::Observer
    void activeStatusChanged() final;
    void didAddTrack(MediaStreamTrackPrivate&) final;
    void didRemoveTrack(MediaStreamTrackPrivate&) final;
    void characteristicsChanged() final;

    MediaProducerMediaStateFlags mediaState() const;

    // MediaCanStartListener
    void mediaCanStart(Document&) final;

    // ActiveDOMObject API.
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    void updateActiveState();
    void activityEventTimerFired();
    void setIsActive(bool);
    void statusDidChange();

    MediaStreamTrackVector filteredTracks(const Function<bool(const MediaStreamTrack&)>&) const;

    Document* document() const;

    Ref<MediaStreamPrivate> m_private;

    MemoryCompactRobinHoodHashMap<String, Ref<MediaStreamTrack>> m_trackMap;

    MediaProducerMediaStateFlags m_state;

    bool m_isActive { false };
    bool m_isProducingData { false };
    bool m_isWaitingUntilMediaCanStart { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
