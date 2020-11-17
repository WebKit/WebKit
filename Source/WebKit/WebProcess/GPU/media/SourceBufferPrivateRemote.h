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
#include "RemoteSourceBufferIdentifier.h"
#include <WebCore/ContentType.h>
#include <WebCore/MediaSample.h>
#include <WebCore/SourceBufferPrivate.h>
#include <WebCore/SourceBufferPrivateClient.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

namespace IPC {
class Connection;
class Decoder;
class DataReference;
}

namespace WebKit {

class MediaSourcePrivateRemote;

class SourceBufferPrivateRemote final
    : public CanMakeWeakPtr<SourceBufferPrivateRemote>
    , public WebCore::SourceBufferPrivate
    , private IPC::MessageReceiver
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<SourceBufferPrivateRemote> create(GPUProcessConnection&, RemoteSourceBufferIdentifier, const MediaSourcePrivateRemote&);
    virtual ~SourceBufferPrivateRemote();

    void clearMediaSource() { m_mediaSourcePrivate = nullptr; }

private:
    SourceBufferPrivateRemote(GPUProcessConnection&, RemoteSourceBufferIdentifier, const MediaSourcePrivateRemote&);

    // SourceBufferPrivate overrides
    void setClient(WebCore::SourceBufferPrivateClient*) final;
    void append(Vector<unsigned char>&&) final;
    void abort() final;
    void resetParserState() final;
    void removedFromMediaSource() final;
    WebCore::MediaPlayer::ReadyState readyState() const final;
    void setReadyState(WebCore::MediaPlayer::ReadyState) final;
    void flush(const AtomString& trackID) final;
    void enqueueSample(Ref<WebCore::MediaSample>&&, const AtomString& trackID) final;
    bool isReadyForMoreSamples(const AtomString& trackID) final;
    void setActive(bool) final;
    void notifyClientWhenReadyForMoreSamples(const AtomString& trackID) final;
    bool canSetMinimumUpcomingPresentationTime(const AtomString&) const final;
    void setMinimumUpcomingPresentationTime(const AtomString&, const MediaTime&) final;
    void clearMinimumUpcomingPresentationTime(const AtomString&) final;
    bool canSwitchToType(const WebCore::ContentType&) final;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void sourceBufferPrivateAppendComplete(WebCore::SourceBufferPrivateClient::AppendResult);

    GPUProcessConnection& m_gpuProcessConnection;
    RemoteSourceBufferIdentifier m_remoteSourceBufferIdentifier;
    WeakPtr<MediaSourcePrivateRemote> m_mediaSourcePrivate;
    WebCore::SourceBufferPrivateClient* m_client { nullptr };

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const override { return "SourceBufferPrivateRemote"; }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const Logger& sourceBufferLogger() const final { return m_logger.get(); }
    const void* sourceBufferLogIdentifier() final { return logIdentifier(); }

    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
