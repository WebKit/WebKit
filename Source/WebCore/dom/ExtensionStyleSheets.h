/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2010, 2012-2013, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <WebCore/UserStyleSheet.h>
#include <memory>
#include <wtf/CheckedRef.h>
#include <wtf/RefPtr.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include <WebCore/ContentExtensionStyleSheet.h>
#endif

namespace WebCore {

class CSSStyleSheet;
class Document;
class Node;
class StyleSheet;
class StyleSheetContents;
class StyleSheetList;
class WeakPtrImplWithEventTargetData;

class ExtensionStyleSheets final : public CanMakeCheckedPtr<ExtensionStyleSheets> {
    WTF_MAKE_TZONE_ALLOCATED(ExtensionStyleSheets);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ExtensionStyleSheets);
public:
    explicit ExtensionStyleSheets(Document&);
    ~ExtensionStyleSheets();

    CSSStyleSheet* pageUserSheet();
    const Vector<Ref<CSSStyleSheet>>& documentUserStyleSheets() const LIFETIME_BOUND { return m_userStyleSheets; }
    const Vector<Ref<CSSStyleSheet>>& injectedUserStyleSheets() const LIFETIME_BOUND;
    const Vector<Ref<CSSStyleSheet>>& injectedAuthorStyleSheets() const LIFETIME_BOUND;
    const Vector<Ref<CSSStyleSheet>>& authorStyleSheetsForTesting() const LIFETIME_BOUND { return m_authorStyleSheetsForTesting; }

    bool NODELETE hasCachedInjectedStyleSheets() const;

    void clearPageUserSheet();
    void updatePageUserSheet();
    void invalidateInjectedStyleSheetCache();
    void updateInjectedStyleSheetCache() const;

    WEBCORE_EXPORT void addUserStyleSheet(Ref<StyleSheetContents>&&);

    WEBCORE_EXPORT void addAuthorStyleSheetForTesting(Ref<StyleSheetContents>&&);

#if ENABLE(CONTENT_EXTENSIONS)
    void addDisplayNoneSelector(const String& identifier, const String& selector, uint32_t selectorID);
    void maybeAddContentExtensionSheet(const String& identifier, StyleSheetContents&);
#endif

    void injectPageSpecificUserStyleSheet(const UserStyleSheet&);
    void removePageSpecificUserStyleSheet(const UserStyleSheet&);

    String NODELETE contentForInjectedStyleSheet(CSSStyleSheet&) const;

    void detachFromDocument();

private:
    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;

    RefPtr<CSSStyleSheet> m_pageUserSheet;

    mutable Vector<Ref<CSSStyleSheet>> m_injectedUserStyleSheets;
    mutable Vector<Ref<CSSStyleSheet>> m_injectedAuthorStyleSheets;
    mutable HashMap<Ref<CSSStyleSheet>, String> m_injectedStyleSheetToSource;
    mutable bool m_injectedStyleSheetCacheValid { false };

    Vector<Ref<CSSStyleSheet>> m_userStyleSheets;
    Vector<Ref<CSSStyleSheet>> m_authorStyleSheetsForTesting;
    Vector<UserStyleSheet> m_pageSpecificStyleSheets;

#if ENABLE(CONTENT_EXTENSIONS)
    MemoryCompactRobinHoodHashMap<String, Ref<CSSStyleSheet>> m_contentExtensionSheets;
    MemoryCompactRobinHoodHashMap<String, Ref<ContentExtensions::ContentExtensionStyleSheet>> m_contentExtensionSelectorSheets;
#endif
};

} // namespace WebCore
