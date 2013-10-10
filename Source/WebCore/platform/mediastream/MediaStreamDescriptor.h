/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef MediaStreamDescriptor_h
#define MediaStreamDescriptor_h

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamSource.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class MediaStreamDescriptorClient {
public:
    virtual ~MediaStreamDescriptorClient() { }

    virtual void trackDidEnd() = 0;
    virtual void streamDidEnd() = 0;
    virtual void addRemoteSource(MediaStreamSource*) = 0;
    virtual void removeRemoteSource(MediaStreamSource*) = 0;
};

class MediaStreamDescriptor : public RefCounted<MediaStreamDescriptor> {
public:
    enum EndedAtCreationFlag { IsNotEnded, IsEnded };

    static PassRefPtr<MediaStreamDescriptor> create(const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources, EndedAtCreationFlag);

    virtual ~MediaStreamDescriptor();

    MediaStreamDescriptorClient* client() const { return m_client; }
    void setClient(MediaStreamDescriptorClient* client) { m_client = client; }

    String id() const { return m_id; }

    unsigned numberOfAudioStreams() const { return m_audioStreamSources.size(); }
    MediaStreamSource* audioStreams(unsigned index) const { return m_audioStreamSources[index].get(); }

    unsigned numberOfVideoStreams() const { return m_videoStreamSources.size(); }
    MediaStreamSource* videoStreams(unsigned index) const { return m_videoStreamSources[index].get(); }

    void addSource(PassRefPtr<MediaStreamSource>);
    void removeSource(PassRefPtr<MediaStreamSource>);

    void addRemoteSource(MediaStreamSource*);
    void removeRemoteSource(MediaStreamSource*);

    bool ended() const { return m_ended; }
    void setEnded();

private:
    MediaStreamDescriptor(const String& id, const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources, bool ended);

    MediaStreamDescriptorClient* m_client;
    String m_id;
    Vector<RefPtr<MediaStreamSource> > m_audioStreamSources;
    Vector<RefPtr<MediaStreamSource> > m_videoStreamSources;
    bool m_ended;
};

typedef Vector<RefPtr<MediaStreamDescriptor> > MediaStreamDescriptorVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamDescriptor_h
