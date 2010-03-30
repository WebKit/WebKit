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
#include "Document.h"
#include "Frame.h"
#include "IndexedDatabase.h"
#include "Page.h"
#include "Settings.h"
#include "StorageNamespace.h"

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

static bool shouldTrackVisitedLinks = false;

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
        if (it->second->hasLocalStorage())
            it->second->localStorage()->close();
    }
#endif
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
StorageNamespace* PageGroup::localStorage()
{
    if (!m_localStorage) {
        // Need a page in this page group to query the settings for the local storage database path.
        // Having these parameters attached to the page settings is unfortunate since these settings are
        // not per-page (and, in fact, we simply grab the settings from some page at random), but
        // at this point we're stuck with it.
        Page* page = *m_pages.begin();
        const String& path = page->settings()->localStorageDatabasePath();
        unsigned quota = page->settings()->localStorageQuota();
        m_localStorage = StorageNamespace::localStorageNamespace(path, quota);
    }

    return m_localStorage.get();
}
#endif

#if ENABLE(INDEXED_DATABASE)
IndexedDatabase* PageGroup::indexedDatabase()
{
    // Do not add page setting based access control here since this object is shared by all pages in
    // the group and having per-page controls is misleading.
    if (!m_indexedDatabase)
        m_indexedDatabase = IndexedDatabase::create();
    return m_indexedDatabase.get();
}
#endif

void PageGroup::addUserScriptToWorld(DOMWrapperWorld* world, const String& source, const KURL& url,  PassOwnPtr<Vector<String> > whitelist,
                                     PassOwnPtr<Vector<String> > blacklist, UserScriptInjectionTime injectionTime)
{
    ASSERT_ARG(world, world);

    OwnPtr<UserScript> userScript(new UserScript(source, url, whitelist, blacklist, injectionTime));
    if (!m_userScripts)
        m_userScripts.set(new UserScriptMap);
    UserScriptVector*& scriptsInWorld = m_userScripts->add(world, 0).first->second;
    if (!scriptsInWorld)
        scriptsInWorld = new UserScriptVector;
    scriptsInWorld->append(userScript.release());
}

void PageGroup::addUserStyleSheetToWorld(DOMWrapperWorld* world, const String& source, const KURL& url, PassOwnPtr<Vector<String> > whitelist,
                                         PassOwnPtr<Vector<String> > blacklist)
{
    ASSERT_ARG(world, world);

    OwnPtr<UserStyleSheet> userStyleSheet(new UserStyleSheet(source, url, whitelist, blacklist));
    if (!m_userStyleSheets)
        m_userStyleSheets.set(new UserStyleSheetMap);
    UserStyleSheetVector*& styleSheetsInWorld = m_userStyleSheets->add(world, 0).first->second;
    if (!styleSheetsInWorld)
        styleSheetsInWorld = new UserStyleSheetVector;
    styleSheetsInWorld->append(userStyleSheet.release());
    
    // Clear our cached sheets and have them just reparse.
    HashSet<Page*>::const_iterator end = m_pages.end();
    for (HashSet<Page*>::const_iterator it = m_pages.begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frame->document()->clearPageGroupUserSheets();
    }
}

void PageGroup::removeUserScriptFromWorld(DOMWrapperWorld* world, const KURL& url)
{
    ASSERT_ARG(world, world);

    if (!m_userScripts)
        return;

    UserScriptMap::iterator it = m_userScripts->find(world);
    if (it == m_userScripts->end())
        return;
    
    UserScriptVector* scripts = it->second;
    for (int i = scripts->size() - 1; i >= 0; --i) {
        if (scripts->at(i)->url() == url)
            scripts->remove(i);
    }
    
    if (!scripts->isEmpty())
        return;
    
    delete it->second;
    m_userScripts->remove(it);
}

void PageGroup::removeUserStyleSheetFromWorld(DOMWrapperWorld* world, const KURL& url)
{
    ASSERT_ARG(world, world);

    if (!m_userStyleSheets)
        return;

    UserStyleSheetMap::iterator it = m_userStyleSheets->find(world);
    bool sheetsChanged = false;
    if (it == m_userStyleSheets->end())
        return;
    
    UserStyleSheetVector* stylesheets = it->second;
    for (int i = stylesheets->size() - 1; i >= 0; --i) {
        if (stylesheets->at(i)->url() == url) {
            stylesheets->remove(i);
            sheetsChanged = true;
        }
    }
        
    if (!sheetsChanged)
        return;

    if (!stylesheets->isEmpty()) {
        delete it->second;
        m_userStyleSheets->remove(it);
    }
    
    // Clear our cached sheets and have them just reparse.
    HashSet<Page*>::const_iterator end = m_pages.end();
    for (HashSet<Page*>::const_iterator it = m_pages.begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frame->document()->clearPageGroupUserSheets();
    }
}

void PageGroup::removeUserScriptsFromWorld(DOMWrapperWorld* world)
{
    ASSERT_ARG(world, world);

    if (!m_userScripts)
        return;

    UserScriptMap::iterator it = m_userScripts->find(world);
    if (it == m_userScripts->end())
        return;
       
    delete it->second;
    m_userScripts->remove(it);
}

void PageGroup::removeUserStyleSheetsFromWorld(DOMWrapperWorld* world)
{
    ASSERT_ARG(world, world);

    if (!m_userStyleSheets)
        return;
    
    UserStyleSheetMap::iterator it = m_userStyleSheets->find(world);
    if (it == m_userStyleSheets->end())
        return;
    
    delete it->second;
    m_userStyleSheets->remove(it);

    // Clear our cached sheets and have them just reparse.
    HashSet<Page*>::const_iterator end = m_pages.end();
    for (HashSet<Page*>::const_iterator it = m_pages.begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frame->document()->clearPageGroupUserSheets();
    }
}

void PageGroup::removeAllUserContent()
{
    if (m_userScripts) {
        deleteAllValues(*m_userScripts);
        m_userScripts.clear();
    }
    
    
    if (m_userStyleSheets) {
        deleteAllValues(*m_userStyleSheets);
        m_userStyleSheets.clear();
    }
}

} // namespace WebCore
