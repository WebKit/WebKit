/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#include "CallbackID.h"
#include "HTTPCookieAcceptPolicy.h"
#include "MessageReceiver.h"
#include "NetworkProcessSupplement.h"
#include "OptionalCallbackID.h"
#include <pal/SessionID.h>
#include <stdint.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

#if USE(SOUP)
#include "SoupCookiePersistentStorageType.h"
#endif

namespace WebCore {
struct Cookie;
}

namespace WebKit {

class NetworkProcess;

class WebCookieManager : public NetworkProcessSupplement, public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebCookieManager);
public:
    WebCookieManager(NetworkProcess&);
    ~WebCookieManager();

    static const char* supplementName();

    void setHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy, OptionalCallbackID);

#if USE(SOUP)
    void setCookiePersistentStorage(const String& storagePath, uint32_t storageType);
#endif

    void notifyCookiesDidChange(PAL::SessionID);

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void getHostnamesWithCookies(PAL::SessionID, CallbackID);

    void deleteCookie(PAL::SessionID, const WebCore::Cookie&, CallbackID);
    void deleteCookiesForHostname(PAL::SessionID, const String&);
    void deleteAllCookies(PAL::SessionID);
    void deleteAllCookiesModifiedSince(PAL::SessionID, WallTime, CallbackID);

    void setCookie(PAL::SessionID, const WebCore::Cookie&, CallbackID);
    void setCookies(PAL::SessionID, const Vector<WebCore::Cookie>&, const URL&, const URL& mainDocumentURL, CallbackID);
    void getAllCookies(PAL::SessionID, CallbackID);
    void getCookies(PAL::SessionID, const URL&, CallbackID);

    void platformSetHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy);
    void getHTTPCookieAcceptPolicy(CallbackID);
    HTTPCookieAcceptPolicy platformGetHTTPCookieAcceptPolicy();

    void startObservingCookieChanges(PAL::SessionID);
    void stopObservingCookieChanges(PAL::SessionID);

    NetworkProcess& m_process;
};

} // namespace WebKit
