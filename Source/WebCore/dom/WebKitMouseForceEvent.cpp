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

#include "config.h"
#include "WebKitMouseForceEvent.h"

#include "DataTransfer.h"
#include "EventNames.h"
#include "PlatformMouseEvent.h"

namespace WebCore {

WebKitMouseForceEvent::WebKitMouseForceEvent()
{
}

WebKitMouseForceEvent::WebKitMouseForceEvent(const AtomicString& type, double force, const PlatformMouseEvent& event, PassRefPtr<AbstractView> view)
    : MouseEvent(type, true, true, event.timestamp(), view, 0, event.globalPosition().x(), event.globalPosition().y(), event.position().x(), event.position().y()
#if ENABLE(POINTER_LOCK)
    , 0, 0
#endif
    , event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(), 0, 0, 0, false)
    , m_force(force)
{
}

WebKitMouseForceEvent::WebKitMouseForceEvent(const AtomicString& type, const WebKitMouseForceEventInit& initializer)
    : MouseEvent(type, initializer)
    , m_force(initializer.force)
{
}

EventInterface WebKitMouseForceEvent::eventInterface() const
{
    return WebKitMouseForceEventInterfaceType;
}

} // namespace WebCore
