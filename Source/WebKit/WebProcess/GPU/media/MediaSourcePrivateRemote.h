/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "GPUProcessConnection.h"
#include "MessageReceiver.h"
#include "RemoteMediaPlayerMIMETypeCache.h"
#include "RemoteMediaSourceIdentifier.h"
#include <WebCore/ContentType.h>
#include <WebCore/MediaSourcePrivate.h>
#include <WebCore/MediaSourcePrivateClient.h>
#include <WebCore/SourceBufferPrivate.h>
#include <wtf/LoggerHelper.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class MediaPlayerPrivateRemote;
class SourceBufferPrivateRemote;

class MediaSourcePrivateRemote final
    : public WebCore::MediaSourcePrivate
    , public CanMakeWeakPtr<MediaSourcePrivateRemote>
    , private IPC::MessageReceiver
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<MediaSourcePrivateRemote> create(GPUProcessConnection&, RemoteMediaSourceIdentifier, RemoteMediaPlayerMIMETypeCache&, const MediaPlayerPrivateRemote&, WebCore::MediaSourcePrivateClient*);
    virtual ~MediaSourcePrivateRemote();

    // MediaSourcePrivate overrides
    AddStatus addSourceBuffer(const WebCore::ContentType&, bool webMParserEnabled, RefPtr<WebCore::SourceBufferPrivate>&) final;
    void durationChanged(const MediaTime&) final;
    void markEndOfStream(EndOfStreamStatus) final;
    void unmarkEndOfStream() final;
    bool isEnded() const final;
    WebCore::MediaPlayer::ReadyState readyState() const final;
    void setReadyState(WebCore::MediaPlayer::ReadyState) final;
    void setIsSeeking(bool) final;
    void waitForSeekCompleted() final;
    void seekCompleted() final;
    void setTimeFudgeFactor(const MediaTime&) final;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* nextSourceBufferLogIdentifier() { return childLogIdentifier(m_logIdentifier, ++m_nextSourceBufferID); }
#endif

private:
    MediaSourcePrivateRemote(GPUProcessConnection&, RemoteMediaSourceIdentifier, RemoteMediaPlayerMIMETypeCache&, const MediaPlayerPrivateRemote&, WebCore::MediaSourcePrivateClient*);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void seekToTime(const MediaTime&);

    GPUProcessConnection& m_gpuProcessConnection;
    RemoteMediaSourceIdentifier m_identifier;
    RemoteMediaPlayerMIMETypeCache& m_mimeTypeCache;
    WeakPtr<MediaPlayerPrivateRemote> m_mediaPlayerPrivate;
    RefPtr<WebCore::MediaSourcePrivateClient> m_client;
    Vector<RefPtr<SourceBufferPrivateRemote>> m_sourceBuffers;

    MediaTime m_duration;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "MediaSourcePrivateRemote"; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
    uint64_t m_nextSourceBufferID { 0 };
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
