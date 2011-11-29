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

#include "config.h"
#include "WebNotificationManagerProxy.h"

#include "WebContext.h"
#include "WebNotification.h"
#include <WebCore/NotificationContents.h>

using namespace WTF;
using namespace WebCore;

namespace WebKit {

PassRefPtr<WebNotificationManagerProxy> WebNotificationManagerProxy::create(WebContext* context)
{
    return adoptRef(new WebNotificationManagerProxy(context));
}

WebNotificationManagerProxy::WebNotificationManagerProxy(WebContext* context)
    : m_context(context)
{
}

WebNotificationManagerProxy::~WebNotificationManagerProxy()
{
}

void WebNotificationManagerProxy::invalidate()
{
}

void WebNotificationManagerProxy::initializeProvider(const WKNotificationProvider *provider)
{
    m_provider.initialize(provider);
}

void WebNotificationManagerProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    didReceiveWebNotificationManagerProxyMessage(connection, messageID, arguments);
}

void WebNotificationManagerProxy::show(const String& title, const String& body, uint64_t notificationID)
{
    RefPtr<WebNotification> notification = WebNotification::create(title, body);
    m_provider.show(notification.get());
}

void WebNotificationManagerProxy::cancel(uint64_t notificationID)
{
    m_provider.cancel(0);
}

} // namespace WebKit
