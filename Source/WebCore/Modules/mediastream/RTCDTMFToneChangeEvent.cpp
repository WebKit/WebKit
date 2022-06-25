/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RTCDTMFToneChangeEvent.h"

#if ENABLE(WEB_RTC)

#include "EventNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCDTMFToneChangeEvent);

Ref<RTCDTMFToneChangeEvent> RTCDTMFToneChangeEvent::create(const String& tone)
{
    return adoptRef(*new RTCDTMFToneChangeEvent(tone));
}

Ref<RTCDTMFToneChangeEvent> RTCDTMFToneChangeEvent::create(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new RTCDTMFToneChangeEvent(type, initializer, isTrusted));
}

RTCDTMFToneChangeEvent::RTCDTMFToneChangeEvent(const String& tone)
    : Event(eventNames().tonechangeEvent, CanBubble::No, IsCancelable::No)
    , m_tone(tone)
{
}

RTCDTMFToneChangeEvent::RTCDTMFToneChangeEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_tone(initializer.tone)
{
}

RTCDTMFToneChangeEvent::~RTCDTMFToneChangeEvent() = default;

const String& RTCDTMFToneChangeEvent::tone() const
{
    return m_tone;
}

EventInterface RTCDTMFToneChangeEvent::eventInterface() const
{
    return RTCDTMFToneChangeEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

