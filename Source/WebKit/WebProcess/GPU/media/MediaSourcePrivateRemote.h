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
#include "RemoteMediaPlayerMIMETypeCache.h"
#include "RemoteMediaSourceIdentifier.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/ContentType.h>
#include <WebCore/MediaSourcePrivate.h>
#include <WebCore/MediaSourcePrivateClient.h>
#include <WebCore/SourceBufferPrivate.h>
#include <atomic>
#include <wtf/Forward.h>
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
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<MediaSourcePrivateRemote> create(GPUProcessConnection&, RemoteMediaSourceIdentifier, RemoteMediaPlayerMIMETypeCache&, const MediaPlayerPrivateRemote&, WebCore::MediaSourcePrivateClient&);
    virtual ~MediaSourcePrivateRemote();

    // MediaSourcePrivate overrides
    RefPtr<WebCore::MediaPlayerPrivateInterface> player() const final;
    constexpr WebCore::MediaPlatformType platformType() const final { return WebCore::MediaPlatformType::Remote; }
    AddStatus addSourceBuffer(const WebCore::ContentType&, bool webMParserEnabled, RefPtr<WebCore::SourceBufferPrivate>&) final;
    void removeSourceBuffer(WebCore::SourceBufferPrivate&) final { }
    void notifyActiveSourceBuffersChanged() final { };
    void durationChanged(const MediaTime&) final;
    void markEndOfStream(EndOfStreamStatus) final;
    void unmarkEndOfStream() final;
    WebCore::MediaPlayer::ReadyState mediaPlayerReadyState() const final;
    void setMediaPlayerReadyState(WebCore::MediaPlayer::ReadyState) final;

    void setTimeFudgeFactor(const MediaTime&) final;

    static WorkQueue& queue();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* nextSourceBufferLogIdentifier() { return childLogIdentifier(m_logIdentifier, ++m_nextSourceBufferID); }
#endif

    class MessageReceiver : public IPC::WorkQueueMessageReceiver {
    public:
        static Ref<MessageReceiver> create(MediaSourcePrivateRemote& parent)
        {
            return adoptRef(*new MessageReceiver(parent));
        }

    private:
        MessageReceiver(MediaSourcePrivateRemote&);
        void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
        void proxyWaitForTarget(const WebCore::SeekTarget&, CompletionHandler<void(WebCore::MediaTimePromise::Result&&)>&&);
        void proxySeekToTime(const MediaTime&, CompletionHandler<void(WebCore::MediaPromise::Result&&)>&&);
        void mediaSourcePrivateShuttingDown(CompletionHandler<void()>&&);

        RefPtr<WebCore::MediaSourcePrivateClient> client() const;
        ThreadSafeWeakPtr<MediaSourcePrivateRemote> m_parent;
    };
private:
    friend class MessageReceiver;
    MediaSourcePrivateRemote(GPUProcessConnection&, RemoteMediaSourceIdentifier, RemoteMediaPlayerMIMETypeCache&, const MediaPlayerPrivateRemote&, WebCore::MediaSourcePrivateClient&);

    void bufferedChanged(const WebCore::PlatformTimeRanges&) final;

    void ensureOnDispatcherSync(Function<void()>&&) const;

    bool isGPURunning() const { return !m_shutdown; }

    ThreadSafeWeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    Ref<MessageReceiver> m_receiver;
    RemoteMediaSourceIdentifier m_identifier;
    RemoteMediaPlayerMIMETypeCache& m_mimeTypeCache;
    ThreadSafeWeakPtr<MediaPlayerPrivateRemote> m_mediaPlayerPrivate;
    std::atomic<bool> m_shutdown { false };
    std::atomic<WebCore::MediaPlayer::ReadyState> m_mediaPlayerReadyState { WebCore::MediaPlayer::ReadyState::HaveNothing };

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const override { return "MediaSourcePrivateRemote"_s; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;

    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
    uint64_t m_nextSourceBufferID { 0 };
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
