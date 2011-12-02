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

#ifndef WebNotificationManager_h
#define WebNotificationManager_h

#include "MessageID.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace CoreIPC {
class ArgumentDecoder;
class Connection;
}

namespace WebCore {
class Notification;    
}

namespace WebKit {

class WebPage;
class WebProcess;

class WebNotificationManager {
    WTF_MAKE_NONCOPYABLE(WebNotificationManager);
public:
    explicit WebNotificationManager(WebProcess*);
    ~WebNotificationManager();

    bool show(WebCore::Notification*, WebPage*);
    void cancel(WebCore::Notification*, WebPage*);
    // This callback comes from WebCore, not messaged from the UI process.
    void didDestroyNotification(WebCore::Notification*, WebPage*);

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

private:
    // Implemented in generated WebNotificationManagerMessageReceiver.cpp
    void didReceiveWebNotificationManagerMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    
    void didShowNotification(uint64_t notificationID);
    void didClickNotification(uint64_t notificationID);
    void didCloseNotifications(const Vector<uint64_t>& notificationIDs);

    WebProcess* m_process;

#if ENABLE(NOTIFICATIONS)
    typedef HashMap<RefPtr<WebCore::Notification>, uint64_t> NotificationMap;
    NotificationMap m_notificationMap;
    
    typedef HashMap<uint64_t, RefPtr<WebCore::Notification> > NotificationIDMap;
    NotificationIDMap m_notificationIDMap;
#endif
};

} // namespace WebKit

#endif
