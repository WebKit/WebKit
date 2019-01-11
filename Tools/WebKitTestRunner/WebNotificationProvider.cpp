/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <WebKit/WKMutableArray.h>
#include <WebKit/WKNotification.h>
#include <WebKit/WKNotificationManager.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKSecurityOriginRef.h>
#include <wtf/Assertions.h>

namespace WTR {

static void showWebNotification(WKPageRef page, WKNotificationRef notification, const void* clientInfo)
{
    static_cast<WebNotificationProvider*>(const_cast<void*>(clientInfo))->showWebNotification(page, notification);
}

static void closeWebNotification(WKNotificationRef notification, const void* clientInfo)
{
    static_cast<WebNotificationProvider*>(const_cast<void*>(clientInfo))->closeWebNotification(notification);
}

static void addNotificationManager(WKNotificationManagerRef manager, const void* clientInfo)
{
    static_cast<WebNotificationProvider*>(const_cast<void*>(clientInfo))->addNotificationManager(manager);
}

static void removeNotificationManager(WKNotificationManagerRef manager, const void* clientInfo)
{
    static_cast<WebNotificationProvider*>(const_cast<void*>(clientInfo))->removeNotificationManager(manager);
}

static WKDictionaryRef notificationPermissions(const void* clientInfo)
{
    return static_cast<WebNotificationProvider*>(const_cast<void*>(clientInfo))->notificationPermissions();
}

WebNotificationProvider::WebNotificationProvider()
{
}

WebNotificationProvider::~WebNotificationProvider()
{
    for (auto& manager : m_ownedNotifications)
        WKNotificationManagerSetProvider(manager.key.get(), nullptr);
}

WKNotificationProviderV0 WebNotificationProvider::provider()
{
    WKNotificationProviderV0 notificationProvider = {
        { 0, this },
        WTR::showWebNotification,
        WTR::closeWebNotification,
        0, // didDestroyNotification
        WTR::addNotificationManager,
        WTR::removeNotificationManager,
        WTR::notificationPermissions,
        0, // clearNotifications
    };
    return notificationProvider;
}

void WebNotificationProvider::showWebNotification(WKPageRef page, WKNotificationRef notification)
{
    auto context = WKPageGetContext(page);
    auto notificationManager = WKContextGetNotificationManager(context);
    uint64_t id = WKNotificationGetID(notification);

    ASSERT(m_ownedNotifications.contains(notificationManager));
    auto addResult = m_ownedNotifications.find(notificationManager)->value.add(id);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    auto addResult2 = m_owningManager.set(id, notificationManager);
    ASSERT_UNUSED(addResult2, addResult2.isNewEntry);
    auto addResult3 = m_localToGlobalNotificationIDMap.add(std::make_pair(page, WKNotificationManagerGetLocalIDForTesting(notificationManager, notification)), id);
    ASSERT_UNUSED(addResult3, addResult3.isNewEntry);

    WKNotificationManagerProviderDidShowNotification(notificationManager, id);
}

static void removeGlobalIDFromIDMap(HashMap<std::pair<WKPageRef, uint64_t>, uint64_t>& map, uint64_t id)
{
    for (auto iter = map.begin(); iter != map.end(); ++iter) {
        if (iter->value == id) {
            map.remove(iter);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void WebNotificationProvider::closeWebNotification(WKNotificationRef notification)
{
    uint64_t id = WKNotificationGetID(notification);
    ASSERT(m_owningManager.contains(id));
    auto notificationManager = m_owningManager.get(id);

    ASSERT(m_ownedNotifications.contains(notificationManager));
    bool success = m_ownedNotifications.find(notificationManager)->value.remove(id);
    ASSERT_UNUSED(success, success);
    m_owningManager.remove(id);

    removeGlobalIDFromIDMap(m_localToGlobalNotificationIDMap, id);

    WKRetainPtr<WKUInt64Ref> wkID(AdoptWK, WKUInt64Create(id));
    WKRetainPtr<WKMutableArrayRef> array(AdoptWK, WKMutableArrayCreate());
    WKArrayAppendItem(array.get(), wkID.get());
    WKNotificationManagerProviderDidCloseNotifications(notificationManager, array.get());
}

void WebNotificationProvider::addNotificationManager(WKNotificationManagerRef manager)
{
    m_ownedNotifications.add(manager, HashSet<uint64_t>());
}

void WebNotificationProvider::removeNotificationManager(WKNotificationManagerRef manager)
{
    auto iterator = m_ownedNotifications.find(manager);
    ASSERT(iterator != m_ownedNotifications.end());
    auto toRemove = iterator->value;
    WKRetainPtr<WKNotificationManagerRef> guard(manager);
    m_ownedNotifications.remove(iterator);
    WKRetainPtr<WKMutableArrayRef> array = adoptWK(WKMutableArrayCreate());
    for (uint64_t notificationID : toRemove) {
        bool success = m_owningManager.remove(notificationID);
        ASSERT_UNUSED(success, success);
        removeGlobalIDFromIDMap(m_localToGlobalNotificationIDMap, notificationID);
        WKArrayAppendItem(array.get(), adoptWK(WKUInt64Create(notificationID)).get());
    }
    WKNotificationManagerProviderDidCloseNotifications(manager, array.get());
}

WKDictionaryRef WebNotificationProvider::notificationPermissions()
{
    // Initial permissions are always empty.
    return WKMutableDictionaryCreate();
}

void WebNotificationProvider::simulateWebNotificationClick(WKPageRef page, uint64_t notificationID)
{
    ASSERT(m_localToGlobalNotificationIDMap.contains(std::make_pair(page, notificationID)));
    auto globalID = m_localToGlobalNotificationIDMap.get(std::make_pair(page, notificationID));
    ASSERT(m_owningManager.contains(globalID));
    WKNotificationManagerProviderDidClickNotification(m_owningManager.get(globalID), globalID);
}

void WebNotificationProvider::reset()
{
    for (auto& notificationPair : m_ownedNotifications) {
        if (notificationPair.value.isEmpty())
            continue;
        WKRetainPtr<WKMutableArrayRef> array = adoptWK(WKMutableArrayCreate());
        for (uint64_t notificationID : notificationPair.value)
            WKArrayAppendItem(array.get(), adoptWK(WKUInt64Create(notificationID)).get());

        notificationPair.value.clear();
        WKNotificationManagerProviderDidCloseNotifications(notificationPair.key.get(), array.get());
    }
    m_owningManager.clear();
    m_localToGlobalNotificationIDMap.clear();
}

} // namespace WTR
