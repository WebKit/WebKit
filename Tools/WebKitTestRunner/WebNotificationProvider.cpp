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

#include "DataFunctions.h"
#include "StringFunctions.h"
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
    m_permissions = adoptWK(WKMutableDictionaryCreate());
}

WebNotificationProvider::~WebNotificationProvider()
{
    for (auto& manager : m_knownManagers)
        WKNotificationManagerSetProvider(manager.get(), nullptr);
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

static WKNotificationManagerRef notificationManagerForPage(WKPageRef page)
{
    if (page)
        return WKContextGetNotificationManager(WKPageGetContext(page));

    return WKNotificationManagerGetSharedServiceWorkerNotificationManager();
}

void WebNotificationProvider::showWebNotification(WKPageRef page, WKNotificationRef notification)
{
    if (WKNotificationGetIsPersistent(notification))
        m_knownPersistentNotifications.add(notification);

    auto notificationManager = notificationManagerForPage(page);
    ASSERT(m_knownManagers.contains(notificationManager));

    uint64_t identifier = WKNotificationGetID(notification);
    auto coreIdentifier = adoptWK(WKNotificationCopyCoreIDForTesting(notification));

    auto addResult = m_owningManager.set(dataToUUID(coreIdentifier.get()), notificationManager);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    WKNotificationManagerProviderDidShowNotification(notificationManager, identifier);
}

void WebNotificationProvider::closeWebNotification(WKNotificationRef notification)
{
    if (WKNotificationGetIsPersistent(notification))
        m_knownPersistentNotifications.remove(notification);

    auto identifier = adoptWK(WKNotificationCopyCoreIDForTesting(notification));

    auto notificationManager = m_owningManager.take(dataToUUID(identifier.get()));
    ASSERT(notificationManager);
    ASSERT(m_knownManagers.contains(notificationManager));

    auto wkID = adoptWK(WKUInt64Create(WKNotificationGetID(notification)));
    auto array = adoptWK(WKMutableArrayCreate());
    WKArrayAppendItem(array.get(), wkID.get());
    WKNotificationManagerProviderDidCloseNotifications(notificationManager, array.get());
}

void WebNotificationProvider::addNotificationManager(WKNotificationManagerRef manager)
{
    ASSERT(!m_knownManagers.contains(manager) || manager == WKNotificationManagerGetSharedServiceWorkerNotificationManager());
    m_knownManagers.add(manager);
}

void WebNotificationProvider::removeNotificationManager(WKNotificationManagerRef manager)
{
    auto protectedManager = m_knownManagers.take(manager);
    ASSERT(protectedManager);

    auto toRemove = Vector<UUID> { };
    for (auto& iterator : m_owningManager) {
        if (iterator.value != manager)
            continue;
        toRemove.append(iterator.key);
    }

    auto array = adoptWK(WKMutableArrayCreate());
    for (auto& identifier : toRemove) {
        WKArrayAppendItem(array.get(), uuidToData(identifier).get());
        m_owningManager.remove(identifier);
    }

    WKNotificationManagerProviderDidCloseNotifications(manager, array.get());
}

WKDictionaryRef WebNotificationProvider::notificationPermissions()
{
    WKRetain(m_permissions.get());
    return m_permissions.get();
}

// If the state is not stored in m_permissions, return nullopt. Otherwise, return true if
// the permission is granted and false if it's denied.
std::optional<bool> WebNotificationProvider::permissionState(WKSecurityOriginRef securityOrigin)
{
    auto securityOriginString = adoptWK(WKSecurityOriginCopyToString(securityOrigin));
    if (auto value = WKDictionaryGetItemForKey(m_permissions.get(), securityOriginString.get()))
        return WKBooleanGetValue(static_cast<WKBooleanRef>(value));

    return std::nullopt;
}

void WebNotificationProvider::setPermission(const String& origin, bool allowed)
{
    auto wkAllowed = adoptWK(WKBooleanCreate(allowed));
    WKDictionarySetItem(m_permissions.get(), toWK(origin).get(), wkAllowed.get());
}

void WebNotificationProvider::simulateWebNotificationClick(WKPageRef, WKDataRef notificationID)
{
    auto identifier = dataToUUID(notificationID);
    ASSERT(m_owningManager.contains(identifier));

    WKNotificationManagerProviderDidClickNotification_b(m_owningManager.get(identifier), notificationID);
}

#if !PLATFORM(COCOA)
void WebNotificationProvider::simulateWebNotificationClickForServiceWorkerNotifications()
{
    auto sharedManager = WKNotificationManagerGetSharedServiceWorkerNotificationManager();
    for (auto& iterator : m_owningManager) {
        if (iterator.value != sharedManager)
            continue;
        WKNotificationManagerProviderDidClickNotification_b(sharedManager, uuidToData(iterator.key).get());
    }
}
#endif

WKRetainPtr<WKArrayRef> securityOriginsFromStrings(WKArrayRef originStrings)
{
    auto origins = adoptWK(WKMutableArrayCreate());
    for (size_t i = 0; i < WKArrayGetSize(originStrings); i++) {
        auto originString = static_cast<WKStringRef>(WKArrayGetItemAtIndex(originStrings, i));
        auto origin = adoptWK(WKSecurityOriginCreateFromString(originString));
        WKArrayAppendItem(origins.get(), static_cast<WKTypeRef>(origin.get()));
    }
    return origins;
}

void WebNotificationProvider::reset()
{
    for (auto iterator : m_owningManager) {
        auto array = adoptWK(WKMutableArrayCreate());
        WKArrayAppendItem(array.get(), uuidToData(iterator.key).get());
        WKNotificationManagerProviderDidCloseNotifications(iterator.value, array.get());
    }

    m_knownPersistentNotifications.clear();
    m_owningManager.clear();

    auto originStrings = adoptWK(WKDictionaryCopyKeys(static_cast<WKDictionaryRef>(m_permissions.get())));
    auto origins = securityOriginsFromStrings(originStrings.get());
    for (auto manager : m_knownManagers)
        WKNotificationManagerProviderDidRemoveNotificationPolicies(manager.get(), origins.get());

    m_permissions = adoptWK(WKMutableDictionaryCreate());
}

} // namespace WTR
