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

#include "MessageReceiver.h"
#include "NetworkProcessSupplement.h"
#include <pal/SessionID.h>
#include <stdint.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WallTime.h>
#include <wtf/WeakRef.h>

#if USE(SOUP)
#include "SoupCookiePersistentStorageType.h"
#endif

OBJC_CLASS NSHTTPCookieStorage;

namespace WebCore {
struct Cookie;
enum class HTTPCookieAcceptPolicy : uint8_t;
}

namespace WebKit {

class NetworkProcess;

class WebCookieManager : public NetworkProcessSupplement, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(WebCookieManager);
    WTF_MAKE_NONCOPYABLE(WebCookieManager);
public:
    WebCookieManager(NetworkProcess&);
    ~WebCookieManager();

    void ref() const;
    void deref() const;

    static ASCIILiteral supplementName();

    void setHTTPCookieAcceptPolicy(PAL::SessionID, WebCore::HTTPCookieAcceptPolicy, CompletionHandler<void()>&&);

#if USE(SOUP)
    void setCookiePersistentStorage(PAL::SessionID, const String& storagePath, SoupCookiePersistentStorageType);
#endif

    void notifyCookiesDidChange(PAL::SessionID);

private:
    Ref<NetworkProcess> protectedProcess();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void getHostnamesWithCookies(PAL::SessionID, CompletionHandler<void(Vector<String>&&)>&&);

    void deleteCookie(PAL::SessionID, const WebCore::Cookie&, CompletionHandler<void()>&&);
    void deleteCookiesForHostnames(PAL::SessionID, const Vector<String>&, CompletionHandler<void()>&&);
    void deleteAllCookies(PAL::SessionID, CompletionHandler<void()>&&);
    void deleteAllCookiesModifiedSince(PAL::SessionID, WallTime, CompletionHandler<void()>&&);

    void setCookie(PAL::SessionID, const Vector<WebCore::Cookie>&, CompletionHandler<void()>&&);
    void setCookies(PAL::SessionID, const Vector<WebCore::Cookie>&, const URL&, const URL& mainDocumentURL, CompletionHandler<void()>&&);
    void getAllCookies(PAL::SessionID, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&&);
    void getCookies(PAL::SessionID, const URL&, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&&);

    void platformSetHTTPCookieAcceptPolicy(PAL::SessionID, WebCore::HTTPCookieAcceptPolicy, CompletionHandler<void()>&&);
    void getHTTPCookieAcceptPolicy(PAL::SessionID, CompletionHandler<void(WebCore::HTTPCookieAcceptPolicy)>&&);

#if USE(SOUP)
    void replaceCookies(PAL::SessionID, const Vector<WebCore::Cookie>&, CompletionHandler<void()>&&);
#endif

    void startObservingCookieChanges(PAL::SessionID);
    void stopObservingCookieChanges(PAL::SessionID);

    WeakRef<NetworkProcess> m_process;
};

#if PLATFORM(COCOA)
void saveCookies(NSHTTPCookieStorage *, CompletionHandler<void()>&&);
#endif

} // namespace WebKit
