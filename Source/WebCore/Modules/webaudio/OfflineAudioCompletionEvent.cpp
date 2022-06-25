/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "OfflineAudioCompletionEvent.h"

#include "AudioBuffer.h"
#include "EventNames.h"
#include "OfflineAudioCompletionEventInit.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OfflineAudioCompletionEvent);

Ref<OfflineAudioCompletionEvent> OfflineAudioCompletionEvent::create(Ref<AudioBuffer>&& renderedBuffer)
{
    return adoptRef(*new OfflineAudioCompletionEvent(WTFMove(renderedBuffer)));
}

Ref<OfflineAudioCompletionEvent> OfflineAudioCompletionEvent::create(const AtomString& eventType, OfflineAudioCompletionEventInit&& init)
{
    RELEASE_ASSERT(init.renderedBuffer);
    return adoptRef(*new OfflineAudioCompletionEvent(eventType, WTFMove(init)));
}

OfflineAudioCompletionEvent::OfflineAudioCompletionEvent(Ref<AudioBuffer>&& renderedBuffer)
    : Event(eventNames().completeEvent, CanBubble::Yes, IsCancelable::No)
    , m_renderedBuffer(WTFMove(renderedBuffer))
{
}

OfflineAudioCompletionEvent::OfflineAudioCompletionEvent(const AtomString& eventType, OfflineAudioCompletionEventInit&& init)
    : Event(eventType, init, IsTrusted::No)
    , m_renderedBuffer(init.renderedBuffer.releaseNonNull())
{
}

OfflineAudioCompletionEvent::~OfflineAudioCompletionEvent() = default;

EventInterface OfflineAudioCompletionEvent::eventInterface() const
{
    return OfflineAudioCompletionEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
