/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef WebNotificationClient_h
#define WebNotificationClient_h

#if ENABLE(NOTIFICATIONS)

#include <WebCore/NotificationClient.h>

namespace WebCore {
class ScriptExecutionContext;
} // namespace WebCore

namespace WebKit {

class WebPage;

class WebNotificationClient : public WebCore::NotificationClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebNotificationClient(WebPage*);
    virtual ~WebNotificationClient();

private:
    bool show(WebCore::Notification*) override;
    void cancel(WebCore::Notification*) override;
    void clearNotifications(WebCore::ScriptExecutionContext*) override;
    void notificationObjectDestroyed(WebCore::Notification*) override;
    void notificationControllerDestroyed() override;
    void requestPermission(WebCore::ScriptExecutionContext&, PermissionHandler&&) override;
    WebCore::NotificationClient::Permission checkPermission(WebCore::ScriptExecutionContext*) override;
    
    WebPage* m_page;
};

} // namespace WebKit

#endif // ENABLE(NOTIFICATIONS)

#endif // WebNotificationClient_h
