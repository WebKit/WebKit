/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Metrological Group B.V.
 * Copyright (C) 2015, 2016 Igalia, S.L
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
#include "MediaSourcePrivate.h"

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/LoggerHelper.h>

typedef struct _WebKitMediaSrc WebKitMediaSrc;

namespace WebCore {

class SourceBufferPrivateGStreamer;
class MediaSourceClientGStreamerMSE;
class MediaPlayerPrivateGStreamerMSE;
class PlatformTimeRanges;

class MediaSourcePrivateGStreamer final : public MediaSourcePrivate
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static void open(MediaSourcePrivateClient&, MediaPlayerPrivateGStreamerMSE&);
    virtual ~MediaSourcePrivateGStreamer();

    MediaSourceClientGStreamerMSE& client() { return m_client.get(); }
    AddStatus addSourceBuffer(const ContentType&, RefPtr<SourceBufferPrivate>&) override;
    void removeSourceBuffer(SourceBufferPrivate*);

    void durationChanged() override;
    void markEndOfStream(EndOfStreamStatus) override;
    void unmarkEndOfStream() override;

    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState) override;

    void waitForSeekCompleted() override;
    void seekCompleted() override;

    void sourceBufferPrivateDidChangeActiveState(SourceBufferPrivateGStreamer*, bool);

    std::unique_ptr<PlatformTimeRanges> buffered();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger; }
    const char* logClassName() const override { return "MediaSourcePrivateGStreamer"; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    const void* nextSourceBufferLogIdentifier() { return childLogIdentifier(m_logIdentifier, ++m_nextSourceBufferID); }
#endif

private:
    MediaSourcePrivateGStreamer(MediaSourcePrivateClient&, MediaPlayerPrivateGStreamerMSE&);

    HashSet<RefPtr<SourceBufferPrivateGStreamer>> m_sourceBuffers;
    HashSet<SourceBufferPrivateGStreamer*> m_activeSourceBuffers;
    Ref<MediaSourceClientGStreamerMSE> m_client;
    Ref<MediaSourcePrivateClient> m_mediaSource;
    MediaPlayerPrivateGStreamerMSE& m_playerPrivate;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
    uint64_t m_nextSourceBufferID { 0 };
#endif
};

}

#endif
