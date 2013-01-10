/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef AutocompleteErrorEvent_h
#define AutocompleteErrorEvent_h

#if ENABLE(REQUEST_AUTOCOMPLETE)

#include "Event.h"
#include "EventNames.h"

namespace WebCore {

struct AutocompleteErrorEventInit : public EventInit {
    String reason;
};

class AutocompleteErrorEvent : public Event {
public:
    static PassRefPtr<AutocompleteErrorEvent> create()
    {
        return adoptRef(new AutocompleteErrorEvent);
    }

    static PassRefPtr<AutocompleteErrorEvent> create(const String& reason)
    {
        return adoptRef(new AutocompleteErrorEvent(reason));
    }

    static PassRefPtr<AutocompleteErrorEvent> create(const AtomicString& eventType, const AutocompleteErrorEventInit& initializer)
    {
        return adoptRef(new AutocompleteErrorEvent(eventType, initializer));
    }

    const String& reason() const { return m_reason; }

    virtual const AtomicString& interfaceName() const { return eventNames().interfaceForAutocompleteErrorEvent; }

private:
    AutocompleteErrorEvent()
    {
    }

    AutocompleteErrorEvent(const String& reason)
        : Event(eventNames().autocompleteerrorEvent, false, false)
        , m_reason(reason)
    {
    }

    AutocompleteErrorEvent(const AtomicString& eventType, const AutocompleteErrorEventInit& initializer)
        : Event(eventType, initializer)
        , m_reason(initializer.reason)
    {
    }

    String m_reason;
};

} // namespace WebCore

#endif // ENABLE(REQUEST_AUTOCOMPLETE)

#endif // AutocompleteErrorEvent_h
