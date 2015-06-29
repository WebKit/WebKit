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

#ifndef MediaStreamTrackPrivate_h
#define MediaStreamTrackPrivate_h

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSource.h"
#include <wtf/RefCounted.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class MediaSourceStates;
class RealtimeMediaSourceCapabilities;

class MediaStreamTrackPrivateClient {
public:
    virtual ~MediaStreamTrackPrivateClient() { }

    virtual void trackEnded() = 0;
    virtual void trackMutedChanged() = 0;
};

class MediaStreamTrackPrivate : public RefCounted<MediaStreamTrackPrivate>, public RealtimeMediaSource::Observer {
public:
    static RefPtr<MediaStreamTrackPrivate> create(RefPtr<RealtimeMediaSource>&&);
    static RefPtr<MediaStreamTrackPrivate> create(RefPtr<RealtimeMediaSource>&&, const String& id);

    virtual ~MediaStreamTrackPrivate();

    const String& id() const { return m_id; }
    const String& label() const;

    bool ended() const { return m_isEnded; }

    bool muted() const;

    bool readonly() const;
    bool remote() const;

    bool enabled() const { return m_isEnabled; }
    void setEnabled(bool);

    RefPtr<MediaStreamTrackPrivate> clone();

    RealtimeMediaSource* source() const { return m_source.get(); }
    RealtimeMediaSource::Type type() const;

    void endTrack();

    void setClient(MediaStreamTrackPrivateClient* client) { m_client = client; }

    const RealtimeMediaSourceStates& states() const;
    RefPtr<RealtimeMediaSourceCapabilities> capabilities() const;

    RefPtr<MediaConstraints> constraints() const;
    void applyConstraints(const MediaConstraints&);

    void configureTrackRendering();

private:
    explicit MediaStreamTrackPrivate(const MediaStreamTrackPrivate&);
    MediaStreamTrackPrivate(RefPtr<RealtimeMediaSource>&&, const String& id);

    MediaStreamTrackPrivateClient* client() const { return m_client; }

    // RealtimeMediaSourceObserver
    virtual void sourceStopped() override final;
    virtual void sourceMutedChanged() override final;
    virtual bool preventSourceFromStopping() override final;
    
    RefPtr<RealtimeMediaSource> m_source;
    MediaStreamTrackPrivateClient* m_client;
    RefPtr<MediaConstraints> m_constraints;

    String m_id;
    bool m_isEnabled;
    bool m_isEnded;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamTrackPrivate_h
