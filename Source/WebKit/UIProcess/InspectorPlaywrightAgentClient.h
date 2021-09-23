/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(REMOTE_INSPECTOR)

#include <memory>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class WebsiteDataStore;
class WebPageProxy;
class WebProcessPool;

class BrowserContext {
    WTF_MAKE_NONCOPYABLE(BrowserContext);
    WTF_MAKE_FAST_ALLOCATED;
public:
    BrowserContext();
    ~BrowserContext();

    RefPtr<WebsiteDataStore> dataStore;
    RefPtr<WebProcessPool> processPool;
    HashSet<WebPageProxy*> pages;
};

class InspectorPlaywrightAgentClient {
public:
    virtual ~InspectorPlaywrightAgentClient() = default;
    virtual RefPtr<WebKit::WebPageProxy> createPage(WTF::String& error, const BrowserContext& context) = 0;
    virtual void closeBrowser() = 0;
    virtual std::unique_ptr<BrowserContext> createBrowserContext(WTF::String& error, const WTF::String& proxyServer, const WTF::String& proxyBypassList) = 0;
    virtual void deleteBrowserContext(WTF::String& error, PAL::SessionID) = 0;
};

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
