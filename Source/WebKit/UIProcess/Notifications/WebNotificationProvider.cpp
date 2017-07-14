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
#include "WebNotificationProvider.h"

#include "APIArray.h"
#include "APIDictionary.h"
#include "APINumber.h"
#include "APISecurityOrigin.h"
#include "WKAPICast.h"
#include "WebNotification.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageProxy.h"

namespace WebKit {

WebNotificationProvider::WebNotificationProvider(const WKNotificationProviderBase* provider)
{
    initialize(provider);
}

void WebNotificationProvider::show(WebPageProxy& page, WebNotification& notification)
{
    if (!m_client.show)
        return;

    m_client.show(toAPI(&page), toAPI(&notification), m_client.base.clientInfo);
}

void WebNotificationProvider::cancel(WebNotification& notification)
{
    if (!m_client.cancel)
        return;

    m_client.cancel(toAPI(&notification), m_client.base.clientInfo);
}

void WebNotificationProvider::didDestroyNotification(WebNotification& notification)
{
    if (!m_client.didDestroyNotification)
        return;

    m_client.didDestroyNotification(toAPI(&notification), m_client.base.clientInfo);
}

void WebNotificationProvider::clearNotifications(const Vector<uint64_t>& notificationIDs)
{
    if (!m_client.clearNotifications)
        return;

    Vector<RefPtr<API::Object>> arrayIDs;
    arrayIDs.reserveInitialCapacity(notificationIDs.size());

    for (const auto& notificationID : notificationIDs)
        arrayIDs.uncheckedAppend(API::UInt64::create(notificationID));

    m_client.clearNotifications(toAPI(API::Array::create(WTFMove(arrayIDs)).ptr()), m_client.base.clientInfo);
}

void WebNotificationProvider::addNotificationManager(WebNotificationManagerProxy& manager)
{
    if (!m_client.addNotificationManager)
        return;

    m_client.addNotificationManager(toAPI(&manager), m_client.base.clientInfo);
}

void WebNotificationProvider::removeNotificationManager(WebNotificationManagerProxy& manager)
{
    if (!m_client.removeNotificationManager)
        return;

    m_client.removeNotificationManager(toAPI(&manager), m_client.base.clientInfo);
}

HashMap<WTF::String, bool> WebNotificationProvider::notificationPermissions()
{
    HashMap<WTF::String, bool> permissions;
    if (!m_client.notificationPermissions)
        return permissions;

    RefPtr<API::Dictionary> knownPermissions = adoptRef(toImpl(m_client.notificationPermissions(m_client.base.clientInfo)));
    if (!knownPermissions)
        return permissions;

    Ref<API::Array> knownOrigins = knownPermissions->keys();
    for (size_t i = 0; i < knownOrigins->size(); ++i) {
        API::String* origin = knownOrigins->at<API::String>(i);
        permissions.set(origin->string(), knownPermissions->get<API::Boolean>(origin->string())->value());
    }
    return permissions;
}

} // namespace WebKit
