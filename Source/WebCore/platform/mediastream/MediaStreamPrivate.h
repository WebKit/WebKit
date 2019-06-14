/*
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamPrivate_h
#define MediaStreamPrivate_h

#if ENABLE(MEDIA_STREAM)

#include "FloatSize.h"
#include "MediaStreamTrack.h"
#include "MediaStreamTrackPrivate.h"
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/UUID.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaStream;
class OrientationNotifier;

class MediaStreamPrivate final
    : public MediaStreamTrackPrivate::Observer
    , public RefCounted<MediaStreamPrivate>
    , public CanMakeWeakPtr<MediaStreamPrivate>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    class Observer {
    public:
        virtual ~Observer() = default;

        virtual void characteristicsChanged() { }
        virtual void activeStatusChanged() { }
        virtual void didAddTrack(MediaStreamTrackPrivate&) { }
        virtual void didRemoveTrack(MediaStreamTrackPrivate&) { }
    };

    static Ref<MediaStreamPrivate> create(Ref<const Logger>&&, Ref<RealtimeMediaSource>&&);
    static Ref<MediaStreamPrivate> create(Ref<const Logger>&&, const Vector<Ref<RealtimeMediaSource>>& audioSources, const Vector<Ref<RealtimeMediaSource>>& videoSources);
    static Ref<MediaStreamPrivate> create(Ref<const Logger>&& logger, const MediaStreamTrackPrivateVector& tracks, String&& id = createCanonicalUUIDString()) { return adoptRef(*new MediaStreamPrivate(WTFMove(logger), tracks, WTFMove(id))); }

    virtual ~MediaStreamPrivate();

    enum class NotifyClientOption { Notify, DontNotify };

    void addObserver(Observer&);
    void removeObserver(Observer&);

    String id() const { return m_id; }

    MediaStreamTrackPrivateVector tracks() const;
    MediaStreamTrackPrivate* activeVideoTrack() { return m_activeVideoTrack; }

    bool active() const { return m_isActive; }
    void updateActiveState(NotifyClientOption);

    void addTrack(RefPtr<MediaStreamTrackPrivate>&&, NotifyClientOption = NotifyClientOption::Notify);
    void removeTrack(MediaStreamTrackPrivate&, NotifyClientOption = NotifyClientOption::Notify);

    void startProducingData();
    void stopProducingData();
    bool isProducingData() const;

    bool hasVideo() const;
    bool hasAudio() const;
    bool muted() const;

    bool hasCaptureVideoSource() const;
    bool hasCaptureAudioSource() const;

    FloatSize intrinsicSize() const;

    void monitorOrientation(OrientationNotifier&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    const void* logIdentifier() const final { return m_logIdentifier; }
#endif

private:
    MediaStreamPrivate(Ref<const Logger>&&, const MediaStreamTrackPrivateVector&, String&&);

    // MediaStreamTrackPrivate::Observer
    void trackStarted(MediaStreamTrackPrivate&) override;
    void trackEnded(MediaStreamTrackPrivate&) override;
    void trackMutedChanged(MediaStreamTrackPrivate&) override;
    void trackSettingsChanged(MediaStreamTrackPrivate&) override;
    void trackEnabledChanged(MediaStreamTrackPrivate&) override;

    void characteristicsChanged();
    void updateActiveVideoTrack();

    void scheduleDeferredTask(Function<void ()>&&);
    void forEachObserver(const WTF::Function<void(Observer&)>&) const;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "MediaStreamPrivate"; }
    WTFLogChannel& logChannel() const final;
#endif

    HashSet<Observer*> m_observers;
    String m_id;
    MediaStreamTrackPrivate* m_activeVideoTrack { nullptr };
    HashMap<String, RefPtr<MediaStreamTrackPrivate>> m_trackSet;
    bool m_isActive { false };
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

typedef Vector<RefPtr<MediaStreamPrivate>> MediaStreamPrivateVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamPrivate_h
