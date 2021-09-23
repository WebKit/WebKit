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

#include "config.h"
#include "InspectorPlaywrightAgentClientGLib.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "InspectorPlaywrightAgent.h"
#include "WebKitBrowserInspectorPrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebPageProxy.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static WebCore::SoupNetworkProxySettings parseRawProxySettings(const String& proxyServer, const char* const* ignoreHosts)
{
    WebCore::SoupNetworkProxySettings settings;
    if (proxyServer.isEmpty())
        return settings;

    settings.mode = WebCore::SoupNetworkProxySettings::Mode::Custom;
    settings.defaultProxyURL = proxyServer.utf8();
    settings.ignoreHosts.reset(g_strdupv(const_cast<char**>(ignoreHosts)));
    return settings;
}

static WebCore::SoupNetworkProxySettings parseProxySettings(const String& proxyServer, const String& proxyBypassList)
{
    Vector<const char*> ignoreHosts;
    if (!proxyBypassList.isEmpty()) {
        Vector<String> tokens = proxyBypassList.split(',');
        Vector<CString> protectTokens;
        for (String token : tokens) {
            CString cstr = token.utf8();
            ignoreHosts.append(cstr.data());
            protectTokens.append(WTFMove(cstr));
        }
    }
    ignoreHosts.append(nullptr);
    return parseRawProxySettings(proxyServer, ignoreHosts.data());
}

InspectorPlaywrightAgentClientGlib::InspectorPlaywrightAgentClientGlib(const WTF::String& proxyURI, const char* const* ignoreHosts)
    : m_proxySettings(parseRawProxySettings(proxyURI, ignoreHosts))
{
}

RefPtr<WebPageProxy> InspectorPlaywrightAgentClientGlib::createPage(WTF::String& error, const BrowserContext& browserContext)
{
    auto sessionID = browserContext.dataStore->sessionID();
    WebKitWebContext* context = m_idToContext.get(sessionID);
    if (!context && !browserContext.dataStore->isPersistent()) {
        ASSERT_NOT_REACHED();
        error = "Context with provided id not found";
        return nullptr;
    }

    RefPtr<WebPageProxy> page = webkitBrowserInspectorCreateNewPageInContext(context);
    if (page == nullptr) {
        error = "Failed to create new page in the context";
        return nullptr;
    }

    if (context == nullptr && sessionID != page->sessionID()) {
        ASSERT_NOT_REACHED();
        error = " Failed to create new page in default context";
        return nullptr;
    }

    return page;
}

void InspectorPlaywrightAgentClientGlib::closeBrowser()
{
    m_idToContext.clear();
    webkitBrowserInspectorQuitApplication();
    if (webkitWebContextExistingCount() > 1)
        fprintf(stderr, "LEAK: %d contexts are still alive when closing browser\n", webkitWebContextExistingCount());
}

static PAL::SessionID sessionIDFromContext(WebKitWebContext* context)
{
    WebKitWebsiteDataManager* data_manager = webkit_web_context_get_website_data_manager(context);
    WebsiteDataStore& websiteDataStore = webkitWebsiteDataManagerGetDataStore(data_manager);
    return websiteDataStore.sessionID();
}

std::unique_ptr<BrowserContext> InspectorPlaywrightAgentClientGlib::createBrowserContext(WTF::String& error, const WTF::String& proxyServer, const WTF::String& proxyBypassList)
{
    GRefPtr<WebKitWebsiteDataManager> data_manager = adoptGRef(webkit_website_data_manager_new_ephemeral());
    GRefPtr<WebKitWebContext> context = adoptGRef(WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, "website-data-manager", data_manager.get(), "process-swap-on-cross-site-navigation-enabled", true, nullptr)));
    if (!context) {
        error = "Failed to create GLib ephemeral context";
        return nullptr;
    }
    auto browserContext = std::make_unique<BrowserContext>();
    browserContext->processPool = &webkitWebContextGetProcessPool(context.get());
    browserContext->dataStore = &webkitWebsiteDataManagerGetDataStore(data_manager.get());
    PAL::SessionID sessionID = sessionIDFromContext(context.get());
    m_idToContext.set(sessionID, WTFMove(context));

    if (!proxyServer.isEmpty()) {
        WebCore::SoupNetworkProxySettings contextProxySettings = parseProxySettings(proxyServer, proxyBypassList);
        browserContext->dataStore->setNetworkProxySettings(WTFMove(contextProxySettings));
    } else {
        browserContext->dataStore->setNetworkProxySettings(WebCore::SoupNetworkProxySettings(m_proxySettings));
    }
    return browserContext;
}

void InspectorPlaywrightAgentClientGlib::deleteBrowserContext(WTF::String& error, PAL::SessionID sessionID)
{
    m_idToContext.remove(sessionID);
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
