/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef PageGroup_h
#define PageGroup_h

#include "LinkHash.h"
#include "SecurityOriginHash.h"
#include "Supplementable.h"
#include "UserScript.h"
#include "UserStyleSheet.h"
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

    class URL;
    class GroupSettings;
    class IDBFactoryBackendInterface;
    class Page;
    class SecurityOrigin;
    class StorageNamespace;
    class VisitedLinkProvider;
    class UserContentController;

    class PageGroup : public Supplementable<PageGroup> {
        WTF_MAKE_NONCOPYABLE(PageGroup); WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit PageGroup(const String& name);
        explicit PageGroup(Page&);
        ~PageGroup();

        static PageGroup* pageGroup(const String& groupName);

        static void closeLocalStorage();

        static void clearLocalStorageForAllOrigins();
        static void clearLocalStorageForOrigin(SecurityOrigin*);
        static void closeIdleLocalStorageDatabases();
        // DumpRenderTree helper that triggers a StorageArea sync.
        static void syncLocalStorage();

        const HashSet<Page*>& pages() const { return m_pages; }

        void addPage(Page&);
        void removePage(Page&);

        VisitedLinkProvider& visitedLinkProvider() { return *m_visitedLinkProvider; }

        bool isLinkVisited(LinkHash);

        void addVisitedLink(const URL&);
        void addVisitedLink(const UChar*, size_t);
        void addVisitedLinkHash(LinkHash);
        void removeVisitedLink(const URL&);
        void removeVisitedLinks();

        static void setShouldTrackVisitedLinks(bool);
        static void removeAllVisitedLinks();

        const String& name() { return m_name; }
        unsigned identifier() { return m_identifier; }

        StorageNamespace* localStorage();
        bool hasLocalStorage() { return m_localStorage; }

        StorageNamespace* transientLocalStorage(SecurityOrigin* topOrigin);

        void addUserScriptToWorld(DOMWrapperWorld&, const String& source, const URL&, const Vector<String>& whitelist, const Vector<String>& blacklist, UserScriptInjectionTime, UserContentInjectedFrames);
        void addUserStyleSheetToWorld(DOMWrapperWorld&, const String& source, const URL&, const Vector<String>& whitelist, const Vector<String>& blacklist, UserContentInjectedFrames, UserStyleLevel = UserStyleUserLevel, UserStyleInjectionTime = InjectInExistingDocuments);
        void removeUserStyleSheetFromWorld(DOMWrapperWorld&, const URL&);
        void removeUserScriptFromWorld(DOMWrapperWorld&, const URL&);
        void removeUserScriptsFromWorld(DOMWrapperWorld&);
        void removeUserStyleSheetsFromWorld(DOMWrapperWorld&);
        void removeAllUserContent();

        GroupSettings& groupSettings() const { return *m_groupSettings; }

    private:
        void addVisitedLink(LinkHash);

        String m_name;
        HashSet<Page*> m_pages;

        RefPtr<VisitedLinkProvider> m_visitedLinkProvider;

        HashSet<LinkHash, LinkHashHash> m_visitedLinkHashes;
        bool m_visitedLinksPopulated;

        unsigned m_identifier;
        RefPtr<StorageNamespace> m_localStorage;
        HashMap<RefPtr<SecurityOrigin>, RefPtr<StorageNamespace>> m_transientLocalStorageMap;

        RefPtr<UserContentController> m_userContentController;

        const std::unique_ptr<GroupSettings> m_groupSettings;
    };

} // namespace WebCore
    
#endif // PageGroup_h
