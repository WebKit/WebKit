/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MediaPlayerPrivateWebM.h"

#if ENABLE(ALTERNATE_WEBM_PLAYER)

#import "Logging.h"
#import "MediaPlayer.h"
#import "NotImplemented.h"
#import "SourceBufferParserWebM.h"

namespace WebCore {

MediaPlayerPrivateWebM::MediaPlayerPrivateWebM(MediaPlayer* player)
    : m_player(player)
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
{
    ALWAYS_LOG(LOGIDENTIFIER);
    UNUSED_VARIABLE(m_player);
}

MediaPlayerPrivateWebM::~MediaPlayerPrivateWebM()
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

static HashSet<String, ASCIICaseInsensitiveHash>& mimeTypeCache()
{
    static NeverDestroyed cache = HashSet<String, ASCIICaseInsensitiveHash>();
    if (cache->isEmpty()) {
        auto types = SourceBufferParserWebM::supportedMIMETypes();
        cache->add(types.begin(), types.end());
    }
    return cache;
}

void MediaPlayerPrivateWebM::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivateWebM::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.isMediaSource || parameters.isMediaStream)
        return MediaPlayer::SupportsType::IsNotSupported;
    return SourceBufferParserWebM::isContentTypeSupported(parameters.type);
}

void MediaPlayerPrivateWebM::load(const String&)
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateWebM::load(const URL&, const ContentType&, MediaSourcePrivateClient&)
{
    ERROR_LOG(LOGIDENTIFIER);
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateWebM::load(MediaStreamPrivate&)
{
    ERROR_LOG(LOGIDENTIFIER);
}
#endif

void MediaPlayerPrivateWebM::cancelLoad()
{
    notImplemented();
}

PlatformLayer* MediaPlayerPrivateWebM::platformLayer() const
{
    return nullptr;
}

void MediaPlayerPrivateWebM::play()
{
    notImplemented();
}

void MediaPlayerPrivateWebM::pause()
{
    notImplemented();
}

void MediaPlayerPrivateWebM::setPageIsVisible(bool)
{
    notImplemented();
}

MediaTime MediaPlayerPrivateWebM::currentMediaTime() const
{
    return m_currentTime;
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateWebM::buffered() const
{
    return makeUnique<PlatformTimeRanges>();
}

bool MediaPlayerPrivateWebM::didLoadingProgress() const
{
    return false;
}

WTFLogChannel& MediaPlayerPrivateWebM::logChannel() const
{
    return LogMedia;
}

class MediaPlayerFactoryWebM final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::CocoaWebM; };

    std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return makeUnique<MediaPlayerPrivateWebM>(player);
    }

    void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types) const final
    {
        return MediaPlayerPrivateWebM::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateWebM::supportsType(parameters);
    }
};

void MediaPlayerPrivateWebM::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryWebM>());
}

} // namespace WebCore

#endif // ENABLE(ALTERNATE_WEBM_PLAYER)
