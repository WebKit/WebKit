/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSource.h"
#include <wtf/LoggerHelper.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class GraphicsContext;
class MediaSample;
class RealtimeMediaSourceCapabilities;
class WebAudioSourceProvider;

class MediaStreamTrackPrivate final
    : public RefCounted<MediaStreamTrackPrivate>
    , public RealtimeMediaSource::Observer
#if !RELEASE_LOG_DISABLED
    , public LoggerHelper
#endif
{
public:
    class Observer : public CanMakeWeakPtr<Observer> {
    public:
        virtual ~Observer() = default;

        virtual void trackStarted(MediaStreamTrackPrivate&) { };
        virtual void trackEnded(MediaStreamTrackPrivate&) = 0;
        virtual void trackMutedChanged(MediaStreamTrackPrivate&) = 0;
        virtual void trackSettingsChanged(MediaStreamTrackPrivate&) = 0;
        virtual void trackEnabledChanged(MediaStreamTrackPrivate&) = 0;
        virtual void readyStateChanged(MediaStreamTrackPrivate&) { };
    };

    static Ref<MediaStreamTrackPrivate> create(Ref<const Logger>&&, Ref<RealtimeMediaSource>&&);
    static Ref<MediaStreamTrackPrivate> create(Ref<const Logger>&&, Ref<RealtimeMediaSource>&&, String&& id);

    WEBCORE_EXPORT virtual ~MediaStreamTrackPrivate();

    const String& id() const { return m_id; }
    const String& label() const;

    bool isActive() const { return enabled() && !ended() && !muted(); }

    bool ended() const { return m_isEnded; }

    enum class HintValue { Empty, Speech, Music, Motion, Detail, Text };
    HintValue contentHint() const { return m_contentHint; }
    void setContentHint(HintValue);
    
    void startProducingData() { m_source->start(); }
    void stopProducingData() { m_source->stop(); }
    bool isProducingData() { return m_source->isProducingData(); }

    bool isIsolated() const { return m_source->isIsolated(); }

    bool muted() const;
    void setMuted(bool muted) { m_source->setMuted(muted); }

    bool isCaptureTrack() const;

    bool enabled() const { return m_isEnabled; }
    void setEnabled(bool);

    Ref<MediaStreamTrackPrivate> clone();

    RealtimeMediaSource& source() { return m_source.get(); }
    const RealtimeMediaSource& source() const { return m_source.get(); }
    WEBCORE_EXPORT RealtimeMediaSource::Type type() const;

    void endTrack();

    void addObserver(Observer&);
    void removeObserver(Observer&);
    bool hasObserver(Observer& observer) const { return m_observers.contains(observer); }

    WEBCORE_EXPORT const RealtimeMediaSourceSettings& settings() const;
    const RealtimeMediaSourceCapabilities& capabilities() const;

    void applyConstraints(const MediaConstraints&, RealtimeMediaSource::ApplyConstraintsHandler&&);

    RefPtr<WebAudioSourceProvider> createAudioSourceProvider();

    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&);

    enum class ReadyState { None, Live, Ended };
    ReadyState readyState() const { return m_readyState; }

    void setIdForTesting(String&& id) { m_id = WTFMove(id); }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    const void* logIdentifier() const final { return m_logIdentifier; }
#endif

private:
    MediaStreamTrackPrivate(Ref<const Logger>&&, Ref<RealtimeMediaSource>&&, String&& id);

    // RealtimeMediaSource::Observer
    void sourceStarted() final;
    void sourceStopped() final;
    void sourceMutedChanged() final;
    void sourceSettingsChanged() final;
    bool preventSourceFromStopping() final;
    void audioUnitWillStart() final;
    void hasStartedProducingData() final;

    void updateReadyState();

    void forEachObserver(const Function<void(Observer&)>&);

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "MediaStreamTrackPrivate"; }
    WTFLogChannel& logChannel() const final;
#endif

    WeakHashSet<Observer> m_observers;
    Ref<RealtimeMediaSource> m_source;

    String m_id;
    ReadyState m_readyState { ReadyState::None };
    bool m_isEnabled { true };
    bool m_isEnded { false };
    bool m_hasStartedProducingData { false };
    HintValue m_contentHint { HintValue::Empty };
    Ref<const Logger> m_logger;
#if !RELEASE_LOG_DISABLED
    const void* m_logIdentifier;
#endif
};

typedef Vector<RefPtr<MediaStreamTrackPrivate>> MediaStreamTrackPrivateVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
