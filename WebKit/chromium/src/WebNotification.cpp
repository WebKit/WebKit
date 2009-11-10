/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebNotification.h"

#if ENABLE(NOTIFICATIONS)

#include "Notification.h"

#include "WebString.h"
#include "WebURL.h"

#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

class WebNotificationPrivate : public Notification {
};

void WebNotification::reset()
{
    assign(0);
}

void WebNotification::assign(const WebNotification& other)
{
    WebNotificationPrivate* p = const_cast<WebNotificationPrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

bool WebNotification::lessThan(const WebNotification& other) const
{
    return reinterpret_cast<uintptr_t>(m_private) < reinterpret_cast<uintptr_t>(other.m_private);
}

bool WebNotification::isHTML() const
{
    return m_private->isHTML();
}

WebURL WebNotification::url() const
{
    ASSERT(isHTML());
    return m_private->url();
}

WebString WebNotification::icon() const
{
    ASSERT(!isHTML());
    return m_private->contents().icon();
}

WebString WebNotification::title() const
{
    ASSERT(!isHTML());
    return m_private->contents().title();
}

WebString WebNotification::body() const
{
    ASSERT(!isHTML());
    return m_private->contents().body();
}

void WebNotification::dispatchDisplayEvent()
{
    RefPtr<Event> event = Event::create("display", false, true);
    m_private->dispatchEvent(event.release());
}

void WebNotification::dispatchErrorEvent(const WebKit::WebString& /* errorMessage */)
{
    // FIXME: errorMessage not supported by WebCore yet
    RefPtr<Event> event = Event::create(eventNames().errorEvent, false, true);
    m_private->dispatchEvent(event.release());
}

void WebNotification::dispatchCloseEvent(bool /* byUser */)
{
    // FIXME: byUser flag not supported by WebCore yet
    RefPtr<Event> event = Event::create(eventNames().closeEvent, false, true);
    m_private->dispatchEvent(event.release());
}

WebNotification::WebNotification(const WTF::PassRefPtr<Notification>& notification)
    : m_private(static_cast<WebNotificationPrivate*>(notification.releaseRef()))
{
}

WebNotification& WebNotification::operator=(const WTF::PassRefPtr<Notification>& notification)
{
    assign(static_cast<WebNotificationPrivate*>(notification.releaseRef()));
    return *this;
}

WebNotification::operator WTF::PassRefPtr<Notification>() const
{
    return WTF::PassRefPtr<Notification>(const_cast<WebNotificationPrivate*>(m_private));
}

void WebNotification::assign(WebNotificationPrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

} // namespace WebKit

#endif // ENABLE(NOTIFICATIONS)
