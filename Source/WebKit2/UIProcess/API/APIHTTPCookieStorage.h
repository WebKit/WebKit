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
#include "HTTPCookieAcceptPolicy.h"
#include <wtf/Function.h>
#include <wtf/Vector.h>

namespace WebCore {
class URL;
struct Cookie;
}

namespace API {

class WebsiteDataStore;

class HTTPCookieStorage final : public ObjectImpl<Object::Type::HTTPCookieStorage> {
public:
    static Ref<HTTPCookieStorage> create(WebsiteDataStore& websiteDataStore)
    {
        return adoptRef(*new HTTPCookieStorage(websiteDataStore));
    }

    virtual ~HTTPCookieStorage();

    void cookies(Function<void (const Vector<WebCore::Cookie>&)>&& completionHandler);
    void cookies(const WebCore::URL&, Function<void (const Vector<WebCore::Cookie>&)>&& completionHandler);

    void setCookie(const WebCore::Cookie&, Function<void ()>&& completionHandler);
    void setCookies(const Vector<WebCore::Cookie>&, const WebCore::URL&, const WebCore::URL& mainDocumentURL, Function<void ()>&& completionHandler);
    void deleteCookie(const WebCore::Cookie&, Function<void ()>&& completionHandler);

    void removeCookiesSinceDate(std::chrono::system_clock::time_point, Function<void ()>&& completionHandler);

    void setHTTPCookieAcceptPolicy(WebKit::HTTPCookieAcceptPolicy, Function<void ()>&& completionHandler);
    void getHTTPCookieAcceptPolicy(Function<void (WebKit::HTTPCookieAcceptPolicy)>&& completionHandler);
    
private:
    HTTPCookieStorage(WebsiteDataStore&);

    WebsiteDataStore& m_owningDataStore;
};

}
