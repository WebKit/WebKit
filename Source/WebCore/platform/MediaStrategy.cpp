/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "MediaStrategy.h"

#include "MediaPlayer.h"
#if ENABLE(MEDIA_SOURCE)
#include "DeprecatedGlobalSettings.h"
#include "MockMediaPlayerMediaSource.h"
#endif

namespace WebCore {

MediaStrategy::MediaStrategy() = default;

MediaStrategy::~MediaStrategy() = default;

std::unique_ptr<NowPlayingManager> MediaStrategy::createNowPlayingManager() const
{
    return makeUnique<NowPlayingManager>();
}

void MediaStrategy::resetMediaEngines()
{
#if ENABLE(VIDEO)
    MediaPlayer::resetMediaEngines();
#endif
    m_mockMediaSourceEnabled = false;
}

bool MediaStrategy::hasThreadSafeMediaSourceSupport() const
{
    return false;
}

#if ENABLE(MEDIA_SOURCE)
void MediaStrategy::enableMockMediaSource()
{
#if USE(AVFOUNDATION)
    WebCore::DeprecatedGlobalSettings::setAVFoundationEnabled(false);
#endif
#if USE(GSTREAMER)
    WebCore::DeprecatedGlobalSettings::setGStreamerEnabled(false);
#endif
    addMockMediaSourceEngine();
}

bool MediaStrategy::mockMediaSourceEnabled() const
{
    return m_mockMediaSourceEnabled;
}

void MediaStrategy::addMockMediaSourceEngine()
{
    MediaPlayerFactorySupport::callRegisterMediaEngine(MockMediaPlayerMediaSource::registerMediaEngine);
}
#endif

}
