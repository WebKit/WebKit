/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
 */

#ifndef WebKitMouseForceEvent_h
#define WebKitMouseForceEvent_h

#if ENABLE(MOUSE_FORCE_EVENTS)

#include "MouseEvent.h"

namespace WebCore {

struct WebKitMouseForceEventInit : public MouseEventInit {
    double force { 0 };
};

class WebKitMouseForceEvent final : public MouseEvent {
public:
    static Ref<WebKitMouseForceEvent> create()
    {
        return adoptRef(*new WebKitMouseForceEvent);
    }
    static Ref<WebKitMouseForceEvent> create(const AtomicString& type, double force, const PlatformMouseEvent& event, PassRefPtr<AbstractView> view)
    {
        return adoptRef(*new WebKitMouseForceEvent(type, force, event, view));
    }
    static Ref<WebKitMouseForceEvent> create(const AtomicString& type, const WebKitMouseForceEventInit& initializer)
    {
        return adoptRef(*new WebKitMouseForceEvent(type, initializer));
    }

    double force() const { return m_force; }

private:
    WebKitMouseForceEvent();
    WebKitMouseForceEvent(const AtomicString&, double m_force, const PlatformMouseEvent&, PassRefPtr<AbstractView>);
    WebKitMouseForceEvent(const AtomicString&, const WebKitMouseForceEventInit&);

    virtual EventInterface eventInterface() const override;

    double m_force { 0 };
};

} // namespace WebCore

#endif // ENABLE(MOUSE_FORCE_EVENTS)

#endif // WebKitMouseForceEvent_h
