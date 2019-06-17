/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "WebVisitedLinkStore.h"

#include "WebHistory.h"
#include "WebView.h"
#include <WebCore/PageCache.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

static bool s_shouldTrackVisitedLinks;

static HashSet<WebVisitedLinkStore*>& visitedLinkStores()
{
    static NeverDestroyed<HashSet<WebVisitedLinkStore*>> visitedLinkStores;

    return visitedLinkStores;
}

Ref<WebVisitedLinkStore> WebVisitedLinkStore::create()
{
    return adoptRef(*new WebVisitedLinkStore);
}

WebVisitedLinkStore::WebVisitedLinkStore()
    : m_visitedLinksPopulated(false)
{
    visitedLinkStores().add(this);
}

WebVisitedLinkStore::~WebVisitedLinkStore()
{
    visitedLinkStores().remove(this);
}

void WebVisitedLinkStore::setShouldTrackVisitedLinks(bool shouldTrackVisitedLinks)
{
    if (s_shouldTrackVisitedLinks == shouldTrackVisitedLinks)
        return;

    s_shouldTrackVisitedLinks = shouldTrackVisitedLinks;
    if (!s_shouldTrackVisitedLinks)
        removeAllVisitedLinks();
}

void WebVisitedLinkStore::removeAllVisitedLinks()
{
    for (auto& visitedLinkStore : visitedLinkStores())
        visitedLinkStore->removeVisitedLinkHashes();
}

void WebVisitedLinkStore::addVisitedLink(const String& urlString)
{
    addVisitedLinkHash(computeSharedStringHash(urlString));
}

bool WebVisitedLinkStore::isLinkVisited(Page& page, SharedStringHash linkHash, const URL& baseURL, const AtomString& attributeURL)
{
    populateVisitedLinksIfNeeded(page);

    return m_visitedLinkHashes.contains(linkHash);
}

void WebVisitedLinkStore::addVisitedLink(Page&, SharedStringHash linkHash)
{
    if (!s_shouldTrackVisitedLinks)
        return;

    addVisitedLinkHash(linkHash);
}

void WebVisitedLinkStore::populateVisitedLinksIfNeeded(Page& sourcePage)
{
    if (m_visitedLinksPopulated)
        return;

    m_visitedLinksPopulated = true;

    WebView* webView = kit(&sourcePage);
    if (!webView)
        return;

    COMPtr<IWebHistoryDelegate> historyDelegate;
    webView->historyDelegate(&historyDelegate);
    if (historyDelegate) {
        historyDelegate->populateVisitedLinksForWebView(webView);
        return;
    }

    WebHistory* history = WebHistory::sharedHistory();
    if (!history)
        return;
    history->addVisitedLinksToVisitedLinkStore(*this);
}

void WebVisitedLinkStore::addVisitedLinkHash(SharedStringHash linkHash)
{
    ASSERT(s_shouldTrackVisitedLinks);
    m_visitedLinkHashes.add(linkHash);

    invalidateStylesForLink(linkHash);
}

void WebVisitedLinkStore::removeVisitedLinkHashes()
{
    m_visitedLinksPopulated = false;
    if (m_visitedLinkHashes.isEmpty())
        return;
    m_visitedLinkHashes.clear();

    invalidateStylesForAllLinks();
}
