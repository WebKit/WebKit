/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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

#include "NotificationCenter.h"

#include "ExceptionCode.h"
#include "Notification.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "VoidCallback.h"

namespace WebCore {

Ref<NotificationCenter> NotificationCenter::create(ScriptExecutionContext& context, NotificationClient* client)
{
    auto notificationCenter = adoptRef(*new NotificationCenter(context, client));
    notificationCenter->suspendIfNeeded();
    return notificationCenter;
}

NotificationCenter::NotificationCenter(ScriptExecutionContext& context, NotificationClient* client)
    : ActiveDOMObject(&context)
    , m_client(client)
    , m_timer([this]() { timerFired(); })
{
}

#if ENABLE(LEGACY_NOTIFICATIONS)

ExceptionOr<Ref<Notification>> NotificationCenter::createNotification(const String& iconURI, const String& title, const String& body)
{
    if (!m_client || !scriptExecutionContext())
        return Exception { INVALID_STATE_ERR };
    return Notification::create(title, body, iconURI, *scriptExecutionContext(), *this);
}

int NotificationCenter::checkPermission()
{
    if (!m_client || !scriptExecutionContext())
        return NotificationClient::PermissionDenied;

    switch (scriptExecutionContext()->securityOrigin()->canShowNotifications()) {
    case SecurityOrigin::AlwaysAllow:
        return NotificationClient::PermissionAllowed;
    case SecurityOrigin::AlwaysDeny:
        return NotificationClient::PermissionDenied;
    case SecurityOrigin::Ask:
        return m_client->checkPermission(scriptExecutionContext());
    }

    ASSERT_NOT_REACHED();
    return m_client->checkPermission(scriptExecutionContext());
}

void NotificationCenter::requestPermission(RefPtr<VoidCallback>&& callback)
{
    if (!m_client || !scriptExecutionContext())
        return;

    switch (scriptExecutionContext()->securityOrigin()->canShowNotifications()) {
    case SecurityOrigin::AlwaysAllow:
    case SecurityOrigin::AlwaysDeny:
        if (m_callbacks.isEmpty()) {
            ref(); // Balanced by the derefs in NotificationCenter::stop and NotificationCenter::timerFired.
            m_timer.startOneShot(0);
        }
        m_callbacks.append([callback]() {
            if (callback)
                callback->handleEvent();
        });
        return;
    case SecurityOrigin::Ask:
        m_client->requestPermission(scriptExecutionContext(), WTFMove(callback));
        return;
    }

    ASSERT_NOT_REACHED();
    m_client->requestPermission(scriptExecutionContext(), WTFMove(callback));
}

#endif

void NotificationCenter::stop()
{
    if (!m_client)
        return;

    // Clear m_client immediately to guarantee reentrant calls to NotificationCenter do nothing.
    // Also protect |this| so it's not indirectly destroyed under us by work done by the client.
    auto& client = *std::exchange(m_client, nullptr);
    Ref<NotificationCenter> protectedThis(*this);

    if (!m_callbacks.isEmpty())
        deref(); // Balanced by the ref in NotificationCenter::requestPermission.

    m_timer.stop();
    m_callbacks.clear();

    client.cancelRequestsForPermission(scriptExecutionContext());
    client.clearNotifications(scriptExecutionContext());
}

const char* NotificationCenter::activeDOMObjectName() const
{
    return "NotificationCenter";
}

bool NotificationCenter::canSuspendForDocumentSuspension() const
{
    // We don't need to worry about Notifications because those are ActiveDOMObject too.
    // The NotificationCenter can safely be suspended if there are no pending permission requests.
    return m_callbacks.isEmpty() && (!m_client || !m_client->hasPendingPermissionRequests(scriptExecutionContext()));
}

void NotificationCenter::timerFired()
{
    ASSERT(m_client);
    auto callbacks = WTFMove(m_callbacks);
    for (auto& callback : callbacks)
        callback();
    deref(); // Balanced by the ref in NotificationCenter::requestPermission.
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
