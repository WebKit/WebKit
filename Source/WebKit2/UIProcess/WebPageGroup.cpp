/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPageGroup.h"

#include "APIArray.h"
#include "APIUserContentExtension.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "WebCompiledContentExtension.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebUserContentControllerProxy.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenate.h>

namespace WebKit {

static uint64_t generatePageGroupID()
{
    static uint64_t uniquePageGroupID = 1;
    return uniquePageGroupID++;
}

typedef HashMap<uint64_t, WebPageGroup*> WebPageGroupMap;

static WebPageGroupMap& webPageGroupMap()
{
    static NeverDestroyed<WebPageGroupMap> map;
    return map;
}

PassRefPtr<WebPageGroup> WebPageGroup::create(const String& identifier, bool visibleToInjectedBundle, bool visibleToHistoryClient)
{
    return adoptRef(new WebPageGroup(identifier, visibleToInjectedBundle, visibleToHistoryClient));
}

Ref<WebPageGroup> WebPageGroup::createNonNull(const String& identifier, bool visibleToInjectedBundle, bool visibleToHistoryClient)
{
    return adoptRef(*new WebPageGroup(identifier, visibleToInjectedBundle, visibleToHistoryClient));
}

WebPageGroup* WebPageGroup::get(uint64_t pageGroupID)
{
    return webPageGroupMap().get(pageGroupID);
}

static WebPageGroupData pageGroupData(const String& identifier, bool visibleToInjectedBundle, bool visibleToHistoryClient)
{
    WebPageGroupData data;

    data.pageGroupID = generatePageGroupID();

    if (!identifier.isEmpty())
        data.identifier = identifier;
    else
        data.identifier = makeString("__uniquePageGroupID-", String::number(data.pageGroupID));

    data.visibleToInjectedBundle = visibleToInjectedBundle;
    data.visibleToHistoryClient = visibleToHistoryClient;

    return data;
}

// FIXME: Why does the WebPreferences object here use ".WebKit2" instead of "WebKit2." which all the other constructors use.
// If it turns out that it's wrong, we can change it to to "WebKit2." and get rid of the globalDebugKeyPrefix from WebPreferences.
WebPageGroup::WebPageGroup(const String& identifier, bool visibleToInjectedBundle, bool visibleToHistoryClient)
    : m_data(pageGroupData(identifier, visibleToInjectedBundle, visibleToHistoryClient))
    , m_preferences(WebPreferences::createWithLegacyDefaults(m_data.identifier, ".WebKit2", "WebKit2."))
    , m_userContentController(WebUserContentControllerProxy::create())
{
    m_data.userContentControllerIdentifier = m_userContentController->identifier();

    webPageGroupMap().set(m_data.pageGroupID, this);
}

WebPageGroup::~WebPageGroup()
{
    webPageGroupMap().remove(pageGroupID());
}

void WebPageGroup::addPage(WebPageProxy* page)
{
    m_pages.add(page);
}

void WebPageGroup::removePage(WebPageProxy* page)
{
    m_pages.remove(page);
}

void WebPageGroup::setPreferences(WebPreferences* preferences)
{
    if (preferences == m_preferences)
        return;

    m_preferences = preferences;

    for (auto& webPageProxy : m_pages)
        webPageProxy->setPreferences(*m_preferences);
}

WebPreferences& WebPageGroup::preferences() const
{
    return *m_preferences;
}

void WebPageGroup::preferencesDidChange()
{
    for (HashSet<WebPageProxy*>::iterator it = m_pages.begin(), end = m_pages.end(); it != end; ++it) {
        WebPageProxy* page = *it;
        page->preferencesDidChange();
    }
}

WebUserContentControllerProxy& WebPageGroup::userContentController()
{
    return *m_userContentController;
}

void WebPageGroup::addUserStyleSheet(const String& source, const String& baseURL, API::Array* whitelist, API::Array* blacklist, WebCore::UserContentInjectedFrames injectedFrames, WebCore::UserStyleLevel level)
{
    if (source.isEmpty())
        return;

    Ref<API::UserStyleSheet> userStyleSheet = API::UserStyleSheet::create(WebCore::UserStyleSheet { source, (baseURL.isEmpty() ? WebCore::blankURL() : WebCore::URL(WebCore::URL(), baseURL)), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), injectedFrames, level }, API::UserContentWorld::normalWorld());
    m_userContentController->addUserStyleSheet(userStyleSheet.get());
}

void WebPageGroup::addUserScript(const String& source, const String& baseURL, API::Array* whitelist, API::Array* blacklist, WebCore::UserContentInjectedFrames injectedFrames, WebCore::UserScriptInjectionTime injectionTime)
{
    if (source.isEmpty())
        return;

    Ref<API::UserScript> userScript = API::UserScript::create(WebCore::UserScript { source, (baseURL.isEmpty() ? WebCore::blankURL() : WebCore::URL(WebCore::URL(), baseURL)), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), injectionTime, injectedFrames }, API::UserContentWorld::normalWorld());
    m_userContentController->addUserScript(userScript.get());
}

void WebPageGroup::removeAllUserStyleSheets()
{
    m_userContentController->removeAllUserStyleSheets();
}

void WebPageGroup::removeAllUserScripts()
{
    m_userContentController->removeAllUserScripts();
}

void WebPageGroup::removeAllUserContent()
{
    m_userContentController->removeAllUserStyleSheets();
    m_userContentController->removeAllUserScripts();
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebPageGroup::addUserContentExtension(API::UserContentExtension& userContentExtension)
{
    m_userContentController->addUserContentExtension(userContentExtension);
}

void WebPageGroup::removeUserContentExtension(const String& contentExtensionName)
{
    m_userContentController->removeUserContentExtension(contentExtensionName);
}

void WebPageGroup::removeAllUserContentExtensions()
{
    m_userContentController->removeAllUserContentExtensions();
}
#endif

} // namespace WebKit
