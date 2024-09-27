/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlaybackSessionInterfaceIOS.h"

#if PLATFORM(COCOA) && HAVE(AVKIT)

#import "Logging.h"
#import "MediaSelectionOption.h"
#import "PlaybackSessionModel.h"
#import "TimeRanges.h"
#import <AVFoundation/AVTime.h>
#import <wtf/LoggerHelper.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionInterfaceIOS);

PlaybackSessionInterfaceIOS::PlaybackSessionInterfaceIOS(PlaybackSessionModel& model)
    : m_playbackSessionModel(&model)
{
    model.addClient(*this);
}

PlaybackSessionInterfaceIOS::~PlaybackSessionInterfaceIOS()
{
    ASSERT(isUIThread());
}

void PlaybackSessionInterfaceIOS::initialize()
{
    auto& model = *m_playbackSessionModel;

    durationChanged(model.duration());
    currentTimeChanged(model.currentTime(), [[NSProcessInfo processInfo] systemUptime]);
    bufferedTimeChanged(model.bufferedTime());
    OptionSet<PlaybackSessionModel::PlaybackState> playbackState;
    if (model.isPlaying())
        playbackState.add(PlaybackSessionModel::PlaybackState::Playing);
    if (model.isStalled())
        playbackState.add(PlaybackSessionModel::PlaybackState::Stalled);
    rateChanged(playbackState, model.playbackRate(), model.defaultPlaybackRate());
    seekableRangesChanged(model.seekableRanges(), model.seekableTimeRangesLastModifiedTime(), model.liveUpdateInterval());
    canPlayFastReverseChanged(model.canPlayFastReverse());
    audioMediaSelectionOptionsChanged(model.audioMediaSelectionOptions(), model.audioMediaSelectedIndex());
    legibleMediaSelectionOptionsChanged(model.legibleMediaSelectionOptions(), model.legibleMediaSelectedIndex());
    externalPlaybackChanged(model.externalPlaybackEnabled(), model.externalPlaybackTargetType(), model.externalPlaybackLocalizedDeviceName());
    wirelessVideoPlaybackDisabledChanged(model.wirelessVideoPlaybackDisabled());
}

void PlaybackSessionInterfaceIOS::invalidate()
{
    if (!m_playbackSessionModel)
        return;
    m_playbackSessionModel->removeClient(*this);
    m_playbackSessionModel = nullptr;
}

PlaybackSessionModel* PlaybackSessionInterfaceIOS::playbackSessionModel() const
{
    return m_playbackSessionModel;
}

void PlaybackSessionInterfaceIOS::modelDestroyed()
{
    ASSERT(isUIThread());
    invalidate();
    ASSERT(!m_playbackSessionModel);
}

std::optional<MediaPlayerIdentifier> PlaybackSessionInterfaceIOS::playerIdentifier() const
{
    return m_playerIdentifier;
}

void PlaybackSessionInterfaceIOS::setPlayerIdentifier(std::optional<MediaPlayerIdentifier> identifier)
{
    m_playerIdentifier = WTFMove(identifier);
}

void PlaybackSessionInterfaceIOS::startObservingNowPlayingMetadata()
{
}

void PlaybackSessionInterfaceIOS::stopObservingNowPlayingMetadata()
{
}

#if !RELEASE_LOG_DISABLED
uint64_t PlaybackSessionInterfaceIOS::logIdentifier() const
{
    return m_playbackSessionModel ? m_playbackSessionModel->logIdentifier() : 0;
}

const Logger* PlaybackSessionInterfaceIOS::loggerPtr() const
{
    return m_playbackSessionModel ? m_playbackSessionModel->loggerPtr() : nullptr;
}

WTFLogChannel& PlaybackSessionInterfaceIOS::logChannel() const
{
    return LogMedia;
}

uint32_t PlaybackSessionInterfaceIOS::ptrCount() const
{
    return CanMakeCheckedPtr::ptrCount();
}

uint32_t PlaybackSessionInterfaceIOS::ptrCountWithoutThreadCheck() const
{
    return CanMakeCheckedPtr::ptrCountWithoutThreadCheck();
}

void PlaybackSessionInterfaceIOS::incrementPtrCount() const
{
    CanMakeCheckedPtr::incrementPtrCount();
}

void PlaybackSessionInterfaceIOS::decrementPtrCount() const
{
    CanMakeCheckedPtr::decrementPtrCount();
}

#endif

} // namespace WebCore

#endif // PLATFORM(COCOA) && HAVE(AVKIT)
