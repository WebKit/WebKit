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

#ifndef NotificationPermissionRequestManager_h
#define NotificationPermissionRequestManager_h

#include <WebCore/NotificationPresenter.h>
#include <WebCore/VoidCallback.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class Notification;    
class SecurityOrigin;
}

namespace WebKit {

class WebPage;

/// FIXME: Need to keep a queue of pending notifications which permission is still being requested.
class NotificationPermissionRequestManager : public RefCounted<NotificationPermissionRequestManager> {
public:
    static PassRefPtr<NotificationPermissionRequestManager> create(WebPage*);
    
    void startRequest(WebCore::SecurityOrigin*, PassRefPtr<WebCore::VoidCallback>);
    void cancelRequest(WebCore::SecurityOrigin*);
    
    // Synchronous call to retrieve permission level for given security origin
    WebCore::NotificationPresenter::Permission permissionLevel(WebCore::SecurityOrigin*);
    
    void didReceiveNotificationPermissionDecision(uint64_t notificationID, bool allowed);
    
private:
    NotificationPermissionRequestManager(WebPage*);
    
    HashMap<uint64_t, RefPtr<WebCore::VoidCallback> > m_idToCallbackMap;
    HashMap<RefPtr<WebCore::SecurityOrigin>, uint64_t> m_originToIDMap;
    HashMap<uint64_t, RefPtr<WebCore::SecurityOrigin> > m_idToOriginMap;

    WebPage* m_page;
};

inline bool isRequestIDValid(uint64_t id)
{
    // This check makes sure that the ID is not equal to values needed by
    // HashMap for bucketing.
    return id && id != static_cast<uint64_t>(-1);
}

} // namespace WebKit

#endif // NotificationPermissionRequestManager_h
