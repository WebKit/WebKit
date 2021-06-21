/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <WebCore/Cookie.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
struct Cookie;
enum class HTTPCookieAcceptPolicy : uint8_t;
}

namespace WebKit {
class WebCookieManagerProxy;
class WebsiteDataStore;
}

namespace API {

class APIWebCookieManagerProxyObserver;

class HTTPCookieStore final : public ObjectImpl<Object::Type::HTTPCookieStore> {
public:
    static Ref<HTTPCookieStore> create(WebKit::WebsiteDataStore& websiteDataStore)
    {
        return adoptRef(*new HTTPCookieStore(websiteDataStore));
    }

    virtual ~HTTPCookieStore();

    void cookies(CompletionHandler<void(const Vector<WebCore::Cookie>&)>&&);
    void cookiesForURL(WTF::URL&&, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&&);
    void setCookies(Vector<WebCore::Cookie>&&, CompletionHandler<void()>&&);
    void deleteCookie(const WebCore::Cookie&, CompletionHandler<void()>&&);
    
    void deleteAllCookies(CompletionHandler<void()>&&);
    void setHTTPCookieAcceptPolicy(WebCore::HTTPCookieAcceptPolicy, CompletionHandler<void()>&&);
    void flushCookies(CompletionHandler<void()>&&);

    class Observer : public CanMakeWeakPtr<Observer> {
    public:
        virtual ~Observer() { }
        virtual void cookiesDidChange(HTTPCookieStore&) = 0;
    };

    void registerObserver(Observer&);
    void unregisterObserver(Observer&);

    void cookiesDidChange();

    void filterAppBoundCookies(Vector<WebCore::Cookie>&&, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&&);

private:
    HTTPCookieStore(WebKit::WebsiteDataStore&);
    
    WeakPtr<WebKit::WebsiteDataStore> m_owningDataStore;
    WeakHashSet<Observer> m_observers;
    WeakPtr<WebKit::WebCookieManagerProxy> m_observedCookieManagerProxy;
    std::unique_ptr<APIWebCookieManagerProxyObserver> m_cookieManagerProxyObserver;
};

}
