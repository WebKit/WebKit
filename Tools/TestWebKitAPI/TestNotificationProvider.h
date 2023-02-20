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

#include <WebKit/WKNotificationManager.h>
#include <WebKit/WKNotificationProvider.h>
#include <WebKit/WKRetainPtr.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

class TestNotificationProvider {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit TestNotificationProvider(Vector<WKNotificationManagerRef>&&);
    ~TestNotificationProvider();

    void setPermission(const String& origin, bool allowed);
    void resetPermission(const String& origin);

    WKDictionaryRef notificationPermissions() const;
    void showWebNotification(WKPageRef, WKNotificationRef);
    void closeWebNotification(WKNotificationRef);
    bool simulateNotificationClick();
    bool simulateNotificationClose();
    WKStringRef lastNotificationDataStoreIdentifier() const { return m_lastNotificationDataStoreIdentifier.get(); };

    bool hasReceivedShowNotification() const { return m_hasReceivedShowNotification; }
    bool hasReceivedCloseNotification() const { return m_hasReceivedCloseNotification; }
    void resetHasReceivedNotification();

private:
    Vector<WKNotificationManagerRef> m_managers;
    HashMap<String, bool> m_permissions;
    WKNotificationProviderV0 m_provider;

    bool m_hasReceivedShowNotification { false };
    bool m_hasReceivedCloseNotification { false };
    std::pair<WKNotificationManagerRef, uint64_t> m_pendingNotification;
    WKRetainPtr<WKStringRef> m_lastNotificationDataStoreIdentifier;
};

}
