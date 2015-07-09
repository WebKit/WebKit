/*
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
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

#include "MediaStreamTrack.h"
#include "MediaStreamTrackPrivate.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class MediaStreamTrackPrivate;

class MediaStreamPrivateClient : public RefCounted<MediaStreamPrivateClient> {
public:
    virtual ~MediaStreamPrivateClient() { }

    virtual void activeStatusChanged() = 0;
    virtual void didAddTrackToPrivate(MediaStreamTrackPrivate&) = 0;
    virtual void didRemoveTrackFromPrivate(MediaStreamTrackPrivate&) = 0;
    virtual Vector<RefPtr<MediaStreamTrack>> getVideoTracks() = 0;
    virtual Vector<RefPtr<MediaStreamTrack>> getAudioTracks() = 0;
};

class MediaStreamPrivate : public RefCounted<MediaStreamPrivate> {
public:
    static RefPtr<MediaStreamPrivate> create(const Vector<RefPtr<RealtimeMediaSource>>& audioSources, const Vector<RefPtr<RealtimeMediaSource>>& videoSources);
    static RefPtr<MediaStreamPrivate> create(const Vector<RefPtr<MediaStreamTrackPrivate>>&);
    static RefPtr<MediaStreamPrivate> create();

    virtual ~MediaStreamPrivate() { }

    enum class NotifyClientOption { Notify, DontNotify };

    MediaStreamPrivateClient* client() const { return m_client; }
    void setClient(MediaStreamPrivateClient* client) { m_client = client; }

    String id() const { return m_id; }

    Vector<RefPtr<MediaStreamTrackPrivate>> tracks() const;

    bool active() const { return m_isActive; }
    void updateActiveState(NotifyClientOption);

    void addTrack(RefPtr<MediaStreamTrackPrivate>&&, NotifyClientOption);
    void removeTrack(MediaStreamTrackPrivate&, NotifyClientOption);

private:
    MediaStreamPrivate(const String& id, const Vector<RefPtr<MediaStreamTrackPrivate>>&);

    MediaStreamPrivateClient* m_client;
    String m_id;
    bool m_isActive;

    HashMap<String, RefPtr<MediaStreamTrackPrivate>> m_trackSet;
};

typedef Vector<RefPtr<MediaStreamPrivate>> MediaStreamPrivateVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamPrivate_h
