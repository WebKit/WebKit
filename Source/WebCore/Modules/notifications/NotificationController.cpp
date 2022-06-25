/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "NotificationController.h"

#if ENABLE(NOTIFICATIONS)

#include "NotificationClient.h"

namespace WebCore {

NotificationController::NotificationController(NotificationClient* client)
    : m_client(*client)
{
    ASSERT(client);
}

NotificationController::~NotificationController()
{
    m_client.notificationControllerDestroyed();
}

NotificationClient* NotificationController::clientFrom(Page& page)
{
    auto* controller = NotificationController::from(&page);
    if (!controller)
        return nullptr;
    return &controller->client();
}

const char* NotificationController::supplementName()
{
    return "NotificationController";
}

void provideNotification(Page* page, NotificationClient* client)
{
    NotificationController::provideTo(page, NotificationController::supplementName(), makeUnique<NotificationController>(client));
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)
