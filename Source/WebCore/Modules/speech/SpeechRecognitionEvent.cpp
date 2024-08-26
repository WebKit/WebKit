/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SpeechRecognitionEvent.h"

#include "SpeechRecognitionResultList.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SpeechRecognitionEvent);

Ref<SpeechRecognitionEvent> SpeechRecognitionEvent::create(const AtomString& type, Init&& init, IsTrusted isTrusted)
{
    return adoptRef(*new SpeechRecognitionEvent(type, WTFMove(init), isTrusted));
}

Ref<SpeechRecognitionEvent> SpeechRecognitionEvent::create(const AtomString& type, uint64_t resultIndex, RefPtr<SpeechRecognitionResultList>&& results)
{
    return adoptRef(*new SpeechRecognitionEvent(type, resultIndex, WTFMove(results)));
}

SpeechRecognitionEvent::SpeechRecognitionEvent(const AtomString& type, Init&& init, IsTrusted isTrusted)
    : Event(type, init, isTrusted)
    , m_resultIndex(init.resultIndex)
    , m_results(WTFMove(init.results))
{
}

SpeechRecognitionEvent::SpeechRecognitionEvent(const AtomString& type, uint64_t resultIndex, RefPtr<SpeechRecognitionResultList>&& results)
    : Event(type, Event::CanBubble::No, Event::IsCancelable::No)
    , m_resultIndex(resultIndex)
    , m_results(WTFMove(results))
{
}

EventInterface SpeechRecognitionEvent::eventInterface() const
{
    return SpeechRecognitionEventInterfaceType;
}

SpeechRecognitionEvent::~SpeechRecognitionEvent() = default;

} // namespace WebCore
