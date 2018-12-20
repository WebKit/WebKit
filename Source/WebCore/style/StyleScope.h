/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2010, 2012-2013, 2015-2017 Apple Inc. All rights reserved.
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

#include "Timer.h"
#include <memory>
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSStyleSheet;
class Document;
class Element;
class Node;
class ProcessingInstruction;
class StyleResolver;
class StyleSheet;
class StyleSheetContents;
class StyleSheetList;
class ShadowRoot;
class TreeScope;

namespace Style {

// This is used to identify style scopes that can affect an element.
// Scopes are in tree-of-trees order. Styles from earlier scopes win over later ones (modulo !important).
enum class ScopeOrdinal : int {
    ContainingHost = -1, // Author-exposed UA pseudo classes from the host tree scope.
    Element = 0, // Normal rules in the same tree where the element is.
    FirstSlot = 1, // ::slotted rules in the parent's shadow tree. Values greater than FirstSlot indicate subsequent slots in the chain.
    Shadow = std::numeric_limits<int>::max(), // :host rules in element's own shadow tree.
};

class Scope {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit Scope(Document&);
    explicit Scope(ShadowRoot&);

    ~Scope();

    const Vector<RefPtr<CSSStyleSheet>>& activeStyleSheets() const { return m_activeStyleSheets; }

    const Vector<RefPtr<StyleSheet>>& styleSheetsForStyleSheetList();
    const Vector<RefPtr<CSSStyleSheet>> activeStyleSheetsForInspector();

    void addStyleSheetCandidateNode(Node&, bool createdByParser);
    void removeStyleSheetCandidateNode(Node&);

    String preferredStylesheetSetName() const { return m_preferredStylesheetSetName; }
    void setPreferredStylesheetSetName(const String&);

    void addPendingSheet(const Element&);
    void removePendingSheet(const Element&);
    void addPendingSheet(const ProcessingInstruction&);
    void removePendingSheet(const ProcessingInstruction&);
    bool hasPendingSheets() const;
    bool hasPendingSheetsBeforeBody() const;
    bool hasPendingSheetsInBody() const;
    bool hasPendingSheet(const Element&) const;
    bool hasPendingSheetInBody(const Element&) const;
    bool hasPendingSheet(const ProcessingInstruction&) const;

    bool usesStyleBasedEditability() { return m_usesStyleBasedEditability; }

    bool activeStyleSheetsContains(const CSSStyleSheet*) const;

    void evaluateMediaQueriesForViewportChange();
    void evaluateMediaQueriesForAccessibilitySettingsChange();
    void evaluateMediaQueriesForAppearanceChange();

    // This is called when some stylesheet becomes newly enabled or disabled.
    void didChangeActiveStyleSheetCandidates();
    // This is called when contents of a stylesheet is mutated.
    void didChangeStyleSheetContents();
    // This is called when the environment where we intrepret the stylesheets changes (for example switching to printing).
    // The change is assumed to potentially affect all author and user stylesheets including shadow roots.
    WEBCORE_EXPORT void didChangeStyleSheetEnvironment();

    bool hasPendingUpdate() const { return m_pendingUpdate || m_hasDescendantWithPendingUpdate; }
    void flushPendingUpdate();

#if ENABLE(XSLT)
    Vector<Ref<ProcessingInstruction>> collectXSLTransforms();
#endif

    StyleResolver& resolver();
    StyleResolver* resolverIfExists();
    void clearResolver();
    void releaseMemory();

    const Document& document() const { return m_document; }

    static Scope& forNode(Node&);
    static Scope* forOrdinal(Element&, ScopeOrdinal);

private:
    bool shouldUseSharedUserAgentShadowTreeStyleResolver() const;

    void didRemovePendingStylesheet();

    enum class UpdateType { ActiveSet, ContentsOrInterpretation };
    void updateActiveStyleSheets(UpdateType);
    void scheduleUpdate(UpdateType);

    template <typename TestFunction> void evaluateMediaQueries(TestFunction&&);

    WEBCORE_EXPORT void flushPendingSelfUpdate();
    WEBCORE_EXPORT void flushPendingDescendantUpdates();

    void collectActiveStyleSheets(Vector<RefPtr<StyleSheet>>&);

    enum StyleResolverUpdateType {
        Reconstruct,
        Reset,
        Additive
    };
    StyleResolverUpdateType analyzeStyleSheetChange(const Vector<RefPtr<CSSStyleSheet>>& newStylesheets, bool& requiresFullStyleRecalc);
    void updateStyleResolver(Vector<RefPtr<CSSStyleSheet>>&, StyleResolverUpdateType);

    void pendingUpdateTimerFired();
    void clearPendingUpdate();

    Document& m_document;
    ShadowRoot* m_shadowRoot { nullptr };

    std::unique_ptr<StyleResolver> m_resolver;

    Vector<RefPtr<StyleSheet>> m_styleSheetsForStyleSheetList;
    Vector<RefPtr<CSSStyleSheet>> m_activeStyleSheets;

    Timer m_pendingUpdateTimer;

    mutable std::unique_ptr<HashSet<const CSSStyleSheet*>> m_weakCopyOfActiveStyleSheetListForFastLookup;

    // Track the currently loading top-level stylesheets needed for rendering.
    // Sheets loaded using the @import directive are not included in this count.
    // We use this count of pending sheets to detect when we can begin attaching
    // elements and when it is safe to execute scripts.
    HashSet<const ProcessingInstruction*> m_processingInstructionsWithPendingSheets;
    HashSet<const Element*> m_elementsInHeadWithPendingSheets;
    HashSet<const Element*> m_elementsInBodyWithPendingSheets;

    Optional<UpdateType> m_pendingUpdate;
    bool m_hasDescendantWithPendingUpdate { false };

    ListHashSet<Node*> m_styleSheetCandidateNodes;

    String m_preferredStylesheetSetName;

    bool m_usesStyleBasedEditability { false };
    bool m_isUpdatingStyleResolver { false };
};

inline bool Scope::hasPendingSheets() const
{
    return hasPendingSheetsBeforeBody() || !m_elementsInBodyWithPendingSheets.isEmpty();
}

inline bool Scope::hasPendingSheetsBeforeBody() const
{
    return !m_elementsInHeadWithPendingSheets.isEmpty() || !m_processingInstructionsWithPendingSheets.isEmpty();
}

inline bool Scope::hasPendingSheetsInBody() const
{
    return !m_elementsInBodyWithPendingSheets.isEmpty();
}

inline void Scope::flushPendingUpdate()
{
    if (m_hasDescendantWithPendingUpdate)
        flushPendingDescendantUpdates();
    if (m_pendingUpdate)
        flushPendingSelfUpdate();
}

inline ScopeOrdinal& operator++(ScopeOrdinal& ordinal)
{
    ASSERT(ordinal < ScopeOrdinal::Shadow);
    return ordinal = static_cast<ScopeOrdinal>(static_cast<int>(ordinal) + 1);
}

}
}
