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
#include "WebPageGroupProxyMessages.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
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

PassRef<WebPageGroup> WebPageGroup::createNonNull(const String& identifier, bool visibleToInjectedBundle, bool visibleToHistoryClient)
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
        data.identifer = identifier;
    else
        data.identifer = makeString("__uniquePageGroupID-", String::number(data.pageGroupID));

    data.visibleToInjectedBundle = visibleToInjectedBundle;
    data.visibleToHistoryClient = visibleToHistoryClient;

    return data;
}

WebPageGroup::WebPageGroup(const String& identifier, bool visibleToInjectedBundle, bool visibleToHistoryClient)
    : m_data(pageGroupData(identifier, visibleToInjectedBundle, visibleToHistoryClient))
    , m_preferences(WebPreferences::create(m_data.identifer, ".WebKit2"))
{
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

void WebPageGroup::addUserStyleSheet(const String& source, const String& baseURL, API::Array* whitelist, API::Array* blacklist, WebCore::UserContentInjectedFrames injectedFrames, WebCore::UserStyleLevel level)
{
    if (source.isEmpty())
        return;

    WebCore::UserStyleSheet userStyleSheet = WebCore::UserStyleSheet(source, (baseURL.isEmpty() ? WebCore::blankURL() : WebCore::URL(WebCore::URL(), baseURL)), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), injectedFrames, level);

    m_data.userStyleSheets.append(userStyleSheet);
    sendToAllProcessesInGroup(Messages::WebPageGroupProxy::AddUserStyleSheet(userStyleSheet), m_data.pageGroupID);
}

void WebPageGroup::addUserScript(const String& source, const String& baseURL, API::Array* whitelist, API::Array* blacklist, WebCore::UserContentInjectedFrames injectedFrames, WebCore::UserScriptInjectionTime injectionTime)
{
    if (source.isEmpty())
        return;

    WebCore::UserScript userScript = WebCore::UserScript(source, (baseURL.isEmpty() ? WebCore::blankURL() : WebCore::URL(WebCore::URL(), baseURL)), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), injectionTime, injectedFrames);

    m_data.userScripts.append(userScript);
    sendToAllProcessesInGroup(Messages::WebPageGroupProxy::AddUserScript(userScript), m_data.pageGroupID);
}

void WebPageGroup::removeAllUserStyleSheets()
{
    m_data.userStyleSheets.clear();
    sendToAllProcessesInGroup(Messages::WebPageGroupProxy::RemoveAllUserStyleSheets(), m_data.pageGroupID);
}

void WebPageGroup::removeAllUserScripts()
{
    m_data.userScripts.clear();
    sendToAllProcessesInGroup(Messages::WebPageGroupProxy::RemoveAllUserScripts(), m_data.pageGroupID);
}

void WebPageGroup::removeAllUserContent()
{
    m_data.userStyleSheets.clear();
    m_data.userScripts.clear();
    sendToAllProcessesInGroup(Messages::WebPageGroupProxy::RemoveAllUserContent(), m_data.pageGroupID);
}

} // namespace WebKit
