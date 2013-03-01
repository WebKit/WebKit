/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009, 2011, 2012 Apple Inc. All rights reserved.
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

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)

#include "Notification.h"

#include "DOMWindow.h"
#include "DOMWindowNotifications.h"
#include "Dictionary.h"
#include "Document.h"
#include "ErrorEvent.h"
#include "EventNames.h"
#include "NotificationCenter.h"
#include "NotificationClient.h"
#include "NotificationController.h"
#include "NotificationPermissionCallback.h"
#include "WindowFocusAllowedIndicator.h"

namespace WebCore {

Notification::Notification()
    : ActiveDOMObject(0, this)
{
}

#if ENABLE(LEGACY_NOTIFICATIONS)
Notification::Notification(const KURL& url, ScriptExecutionContext* context, ExceptionCode& ec, PassRefPtr<NotificationCenter> provider)
    : ActiveDOMObject(context, this)
    , m_isHTML(true)
    , m_state(Idle)
    , m_notificationCenter(provider)
{
    if (m_notificationCenter->checkPermission() != NotificationClient::PermissionAllowed) {
        ec = SECURITY_ERR;
        return;
    }

    if (url.isEmpty() || !url.isValid()) {
        ec = SYNTAX_ERR;
        return;
    }

    m_notificationURL = url;
}
#endif

#if ENABLE(LEGACY_NOTIFICATIONS)
Notification::Notification(const String& title, const String& body, const String& iconURI, ScriptExecutionContext* context, ExceptionCode& ec, PassRefPtr<NotificationCenter> provider)
    : ActiveDOMObject(context, this)
    , m_isHTML(false)
    , m_title(title)
    , m_body(body)
    , m_state(Idle)
    , m_notificationCenter(provider)
{
    if (m_notificationCenter->checkPermission() != NotificationClient::PermissionAllowed) {
        ec = SECURITY_ERR;
        return;
    }

    m_icon = iconURI.isEmpty() ? KURL() : scriptExecutionContext()->completeURL(iconURI);
    if (!m_icon.isEmpty() && !m_icon.isValid()) {
        ec = SYNTAX_ERR;
        return;
    }
}
#endif

#if ENABLE(NOTIFICATIONS)
Notification::Notification(ScriptExecutionContext* context, const String& title)
    : ActiveDOMObject(context, this)
    , m_isHTML(false)
    , m_title(title)
    , m_state(Idle)
    , m_taskTimer(adoptPtr(new Timer<Notification>(this, &Notification::taskTimerFired)))
{
    ASSERT_WITH_SECURITY_IMPLICATION(context->isDocument());
    m_notificationCenter = DOMWindowNotifications::webkitNotifications(static_cast<Document*>(context)->domWindow());
    
    ASSERT(m_notificationCenter->client());
    m_taskTimer->startOneShot(0);
}
#endif

Notification::~Notification() 
{
}

#if ENABLE(LEGACY_NOTIFICATIONS)
PassRefPtr<Notification> Notification::create(const KURL& url, ScriptExecutionContext* context, ExceptionCode& ec, PassRefPtr<NotificationCenter> provider) 
{ 
    RefPtr<Notification> notification(adoptRef(new Notification(url, context, ec, provider)));
    notification->suspendIfNeeded();
    return notification.release();
}

PassRefPtr<Notification> Notification::create(const String& title, const String& body, const String& iconURI, ScriptExecutionContext* context, ExceptionCode& ec, PassRefPtr<NotificationCenter> provider) 
{ 
    RefPtr<Notification> notification(adoptRef(new Notification(title, body, iconURI, context, ec, provider)));
    notification->suspendIfNeeded();
    return notification.release();
}
#endif

#if ENABLE(NOTIFICATIONS)
PassRefPtr<Notification> Notification::create(ScriptExecutionContext* context, const String& title, const Dictionary& options)
{
    RefPtr<Notification> notification(adoptRef(new Notification(context, title)));
    String argument;
    if (options.get("body", argument))
        notification->setBody(argument);
    if (options.get("tag", argument))
        notification->setTag(argument);
    if (options.get("lang", argument))
        notification->setLang(argument);
    if (options.get("dir", argument))
        notification->setDir(argument);
    if (options.get("icon", argument)) {
        KURL iconURI = argument.isEmpty() ? KURL() : context->completeURL(argument);
        if (!iconURI.isEmpty() && iconURI.isValid())
            notification->setIconURL(iconURI);
    }

    notification->suspendIfNeeded();
    return notification.release();
}
#endif

const AtomicString& Notification::interfaceName() const
{
    return eventNames().interfaceForNotification;
}

void Notification::show() 
{
    // prevent double-showing
    if (m_state == Idle && m_notificationCenter->client()) {
#if ENABLE(NOTIFICATIONS)
        if (!static_cast<Document*>(scriptExecutionContext())->page())
            return;
        if (NotificationController::from(static_cast<Document*>(scriptExecutionContext())->page())->client()->checkPermission(scriptExecutionContext()) != NotificationClient::PermissionAllowed) {
            dispatchErrorEvent();
            return;
        }
#endif
        if (m_notificationCenter->client()->show(this)) {
            m_state = Showing;
            setPendingActivity(this);
        }
    }
}

void Notification::close()
{
    switch (m_state) {
    case Idle:
        break;
    case Showing:
        if (m_notificationCenter->client())
            m_notificationCenter->client()->cancel(this);
        break;
    case Closed:
        break;
    }
}

EventTargetData* Notification::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* Notification::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void Notification::contextDestroyed()
{
    ActiveDOMObject::contextDestroyed();
    if (m_notificationCenter->client())
        m_notificationCenter->client()->notificationObjectDestroyed(this);
}

void Notification::finalize()
{
    if (m_state == Closed)
        return;
    m_state = Closed;
    unsetPendingActivity(this);
}

void Notification::dispatchShowEvent()
{
    dispatchEvent(Event::create(eventNames().showEvent, false, false));
}

void Notification::dispatchClickEvent()
{
    WindowFocusAllowedIndicator windowFocusAllowed;
    dispatchEvent(Event::create(eventNames().clickEvent, false, false));
}

void Notification::dispatchCloseEvent()
{
    dispatchEvent(Event::create(eventNames().closeEvent, false, false));
    finalize();
}

void Notification::dispatchErrorEvent()
{
    dispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

#if ENABLE(NOTIFICATIONS)
void Notification::taskTimerFired(Timer<Notification>* timer)
{
    ASSERT(scriptExecutionContext()->isDocument());
    ASSERT_UNUSED(timer, timer == m_taskTimer.get());
    show();
}
#endif


#if ENABLE(NOTIFICATIONS)
const String& Notification::permission(ScriptExecutionContext* context)
{
    ASSERT_WITH_SECURITY_IMPLICATION(context->isDocument());
    ASSERT(static_cast<Document*>(context)->page());
    return permissionString(NotificationController::from(static_cast<Document*>(context)->page())->client()->checkPermission(context));
}

const String& Notification::permissionString(NotificationClient::Permission permission)
{
    DEFINE_STATIC_LOCAL(const String, allowedPermission, (ASCIILiteral("granted")));
    DEFINE_STATIC_LOCAL(const String, deniedPermission, (ASCIILiteral("denied")));
    DEFINE_STATIC_LOCAL(const String, defaultPermission, (ASCIILiteral("default")));

    switch (permission) {
    case NotificationClient::PermissionAllowed:
        return allowedPermission;
    case NotificationClient::PermissionDenied:
        return deniedPermission;
    case NotificationClient::PermissionNotAllowed:
        return defaultPermission;
    }
    
    ASSERT_NOT_REACHED();
    return deniedPermission;
}

void Notification::requestPermission(ScriptExecutionContext* context, PassRefPtr<NotificationPermissionCallback> callback)
{
    ASSERT_WITH_SECURITY_IMPLICATION(context->isDocument());
    ASSERT(static_cast<Document*>(context)->page());
    NotificationController::from(static_cast<Document*>(context)->page())->client()->requestPermission(context, callback);
}
#endif

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
