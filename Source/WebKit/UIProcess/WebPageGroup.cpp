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
#include "APIContentRuleList.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "WebCompiledContentRuleList.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebUserContentControllerProxy.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenate.h>

namespace WebKit {

typedef HashMap<PageGroupIdentifier, WebPageGroup*> WebPageGroupMap;

static WebPageGroupMap& webPageGroupMap()
{
    static NeverDestroyed<WebPageGroupMap> map;
    return map;
}

Ref<WebPageGroup> WebPageGroup::create(const String& identifier)
{
    return adoptRef(*new WebPageGroup(identifier));
}

WebPageGroup* WebPageGroup::get(PageGroupIdentifier pageGroupID)
{
    return webPageGroupMap().get(pageGroupID);
}

void WebPageGroup::forEach(Function<void(WebPageGroup&)>&& function)
{
    auto allGroups = copyToVectorOf<RefPtr<WebPageGroup>>(webPageGroupMap().values());
    for (auto& group : allGroups) {
        if (group)
            function(*group);
    }
}

static WebPageGroupData pageGroupData(const String& identifier)
{
    WebPageGroupData data;

    static NeverDestroyed<HashMap<String, PageGroupIdentifier>> map;
    if (HashMap<String, PageGroupIdentifier>::isValidKey(identifier)) {
        data.pageGroupID = map.get().ensure(identifier, [] {
            return PageGroupIdentifier::generate();
        }).iterator->value;
    } else
        data.pageGroupID = PageGroupIdentifier::generate();

    if (!identifier.isEmpty())
        data.identifier = identifier;
    else
        data.identifier = makeString("__uniquePageGroupID-", data.pageGroupID.toUInt64());

    return data;
}

// FIXME: Why does the WebPreferences object here use ".WebKit2" instead of "WebKit2." which all the other constructors use.
// If it turns out that it's wrong, we can change it to to "WebKit2." and get rid of the globalDebugKeyPrefix from WebPreferences.
WebPageGroup::WebPageGroup(const String& identifier)
    : m_data(pageGroupData(identifier))
    , m_preferences(WebPreferences::createWithLegacyDefaults(m_data.identifier, ".WebKit2", "WebKit2."))
    , m_userContentController(WebUserContentControllerProxy::create())
{
    webPageGroupMap().set(m_data.pageGroupID, this);
}

WebPageGroup::~WebPageGroup()
{
    webPageGroupMap().remove(pageGroupID());
}

void WebPageGroup::addPage(WebPageProxy& page)
{
    m_pages.add(page);
}

void WebPageGroup::removePage(WebPageProxy& page)
{
    m_pages.remove(page);
}

void WebPageGroup::setPreferences(WebPreferences* preferences)
{
    if (preferences == m_preferences)
        return;

    m_preferences = preferences;

    for (auto& webPageProxy : m_pages)
        webPageProxy.setPreferences(*m_preferences);
}

WebPreferences& WebPageGroup::preferences() const
{
    return *m_preferences;
}

WebUserContentControllerProxy& WebPageGroup::userContentController()
{
    return m_userContentController;
}

} // namespace WebKit
