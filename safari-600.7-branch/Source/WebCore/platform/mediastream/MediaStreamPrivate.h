/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#include "MediaStreamSource.h"
#include "MediaStreamTrack.h"
#include "MediaStreamTrackPrivate.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class MediaStreamTrackPrivate;

class MediaStreamPrivateClient : public MediaStreamTrack::Observer {
public:
    virtual ~MediaStreamPrivateClient() { }

    virtual void setStreamIsActive(bool) = 0;
    virtual void addRemoteSource(MediaStreamSource*) = 0;
    virtual void removeRemoteSource(MediaStreamSource*) = 0;
    virtual void addRemoteTrack(MediaStreamTrackPrivate*) = 0;
    virtual void removeRemoteTrack(MediaStreamTrackPrivate*) = 0;
};

class MediaStreamPrivate : public RefCounted<MediaStreamPrivate> {
public:
    static PassRefPtr<MediaStreamPrivate> create(const Vector<RefPtr<MediaStreamSource>>& audioSources, const Vector<RefPtr<MediaStreamSource>>& videoSources);
    static PassRefPtr<MediaStreamPrivate> create(const Vector<RefPtr<MediaStreamTrackPrivate>>& audioPrivateTracks, const Vector<RefPtr<MediaStreamTrackPrivate>>& videoPrivateTracks);

    virtual ~MediaStreamPrivate() { }

    MediaStreamPrivateClient* client() const { return m_client; }
    void setClient(MediaStreamPrivateClient* client) { m_client = client; }

    String id() const { return m_id; }

    unsigned numberOfAudioSources() const { return m_audioStreamSources.size(); }
    MediaStreamSource* audioSources(unsigned index) const { return m_audioStreamSources[index].get(); }

    unsigned numberOfVideoSources() const { return m_videoStreamSources.size(); }
    MediaStreamSource* videoSources(unsigned index) const { return m_videoStreamSources[index].get(); }

    unsigned numberOfAudioTracks() const { return m_audioPrivateTracks.size(); }
    MediaStreamTrackPrivate* audioTracks(unsigned index) const { return m_audioPrivateTracks[index].get(); }

    unsigned numberOfVideoTracks() const { return m_videoPrivateTracks.size(); }
    MediaStreamTrackPrivate* videoTracks(unsigned index) const { return m_videoPrivateTracks[index].get(); }

    void addSource(PassRefPtr<MediaStreamSource>);
    void removeSource(PassRefPtr<MediaStreamSource>);

    void addRemoteSource(MediaStreamSource*);
    void removeRemoteSource(MediaStreamSource*);

    bool active() const { return m_isActive; }
    void setActive(bool);

    void addTrack(PassRefPtr<MediaStreamTrackPrivate>);
    void removeTrack(PassRefPtr<MediaStreamTrackPrivate>);

    void addRemoteTrack(MediaStreamTrackPrivate*);
    void removeRemoteTrack(MediaStreamTrackPrivate*);

private:
    MediaStreamPrivate(const String& id, const Vector<RefPtr<MediaStreamSource>>& audioSources, const Vector<RefPtr<MediaStreamSource>>& videoSources);
    MediaStreamPrivate(const String& id, const Vector<RefPtr<MediaStreamTrackPrivate>>& audioPrivateTracks, const Vector<RefPtr<MediaStreamTrackPrivate>>& videoPrivateTracks);

    MediaStreamPrivateClient* m_client;
    String m_id;
    Vector<RefPtr<MediaStreamSource>> m_audioStreamSources;
    Vector<RefPtr<MediaStreamSource>> m_videoStreamSources;

    Vector<RefPtr<MediaStreamTrackPrivate>> m_audioPrivateTracks;
    Vector<RefPtr<MediaStreamTrackPrivate>> m_videoPrivateTracks;
    bool m_isActive;
};

typedef Vector<RefPtr<MediaStreamPrivate>> MediaStreamPrivateVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamPrivate_h
