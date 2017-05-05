/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "WorkerGlobalScopeNotifications.h"

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)

#include "NotificationCenter.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"

namespace WebCore {

WorkerGlobalScopeNotifications::WorkerGlobalScopeNotifications(WorkerGlobalScope& scope)
    : m_context(&scope)
{
}

WorkerGlobalScopeNotifications::~WorkerGlobalScopeNotifications()
{
}

const char* WorkerGlobalScopeNotifications::supplementName()
{
    return "WorkerGlobalScopeNotifications";
}

WorkerGlobalScopeNotifications* WorkerGlobalScopeNotifications::from(WorkerGlobalScope& scope)
{
    WorkerGlobalScopeNotifications* supplement = static_cast<WorkerGlobalScopeNotifications*>(Supplement<WorkerGlobalScope>::from(&scope, supplementName()));
    if (!supplement) {
        auto newSupplement = std::make_unique<WorkerGlobalScopeNotifications>(scope);
        supplement = newSupplement.get();
        provideTo(&scope, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

NotificationCenter* WorkerGlobalScopeNotifications::webkitNotifications(WorkerGlobalScope& scope)
{
    return WorkerGlobalScopeNotifications::from(scope)->webkitNotifications();
}

NotificationCenter* WorkerGlobalScopeNotifications::webkitNotifications()
{
    // FIXME: As of this writing, this always passes nullptr for the client.
    // If it wasn't for that, the notification center create function could be taking a reference.
    // How is it useful to create a notification center with no client?
    if (!m_notificationCenter)
        m_notificationCenter = NotificationCenter::create(*m_context, m_context->thread().getNotificationClient());
    return m_notificationCenter.get();
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
