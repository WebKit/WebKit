/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#pragma once

#include "Event.h"
#include "EventNames.h"

namespace WebCore {

class BeforeLoadEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(BeforeLoadEvent);
public:
    static Ref<BeforeLoadEvent> create(const String& url)
    {
        return adoptRef(*new BeforeLoadEvent(url));
    }

    struct Init : EventInit {
        String url;
    };

    static Ref<BeforeLoadEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new BeforeLoadEvent(type, initializer, isTrusted));
    }

    const String& url() const { return m_url; }

    EventInterface eventInterface() const override { return BeforeLoadEventInterfaceType; }

private:
    explicit BeforeLoadEvent(const String& url)
        : Event(eventNames().beforeloadEvent, CanBubble::No, IsCancelable::Yes)
        , m_url(url)
    {
    }

    BeforeLoadEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
        : Event(type, initializer, isTrusted)
        , m_url(initializer.url)
    {
    }

    String m_url;
};

} // namespace WebCore
