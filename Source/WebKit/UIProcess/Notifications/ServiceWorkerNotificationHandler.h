/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "NotificationManagerMessageHandler.h"
#include <WebCore/PageIdentifier.h>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>

namespace WebKit {

class WebsiteDataStore;

class ServiceWorkerNotificationHandler final : public NotificationManagerMessageHandler {
public:
    static ServiceWorkerNotificationHandler& singleton();

    void showNotification(IPC::Connection&, const WebCore::NotificationData&, RefPtr<WebCore::NotificationResources>&&, CompletionHandler<void()>&&) final;
    void cancelNotification(const UUID& notificationID) final;
    void clearNotifications(const Vector<UUID>& notificationIDs) final;
    void didDestroyNotification(const UUID& notificationID) final;

    bool handlesNotification(UUID value) const { return m_notificationToSessionMap.contains(value); }

private:
    explicit ServiceWorkerNotificationHandler() = default;

    void requestSystemNotificationPermission(const String&, CompletionHandler<void(bool)>&&) final;

    WebsiteDataStore* dataStoreForNotificationID(const UUID&);

    HashMap<UUID, PAL::SessionID> m_notificationToSessionMap;
};

} // namespace WebKit
