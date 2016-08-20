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

#ifndef WebCookieManagerProxy_h
#define WebCookieManagerProxy_h

#include "APIObject.h"
#include "GenericCallback.h"
#include "MessageReceiver.h"
#include "WebContextSupplement.h"
#include "WebCookieManagerProxyClient.h"
#include <wtf/PassRefPtr.h>
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

class WebCookieManagerProxy : public API::ObjectImpl<API::Object::Type::CookieManager>, public WebContextSupplement, private IPC::MessageReceiver {
public:
    static const char* supplementName();

    static PassRefPtr<WebCookieManagerProxy> create(WebProcessPool*);
    virtual ~WebCookieManagerProxy();

    void initializeClient(const WKCookieManagerClientBase*);
    
    void getHostnamesWithCookies(std::function<void (API::Array*, CallbackBase::Error)>);
    void deleteCookiesForHostname(const String& hostname);
    void deleteAllCookies();
    void deleteAllCookiesModifiedSince(std::chrono::system_clock::time_point);
    void addCookie(const WebCore::Cookie&, const String& hostname);

    void setHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy);
    void getHTTPCookieAcceptPolicy(std::function<void (HTTPCookieAcceptPolicy, CallbackBase::Error)>);

    void startObservingCookieChanges();
    void stopObservingCookieChanges();

#if USE(SOUP)
    void setCookiePersistentStorage(const String& storagePath, uint32_t storageType);
    void getCookiePersistentStorage(String& storagePath, uint32_t& storageType) const;
#endif

    using API::Object::ref;
    using API::Object::deref;

private:
    WebCookieManagerProxy(WebProcessPool*);

    void didGetHostnamesWithCookies(const Vector<String>&, uint64_t callbackID);
    void didGetHTTPCookieAcceptPolicy(uint32_t policy, uint64_t callbackID);

    void cookiesDidChange();

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

    HashMap<uint64_t, RefPtr<ArrayCallback>> m_arrayCallbacks;
    HashMap<uint64_t, RefPtr<HTTPCookieAcceptPolicyCallback>> m_httpCookieAcceptPolicyCallbacks;

    WebCookieManagerProxyClient m_client;

#if USE(SOUP)
    String m_cookiePersistentStoragePath;
    SoupCookiePersistentStorageType m_cookiePersistentStorageType;
#endif
};

} // namespace WebKit

#endif // WebCookieManagerProxy_h
