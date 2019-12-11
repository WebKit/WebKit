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

#ifndef WebNotificationProvider_h
#define WebNotificationProvider_h

#include <WebKit/WKNotificationManager.h>
#include <WebKit/WKNotificationProvider.h>
#include <WebKit/WKRetainPtr.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WTR {

class WebNotificationProvider {
public:
    WebNotificationProvider();
    ~WebNotificationProvider();
    WKNotificationProviderV0 provider();

    void showWebNotification(WKPageRef, WKNotificationRef);
    void closeWebNotification(WKNotificationRef);
    void addNotificationManager(WKNotificationManagerRef);
    void removeNotificationManager(WKNotificationManagerRef);
    WKDictionaryRef notificationPermissions();

    void simulateWebNotificationClick(WKPageRef, uint64_t notificationID);
    void reset();

private:
    // Inverses of each other.
    HashMap<WKRetainPtr<WKNotificationManagerRef>, HashSet<uint64_t>> m_ownedNotifications;
    HashMap<uint64_t, WKNotificationManagerRef> m_owningManager;

    HashMap<std::pair<WKPageRef, uint64_t>, uint64_t> m_localToGlobalNotificationIDMap;
};

}

#endif
