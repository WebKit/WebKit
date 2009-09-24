/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

EventTargetData* Notification::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* Notification::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)
