/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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
#include "RTCDataChannelEvent.h"

#if ENABLE(WEB_RTC)

#include "RTCDataChannel.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCDataChannelEvent);

Ref<RTCDataChannelEvent> RTCDataChannelEvent::create(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, Ref<RTCDataChannel>&& channel)
{
    return adoptRef(*new RTCDataChannelEvent(type, canBubble, cancelable, WTFMove(channel)));
}

Ref<RTCDataChannelEvent> RTCDataChannelEvent::create(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new RTCDataChannelEvent(type, WTFMove(initializer), isTrusted));
}

RTCDataChannelEvent::RTCDataChannelEvent(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, Ref<RTCDataChannel>&& channel)
    : Event(type, canBubble, cancelable)
    , m_channel(WTFMove(channel))
{
}

RTCDataChannelEvent::RTCDataChannelEvent(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_channel(initializer.channel.releaseNonNull())
{
}

RTCDataChannel& RTCDataChannelEvent::channel()
{
    return m_channel.get();
}

EventInterface RTCDataChannelEvent::eventInterface() const
{
    return RTCDataChannelEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

