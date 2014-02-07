/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "MediaSessionManager.h"

#if USE(AUDIO_SESSION)

#include "AudioSession.h"
#include "Logging.h"
#include "Settings.h"

using namespace WebCore;

static const size_t kWebAudioBufferSize = 128;

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
static const size_t kLowPowerVideoBufferSize = 4096;
#endif

void MediaSessionManager::updateSessionState()
{
    LOG(Media, "MediaSessionManager::updateSessionState() - types: Video(%d), Audio(%d), WebAudio(%d)", count(MediaSession::Video), count(MediaSession::Audio), count(MediaSession::WebAudio));

    if (has(MediaSession::WebAudio))
        AudioSession::sharedSession().setPreferredBufferSize(kWebAudioBufferSize);
    // FIXME: <http://webkit.org/b/116725> Figure out why enabling the code below
    // causes media LayoutTests to fail on 10.8.
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    else if ((has(MediaSession::Video) || has(MediaSession::Audio)) && Settings::lowPowerVideoAudioBufferSizeEnabled())
        AudioSession::sharedSession().setPreferredBufferSize(kLowPowerVideoBufferSize);
#endif
}

#endif // USE(AUDIO_SESSION)
