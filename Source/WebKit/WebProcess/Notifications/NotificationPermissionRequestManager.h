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

#ifndef NotificationPermissionRequestManager_h
#define NotificationPermissionRequestManager_h

#include <WebCore/NotificationClient.h>
#include <WebCore/NotificationPermissionCallback.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class Notification;
}

namespace WebKit {

class WebPage;

/// FIXME: Need to keep a queue of pending notifications which permission is still being requested.
class NotificationPermissionRequestManager : public RefCounted<NotificationPermissionRequestManager> {
public:
    static Ref<NotificationPermissionRequestManager> create(WebPage*);
    ~NotificationPermissionRequestManager();

    using Permission = WebCore::NotificationClient::Permission;
    using PermissionHandler = WebCore::NotificationClient::PermissionHandler;

#if ENABLE(NOTIFICATIONS)
    void startRequest(const WebCore::SecurityOriginData&, PermissionHandler&&);
#endif
    
    Permission permissionLevel(const WebCore::SecurityOriginData&);

    // For testing purposes only.
    void setPermissionLevelForTesting(const String& originString, bool allowed);
    void removeAllPermissionsForTesting();
    
private:
    NotificationPermissionRequestManager(WebPage*);

#if ENABLE(NOTIFICATIONS)
    using PermissionHandlers = Vector<PermissionHandler>;
    static void callPermissionHandlersWith(PermissionHandlers&, Permission);

    HashMap<WebCore::SecurityOriginData, PermissionHandlers> m_requestsPerOrigin;
    WebPage* m_page;
#endif
};

inline bool isRequestIDValid(uint64_t id)
{
    // This check makes sure that the ID is not equal to values needed by
    // HashMap for bucketing.
    return id && id != static_cast<uint64_t>(-1);
}

} // namespace WebKit

#endif // NotificationPermissionRequestManager_h
