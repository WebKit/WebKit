/*
 * Copyright (C) 2020 Microsoft Corporation.
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

#include "InspectorPlaywrightAgentClient.h"
#include <WebKit/WKInspector.h>
#include <wtf/Forward.h>
#include <wtf/text/StringHash.h>

typedef void (*ConfigureDataStoreCallback)(WKWebsiteDataStoreRef dataStore);
typedef WKPageRef (*CreatePageCallback)(WKPageConfigurationRef configuration);
typedef void (*QuitCallback)();

namespace WebKit {

class InspectorPlaywrightAgentClientWin : public InspectorPlaywrightAgentClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorPlaywrightAgentClientWin(ConfigureDataStoreCallback, CreatePageCallback, QuitCallback);
    ~InspectorPlaywrightAgentClientWin() override = default;

    RefPtr<WebPageProxy> createPage(WTF::String& error, const BrowserContext&) override;
    void closeBrowser() override;
    std::unique_ptr<BrowserContext> createBrowserContext(WTF::String& error, const WTF::String& proxyServer, const WTF::String& proxyBypassList) override;
    void deleteBrowserContext(WTF::String& error, PAL::SessionID) override;

private:
    ConfigureDataStoreCallback m_configureDataStore;
    CreatePageCallback m_createPage;
    QuitCallback m_quit;
};

} // namespace API

#endif // ENABLE(REMOTE_INSPECTOR)
