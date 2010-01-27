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
#include "WebEvent.h"

#include "Event.h"
#include "Node.h"
#include <wtf/PassRefPtr.h>

namespace WebKit {

class WebEventPrivate : public WebCore::Event {
};

void WebEvent::reset()
{
    assign(0);
}

void WebEvent::assign(const WebEvent& other)
{
    WebEventPrivate* p = const_cast<WebEventPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

void WebEvent::assign(WebEventPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

WebEvent::WebEvent(const WTF::PassRefPtr<WebCore::Event>& event)
    : m_private(static_cast<WebEventPrivate*>(event.releaseRef()))
{
}

WebString WebEvent::type() const
{
    ASSERT(m_private);
    return m_private->type();
}

WebNode WebEvent::target() const
{
    ASSERT(m_private);
    return WebNode(m_private->target()->toNode());
}

WebNode WebEvent::currentTarget() const
{
    ASSERT(m_private);
    return WebNode(m_private->currentTarget()->toNode());
}

WebEvent::PhaseType WebEvent::eventPhase() const
{
    ASSERT(m_private);
    return static_cast<WebEvent::PhaseType>(m_private->eventPhase());
}

bool WebEvent::bubbles() const
{
    ASSERT(m_private);
    return m_private->bubbles();
}

bool WebEvent::cancelable() const
{
    ASSERT(m_private);
    return m_private->cancelable();
}

bool WebEvent::isUIEvent() const
{
    ASSERT(m_private);
    return m_private->isUIEvent();
}

bool WebEvent::isMouseEvent() const
{
    ASSERT(m_private);
    return m_private->isMouseEvent();
}

bool WebEvent::isMutationEvent() const
{
    ASSERT(m_private);
    return m_private->isMutationEvent();
}

bool WebEvent::isKeyboardEvent() const
{
    ASSERT(m_private);
    return m_private->isKeyboardEvent();
}

bool WebEvent::isTextEvent() const
{
    ASSERT(m_private);
    return m_private->isTextEvent();
}

bool WebEvent::isCompositionEvent() const
{
    ASSERT(m_private);
    return m_private->isCompositionEvent();
}

bool WebEvent::isDragEvent() const
{
    ASSERT(m_private);
    return m_private->isDragEvent();
}

bool WebEvent::isClipboardEvent() const
{
    ASSERT(m_private);
    return m_private->isClipboardEvent();
}

bool WebEvent::isMessageEvent() const
{
    ASSERT(m_private);
    return m_private->isMessageEvent();
}

bool WebEvent::isWheelEvent() const
{
    ASSERT(m_private);
    return m_private->isWheelEvent();
}

bool WebEvent::isBeforeTextInsertedEvent() const
{
    ASSERT(m_private);
    return m_private->isBeforeTextInsertedEvent();
}

bool WebEvent::isOverflowEvent() const
{
    ASSERT(m_private);
    return m_private->isOverflowEvent();
}

bool WebEvent::isPageTransitionEvent() const
{
    ASSERT(m_private);
    return m_private->isPageTransitionEvent();
}

bool WebEvent::isPopStateEvent() const
{
    ASSERT(m_private);
    return m_private->isPopStateEvent();
}

bool WebEvent::isProgressEvent() const
{
    ASSERT(m_private);
    return m_private->isProgressEvent();
}

bool WebEvent::isXMLHttpRequestProgressEvent() const
{
    ASSERT(m_private);
    return m_private->isXMLHttpRequestProgressEvent();
}

bool WebEvent::isWebKitAnimationEvent() const
{
    ASSERT(m_private);
    return m_private->isWebKitAnimationEvent();
}

bool WebEvent::isWebKitTransitionEvent() const
{
    ASSERT(m_private);
    return m_private->isWebKitTransitionEvent();
}

bool WebEvent::isBeforeLoadEvent() const
{
    ASSERT(m_private);
    return m_private->isBeforeLoadEvent();
}

} // namespace WebKit
