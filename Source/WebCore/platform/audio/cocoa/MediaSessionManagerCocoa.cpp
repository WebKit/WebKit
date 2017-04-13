/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
#include "PlatformMediaSessionManager.h"

#if USE(AUDIO_SESSION)

#include "AudioSession.h"
#include "Logging.h"
#include "Settings.h"
#include <wtf/Function.h>

using namespace WebCore;

static const size_t kWebAudioBufferSize = 128;
static const size_t kLowPowerVideoBufferSize = 4096;

void PlatformMediaSessionManager::updateSessionState()
{
    LOG(Media, "PlatformMediaSessionManager::updateSessionState() - types: Video(%d), Audio(%d), WebAudio(%d)", count(PlatformMediaSession::Video), count(PlatformMediaSession::Audio), count(PlatformMediaSession::WebAudio));

    if (has(PlatformMediaSession::WebAudio) || has(PlatformMediaSession::MediaStreamCapturingAudio))
        AudioSession::sharedSession().setPreferredBufferSize(kWebAudioBufferSize);
    else if ((has(PlatformMediaSession::Video) || has(PlatformMediaSession::Audio)) && Settings::lowPowerVideoAudioBufferSizeEnabled()) {
        // FIXME: <http://webkit.org/b/116725> Figure out why enabling the code below
        // causes media LayoutTests to fail on 10.8.

        size_t bufferSize;
        if (m_audioHardwareListener && m_audioHardwareListener->outputDeviceSupportsLowPowerMode())
            bufferSize = kLowPowerVideoBufferSize;
        else
            bufferSize = kWebAudioBufferSize;

        AudioSession::sharedSession().setPreferredBufferSize(bufferSize);
    }

#if PLATFORM(IOS)
    if (!Settings::shouldManageAudioSessionCategory())
        return;

    bool hasAudioMediaType = false;
    bool hasAudibleAudioOrVideoMediaType = false;
    bool hasAudioCapture = anyOfSessions([this, &hasAudioMediaType, &hasAudibleAudioOrVideoMediaType] (PlatformMediaSession& session, size_t) mutable {
        auto type = session.mediaType();
        if (type == PlatformMediaSession::VideoAudio || type == PlatformMediaSession::Audio || type == PlatformMediaSession::WebAudio)
            hasAudioMediaType = true;
        if ((type == PlatformMediaSession::VideoAudio || type == PlatformMediaSession::Audio) && session.canProduceAudio())
            hasAudibleAudioOrVideoMediaType = true;
        return (type == PlatformMediaSession::MediaStreamCapturingAudio);
    });

    if (hasAudioCapture)
        AudioSession::sharedSession().setCategory(AudioSession::PlayAndRecord);
    else if (hasAudibleAudioOrVideoMediaType)
        AudioSession::sharedSession().setCategory(AudioSession::MediaPlayback);
    else if (hasAudioMediaType)
        AudioSession::sharedSession().setCategory(AudioSession::AmbientSound);
    else
        AudioSession::sharedSession().setCategory(AudioSession::None);
#endif
}

#endif // USE(AUDIO_SESSION)
