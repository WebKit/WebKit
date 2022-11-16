/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "MockAudioDestinationCocoa.h"

#if ENABLE(WEB_AUDIO)

#include "AudioUtilitiesCocoa.h"
#include "CAAudioStreamDescription.h"
#include "WebAudioBufferList.h"

namespace WebCore {

const int kRenderBufferSize = 128;

MockAudioDestinationCocoa::MockAudioDestinationCocoa(AudioIOCallback& callback, float sampleRate)
    : AudioDestinationCocoa(callback, 2, sampleRate)
    , m_workQueue(WorkQueue::create("MockAudioDestinationCocoa Render Queue"))
    , m_timer(RunLoop::current(), this, &MockAudioDestinationCocoa::tick)
{
}

MockAudioDestinationCocoa::~MockAudioDestinationCocoa() = default;

void MockAudioDestinationCocoa::startRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    m_timer.startRepeating(Seconds { m_numberOfFramesToProcess / sampleRate() });
    setIsPlaying(true);

    callOnMainThread([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(true);
    });
}

void MockAudioDestinationCocoa::stopRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    m_timer.stop();
    setIsPlaying(false);

    callOnMainThread([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(true);
    });
}

void MockAudioDestinationCocoa::tick()
{
    m_workQueue->dispatch([this, protectedThis = Ref { *this }, sampleRate = sampleRate(), numberOfFramesToProcess = m_numberOfFramesToProcess] {
        AudioStreamBasicDescription streamFormat = audioStreamBasicDescriptionForAudioBus(m_outputBus);
        WebAudioBufferList webAudioBufferList { streamFormat, numberOfFramesToProcess };
        render(0., 0, numberOfFramesToProcess, webAudioBufferList.list());
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
