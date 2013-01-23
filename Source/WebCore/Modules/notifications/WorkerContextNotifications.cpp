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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "WorkerContextNotifications.h"

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)

#include "NotificationCenter.h"
#include "WorkerContext.h"
#include "WorkerThread.h"

namespace WebCore {

WorkerContextNotifications::WorkerContextNotifications(WorkerContext* context)
    : m_context(context)
{
}

WorkerContextNotifications::~WorkerContextNotifications()
{
}

const char* WorkerContextNotifications::supplementName()
{
    return "WorkerContextNotifications";
}

WorkerContextNotifications* WorkerContextNotifications::from(WorkerContext* context)
{
    WorkerContextNotifications* supplement = static_cast<WorkerContextNotifications*>(Supplement<ScriptExecutionContext>::from(context, supplementName()));
    if (!supplement) {
        supplement = new WorkerContextNotifications(context);
        Supplement<ScriptExecutionContext>::provideTo(context, supplementName(), adoptPtr(supplement));
    }
    return supplement;
}

NotificationCenter* WorkerContextNotifications::webkitNotifications(WorkerContext* context)
{
    return WorkerContextNotifications::from(context)->webkitNotifications();
}

NotificationCenter* WorkerContextNotifications::webkitNotifications()
{
    if (!m_notificationCenter)
        m_notificationCenter = NotificationCenter::create(m_context, m_context->thread()->getNotificationClient());
    return m_notificationCenter.get();
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
