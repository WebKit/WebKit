/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/StyleScope.h>

namespace WebCore {

namespace Style {

// Style scope for the document tree. Owns the document-level state that is shared
// by all the tree scopes (the document scope and its descendant shadow tree scopes).
class DocumentScope final : public Scope {
    WTF_MAKE_TZONE_ALLOCATED(DocumentScope);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DocumentScope);
    friend class Scope;
public:
    explicit DocumentScope(Document&);
    ~DocumentScope();

    void setPreferredStylesheetSetName(const WTF::String&);

    void releaseMemory();

    void evaluateMediaQueriesForViewportChange();
    void evaluateMediaQueriesForAccessibilitySettingsChange();
    void evaluateMediaQueriesForAppearanceChange();

    // This is called when the environment where we intrepret the stylesheets changes (for example switching to printing).
    // The change is assumed to potentially affect all author and user stylesheets including shadow roots.
    WEBCORE_EXPORT void didChangeStyleSheetEnvironment();

    // This is called when extension stylesheets change.
    void didChangeExtensionStyleSheets();

    void clearViewTransitionStyles();

    MatchResultCache& matchResultCache() LIFETIME_BOUND;

    struct LayoutDependencyUpdateContext {
        HashSet<CheckedRef<const Element>> invalidatedContainers;
        HashSet<CheckedRef<const Element>> invalidatedAnchorPositioned;
    };
    bool invalidateForLayoutDependencies(LayoutDependencyUpdateContext&);
    bool invalidateForAnchorDependencies(LayoutDependencyUpdateContext&);

    AnchorPositionedToAnchorMap& anchorPositionedToAnchorMap() LIFETIME_BOUND { return m_anchorPositionedToAnchorMap; }
    const AnchorPositionedToAnchorMap& anchorPositionedToAnchorMap() const LIFETIME_BOUND { return m_anchorPositionedToAnchorMap; }
    void updateAnchorPositioningStateAfterStyleResolution();

    std::optional<size_t> lastSuccessfulPositionOptionIndexFor(const Styleable&);
    void setLastSuccessfulPositionOptionIndexMap(HashMap<WeakStyleable, size_t>&&);
    void forgetLastSuccessfulPositionOptionIndex(const Styleable&);

private:
    void createDocumentResolver();

    using ResolverScopes = HashMap<Ref<Resolver>, Vector<WeakPtr<Scope>>>;
    ResolverScopes collectResolverScopes();
    template <typename TestFunction> void evaluateMediaQueries(TestFunction&&);

    using MediaQueryViewportState = std::tuple<IntSize, float, bool>;
    static MediaQueryViewportState mediaQueryViewportStateForDocument(const Document&);

    bool invalidateForContainerDependencies(LayoutDependencyUpdateContext&);
    bool invalidateForPositionTryFallbacks(LayoutDependencyUpdateContext&);

    WTF::String m_preferredStylesheetSetName;

    RefPtr<RuleSet> m_dynamicViewTransitionsStyle;

    std::optional<MediaQueryViewportState> m_viewportStateOnPreviousMediaQueryEvaluation;
    WeakHashMap<Element, LayoutSize, WeakPtrImplWithEventTargetData> m_queryContainerDimensionsOnLastUpdate;

    struct AnchorPosition {
        LayoutRect absoluteRect;
        Vector<LayoutSize, 2> containingBlockSizes;

        bool operator==(const AnchorPosition&) const = default;
    };
    SingleThreadWeakHashMap<const RenderBoxModelObject, AnchorPosition> m_anchorPositionsOnLastUpdate;
    // Stores the last successful position option for each anchor-positioned element.
    // This is recorded when ResizeObserver events are delivered, at Document::updateResizeObservations
    HashMap<WeakStyleable, size_t> m_lastSuccessfulPositionOptionIndexes;

    std::unique_ptr<MatchResultCache> m_matchResultCache;

    HashMap<ResolverSharingKey, Ref<Resolver>> m_sharedShadowTreeResolvers;

    AnchorPositionedToAnchorMap m_anchorPositionedToAnchorMap;
};

}
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Style::DocumentScope)
    static bool isType(const WebCore::Style::Scope& scope) { return !scope.shadowRoot(); }
SPECIALIZE_TYPE_TRAITS_END()
