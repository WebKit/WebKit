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
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/BlockPtr.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/darwin/DispatchExtras.h>

#include <pal/cf/CoreMediaSoftLink.h>

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

MediaPlayerPrivateWirelessPlayback::~MediaPlayerPrivateWirelessPlayback()
{
    destroyTimebase();
}

static bool supportsURL(const URL& url)
{
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    if (url.protocolIsFile())
        return true;
#endif
    return url.protocolIsInHTTPFamily();
}

void MediaPlayerPrivateWirelessPlayback::load(const URL& url, const LoadOptions&)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!supportsURL(url)) {
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }

    m_url = url;
    updateURLIfNeeded();
}

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
        return m_shouldPlayToTarget == ShouldPlayToTarget::Yes && playbackTarget->hasActiveRoute();
    return false;
}

void MediaPlayerPrivateWirelessPlayback::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& playbackTarget)
{
    if (m_playbackTarget && *m_playbackTarget == playbackTarget.get())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, playbackTarget->type());

    m_playbackTarget = WTF::move(playbackTarget);

    if (!wirelessPlaybackTarget())
        return;

    if (RefPtr route = this->route()) {
        route->setClient(this);
        updateURLIfNeeded();
        return;
    }

    setNetworkState(MediaPlayer::NetworkState::FormatError);
}

void MediaPlayerPrivateWirelessPlayback::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    auto shouldPlayToTarget = shouldPlay ? ShouldPlayToTarget::Yes : ShouldPlayToTarget::No;
    if (shouldPlayToTarget == m_shouldPlayToTarget)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, shouldPlayToTarget);
    m_shouldPlayToTarget = shouldPlayToTarget;

    if (!isCurrentPlaybackTargetWireless()) {
        setNetworkState(MediaPlayer::NetworkState::FormatError);
        return;
    }

    updateURLIfNeeded();

    if (RefPtr player = m_player.get())
        player->currentPlaybackTargetIsWirelessChanged(true);
}

MediaPlaybackTargetWirelessPlayback* MediaPlayerPrivateWirelessPlayback::wirelessPlaybackTarget() const
{
    return dynamicDowncast<MediaPlaybackTargetWirelessPlayback>(m_playbackTarget.get());
}

MediaDeviceRoute* MediaPlayerPrivateWirelessPlayback::route() const
{
    if (RefPtr wirelessPlaybackTarget = this->wirelessPlaybackTarget())
        return wirelessPlaybackTarget->route();
    return nullptr;
}

void MediaPlayerPrivateWirelessPlayback::updateURLIfNeeded()
{
    RefPtr route = this->route();
    if (!route || m_shouldPlayToTarget != ShouldPlayToTarget::Yes || networkState() >= MediaPlayer::NetworkState::Loading)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    setNetworkState(MediaPlayer::NetworkState::Loading);

    route->loadURL(m_url, [weakThis = ThreadSafeWeakPtr { *this }](const MediaDeviceRouteLoadURLResult& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!result) {
            protectedThis->setNetworkState(MediaPlayer::NetworkState::FormatError);
            return;
        }

        protectedThis->setNetworkState(MediaPlayer::NetworkState::Idle);
    });
}

void MediaPlayerPrivateWirelessPlayback::play()
{
    RefPtr route = this->route();
    if (!route)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    route->setPlaying(true);

    if (RetainPtr timebase = ensureTimebase()) {
        PAL::CMTimebaseSetRate(timebase.get(), route->playbackSpeed());
        scheduleTimebaseTimer();
    }
}

void MediaPlayerPrivateWirelessPlayback::pause()
{
    RefPtr route = this->route();
    if (!route)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    route->setPlaying(false);

    if (RetainPtr timebase = ensureTimebase())
        PAL::CMTimebaseSetRate(timebase.get(), 0);
}

bool MediaPlayerPrivateWirelessPlayback::hasAudio() const
{
    if (RefPtr route = this->route())
        return route->hasAudio();
    return false;
}

void MediaPlayerPrivateWirelessPlayback::seekToTarget(const SeekTarget& seekTarget)
{
    RefPtr route = this->route();
    if (!route)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, seekTarget);
    route->setCurrentPlaybackPosition(seekTarget.time);
}

bool MediaPlayerPrivateWirelessPlayback::paused() const
{
    if (RefPtr route = this->route())
        return !route->playing();
    return false;
}

MediaTime MediaPlayerPrivateWirelessPlayback::startTime() const
{
    if (RefPtr route = this->route())
        return route->timeRange().start ?: MediaTime::zeroTime();
    return MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateWirelessPlayback::duration() const
{
    RefPtr route = this->route();
    if (!route)
        return MediaTime::zeroTime();

    auto endTime = route->timeRange().end;
    if (!endTime)
        return MediaTime::zeroTime();

    return std::max(MediaTime::zeroTime(), endTime - startTime());
}

MediaTime MediaPlayerPrivateWirelessPlayback::currentTime() const
{
    if (m_timebase)
        return PAL::toMediaTime(PAL::CMTimebaseGetTime(m_timebase.get()));
    return MediaTime::zeroTime();
}

MediaTime MediaPlayerPrivateWirelessPlayback::maxTimeSeekable() const
{
    return startTime() + duration();
}

MediaTime MediaPlayerPrivateWirelessPlayback::minTimeSeekable() const
{
    return startTime();
}

bool MediaPlayerPrivateWirelessPlayback::setCurrentTimeDidChangeCallback(MediaPlayer::CurrentTimeDidChangeCallback&& currentTimeDidChangeCallback)
{
    m_currentTimeDidChangeCallback = WTF::move(currentTimeDidChangeCallback);
    return true;
}

void MediaPlayerPrivateWirelessPlayback::setRate(float rate)
{
    RefPtr route = this->route();
    if (!route)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, rate);
    route->setPlaybackSpeed(rate);

    if (RetainPtr timebase = ensureTimebase()) {
        PAL::CMTimebaseSetRate(timebase, route->playing() ? rate : 0);
        scheduleTimebaseTimer();
    }
}

double MediaPlayerPrivateWirelessPlayback::rate() const
{
    if (RefPtr route = this->route())
        return route->playbackSpeed();
    return 0;
}

void MediaPlayerPrivateWirelessPlayback::setVolumeLocked(bool volumeLocked)
{
    if (m_volumeLocked == volumeLocked)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, volumeLocked);
    m_volumeLocked = volumeLocked;
}

void MediaPlayerPrivateWirelessPlayback::setVolume(float volume)
{
    if (m_volumeLocked)
        return;

    RefPtr route = this->route();
    if (!route || route->volume() == volume)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, volume);
    route->setVolume(volume);
}

float MediaPlayerPrivateWirelessPlayback::volume() const
{
    if (RefPtr route = this->route())
        return route->volume();
    return 1;
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

String MediaPlayerPrivateWirelessPlayback::engineDescription() const
{
    static NeverDestroyed<String> description(MAKE_STATIC_STRING_IMPL("Cocoa Wireless Playback Engine"));
    return description;
}

void MediaPlayerPrivateWirelessPlayback::timeRangeDidChange(MediaDeviceRoute& route)
{
    ASSERT(&route == this->route());
    ALWAYS_LOG(LOGIDENTIFIER, route.timeRange());

    if (RefPtr player = m_player.get())
        player->durationChanged();
}

void MediaPlayerPrivateWirelessPlayback::readyDidChange(MediaDeviceRoute& route)
{
    ASSERT(&route == this->route());
    ALWAYS_LOG(LOGIDENTIFIER, route.ready());

    if (route.ready())
        setReadyState(MediaPlayerReadyState::HaveEnoughData);
}

void MediaPlayerPrivateWirelessPlayback::playbackErrorDidChange(MediaDeviceRoute& route)
{
    ASSERT(&route == this->route());
    ALWAYS_LOG(LOGIDENTIFIER, !!route.playbackError());

    if (route.playbackError())
        setNetworkState(route.ready() ? MediaPlayer::NetworkState::DecodeError : MediaPlayer::NetworkState::FormatError);
}

void MediaPlayerPrivateWirelessPlayback::currentPlaybackPositionDidChange(MediaDeviceRoute& route)
{
    ASSERT(&route == this->route());

    auto currentPlaybackPosition = route.currentPlaybackPosition();
    ALWAYS_LOG(LOGIDENTIFIER, currentPlaybackPosition);

    updateTimebaseTimeAndRate(currentPlaybackPosition, route.playing() ? route.playbackSpeed() : 0);

    auto currentTime = this->currentTime();

    if (RefPtr player = m_player.get()) {
        player->seeked(currentTime);
        player->timeChanged();
    }

    if (m_currentTimeDidChangeCallback)
        m_currentTimeDidChangeCallback(currentTime);
}

CMTimebaseRef MediaPlayerPrivateWirelessPlayback::ensureTimebase()
{
    if (m_timebase)
        return m_timebase.get();

    CMTimebaseRef rawTimebase = nullptr;
    OSStatus result = PAL::CMTimebaseCreateWithSourceClock(kCFAllocatorDefault, PAL::CMClockGetHostTimeClock(), &rawTimebase);
    if (result != noErr) {
        ERROR_LOG(LOGIDENTIFIER, "failed to create timebase with error ", result);
        return nullptr;
    }

    m_timebase = adoptCF(rawTimebase);
    m_timerSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, mainDispatchQueueSingleton()));

    dispatch_source_set_event_handler(m_timerSource.get(), makeBlockPtr([weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->timebaseTimerFired();
    }).get());

    PAL::CMTimebaseAddTimerDispatchSource(m_timebase.get(), m_timerSource.get());
    dispatch_activate(m_timerSource.get());

    if (RefPtr route = this->route())
        updateTimebaseTimeAndRate(route->currentPlaybackPosition() ?: MediaTime::zeroTime(), route->playing() ? route->playbackSpeed() : 0);

    return m_timebase.get();
}

void MediaPlayerPrivateWirelessPlayback::destroyTimebase()
{
    if (!m_timebase)
        return;

    PAL::CMTimebaseSetRate(m_timebase.get(), 0);
    PAL::CMTimebaseRemoveTimerDispatchSource(m_timebase.get(), m_timerSource.get());
    dispatch_source_cancel(m_timerSource.get());

    m_timerSource = nullptr;
    m_timebase = nullptr;
}

void MediaPlayerPrivateWirelessPlayback::updateTimebaseTimeAndRate(MediaTime currentPosition, float rate)
{
    RetainPtr timebase = ensureTimebase();
    if (!timebase)
        return;

    PAL::CMTimebaseSetTime(timebase, PAL::toCMTime(currentPosition));
    PAL::CMTimebaseSetRate(timebase, rate);
    scheduleTimebaseTimer();
}

void MediaPlayerPrivateWirelessPlayback::scheduleTimebaseTimer()
{
    RetainPtr timebase = ensureTimebase();
    if (!timebase)
        return;

    if (PAL::CMTimebaseGetEffectiveRate(timebase) <= 0)
        return;

    auto nextFireTime = PAL::CMTimeAdd(PAL::CMTimebaseGetTime(timebase), PAL::CMTimeMake(1, 10));
    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(timebase, m_timerSource.get(), nextFireTime, 0);
}

void MediaPlayerPrivateWirelessPlayback::timebaseTimerFired()
{
    if (!m_currentTimeDidChangeCallback)
        return;

    auto timebase = ensureTimebase();
    if (!timebase)
        return;

    m_currentTimeDidChangeCallback(PAL::toMediaTime(PAL::CMTimebaseGetTime(timebase)));
    scheduleTimebaseTimer();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaPlayerPrivateWirelessPlayback::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
