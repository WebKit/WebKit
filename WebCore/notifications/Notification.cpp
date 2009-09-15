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

#if ENABLE(NOTIFICATIONS)

#include "Notification.h"
#include "NotificationContents.h"

#include "Document.h"
#include "EventNames.h"
#include "WorkerContext.h" 

namespace WebCore {

Notification::Notification(const String& url, ScriptExecutionContext* context, ExceptionCode& ec, NotificationPresenter* provider)
    : ActiveDOMObject(context, this)
    , m_isHTML(true)
    , m_isShowing(false)
    , m_presenter(provider)
{
    if (m_presenter->checkPermission(context->securityOrigin()) != NotificationPresenter::PermissionAllowed) {
        ec = SECURITY_ERR;
        return;
    }

    m_notificationURL = context->completeURL(url);
    if (url.isEmpty() || !m_notificationURL.isValid()) {
        ec = SYNTAX_ERR;
        return;
    }
}

Notification::Notification(const NotificationContents& contents, ScriptExecutionContext* context, ExceptionCode& ec, NotificationPresenter* provider)
    : ActiveDOMObject(context, this)
    , m_isHTML(false)
    , m_contents(contents)
    , m_isShowing(false)
    , m_presenter(provider)
{
    if (m_presenter->checkPermission(context->securityOrigin()) != NotificationPresenter::PermissionAllowed) {
        ec = SECURITY_ERR;
        return;
    }

    KURL icon = context->completeURL(contents.icon());
    if (!icon.isEmpty() && !icon.isValid()) {
        ec = SYNTAX_ERR;
        return;
    }
}

Notification::~Notification() 
{
    m_presenter->notificationObjectDestroyed(this);
}

void Notification::show() 
{
    // prevent double-showing
    if (!m_isShowing)
        m_isShowing = m_presenter->show(this);
}

void Notification::cancel() 
{
    if (m_isShowing)
        m_presenter->cancel(this);
}

EventListener* Notification::ondisplay() const
{
    return getAttributeEventListener("display");
}

void Notification::setOndisplay(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener("display", eventListener);
}

EventListener* Notification::onerror() const
{
    return getAttributeEventListener(eventNames().errorEvent);
}

void Notification::setOnerror(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().errorEvent, eventListener);
}

EventListener* Notification::onclose() const
{
    return getAttributeEventListener(eventNames().closeEvent);
}

void Notification::setOnclose(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().closeEvent, eventListener);
}

EventListener* Notification::getAttributeEventListener(const AtomicString& eventType) const
{
    const RegisteredEventListenerVector& listeners = m_eventListeners;
    size_t size = listeners.size();
    for (size_t i = 0; i < size; ++i) {
        const RegisteredEventListener& r = *listeners[i];
        if (r.eventType() == eventType && r.listener()->isAttribute())
            return r.listener();
    }
    return 0;
}

void Notification::setAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener)
{
    clearAttributeEventListener(eventType);
    if (listener)
        addEventListener(eventType, listener, false);
}

void Notification::clearAttributeEventListener(const AtomicString& eventType)
{
    RegisteredEventListenerVector* listeners = &m_eventListeners;
    size_t size = listeners->size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *listeners->at(i);
        if (r.eventType() != eventType || !r.listener()->isAttribute())
            continue;

        r.setRemoved(true);
        listeners->remove(i);
        return;
    }
}

void Notification::dispatchDisplayEvent() 
{   
    RefPtr<Event> event = Event::create("display", false, true);
    ExceptionCode ec = 0;
    dispatchEvent(event.release(), ec);
    ASSERT(!ec);
}

void Notification::dispatchErrorEvent()
{  
    RefPtr<Event> event = Event::create(eventNames().errorEvent, false, true);
    ExceptionCode ec = 0;
    dispatchEvent(event.release(), ec);
    ASSERT(!ec);
}

void Notification::dispatchCloseEvent() 
{   
    RefPtr<Event> event = Event::create(eventNames().closeEvent, false, true);
    ExceptionCode ec = 0;
    dispatchEvent(event.release(), ec);
    ASSERT(!ec);
}

void Notification::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    RefPtr<RegisteredEventListener> registeredListener = RegisteredEventListener::create(eventType, listener, useCapture);
    m_eventListeners.append(registeredListener);
}

void Notification::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    size_t size = m_eventListeners.size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *m_eventListeners[i];
        if (r.eventType() == eventType && r.useCapture() == useCapture && *r.listener() == *listener) {
            r.setRemoved(true);
            m_eventListeners.remove(i);
            return; 
        }
    }
}

void Notification::handleEvent(PassRefPtr<Event> event, bool useCapture)
{
    RegisteredEventListenerVector listenersCopy = m_eventListeners;
    size_t size = listenersCopy.size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *listenersCopy[i];
        if (r.eventType() == event->type() && r.useCapture() == useCapture && !r.removed())
            r.listener()->handleEvent(event.get());
    }
}

bool Notification::dispatchEvent(PassRefPtr<Event> inEvent, ExceptionCode&)
{
    RefPtr<Event> event(inEvent);

    event->setEventPhase(Event::AT_TARGET);
    event->setCurrentTarget(this);

    handleEvent(event.get(), true);
    if (!event->propagationStopped()) {
        handleEvent(event.get(), false);
    }

    return !event->defaultPrevented();
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)
