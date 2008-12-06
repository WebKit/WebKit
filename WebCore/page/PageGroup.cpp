/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PageGroup.h"

#include "ChromeClient.h"
#include "Document.h"
#include "Page.h"
#include "Settings.h"

#if ENABLE(DOM_STORAGE)
#include "LocalStorage.h"
#include "StorageArea.h"
#endif

#if PLATFORM(CHROMIUM)
#include "ChromiumBridge.h"
#endif

namespace WebCore {

static unsigned getUniqueIdentifier()
{
    static unsigned currentIdentifier = 0;
    return ++currentIdentifier;
}

// --------

static bool shouldTrackVisitedLinks;

PageGroup::PageGroup(const String& name)
    : m_name(name)
    , m_visitedLinksPopulated(false)
    , m_identifier(getUniqueIdentifier())
{
}

PageGroup::PageGroup(Page* page)
    : m_visitedLinksPopulated(false)
    , m_identifier(getUniqueIdentifier())
{
    ASSERT(page);
    addPage(page);
}

typedef HashMap<String, PageGroup*> PageGroupMap;
static PageGroupMap* pageGroups = 0;

PageGroup* PageGroup::pageGroup(const String& groupName)
{
    ASSERT(!groupName.isEmpty());
    
    if (!pageGroups)
        pageGroups = new PageGroupMap;

    pair<PageGroupMap::iterator, bool> result = pageGroups->add(groupName, 0);

    if (result.second) {
        ASSERT(!result.first->second);
        result.first->second = new PageGroup(groupName);
    }

    ASSERT(result.first->second);
    return result.first->second;
}

void PageGroup::closeLocalStorage()
{
#if ENABLE(DOM_STORAGE)
    if (!pageGroups)
        return;

    PageGroupMap::iterator end = pageGroups->end();

    for (PageGroupMap::iterator it = pageGroups->begin(); it != end; ++it) {
        if (LocalStorage* localStorage = it->second->localStorage())
            localStorage->close();
    }
#endif
}

void PageGroup::addPage(Page* page)
{
    ASSERT(page);
    ASSERT(!m_pages.contains(page));
    m_pages.add(page);
#if ENABLE(DOM_STORAGE)
    if (!m_localStorage)
        m_localStorage = LocalStorage::localStorage(page->settings()->localStorageDatabasePath());
#endif
}

void PageGroup::removePage(Page* page)
{
    ASSERT(page);
    ASSERT(m_pages.contains(page));
    m_pages.remove(page);
}

bool PageGroup::isLinkVisited(LinkHash visitedLinkHash)
{
#if PLATFORM(CHROMIUM)
    // Use Chromium's built-in visited link database.
    return ChromiumBridge::isLinkVisited(visitedLinkHash);
#else
    if (!m_visitedLinksPopulated) {
        m_visitedLinksPopulated = true;
        ASSERT(!m_pages.isEmpty());
        (*m_pages.begin())->chrome()->client()->populateVisitedLinks();
    }
    return m_visitedLinkHashes.contains(visitedLinkHash);
#endif
}

inline void PageGroup::addVisitedLink(LinkHash hash)
{
    ASSERT(shouldTrackVisitedLinks);
#if !PLATFORM(CHROMIUM)
    if (!m_visitedLinkHashes.add(hash).second)
        return;
#endif
    Page::visitedStateChanged(this, hash);
}

void PageGroup::addVisitedLink(const KURL& url)
{
    if (!shouldTrackVisitedLinks)
        return;
    ASSERT(!url.isEmpty());
    addVisitedLink(visitedLinkHash(url.string().characters(), url.string().length()));
}

void PageGroup::addVisitedLink(const UChar* characters, size_t length)
{
    if (!shouldTrackVisitedLinks)
        return;
    addVisitedLink(visitedLinkHash(characters, length));
}

void PageGroup::removeVisitedLinks()
{
    m_visitedLinksPopulated = false;
    if (m_visitedLinkHashes.isEmpty())
        return;
    m_visitedLinkHashes.clear();
    Page::allVisitedStateChanged(this);
}

void PageGroup::removeAllVisitedLinks()
{
    Page::removeAllVisitedLinks();
}

void PageGroup::setShouldTrackVisitedLinks(bool shouldTrack)
{
    if (shouldTrackVisitedLinks == shouldTrack)
        return;
    shouldTrackVisitedLinks = shouldTrack;
    if (!shouldTrackVisitedLinks)
        removeAllVisitedLinks();
}

#if ENABLE(DOM_STORAGE)
LocalStorage* PageGroup::localStorage()
{
    return m_localStorage.get();
}
#endif

} // namespace WebCore
