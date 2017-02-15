/*
 *  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *  Copyright (C) 2015 Ericsson AB. All rights reserved.
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

namespace WebCore {

class AudioSourceProvider;
class GraphicsContext;
class MediaSample;
class RealtimeMediaSourceCapabilities;

class MediaStreamTrackPrivate : public RefCounted<MediaStreamTrackPrivate>, public RealtimeMediaSource::Observer {
public:
    class Observer {
    public:
        virtual ~Observer() { }

        virtual void trackEnded(MediaStreamTrackPrivate&) = 0;
        virtual void trackMutedChanged(MediaStreamTrackPrivate&) = 0;
        virtual void trackSettingsChanged(MediaStreamTrackPrivate&) = 0;
        virtual void trackEnabledChanged(MediaStreamTrackPrivate&) = 0;
        virtual void sampleBufferUpdated(MediaStreamTrackPrivate&, MediaSample&) { };
    };

    static Ref<MediaStreamTrackPrivate> create(Ref<RealtimeMediaSource>&&);
    static Ref<MediaStreamTrackPrivate> create(Ref<RealtimeMediaSource>&&, String&& id);

    virtual ~MediaStreamTrackPrivate();

    const String& id() const { return m_id; }
    const String& label() const;

    bool ended() const { return m_isEnded; }

    void startProducingData() { m_source->startProducingData(); }
    void stopProducingData() { m_source->stopProducingData(); }
    bool isProducingData() { return m_source->isProducingData(); }

    bool muted() const;
    void setMuted(bool muted) { m_source->setMuted(muted); }

    bool readonly() const;
    bool remote() const;

    bool enabled() const { return m_isEnabled; }
    void setEnabled(bool);

    Ref<MediaStreamTrackPrivate> clone();

    RealtimeMediaSource& source() { return m_source.get(); }
    RealtimeMediaSource::Type type() const;

    void endTrack();

    void addObserver(Observer&);
    void removeObserver(Observer&);

    const RealtimeMediaSourceSettings& settings() const;
    RefPtr<RealtimeMediaSourceCapabilities> capabilities() const;

    void applyConstraints(const MediaConstraints&, RealtimeMediaSource::SuccessHandler, RealtimeMediaSource::FailureHandler);

    AudioSourceProvider* audioSourceProvider();

    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&);

private:
    MediaStreamTrackPrivate(Ref<RealtimeMediaSource>&&, String&& id);

    // RealtimeMediaSourceObserver
    void sourceStopped() final;
    void sourceMutedChanged() final;
    void sourceEnabledChanged() final;
    void sourceSettingsChanged() final;
    bool preventSourceFromStopping() final;
    void videoSampleAvailable(MediaSample&) final;

    Vector<Observer*> m_observers;
    Ref<RealtimeMediaSource> m_source;

    String m_id;
    bool m_isEnabled;
    bool m_isEnded;
};

typedef Vector<RefPtr<MediaStreamTrackPrivate>> MediaStreamTrackPrivateVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
