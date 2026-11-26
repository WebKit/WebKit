/*
 * Copyright (C) 2025 Microsoft Corporation. All rights reserved.
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

#if ENABLE(WEBDRIVER_BIDI)

#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
#if ENABLE(2022_GLIB_API)
typedef struct _WebKitNetworkSession WebKitNetworkSession;
#else
typedef struct _WebKitWebsiteDataManager WebKitWebsiteDataManager;
#endif
#endif

namespace WebKit {

class WebPageProxy;
class WebProcessPool;
class WebsiteDataStore;

class BidiUserContext {
    WTF_MAKE_NONCOPYABLE(BidiUserContext);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(BidiUserContext);
public:
#if USE(GLIB)
#if ENABLE(2022_GLIB_API)
    BidiUserContext(WebsiteDataStore&, WebProcessPool&, GRefPtr<WebKitNetworkSession>&&);
    WebKitNetworkSession* networkSession() const { return m_networkSession.get(); }
#else
    BidiUserContext(WebsiteDataStore&, WebProcessPool&, GRefPtr<WebKitWebsiteDataManager>&&);
    WebKitWebsiteDataManager* websiteDataManager() const { return m_websiteDataManager.get(); }
#endif
#else
    BidiUserContext(WebsiteDataStore&, WebProcessPool&);
#endif
    ~BidiUserContext();

    WebsiteDataStore& dataStore() const { return m_dataStore.get(); }
    WebProcessPool& processPool() const { return m_processPool.get(); }

    Vector<Ref<WebPageProxy>> allPages() const;

    unsigned pendingPageCreationCount() const { return m_pendingPageCreationCount; }
    void didStartPageCreation() { ++m_pendingPageCreationCount; }
    void didFinishPageCreation();

private:
    const Ref<WebsiteDataStore> m_dataStore;
    const Ref<WebProcessPool> m_processPool;
    unsigned m_pendingPageCreationCount { 0 };
#if USE(GLIB)
#if ENABLE(2022_GLIB_API)
    GRefPtr<WebKitNetworkSession> m_networkSession;
#else
    GRefPtr<WebKitWebsiteDataManager> m_websiteDataManager;
#endif
#endif
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
