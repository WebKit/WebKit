/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

#include "APIArray.h"
#include "APINotificationProvider.h"
#include "APISecurityOrigin.h"
#include "WebNotification.h"
#include "WebNotificationManagerMessages.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/NotificationData.h>

namespace WebKit {
using namespace WebCore;

const char* WebNotificationManagerProxy::supplementName()
{
    return "WebNotificationManagerProxy";
}

Ref<WebNotificationManagerProxy> WebNotificationManagerProxy::create(WebProcessPool* processPool)
{
    return adoptRef(*new WebNotificationManagerProxy(processPool));
}

WebNotificationManagerProxy::WebNotificationManagerProxy(WebProcessPool* processPool)
    : WebContextSupplement(processPool)
    , m_provider(makeUnique<API::NotificationProvider>())
{
}

void WebNotificationManagerProxy::setProvider(std::unique_ptr<API::NotificationProvider>&& provider)
{
    if (!provider) {
        m_provider = makeUnique<API::NotificationProvider>();
        return;
    }

    m_provider = WTFMove(provider);
    m_provider->addNotificationManager(*this);
}

// WebContextSupplement

void WebNotificationManagerProxy::processPoolDestroyed()
{
    m_provider->removeNotificationManager(*this);
}

void WebNotificationManagerProxy::refWebContextSupplement()
{
    API::Object::ref();
}

void WebNotificationManagerProxy::derefWebContextSupplement()
{
    API::Object::deref();
}

HashMap<String, bool> WebNotificationManagerProxy::notificationPermissions()
{
    return m_provider->notificationPermissions();
}

void WebNotificationManagerProxy::show(WebPageProxy* webPage, const WebCore::NotificationData& notificationData)
{
    auto notification = WebNotification::create(notificationData, webPage->identifier());
    m_globalNotificationMap.set(notification->notificationID(), notification->coreNotificationID());
    m_notifications.set(notification->coreNotificationID(), notification);
    m_provider->show(*webPage, notification.get());
}

void WebNotificationManagerProxy::cancel(WebPageProxy* webPage, const UUID& pageNotificationID)
{
    if (auto webNotification = m_notifications.get(pageNotificationID))
        m_provider->cancel(*webNotification);
}
    
void WebNotificationManagerProxy::didDestroyNotification(WebPageProxy* webPage, const UUID& pageNotificationID)
{
    if (auto webNotification = m_notifications.take(pageNotificationID)) {
        m_globalNotificationMap.remove(webNotification->notificationID());
        m_provider->didDestroyNotification(*webNotification);
    }
}

static bool pageIDsMatch(WebPageProxyIdentifier pageID, const UUID&, WebPageProxyIdentifier desiredPageID, const Vector<UUID>&)
{
    return pageID == desiredPageID;
}

static bool pageAndNotificationIDsMatch(WebPageProxyIdentifier pageID, const UUID& pageNotificationID, WebPageProxyIdentifier desiredPageID, const Vector<UUID>& desiredPageNotificationIDs)
{
    return pageID == desiredPageID && desiredPageNotificationIDs.contains(pageNotificationID);
}

void WebNotificationManagerProxy::clearNotifications(WebPageProxy* webPage)
{
    clearNotifications(webPage, Vector<UUID>(), pageIDsMatch);
}

void WebNotificationManagerProxy::clearNotifications(WebPageProxy* webPage, const Vector<UUID>& pageNotificationIDs)
{
    clearNotifications(webPage, pageNotificationIDs, pageAndNotificationIDsMatch);
}

void WebNotificationManagerProxy::clearNotifications(WebPageProxy* webPage, const Vector<UUID>& pageNotificationIDs, NotificationFilterFunction filterFunction)
{
    auto targetPageProxyID = webPage->identifier();

    Vector<uint64_t> globalNotificationIDs;
    globalNotificationIDs.reserveCapacity(m_globalNotificationMap.size());

    for (auto notification : m_notifications.values()) {
        auto pageProxyID = notification->pageIdentifier();
        auto coreNotificationID = notification->coreNotificationID();
        if (!filterFunction(pageProxyID, coreNotificationID, targetPageProxyID, pageNotificationIDs))
            continue;

        uint64_t globalNotificationID = notification->notificationID();
        globalNotificationIDs.append(globalNotificationID);
    }

    for (auto it = globalNotificationIDs.begin(), end = globalNotificationIDs.end(); it != end; ++it) {
        auto pageNotification = m_globalNotificationMap.take(*it);
        m_notifications.remove(pageNotification);
    }

    m_provider->clearNotifications(globalNotificationIDs);
}

void WebNotificationManagerProxy::providerDidShowNotification(uint64_t globalNotificationID)
{
    auto it = m_globalNotificationMap.find(globalNotificationID);
    if (it == m_globalNotificationMap.end())
        return;

    auto notification = m_notifications.get(it->value);
    if (!notification) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* webPage = WebProcessProxy::webPage(notification->pageIdentifier());
    if (!webPage)
        return;

    webPage->process().send(Messages::WebNotificationManager::DidShowNotification(it->value), 0);
}

void WebNotificationManagerProxy::providerDidClickNotification(uint64_t globalNotificationID)
{
    auto it = m_globalNotificationMap.find(globalNotificationID);
    if (it == m_globalNotificationMap.end())
        return;

    auto notification = m_notifications.get(it->value);
    if (!notification) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto* webPage = WebProcessProxy::webPage(notification->pageIdentifier());
    if (!webPage)
        return;

    webPage->process().send(Messages::WebNotificationManager::DidClickNotification(it->value), 0);
}

void WebNotificationManagerProxy::providerDidClickNotification(const UUID& coreNotificationID)
{
    auto notification = m_notifications.get(coreNotificationID);
    if (!notification)
        return;

    auto* webPage = WebProcessProxy::webPage(notification->pageIdentifier());
    if (!webPage)
        return;

    webPage->process().send(Messages::WebNotificationManager::DidClickNotification(coreNotificationID), 0);
}

void WebNotificationManagerProxy::providerDidCloseNotifications(API::Array* globalNotificationIDs)
{
    HashMap<WebPageProxy*, Vector<UUID>> pageNotificationIDs;

    size_t size = globalNotificationIDs->size();
    for (size_t i = 0; i < size; ++i) {
        // The passed array might have uint64_t identifiers or UUID data identifiers.
        // Handle both.

        std::optional<UUID> coreNotificationID;
        auto* intValue = globalNotificationIDs->at<API::UInt64>(i);
        if (intValue) {
            auto it = m_globalNotificationMap.find(intValue->value());
            if (it == m_globalNotificationMap.end())
                continue;

            coreNotificationID = it->value;
        } else {
            auto* dataValue = globalNotificationIDs->at<API::Data>(i);
            if (!dataValue)
                continue;

            auto span = dataValue->dataReference();
            if (span.size() != 16)
                continue;

            coreNotificationID = UUID { Span<const uint8_t, 16> { span.data(), 16 } };
        }

        ASSERT(coreNotificationID);

        auto notification = m_notifications.take(*coreNotificationID);
        if (!notification)
            continue;

        if (WebPageProxy* webPage = WebProcessProxy::webPage(notification->pageIdentifier())) {
            auto pageIt = pageNotificationIDs.find(webPage);
            if (pageIt == pageNotificationIDs.end()) {
                Vector<UUID> newVector;
                newVector.reserveInitialCapacity(size);
                pageIt = pageNotificationIDs.add(webPage, WTFMove(newVector)).iterator;
            }

            pageIt->value.append(notification->coreNotificationID());
        }

        m_globalNotificationMap.remove(notification->notificationID());
    }

    for (auto it = pageNotificationIDs.begin(), end = pageNotificationIDs.end(); it != end; ++it)
        it->key->process().send(Messages::WebNotificationManager::DidCloseNotifications(it->value), 0);
}

void WebNotificationManagerProxy::providerDidUpdateNotificationPolicy(const API::SecurityOrigin* origin, bool allowed)
{
    if (!processPool())
        return;

    processPool()->sendToAllProcesses(Messages::WebNotificationManager::DidUpdateNotificationDecision(origin->securityOrigin().toString(), allowed));
}

void WebNotificationManagerProxy::providerDidRemoveNotificationPolicies(API::Array* origins)
{
    if (!processPool())
        return;

    size_t size = origins->size();
    if (!size)
        return;
    
    Vector<String> originStrings;
    originStrings.reserveInitialCapacity(size);
    
    for (size_t i = 0; i < size; ++i)
        originStrings.append(origins->at<API::SecurityOrigin>(i)->securityOrigin().toString());
    
    processPool()->sendToAllProcesses(Messages::WebNotificationManager::DidRemoveNotificationDecisions(originStrings));
}

} // namespace WebKit
