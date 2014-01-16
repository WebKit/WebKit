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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebKitPlaybackTargetAvailabilityEvent_h
#define WebKitPlaybackTargetAvailabilityEvent_h

#if ENABLE(IOS_AIRPLAY)

#include "Event.h"
#include "EventNames.h"

namespace WebCore {

struct WebKitPlaybackTargetAvailabilityEventInit : public EventInit {
    WebKitPlaybackTargetAvailabilityEventInit()
    {
    };

    String availability;
};

class WebKitPlaybackTargetAvailabilityEvent : public Event {
public:
    ~WebKitPlaybackTargetAvailabilityEvent() { }

    static PassRefPtr<WebKitPlaybackTargetAvailabilityEvent> create()
    {
        return adoptRef(new WebKitPlaybackTargetAvailabilityEvent);
    }

    static PassRefPtr<WebKitPlaybackTargetAvailabilityEvent> create(const AtomicString& eventType, bool available)
    {
        return adoptRef(new WebKitPlaybackTargetAvailabilityEvent(eventType, available));
    }

    static PassRefPtr<WebKitPlaybackTargetAvailabilityEvent> create(const AtomicString& eventType, const WebKitPlaybackTargetAvailabilityEventInit& initializer)
    {
        return adoptRef(new WebKitPlaybackTargetAvailabilityEvent(eventType, initializer));
    }

    String availability() const { return m_availability; }

    virtual EventInterface eventInterface() const override { return WebKitPlaybackTargetAvailabilityEventInterfaceType; }

private:
    WebKitPlaybackTargetAvailabilityEvent();
    explicit WebKitPlaybackTargetAvailabilityEvent(const AtomicString& eventType, bool available);
    WebKitPlaybackTargetAvailabilityEvent(const AtomicString& eventType, const WebKitPlaybackTargetAvailabilityEventInit&);

    String m_availability;
};

} // namespace WebCore

#endif // ENABLE(IOS_AIRPLAY)

#endif // WebKitPlaybackTargetAvailabilityEvent_h
