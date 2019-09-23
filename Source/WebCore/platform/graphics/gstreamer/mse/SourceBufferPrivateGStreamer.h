/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016, 2017 Metrological Group B.V.
 * Copyright (C) 2015, 2016, 2017 Igalia, S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "ContentType.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "SourceBufferPrivate.h"
#include "SourceBufferPrivateClient.h"
#include "WebKitMediaSourceGStreamer.h"

namespace WebCore {

class MediaSourceGStreamer;

class SourceBufferPrivateGStreamer final : public SourceBufferPrivate {

public:
    static Ref<SourceBufferPrivateGStreamer> create(MediaSourceGStreamer*, Ref<MediaSourceClientGStreamerMSE>, const ContentType&);
    virtual ~SourceBufferPrivateGStreamer() = default;

    void clearMediaSource() { m_mediaSource = nullptr; }

    void setClient(SourceBufferPrivateClient*) final;
    void append(Vector<unsigned char>&&) final;
    void abort() final;
    void resetParserState() final;
    void removedFromMediaSource() final;
    MediaPlayer::ReadyState readyState() const final;
    void setReadyState(MediaPlayer::ReadyState) final;

    void flush(const AtomString&) final;
    void enqueueSample(Ref<MediaSample>&&, const AtomString&) final;
    void allSamplesInTrackEnqueued(const AtomString&) final;
    bool isReadyForMoreSamples(const AtomString&) final;
    void setActive(bool) final;
    void notifyClientWhenReadyForMoreSamples(const AtomString&) final;

    void setReadyForMoreSamples(bool);
    void notifyReadyForMoreSamples();

    void didReceiveInitializationSegment(const SourceBufferPrivateClient::InitializationSegment&);
    void didReceiveSample(MediaSample&);
    void didReceiveAllPendingSamples();
    void appendParsingFailed();

    ContentType type() const { return m_type; }

private:
    SourceBufferPrivateGStreamer(MediaSourceGStreamer*, Ref<MediaSourceClientGStreamerMSE>, const ContentType&);
    friend class MediaSourceClientGStreamerMSE;

    MediaSourceGStreamer* m_mediaSource;
    ContentType m_type;
    Ref<MediaSourceClientGStreamerMSE> m_client;
    SourceBufferPrivateClient* m_sourceBufferPrivateClient { nullptr };
    bool m_isReadyForMoreSamples = true;
    bool m_notifyWhenReadyForMoreSamples = false;
    AtomString m_trackId;
};

}

#endif
