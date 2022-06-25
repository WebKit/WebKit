/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioProcessingEvent.h"

#include "AudioBuffer.h"
#include "AudioProcessingEventInit.h"
#include "EventNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioProcessingEvent);

Ref<AudioProcessingEvent> AudioProcessingEvent::create(const AtomString& eventType, AudioProcessingEventInit&& eventInitDict)
{
    RELEASE_ASSERT(eventInitDict.inputBuffer);
    RELEASE_ASSERT(eventInitDict.outputBuffer);
    return adoptRef(*new AudioProcessingEvent(eventType, WTFMove(eventInitDict)));
}

AudioProcessingEvent::AudioProcessingEvent(RefPtr<AudioBuffer>&& inputBuffer, RefPtr<AudioBuffer>&& outputBuffer, double playbackTime)
    : Event(eventNames().audioprocessEvent, CanBubble::Yes, IsCancelable::No)
    , m_inputBuffer(WTFMove(inputBuffer))
    , m_outputBuffer(WTFMove(outputBuffer))
    , m_playbackTime(playbackTime)
{
}

AudioProcessingEvent::AudioProcessingEvent(const AtomString& eventType, AudioProcessingEventInit&& eventInitDict)
    : Event(eventType, eventInitDict, IsTrusted::No)
    , m_inputBuffer(eventInitDict.inputBuffer.releaseNonNull())
    , m_outputBuffer(eventInitDict.outputBuffer.releaseNonNull())
    , m_playbackTime(eventInitDict.playbackTime)
{
}

AudioProcessingEvent::~AudioProcessingEvent() = default;

EventInterface AudioProcessingEvent::eventInterface() const
{
    return AudioProcessingEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
