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
#include "LocalStorage.h"
#include "Page.h"
#include "StorageArea.h"

namespace WebCore {

// --------

static bool shouldTrackVisitedLinks;

PageGroup::PageGroup(Page* page)
    : m_visitedLinksPopulated(false)
{
    ASSERT(page);
    m_pages.add(page);
}

void PageGroup::addPage(Page* page)
{
    ASSERT(page);
    ASSERT(!m_pages.contains(page));
    m_pages.add(page);
}

void PageGroup::removePage(Page* page)
{
    ASSERT(page);
    ASSERT(m_pages.contains(page));
    m_pages.remove(page);
}

bool PageGroup::isLinkVisited(unsigned visitedLinkHash)
{
    if (!m_visitedLinksPopulated) {
        m_visitedLinksPopulated = true;
        ASSERT(!m_pages.isEmpty());
        (*m_pages.begin())->chrome()->client()->populateVisitedLinks();
    }
    return m_visitedLinkHashes.contains(visitedLinkHash);
}

inline void PageGroup::addVisitedLink(unsigned stringHash)
{
    ASSERT(shouldTrackVisitedLinks);
    unsigned visitedLinkHash = AlreadyHashed::avoidDeletedValue(stringHash);
    if (!m_visitedLinkHashes.add(visitedLinkHash).second)
        return;
    Page::visitedStateChanged(this, visitedLinkHash);
}

void PageGroup::addVisitedLink(const KURL& url)
{
    if (!shouldTrackVisitedLinks)
        return;
    ASSERT(!url.isEmpty());
    addVisitedLink(url.string().impl()->hash());
}

void PageGroup::addVisitedLink(const UChar* characters, size_t length)
{
    if (!shouldTrackVisitedLinks)
        return;
    addVisitedLink(StringImpl::computeHash(characters, length));
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

LocalStorage* PageGroup::localStorage()
{
    if (!m_localStorage)
        m_localStorage = LocalStorage::create(this);
        
    return m_localStorage.get();
}

} // namespace WebCore
