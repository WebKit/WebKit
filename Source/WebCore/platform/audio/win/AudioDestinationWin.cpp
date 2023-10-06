/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc. All rights reserved.
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
#include "AudioDestinationWin.h"

#if ENABLE(WEB_AUDIO)

namespace WebCore {

Ref<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String&, unsigned, unsigned numberOfOutputChannels, float sampleRate)
{
    return adoptRef(*new AudioDestinationWin(callback, numberOfOutputChannels, sampleRate));
}

AudioDestinationWin::AudioDestinationWin(AudioIOCallback& callback, unsigned, float sampleRate)
    : AudioDestination(callback, sampleRate)
{
    ASSERT_NOT_REACHED();
}

AudioDestinationWin::~AudioDestinationWin()
{
    ASSERT_NOT_REACHED();
}

void AudioDestinationWin::start(Function<void(Function<void()>&&)>&&, CompletionHandler<void(bool)>&&)
{
    ASSERT_NOT_REACHED();
}

void AudioDestinationWin::stop(CompletionHandler<void(bool)>&&)
{
    ASSERT_NOT_REACHED();
}

unsigned AudioDestinationWin::framesPerBuffer() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

float AudioDestination::hardwareSampleRate()
{
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned long AudioDestination::maxChannelCount()
{
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
