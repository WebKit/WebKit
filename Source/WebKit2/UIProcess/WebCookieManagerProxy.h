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
#include <WebCore/SessionID.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

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
    
    void getHostnamesWithCookies(WebCore::SessionID, Function<void (API::Array*, CallbackBase::Error)>&&);
    void deleteCookie(WebCore::SessionID, const WebCore::Cookie&, Function<void (CallbackBase::Error)>&&);
    void deleteCookiesForHostname(WebCore::SessionID, const String& hostname);
    void deleteAllCookies(WebCore::SessionID);
    void deleteAllCookiesModifiedSince(WebCore::SessionID, std::chrono::system_clock::time_point, Function<void (CallbackBase::Error)>&&);

    void setCookie(WebCore::SessionID, const WebCore::Cookie&, Function<void (CallbackBase::Error)>&&);
    void setCookies(WebCore::SessionID, const Vector<WebCore::Cookie>&, const WebCore::URL&, const WebCore::URL& mainDocumentURL, Function<void (CallbackBase::Error)>&&);

    void getAllCookies(WebCore::SessionID, Function<void (const Vector<WebCore::Cookie>&, CallbackBase::Error)>&& completionHandler);
    void getCookies(WebCore::SessionID, const WebCore::URL&, Function<void (const Vector<WebCore::Cookie>&, CallbackBase::Error)>&& completionHandler);

    void setHTTPCookieAcceptPolicy(WebCore::SessionID, HTTPCookieAcceptPolicy, Function<void (CallbackBase::Error)>&&);
    void getHTTPCookieAcceptPolicy(WebCore::SessionID, Function<void (HTTPCookieAcceptPolicy, CallbackBase::Error)>&&);

    void setCookieStoragePartitioningEnabled(bool);

    void startObservingCookieChanges(WebCore::SessionID);
    void stopObservingCookieChanges(WebCore::SessionID);

    void setCookieObserverCallback(WebCore::SessionID, WTF::Function<void ()>&&);

    class Observer {
    public:
        virtual ~Observer() { }
        virtual void cookiesDidChange() = 0;
        virtual void managerDestroyed() = 0;
    };

    void registerObserver(WebCore::SessionID, Observer&);
    void unregisterObserver(WebCore::SessionID, Observer&);

#if USE(SOUP)
    void setCookiePersistentStorage(const String& storagePath, uint32_t storageType);
    void getCookiePersistentStorage(String& storagePath, uint32_t& storageType) const;
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

    void cookiesDidChange(WebCore::SessionID);

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

    HashMap<WebCore::SessionID, WTF::Function<void ()>> m_legacyCookieObservers;
    HashMap<WebCore::SessionID, HashSet<Observer*>> m_cookieObservers;

    WebCookieManagerProxyClient m_client;

#if USE(SOUP)
    String m_cookiePersistentStoragePath;
    SoupCookiePersistentStorageType m_cookiePersistentStorageType;
#endif
};

} // namespace WebKit
