/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "MediaPlayerPrivateWirelessPlayback.h"

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#include "Logging.h"
#include "MediaDeviceRoute.h"
#include "MediaPlaybackTargetWirelessPlayback.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayerPrivateWirelessPlayback);

class MediaPlayerFactoryWirelessPlayback final : public MediaPlayerFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaPlayerFactoryWirelessPlayback);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MediaPlayerFactoryWirelessPlayback);
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final
    {
        return MediaPlayerEnums::MediaEngineIdentifier::WirelessPlayback;
    }

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final
    {
        return adoptRef(*new MediaPlayerPrivateWirelessPlayback(player));
    }

    void getSupportedTypes(HashSet<String>&) const final
    {
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        if (MediaPlayerPrivateWirelessPlayback::playbackTargetTypes().contains(parameters.playbackTargetType))
            return MediaPlayer::SupportsType::IsSupported;
        return MediaPlayer::SupportsType::IsNotSupported;
    }
};

void MediaPlayerPrivateWirelessPlayback::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(makeUnique<MediaPlayerFactoryWirelessPlayback>());
}

MediaPlayerPrivateWirelessPlayback::MediaPlayerPrivateWirelessPlayback(MediaPlayer& player)
    : m_player { player }
#if !RELEASE_LOG_DISABLED
    , m_logger { player.mediaPlayerLogger() }
    , m_logIdentifier { player.mediaPlayerLogIdentifier() }
#endif
{
}

MediaPlayerPrivateWirelessPlayback::~MediaPlayerPrivateWirelessPlayback() = default;

static bool supportsURL(const URL& url)
{
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    if (url.protocolIsFile())
        return true;
#endif
    return url.protocolIsInHTTPFamily();
}

void MediaPlayerPrivateWirelessPlayback::load(const String& urlString)
{
    ALWAYS_LOG(LOGIDENTIFIER, urlString);

    URL url { urlString };
    if (!supportsURL(url)) {
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }

    m_url = WTF::move(url);
    updateURLIfNeeded();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

OptionSet<MediaPlaybackTargetType> MediaPlayerPrivateWirelessPlayback::playbackTargetTypes()
{
    return { MediaPlaybackTargetType::WirelessPlayback };
}

String MediaPlayerPrivateWirelessPlayback::wirelessPlaybackTargetName() const
{
    if (RefPtr playbackTarget = m_playbackTarget)
        return playbackTarget->deviceName();
    return { };
}

MediaPlayer::WirelessPlaybackTargetType MediaPlayerPrivateWirelessPlayback::wirelessPlaybackTargetType() const
{
    RefPtr playbackTarget = m_playbackTarget;
    if (!playbackTarget)
        return MediaPlayer::WirelessPlaybackTargetType::TargetTypeNone;

    switch (playbackTarget->targetType()) {
    case MediaPlaybackTargetType::Serialized:
    case MediaPlaybackTargetType::None:
    case MediaPlaybackTargetType::AVOutputContext:
    case MediaPlaybackTargetType::Mock:
        return MediaPlayer::WirelessPlaybackTargetType::TargetTypeNone;
    case MediaPlaybackTargetType::WirelessPlayback:
        return MediaPlayer::WirelessPlaybackTargetType::TargetTypeAirPlay;
    }

    ASSERT_NOT_REACHED();
    return MediaPlayer::WirelessPlaybackTargetType::TargetTypeNone;
}

OptionSet<MediaPlaybackTargetType> MediaPlayerPrivateWirelessPlayback::supportedPlaybackTargetTypes() const
{
    return MediaPlayerPrivateWirelessPlayback::playbackTargetTypes();
}

bool MediaPlayerPrivateWirelessPlayback::isCurrentPlaybackTargetWireless() const
{
    if (RefPtr playbackTarget = m_playbackTarget)
        return m_shouldPlayToTarget && playbackTarget->hasActiveRoute();
    return false;
}

void MediaPlayerPrivateWirelessPlayback::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& playbackTarget)
{
    ALWAYS_LOG(LOGIDENTIFIER, playbackTarget->type());
    m_playbackTarget = WTF::move(playbackTarget);
    updateURLIfNeeded();
}

void MediaPlayerPrivateWirelessPlayback::setShouldPlayToPlaybackTarget(bool shouldPlayToTarget)
{
    if (shouldPlayToTarget == m_shouldPlayToTarget)
        return;

    m_shouldPlayToTarget = shouldPlayToTarget;

    if (RefPtr player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless());
}

void MediaPlayerPrivateWirelessPlayback::updateURLIfNeeded()
{
    RefPtr playbackTarget = dynamicDowncast<MediaPlaybackTargetWirelessPlayback>(m_playbackTarget);
    if (!playbackTarget)
        return;

    playbackTarget->loadURL(m_url, [weakThis = ThreadSafeWeakPtr { *this }](const MediaDeviceRouteLoadURLResult& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!result) {
            protectedThis->setNetworkState(MediaPlayer::NetworkState::FormatError);
            return;
        }

        // FIXME: Advance networkState and readyState once the target has loaded the URL
    });
}

void MediaPlayerPrivateWirelessPlayback::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (networkState == m_networkState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, networkState);
    m_networkState = networkState;
    if (RefPtr player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateWirelessPlayback::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (readyState == m_readyState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, readyState);
    m_readyState = readyState;
    if (RefPtr player = m_player.get())
        player->readyStateChanged();
}

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaPlayerPrivateWirelessPlayback::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
