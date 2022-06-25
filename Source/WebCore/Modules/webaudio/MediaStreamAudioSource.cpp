/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MediaStreamAudioSource.h"

#if ENABLE(MEDIA_STREAM)

#include "NotImplemented.h"
#include "PlatformAudioData.h"

namespace WebCore {

MediaStreamAudioSource::MediaStreamAudioSource(float sampleRate)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Audio, "MediaStreamAudioDestinationNode"_s)
{
    m_currentSettings.setSampleRate(sampleRate);
}

MediaStreamAudioSource::~MediaStreamAudioSource() = default;

const RealtimeMediaSourceCapabilities& MediaStreamAudioSource::capabilities()
{
    // FIXME: implement this.
    // https://bugs.webkit.org/show_bug.cgi?id=122430
    notImplemented();
    return RealtimeMediaSourceCapabilities::emptyCapabilities();
}

const RealtimeMediaSourceSettings& MediaStreamAudioSource::settings()
{
    // FIXME: implement this.
    // https://bugs.webkit.org/show_bug.cgi?id=122430
    notImplemented();
    return m_currentSettings;
}

#if !PLATFORM(COCOA) && !USE(GSTREAMER)
void MediaStreamAudioSource::consumeAudio(AudioBus&, size_t)
{
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
