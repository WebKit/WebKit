/*
 *  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#include "MediaStreamSource.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class MediaStreamTrackPrivateClient {
public:
    virtual void trackReadyStateChanged() = 0;
    virtual void trackMutedChanged() = 0;
};

class MediaStreamTrackPrivate : public RefCounted<MediaStreamTrackPrivate> {
public:
    static PassRefPtr<MediaStreamTrackPrivate> create(PassRefPtr<MediaStreamSource>);
    virtual ~MediaStreamTrackPrivate() { }

    const AtomicString& kind() const;
    const String& id() const;
    const String& label() const;

    bool ended() const;

    bool muted() const;
    void setMuted(bool);

    bool readonly() const;
    bool remote() const;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool);

    void setReadyState(MediaStreamSource::ReadyState);
    MediaStreamSource::ReadyState readyState() const;

    RefPtr<MediaStreamTrackPrivate> clone();

    MediaStreamSource* source() const { return m_source.get(); }
    void setSource(MediaStreamSource*);

    void stop() { m_stopped = true; }
    bool stopped() const { return m_stopped; }

    MediaStreamSource::Type type() const { return m_type; }

    void setClient(MediaStreamTrackPrivateClient* client) { m_client = client; }
protected:
    explicit MediaStreamTrackPrivate(MediaStreamTrackPrivate*);
    MediaStreamTrackPrivate(MediaStreamSource*);

private:
    bool shouldFireTrackReadyStateChanged(MediaStreamSource::ReadyState);
    RefPtr<MediaStreamSource> m_source;
    MediaStreamSource::ReadyState m_readyState;
    mutable String m_id;

    bool m_muted;
    bool m_enabled;
    bool m_stopped;
    MediaStreamSource::Type m_type;
    MediaStreamTrackPrivateClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamTrackPrivate_h
