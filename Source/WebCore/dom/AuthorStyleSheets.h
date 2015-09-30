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

#ifndef AuthorStyleSheets_h
#define AuthorStyleSheets_h

#include "Timer.h"
#include <memory>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSStyleSheet;
class Document;
class Node;
class StyleSheet;
class StyleSheetContents;
class StyleSheetList;
class ShadowRoot;
class TreeScope;

class AuthorStyleSheets {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AuthorStyleSheets(Document&);
    explicit AuthorStyleSheets(ShadowRoot&);

    const Vector<RefPtr<CSSStyleSheet>>& activeStyleSheets() const { return m_activeStyleSheets; }

    const Vector<RefPtr<StyleSheet>>& styleSheetsForStyleSheetList() const { return m_styleSheetsForStyleSheetList; }
    const Vector<RefPtr<CSSStyleSheet>> activeStyleSheetsForInspector() const;

    void addStyleSheetCandidateNode(Node&, bool createdByParser);
    void removeStyleSheetCandidateNode(Node&);

    enum UpdateFlag { NoUpdate = 0, OptimizedUpdate, FullUpdate };

    UpdateFlag pendingUpdateType() const { return m_pendingUpdateType; }
    void setPendingUpdateType(UpdateFlag updateType)
    {
        if (updateType > m_pendingUpdateType)
            m_pendingUpdateType = updateType;
    }

    void flushPendingUpdates()
    {
        if (m_pendingUpdateType != NoUpdate)
            updateActiveStyleSheets(m_pendingUpdateType);
    }

    bool updateActiveStyleSheets(UpdateFlag);

    String preferredStylesheetSetName() const { return m_preferredStylesheetSetName; }
    String selectedStylesheetSetName() const { return m_selectedStylesheetSetName; }
    void setPreferredStylesheetSetName(const String& name) { m_preferredStylesheetSetName = name; }
    void setSelectedStylesheetSetName(const String& name) { m_selectedStylesheetSetName = name; }

    void addPendingSheet() { m_pendingStyleSheetCount++; }
    enum RemovePendingSheetNotificationType {
        RemovePendingSheetNotifyImmediately,
        RemovePendingSheetNotifyLater
    };
    void removePendingSheet(RemovePendingSheetNotificationType = RemovePendingSheetNotifyImmediately);

    bool hasPendingSheets() const { return m_pendingStyleSheetCount > 0; }

    bool usesRemUnits() const { return m_usesRemUnits; }
    void setUsesRemUnit(bool b) { m_usesRemUnits = b; }
    bool usesStyleBasedEditability() { return m_usesStyleBasedEditability; }

    bool activeStyleSheetsContains(const CSSStyleSheet*) const;

private:
    void collectActiveStyleSheets(Vector<RefPtr<StyleSheet>>&);
    enum StyleResolverUpdateType {
        Reconstruct,
        Reset,
        Additive
    };
    StyleResolverUpdateType analyzeStyleSheetChange(UpdateFlag, const Vector<RefPtr<CSSStyleSheet>>& newStylesheets, bool& requiresFullStyleRecalc);
    void updateStyleResolver(Vector<RefPtr<CSSStyleSheet>>&, StyleResolverUpdateType);

    Document& m_document;
    ShadowRoot* m_shadowRoot { nullptr };

    Vector<RefPtr<StyleSheet>> m_styleSheetsForStyleSheetList;
    Vector<RefPtr<CSSStyleSheet>> m_activeStyleSheets;

    // This is a mirror of m_activeAuthorStyleSheets that gets populated on demand for activeStyleSheetsContains().
    mutable std::unique_ptr<HashSet<const CSSStyleSheet*>> m_weakCopyOfActiveStyleSheetListForFastLookup;

    // Track the number of currently loading top-level stylesheets needed for rendering.
    // Sheets loaded using the @import directive are not included in this count.
    // We use this count of pending sheets to detect when we can begin attaching
    // elements and when it is safe to execute scripts.
    int m_pendingStyleSheetCount { 0 };

    bool m_hadActiveLoadingStylesheet { false };
    UpdateFlag m_pendingUpdateType { NoUpdate };

    ListHashSet<Node*> m_styleSheetCandidateNodes;

    String m_preferredStylesheetSetName;
    String m_selectedStylesheetSetName;

    bool m_usesRemUnits { false };
    bool m_usesStyleBasedEditability { false };
};

}

#endif

