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

#include "config.h"
#include "InspectorPlaywrightAgentClientWin.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "APIPageConfiguration.h"
#include "APIProcessPoolConfiguration.h"
#include "InspectorPlaywrightAgent.h"
#include "WebPageProxy.h"
#include "WebsiteDataStore.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebView.h"
#include "WKAPICast.h"
#include <WebCore/CurlProxySettings.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

InspectorPlaywrightAgentClientWin::InspectorPlaywrightAgentClientWin(ConfigureDataStoreCallback configureDataStore, CreatePageCallback createPage, QuitCallback quit)
    : m_configureDataStore(configureDataStore)
    , m_createPage(createPage)
    , m_quit(quit)
{
}

RefPtr<WebPageProxy> InspectorPlaywrightAgentClientWin::createPage(WTF::String& error, const BrowserContext& context)
{
    auto conf = API::PageConfiguration::create();
    conf->setProcessPool(context.processPool.get());
    conf->setWebsiteDataStore(context.dataStore.get());
    return toImpl(m_createPage(toAPI(&conf.get())));
}

void InspectorPlaywrightAgentClientWin::closeBrowser()
{
    m_quit();
}

std::unique_ptr<BrowserContext> InspectorPlaywrightAgentClientWin::createBrowserContext(WTF::String& error, const WTF::String& proxyServer, const WTF::String& proxyBypassList)
{
    auto config = API::ProcessPoolConfiguration::create();
    auto browserContext = std::make_unique<BrowserContext>();
    browserContext->processPool = WebKit::WebProcessPool::create(config);
    browserContext->dataStore = WebKit::WebsiteDataStore::createNonPersistent();
    m_configureDataStore(toAPI(browserContext->dataStore.get()));
    if (!proxyServer.isEmpty()) {
        URL proxyURL = URL(URL(), proxyServer);
        WebCore::CurlProxySettings settings(WTFMove(proxyURL), String(proxyBypassList));
        browserContext->dataStore->setNetworkProxySettings(WTFMove(settings));
    }
    PAL::SessionID sessionID = browserContext->dataStore->sessionID();
    return browserContext;
}

void InspectorPlaywrightAgentClientWin::deleteBrowserContext(WTF::String& error, PAL::SessionID sessionID)
{
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
