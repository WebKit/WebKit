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

#include "config.h"
#include "TestNotificationProvider.h"

#include "WTFStringUtilities.h"
#include <WebKit/WKNotificationManager.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKSecurityOriginRef.h>
#include <WebKit/WKString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

static WKDictionaryRef notificationPermissions(const void* clientInfo)
{
    return static_cast<TestNotificationProvider*>(const_cast<void*>(clientInfo))->notificationPermissions();
}

TestNotificationProvider::TestNotificationProvider(Vector<WKNotificationManagerRef>&& managers)
    : m_managers(WTFMove(managers))
    , m_permissions(adoptWK(WKMutableDictionaryCreate()))
{
    m_provider = {
        { 0, this },
        0, // showWebNotification
        0, // closeWebNotification
        0, // didDestroyNotification
        0, // addNotificationManager
        0, // removeNotificationManager
        &TestWebKitAPI::notificationPermissions,
        0, // clearNotifications
    };

    for (auto& manager : m_managers)
        WKNotificationManagerSetProvider(manager, &m_provider.base);
}

TestNotificationProvider::~TestNotificationProvider()
{
    for (auto& manager : m_managers)
        WKNotificationManagerSetProvider(manager, nullptr);
}

WKDictionaryRef TestNotificationProvider::notificationPermissions() const
{
    WKRetain(m_permissions.get());
    return m_permissions.get();
}

void TestNotificationProvider::setPermission(const String& origin, bool allowed)
{
    auto wkAllowed = adoptWK(WKBooleanCreate(allowed));
    auto wkOriginString = adoptWK(WKStringCreateWithUTF8CString(origin.utf8().data()));
    WKDictionarySetItem(m_permissions.get(), wkOriginString.get(), wkAllowed.get());

    auto wkOrigin = adoptWK(WKSecurityOriginCreateFromString(wkOriginString.get()));
    for (auto& manager : m_managers)
        WKNotificationManagerProviderDidUpdateNotificationPolicy(manager, wkOrigin.get(), allowed);
}

} // namespace TestWebKitAPI
