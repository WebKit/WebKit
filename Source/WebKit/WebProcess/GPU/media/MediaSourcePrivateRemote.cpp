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

#include "config.h"
#include "MediaSourcePrivateRemote.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "Logging.h"
#include "MediaPlayerPrivateRemote.h"
#include "RemoteMediaSourceProxyMessages.h"
#include "RemoteSourceBufferIdentifier.h"
#include "SourceBufferPrivateRemote.h"
#include <WebCore/NotImplemented.h>
#include <wtf/Optional.h>

namespace WebCore {
#if !RELEASE_LOG_DISABLED
extern WTFLogChannel LogMedia;
#endif
}

namespace WebKit {

using namespace WebCore;

Ref<MediaSourcePrivateRemote> MediaSourcePrivateRemote::create(GPUProcessConnection& gpuProcessConnection, RemoteMediaSourceIdentifier identifier, RemoteMediaPlayerMIMETypeCache& mimeTypeCache, const MediaPlayerPrivateRemote& playerPrivate, MediaSourcePrivateClient* client)
{
    auto mediaSourcePrivate = adoptRef(*new MediaSourcePrivateRemote(gpuProcessConnection, identifier, mimeTypeCache, playerPrivate, client));
    client->setPrivateAndOpen(mediaSourcePrivate.copyRef());
    return mediaSourcePrivate;
}

MediaSourcePrivateRemote::MediaSourcePrivateRemote(GPUProcessConnection& gpuProcessConnection, RemoteMediaSourceIdentifier identifier, RemoteMediaPlayerMIMETypeCache& mimeTypeCache, const MediaPlayerPrivateRemote& playerPrivate, MediaSourcePrivateClient* client)
    : m_gpuProcessConnection(gpuProcessConnection)
    , m_identifier(identifier)
    , m_mimeTypeCache(mimeTypeCache)
    , m_playerPrivate(makeWeakPtr(playerPrivate))
    , m_client(client)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_playerPrivate->mediaPlayerLogger())
    , m_logIdentifier(m_playerPrivate->mediaPlayerLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
#if !RELEASE_LOG_DISABLED
    m_client->setLogIdentifier(m_logIdentifier);
#endif
}

MediaSourcePrivateRemote::~MediaSourcePrivateRemote()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    for (auto it = m_sourceBuffers.begin(), end = m_sourceBuffers.end(); it != end; ++it)
        (*it)->clearMediaSource();
}

MediaSourcePrivate::AddStatus MediaSourcePrivateRemote::addSourceBuffer(const ContentType& contentType, RefPtr<SourceBufferPrivate>& outPrivate)
{
    DEBUG_LOG(LOGIDENTIFIER, contentType);

    MediaEngineSupportParameters parameters;
    parameters.isMediaSource = true;
    parameters.type = contentType;
    if (m_mimeTypeCache.supportsTypeAndCodecs(parameters) == MediaPlayer::SupportsType::IsNotSupported)
        return AddStatus::NotSupported;

    AddStatus status;
    Optional<RemoteSourceBufferIdentifier> remoteSourceBufferIdentifier;
    if (!m_gpuProcessConnection.connection().sendSync(Messages::RemoteMediaSourceProxy::AddSourceBuffer(contentType), Messages::RemoteMediaSourceProxy::AddSourceBuffer::Reply(status, remoteSourceBufferIdentifier), m_identifier))
        status = AddStatus::NotSupported;

    if (status == AddStatus::Ok) {
        ASSERT(remoteSourceBufferIdentifier.hasValue());
        auto newSourceBuffer = SourceBufferPrivateRemote::create(m_gpuProcessConnection, *remoteSourceBufferIdentifier, *this);
        outPrivate = newSourceBuffer.copyRef();
        m_sourceBuffers.append(WTFMove(newSourceBuffer));
    }

    return status;
}

void MediaSourcePrivateRemote::durationChanged()
{
    notImplemented();
}

void MediaSourcePrivateRemote::markEndOfStream(EndOfStreamStatus)
{
    notImplemented();
}

void MediaSourcePrivateRemote::unmarkEndOfStream()
{
    notImplemented();
}

MediaPlayer::ReadyState MediaSourcePrivateRemote::readyState() const
{
    return m_playerPrivate ? m_playerPrivate->readyState() : MediaPlayer::ReadyState::HaveNothing;
}

void MediaSourcePrivateRemote::setReadyState(MediaPlayer::ReadyState)
{
    notImplemented();
}

void MediaSourcePrivateRemote::waitForSeekCompleted()
{
    notImplemented();
}

void MediaSourcePrivateRemote::seekCompleted()
{
    notImplemented();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSourcePrivateRemote::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
