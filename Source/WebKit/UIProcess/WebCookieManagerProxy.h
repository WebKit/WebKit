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

#pragma once

#include "APIObject.h"
#include "GenericCallback.h"
#include "MessageReceiver.h"
#include "WebContextSupplement.h"
#include "WebCookieManagerProxyClient.h"
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

#if USE(SOUP)
#include "SoupCookiePersistentStorageType.h"
#endif

namespace API {
class Array;
}

namespace WebCore {
struct Cookie;
}

namespace WebKit {

class WebProcessPool;
class WebProcessProxy;

typedef GenericCallback<API::Array*> ArrayCallback;
typedef GenericCallback<HTTPCookieAcceptPolicy> HTTPCookieAcceptPolicyCallback;
typedef GenericCallback<const Vector<WebCore::Cookie>&> GetCookiesCallback;

class WebCookieManagerProxy : public API::ObjectImpl<API::Object::Type::CookieManager>, public WebContextSupplement, private IPC::MessageReceiver {
public:
    static const char* supplementName();

    static Ref<WebCookieManagerProxy> create(WebProcessPool*);
    virtual ~WebCookieManagerProxy();

    void initializeClient(const WKCookieManagerClientBase*);
    
    void getHostnamesWithCookies(PAL::SessionID, Function<void (API::Array*, CallbackBase::Error)>&&);
    void deleteCookie(PAL::SessionID, const WebCore::Cookie&, Function<void (CallbackBase::Error)>&&);
    void deleteCookiesForHostname(PAL::SessionID, const String& hostname);
    void deleteAllCookies(PAL::SessionID);
    void deleteAllCookiesModifiedSince(PAL::SessionID, WallTime, Function<void (CallbackBase::Error)>&&);

    void setCookie(PAL::SessionID, const WebCore::Cookie&, Function<void (CallbackBase::Error)>&&);
    void setCookies(PAL::SessionID, const Vector<WebCore::Cookie>&, const URL&, const URL& mainDocumentURL, Function<void(CallbackBase::Error)>&&);

    void getAllCookies(PAL::SessionID, Function<void (const Vector<WebCore::Cookie>&, CallbackBase::Error)>&& completionHandler);
    void getCookies(PAL::SessionID, const URL&, Function<void(const Vector<WebCore::Cookie>&, CallbackBase::Error)>&& completionHandler);

    void setHTTPCookieAcceptPolicy(PAL::SessionID, HTTPCookieAcceptPolicy, Function<void (CallbackBase::Error)>&&);
    void getHTTPCookieAcceptPolicy(PAL::SessionID, Function<void (HTTPCookieAcceptPolicy, CallbackBase::Error)>&&);

    void setStorageAccessAPIEnabled(bool);

    void startObservingCookieChanges(PAL::SessionID);
    void stopObservingCookieChanges(PAL::SessionID);

    void setCookieObserverCallback(PAL::SessionID, WTF::Function<void ()>&&);

    class Observer {
    public:
        virtual ~Observer() { }
        virtual void cookiesDidChange() = 0;
        virtual void managerDestroyed() = 0;
    };

    void registerObserver(PAL::SessionID, Observer&);
    void unregisterObserver(PAL::SessionID, Observer&);

#if USE(SOUP)
    void setCookiePersistentStorage(PAL::SessionID, const String& storagePath, uint32_t storageType);
    void getCookiePersistentStorage(PAL::SessionID, String& storagePath, uint32_t& storageType) const;
#endif

    using API::Object::ref;
    using API::Object::deref;

private:
    WebCookieManagerProxy(WebProcessPool*);

    void didGetHostnamesWithCookies(const Vector<String>&, WebKit::CallbackID);
    void didGetHTTPCookieAcceptPolicy(uint32_t policy, WebKit::CallbackID);

    void didSetHTTPCookieAcceptPolicy(WebKit::CallbackID);
    void didSetCookies(WebKit::CallbackID);
    void didGetCookies(const Vector<WebCore::Cookie>&, WebKit::CallbackID);
    void didDeleteCookies(WebKit::CallbackID);

    void cookiesDidChange(PAL::SessionID);

    // WebContextSupplement
    void processPoolDestroyed() override;
    void processDidClose(WebProcessProxy*) override;
    void processDidClose(NetworkProcessProxy*) override;
    void refWebContextSupplement() override;
    void derefWebContextSupplement() override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

#if PLATFORM(COCOA)
    void persistHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy);
#endif

    CallbackMap m_callbacks;

    HashMap<PAL::SessionID, WTF::Function<void ()>> m_legacyCookieObservers;
    HashMap<PAL::SessionID, HashSet<Observer*>> m_cookieObservers;

    WebCookieManagerProxyClient m_client;

#if USE(SOUP)
    using CookiePersistentStorageMap = HashMap<PAL::SessionID, std::pair<String, SoupCookiePersistentStorageType>>;
    CookiePersistentStorageMap m_cookiePersistentStorageMap;
#endif
};

} // namespace WebKit
