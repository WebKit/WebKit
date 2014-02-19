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

#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "DocumentStyleSheetCollection.h"
#include "GroupSettings.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageCache.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StorageNamespace.h"
#include "UserContentController.h"
#include "VisitedLinkProvider.h"
#include <wtf/StdLibExtras.h>

#if ENABLE(VIDEO_TRACK)
#if PLATFORM(MAC) || HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#include "CaptionUserPreferencesMediaAF.h"
#else
#include "CaptionUserPreferences.h"
#endif
#endif

namespace WebCore {

static unsigned getUniqueIdentifier()
{
    static unsigned currentIdentifier = 0;
    return ++currentIdentifier;
}

// --------

static bool shouldTrackVisitedLinks = false;

PageGroup::PageGroup(const String& name)
    : m_name(name)
    , m_visitedLinkProvider(VisitedLinkProvider::create())
    , m_visitedLinksPopulated(false)
    , m_identifier(getUniqueIdentifier())
    , m_userContentController(UserContentController::create())
    , m_groupSettings(std::make_unique<GroupSettings>())
{
}

PageGroup::PageGroup(Page& page)
    : m_visitedLinkProvider(VisitedLinkProvider::create())
    , m_visitedLinksPopulated(false)
    , m_identifier(getUniqueIdentifier())
    , m_userContentController(UserContentController::create())
    , m_groupSettings(std::make_unique<GroupSettings>())
{
    addPage(page);
}

PageGroup::~PageGroup()
{
    removeAllUserContent();
}

typedef HashMap<String, PageGroup*> PageGroupMap;
static PageGroupMap* pageGroups = 0;

PageGroup* PageGroup::pageGroup(const String& groupName)
{
    ASSERT(!groupName.isEmpty());
    
    if (!pageGroups)
        pageGroups = new PageGroupMap;

    PageGroupMap::AddResult result = pageGroups->add(groupName, nullptr);

    if (result.isNewEntry) {
        ASSERT(!result.iterator->value);
        result.iterator->value = new PageGroup(groupName);
    }

    ASSERT(result.iterator->value);
    return result.iterator->value;
}

void PageGroup::closeLocalStorage()
{
    if (!pageGroups)
        return;

    for (auto it = pageGroups->begin(), end = pageGroups->end(); it != end; ++it) {
        if (it->value->hasLocalStorage())
            it->value->localStorage()->close();
    }
}

void PageGroup::clearLocalStorageForAllOrigins()
{
    if (!pageGroups)
        return;

    for (auto it = pageGroups->begin(), end = pageGroups->end(); it != end; ++it) {
        if (it->value->hasLocalStorage())
            it->value->localStorage()->clearAllOriginsForDeletion();
    }
}

void PageGroup::clearLocalStorageForOrigin(SecurityOrigin* origin)
{
    if (!pageGroups)
        return;

    for (auto it = pageGroups->begin(), end = pageGroups->end(); it != end; ++it) {
        if (it->value->hasLocalStorage())
            it->value->localStorage()->clearOriginForDeletion(origin);
    }
}

void PageGroup::closeIdleLocalStorageDatabases()
{
    if (!pageGroups)
        return;

    for (auto it = pageGroups->begin(), end = pageGroups->end(); it != end; ++it) {
        if (it->value->hasLocalStorage())
            it->value->localStorage()->closeIdleLocalStorageDatabases();
    }
}

void PageGroup::syncLocalStorage()
{
    if (!pageGroups)
        return;

    for (auto it = pageGroups->begin(), end = pageGroups->end(); it != end; ++it) {
        if (it->value->hasLocalStorage())
            it->value->localStorage()->sync();
    }
}

void PageGroup::addPage(Page& page)
{
    ASSERT(!m_pages.contains(&page));
    m_pages.add(&page);

    page.setUserContentController(m_userContentController.get());
}

void PageGroup::removePage(Page& page)
{
    ASSERT(m_pages.contains(&page));
    m_pages.remove(&page);

    page.setUserContentController(nullptr);
}

bool PageGroup::isLinkVisited(LinkHash visitedLinkHash)
{
    if (!m_visitedLinksPopulated) {
        m_visitedLinksPopulated = true;
        ASSERT(!m_pages.isEmpty());
        (*m_pages.begin())->chrome().client().populateVisitedLinks();
    }
    return m_visitedLinkHashes.contains(visitedLinkHash);
}

void PageGroup::addVisitedLinkHash(LinkHash hash)
{
    if (shouldTrackVisitedLinks)
        addVisitedLink(hash);
}

inline void PageGroup::addVisitedLink(LinkHash hash)
{
    ASSERT(shouldTrackVisitedLinks);
    if (!m_visitedLinkHashes.add(hash).isNewEntry)
        return;
    Page::visitedStateChanged(this, hash);
    pageCache()->markPagesForVistedLinkStyleRecalc();
}

void PageGroup::addVisitedLink(const URL& url)
{
    if (!shouldTrackVisitedLinks)
        return;
    ASSERT(!url.isEmpty());
    addVisitedLink(visitedLinkHash(url.string()));
}

void PageGroup::addVisitedLink(const UChar* characters, size_t length)
{
    if (!shouldTrackVisitedLinks)
        return;
    addVisitedLink(visitedLinkHash(characters, length));
}

void PageGroup::removeVisitedLink(const URL& url)
{
    LinkHash hash = visitedLinkHash(url.string());
    ASSERT(m_visitedLinkHashes.contains(hash));
    m_visitedLinkHashes.remove(hash);

    Page::allVisitedStateChanged(this);
    pageCache()->markPagesForVistedLinkStyleRecalc();
}

void PageGroup::removeVisitedLinks()
{
    m_visitedLinksPopulated = false;
    if (m_visitedLinkHashes.isEmpty())
        return;
    m_visitedLinkHashes.clear();
    Page::allVisitedStateChanged(this);
    pageCache()->markPagesForVistedLinkStyleRecalc();
}

void PageGroup::removeAllVisitedLinks()
{
    Page::removeAllVisitedLinks();
    pageCache()->markPagesForVistedLinkStyleRecalc();
}

void PageGroup::setShouldTrackVisitedLinks(bool shouldTrack)
{
    if (shouldTrackVisitedLinks == shouldTrack)
        return;
    shouldTrackVisitedLinks = shouldTrack;
    if (!shouldTrackVisitedLinks)
        removeAllVisitedLinks();
}

StorageNamespace* PageGroup::localStorage()
{
    if (!m_localStorage)
        m_localStorage = StorageNamespace::localStorageNamespace(this);

    return m_localStorage.get();
}

StorageNamespace* PageGroup::transientLocalStorage(SecurityOrigin* topOrigin)
{
    auto result = m_transientLocalStorageMap.add(topOrigin, nullptr);

    if (result.isNewEntry)
        result.iterator->value = StorageNamespace::transientLocalStorageNamespace(this, topOrigin);

    return result.iterator->value.get();
}

void PageGroup::addUserScriptToWorld(DOMWrapperWorld& world, const String& source, const URL& url, const Vector<String>& whitelist, const Vector<String>& blacklist, UserScriptInjectionTime injectionTime, UserContentInjectedFrames injectedFrames)
{
    auto userScript = std::make_unique<UserScript>(source, url, whitelist, blacklist, injectionTime, injectedFrames);
    m_userContentController->addUserScript(world, std::move(userScript));
}

void PageGroup::addUserStyleSheetToWorld(DOMWrapperWorld& world, const String& source, const URL& url, const Vector<String>& whitelist, const Vector<String>& blacklist, UserContentInjectedFrames injectedFrames, UserStyleLevel level, UserStyleInjectionTime injectionTime)
{
    auto userStyleSheet = std::make_unique<UserStyleSheet>(source, url, whitelist, blacklist, injectedFrames, level);
    m_userContentController->addUserStyleSheet(world, std::move(userStyleSheet), injectionTime);

}

void PageGroup::removeUserScriptFromWorld(DOMWrapperWorld& world, const URL& url)
{
    m_userContentController->removeUserScript(world, url);
}

void PageGroup::removeUserStyleSheetFromWorld(DOMWrapperWorld& world, const URL& url)
{
    m_userContentController->removeUserStyleSheet(world, url);
}

void PageGroup::removeUserScriptsFromWorld(DOMWrapperWorld& world)
{
    m_userContentController->removeUserScripts(world);
}

void PageGroup::removeUserStyleSheetsFromWorld(DOMWrapperWorld& world)
{
    m_userContentController->removeUserStyleSheets(world);
}

void PageGroup::removeAllUserContent()
{
    m_userContentController->removeAllUserContent();
}

#if ENABLE(VIDEO_TRACK)
void PageGroup::captionPreferencesChanged()
{
    for (auto it = m_pages.begin(), end = m_pages.end(); it != end; ++it)
        (*it)->captionPreferencesChanged();
    pageCache()->markPagesForCaptionPreferencesChanged();
}

CaptionUserPreferences* PageGroup::captionPreferences()
{
    if (!m_captionPreferences) {
#if PLATFORM(MAC) || HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
        m_captionPreferences = std::make_unique<CaptionUserPreferencesMediaAF>(*this);
#else
        m_captionPreferences = std::make_unique<CaptionUserPreferences>(*this);
#endif
    }

    return m_captionPreferences.get();
}
#endif

} // namespace WebCore
