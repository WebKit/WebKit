/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebDOMEvent.h"

#include "Event.h"
#include "EventNames.h"
#include "Node.h"
#include <wtf/PassRefPtr.h>

using WebCore::eventNames;

namespace WebKit {

class WebDOMEventPrivate : public WebCore::Event {
};

void WebDOMEvent::reset()
{
    assign(0);
}

void WebDOMEvent::assign(const WebDOMEvent& other)
{
    WebDOMEventPrivate* p = const_cast<WebDOMEventPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

void WebDOMEvent::assign(WebDOMEventPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

WebDOMEvent::WebDOMEvent(const WTF::PassRefPtr<WebCore::Event>& event)
    : m_private(static_cast<WebDOMEventPrivate*>(event.leakRef()))
{
}

WebDOMEvent::operator WTF::PassRefPtr<WebCore::Event>() const
{
    return static_cast<WebCore::Event*>(m_private);
}

WebString WebDOMEvent::type() const
{
    ASSERT(m_private);
    return m_private->type();
}

WebNode WebDOMEvent::target() const
{
    ASSERT(m_private);
    return WebNode(m_private->target()->toNode());
}

WebNode WebDOMEvent::currentTarget() const
{
    ASSERT(m_private);
    return WebNode(m_private->currentTarget()->toNode());
}

WebDOMEvent::PhaseType WebDOMEvent::eventPhase() const
{
    ASSERT(m_private);
    return static_cast<WebDOMEvent::PhaseType>(m_private->eventPhase());
}

bool WebDOMEvent::bubbles() const
{
    ASSERT(m_private);
    return m_private->bubbles();
}

bool WebDOMEvent::cancelable() const
{
    ASSERT(m_private);
    return m_private->cancelable();
}

bool WebDOMEvent::isUIEvent() const
{
    ASSERT(m_private);
    return m_private->isUIEvent();
}

bool WebDOMEvent::isMouseEvent() const
{
    ASSERT(m_private);
    return m_private->isMouseEvent();
}

bool WebDOMEvent::isKeyboardEvent() const
{
    ASSERT(m_private);
    return m_private->isKeyboardEvent();
}

bool WebDOMEvent::isMutationEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(WebCore::eventNames().interfaceForMutationEvent);
}

bool WebDOMEvent::isTextEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForTextEvent);
}

bool WebDOMEvent::isCompositionEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForCompositionEvent);
}

bool WebDOMEvent::isDragEvent() const
{
    ASSERT(m_private);
    return m_private->isDragEvent();
}

bool WebDOMEvent::isClipboardEvent() const
{
    ASSERT(m_private);
    return m_private->isClipboardEvent();
}

bool WebDOMEvent::isMessageEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForMessageEvent);
}

bool WebDOMEvent::isWheelEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForWheelEvent);
}

bool WebDOMEvent::isBeforeTextInsertedEvent() const
{
    ASSERT(m_private);
    return m_private->isBeforeTextInsertedEvent();
}

bool WebDOMEvent::isOverflowEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForOverflowEvent);
}

bool WebDOMEvent::isPageTransitionEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForPageTransitionEvent);
}

bool WebDOMEvent::isPopStateEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForPopStateEvent);
}

bool WebDOMEvent::isProgressEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForProgressEvent);
}

bool WebDOMEvent::isXMLHttpRequestProgressEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForXMLHttpRequestProgressEvent);
}

bool WebDOMEvent::isWebKitAnimationEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForWebKitAnimationEvent);
}

bool WebDOMEvent::isWebKitTransitionEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForWebKitTransitionEvent);
}

bool WebDOMEvent::isBeforeLoadEvent() const
{
    ASSERT(m_private);
    return m_private->hasInterface(eventNames().interfaceForBeforeLoadEvent);
}

} // namespace WebKit
