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

#ifndef WebNotificationManagerProxy_h
#define WebNotificationManagerProxy_h

#include "APIObject.h"
#include "MessageReceiver.h"
#include "WebContextSupplement.h"
#include "WebNotificationProvider.h"
#include <WebCore/NotificationClient.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/StringHash.h>

namespace WebKit {

class ImmutableArray;
class WebContext;
class WebPageProxy;
class WebSecurityOrigin;

class WebNotificationManagerProxy : public APIObject, public WebContextSupplement, private CoreIPC::MessageReceiver {
public:
    static const Type APIType = TypeNotificationManager;

    static const AtomicString& supplementName();

    static PassRefPtr<WebNotificationManagerProxy> create(WebContext*);

    void initializeProvider(const WKNotificationProvider*);
    void populateCopyOfNotificationPermissions(HashMap<String, bool>&);

    void show(WebPageProxy*, const String& title, const String& body, const String& iconURL, const String& tag, const String& lang, const String& dir, const String& originString, uint64_t notificationID);

    void providerDidShowNotification(uint64_t notificationID);
    void providerDidClickNotification(uint64_t notificationID);
    void providerDidCloseNotifications(ImmutableArray* notificationIDs);
    void providerDidUpdateNotificationPolicy(const WebSecurityOrigin*, bool allowed);
    void providerDidRemoveNotificationPolicies(ImmutableArray* origins);

    using APIObject::ref;
    using APIObject::deref;

private:
    explicit WebNotificationManagerProxy(WebContext*);
    
    virtual Type type() const { return APIType; }

    // WebContextSupplement
    virtual void contextDestroyed() OVERRIDE;
    virtual void processDidClose(WebProcessProxy*) OVERRIDE;
    virtual void refWebContextSupplement() OVERRIDE;
    virtual void derefWebContextSupplement() OVERRIDE;

    // CoreIPC::MessageReceiver
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&) OVERRIDE;
    void didReceiveWebNotificationManagerProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    
    // Message handlers
    void cancel(uint64_t notificationID);
    void didDestroyNotification(uint64_t notificationID);
    void clearNotifications(const Vector<uint64_t>& notificationIDs);

    typedef HashMap<uint64_t, RefPtr<WebNotification> > WebNotificationMap;
    
    WebNotificationProvider m_provider;
    WebNotificationMap m_notifications;
};

} // namespace WebKit

#endif // WebNotificationManagerProxy_h
