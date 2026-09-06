/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
 */

#include "config.h"
#include "StyleDocumentScope.h"

#include "CSSCounterStyleRegistry.h"
#include "CSSFontSelector.h"
#include "CSSKeyframesRule.h"
#include "CSSStyleSheet.h"
#include "ContainerNodeInlines.h"
#include "DocumentInlines.h"
#include "DocumentView.h"
#include "Element.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "InspectorInstrumentation.h"
#include "LocalFrameView.h"
#include "MatchResultCache.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "RuleSet.h"
#include "ScrollableArea.h"
#include "ShadowRoot.h"
#include "StyleableInlines.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleEnvironmentVariables.h"
#include "StyleInvalidator.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DocumentScope);

DocumentScope::DocumentScope(Document& document)
    : Scope(document)
{
}

DocumentScope::~DocumentScope() = default;

void DocumentScope::createDocumentResolver()
{
    ASSERT(!m_resolver);
    ASSERT(!m_shadowRoot);

    SetForScope isUpdatingStyleResolver { m_isUpdatingStyleResolver, true };

    m_resolver = Resolver::create(m_document, Resolver::ScopeType::Document);

    if (!m_dynamicViewTransitionsStyle)
        m_dynamicViewTransitionsStyle = RuleSet::create();

    m_resolver->ruleSets().setDynamicViewTransitionsStyle(m_dynamicViewTransitionsStyle.get());

    for (auto& keyframes : m_viewTransitionKeyframes)
        m_resolver->addKeyframeStyle(keyframes.copyRef());

    protect(m_document->fontSelector())->buildStarted();

    m_resolver->ruleSets().initializeUserStyle();
    m_resolver->addCurrentSVGFontFaceRules();
    m_resolver->appendAuthorStyleSheets(m_activeStyleSheets);

    protect(m_document->fontSelector())->buildCompleted();
}

void DocumentScope::clearViewTransitionStyles()
{
    clearResolver();
    m_dynamicViewTransitionsStyle = nullptr;
    m_viewTransitionKeyframes.clear();
}

void DocumentScope::addViewTransitionKeyframes(Ref<StyleRuleKeyframes>&& rule)
{
    // Add to the existing resolver, if any, and retain the keyframes so they can be
    // re-established when the resolver is recreated (e.g. by a stylesheet mutation)
    // during the transition.
    if (RefPtr resolver = resolverIfExists())
        resolver->addKeyframeStyle(rule.copyRef());
    m_viewTransitionKeyframes.append(WTF::move(rule));
}

void DocumentScope::releaseMemory()
{
    for (auto& descendantShadowRoot : m_document->inDocumentShadowRoots())
        const_cast<ShadowRoot&>(descendantShadowRoot).styleScope().releaseMemory();

    Scope::releaseMemory();

    m_sharedShadowTreeResolvers.clear();
    m_matchResultCache = { };
}

void DocumentScope::setPreferredStylesheetSetName(const WTF::String& name)
{
    if (m_preferredStylesheetSetName == name)
        return;
    m_preferredStylesheetSetName = name;
    didChangeActiveStyleSheetCandidates();
}

auto DocumentScope::mediaQueryViewportStateForDocument(const Document& document) -> MediaQueryViewportState
{
    // These things affect evaluation of viewport dependent media queries.
    return { document.view()->layoutSize(), document.frame()->pageZoomFactor(), document.printing() };
}

void DocumentScope::evaluateMediaQueriesForViewportChange()
{
    auto viewportState = mediaQueryViewportStateForDocument(m_document);

    if (m_viewportStateOnPreviousMediaQueryEvaluation && *m_viewportStateOnPreviousMediaQueryEvaluation == viewportState)
        return;
    // This doesn't need to be invalidated as any changes to the rules will compute their media queries to correct values.
    m_viewportStateOnPreviousMediaQueryEvaluation = viewportState;

    evaluateMediaQueries([] (Resolver& resolver) {
        return resolver.evaluateDynamicMediaQueries();
    });
}

void DocumentScope::evaluateMediaQueriesForAccessibilitySettingsChange()
{
    evaluateMediaQueries([] (Resolver& resolver) {
        return resolver.evaluateDynamicMediaQueries();
    });
}

void DocumentScope::evaluateMediaQueriesForAppearanceChange()
{
    evaluateMediaQueries([] (Resolver& resolver) {
        return resolver.evaluateDynamicMediaQueries();
    });
}

auto DocumentScope::collectResolverScopes() -> ResolverScopes
{
    ResolverScopes resolverScopes;

    if (RefPtr resolver = resolverIfExists())
        resolverScopes.add(*resolver, Vector<WeakPtr<Scope>> { this });

    for (Ref shadowRoot : m_document->inDocumentShadowRoots()) {
        auto& scope = const_cast<ShadowRoot&>(shadowRoot.get()).styleScope();

        if (RefPtr resolver = scope.resolverIfExists())
            resolverScopes.add(*resolver, Vector<WeakPtr<Scope>> { }).iterator->value.append(&scope);
    }
    return resolverScopes;
}

template <typename TestFunction>
void DocumentScope::evaluateMediaQueries(TestFunction&& testFunction)
{
    bool hadChanges = false;

    auto resolverScopes = collectResolverScopes();
    for (auto& [resolver, scopes] : resolverScopes) {
        auto evaluationChanges = testFunction(resolver.get());
        if (!evaluationChanges)
            continue;
        hadChanges = true;

        for (auto& scope : scopes) {
            switch (evaluationChanges->type) {
            case DynamicMediaQueryEvaluationChanges::Type::InvalidateStyle: {
                Invalidator invalidator(evaluationChanges->invalidationRuleSets);
                invalidator.invalidateStyle(*scope);
                break;
            }
            case DynamicMediaQueryEvaluationChanges::Type::ResetStyle:
                scope->scheduleUpdate(UpdateType::ContentsOrInterpretation);
                break;
            }
        }
    }

    if (hadChanges)
        InspectorInstrumentation::mediaQueryResultChanged(m_document);
}

void DocumentScope::didChangeStyleSheetEnvironment()
{
    RELEASE_ASSERT(!m_isUpdatingStyleResolver);
    RELEASE_ASSERT(!m_document->isResolvingTreeStyle());

    m_sharedShadowTreeResolvers.clear();

    for (auto& descendantShadowRoot : m_document->inDocumentShadowRoots())
        const_cast<ShadowRoot&>(descendantShadowRoot).styleScope().scheduleUpdate(UpdateType::ContentsOrInterpretation);

    m_document->invalidateCachedCSSParserContext();
    m_document->invalidateCachedInitialStyle();

    scheduleUpdate(UpdateType::ContentsOrInterpretation);
}

void DocumentScope::didChangeExtensionStyleSheets()
{
    // Extension stylesheets may mutate in the middle of a style update when resource loading triggers
    // content extension processing. In this case we schedule an asyncronous full stylesheet update.
    // FIXME: We should defer all resource loading after style resolution completes.
    for (auto& descendantShadowRoot : m_document->inDocumentShadowRoots())
        const_cast<ShadowRoot&>(descendantShadowRoot).styleScope().scheduleUpdate(UpdateType::FullForExtensionStyleSheets);

    scheduleUpdate(UpdateType::FullForExtensionStyleSheets);
}

bool DocumentScope::invalidateForLayoutDependencies(LayoutDependencyUpdateContext& context)
{
    auto didInvalidate = false;
    didInvalidate |= invalidateForContainerDependencies(context);
    didInvalidate |= invalidateForAnchorDependencies(context);
    didInvalidate |= invalidateForPositionTryFallbacks(context);
    return didInvalidate;
}

bool DocumentScope::invalidateForContainerDependencies(LayoutDependencyUpdateContext& context)
{
    if (!m_document->renderView())
        return false;

    auto previousQueryContainerDimensions = WTF::move(m_queryContainerDimensionsOnLastUpdate);
    m_queryContainerDimensionsOnLastUpdate.clear();

    Vector<CheckedPtr<Element>> containersToInvalidate;

    for (auto& containerRenderer : m_document->renderView()->containerQueryBoxes()) {
        CheckedPtr containerElement = containerRenderer.element();

        // Invalidation uses real elements, replace ::before/::after with its host.
        if (auto* pseudoElement = dynamicDowncast<PseudoElement>(containerElement.get()))
            containerElement = pseudoElement->hostElement();

        if (!containerElement)
            continue;

        auto size = containerRenderer.logicalSize();

        auto sizeChanged = [&](LayoutSize oldSize) {
            auto& type = containerRenderer.style().containerType();
            if (type.hasInlineSize())
                return size.width() != oldSize.width();
            if (type.hasSize())
                return size != oldSize;
            RELEASE_ASSERT_NOT_REACHED();
        };

        auto it = previousQueryContainerDimensions.find(*containerElement);
        bool changed = it == previousQueryContainerDimensions.end() || sizeChanged(it->value);
        // Protect against unstable layout by invalidating only once per container.
        if (changed && context.invalidatedContainers.add(*containerElement).isNewEntry)
            containersToInvalidate.append(containerElement);
        m_queryContainerDimensionsOnLastUpdate.add(*containerElement, size);
    }

    for (auto& toInvalidate : containersToInvalidate)
        toInvalidate->invalidateForQueryContainerChange();

    return !containersToInvalidate.isEmpty();
}

// Edges the container can currently be scrolled further toward.
static RectEdges<bool> scrollableEdges(const RenderBox& containerRenderer)
{
    // This mirrors ScrollableArea::edgePinnedState(), except for where it takes the axes from. That
    // function decides whether an axis can scroll at all from the Scrollbar objects the area owns,
    // and a frame view has none when scrolling is delegated to a native scroll view, so it reports
    // every edge as pinned there. Take the axes from the renderer and the frame view instead, like
    // RenderLayerScrollableArea::hasScrollableHorizontalOverflow() and LocalFrameView::isScrollable().
    CheckedPtr<const ScrollableArea> scrollableArea;
    bool canScrollHorizontally = false;
    bool canScrollVertically = false;

    if (containerRenderer.isDocumentElementRenderer()) {
        // In standards mode the document element scrolls the viewport, which is driven by the frame
        // view rather than the element's own layer.
        auto& frameView = containerRenderer.view().frameView();
        scrollableArea = &frameView;
        canScrollHorizontally = frameView.horizontalScrollbarMode() != ScrollbarMode::AlwaysOff;
        canScrollVertically = frameView.verticalScrollbarMode() != ScrollbarMode::AlwaysOff;
    } else if (CheckedPtr layer = containerRenderer.layer()) {
        scrollableArea = layer->scrollableArea();
        // Only auto and scroll overflow are reachable through user initiated scrolling, which is what
        // the feature queries; scrollable never matches a hidden container.
        canScrollHorizontally = containerRenderer.scrollsOverflowX();
        canScrollVertically = containerRenderer.scrollsOverflowY();
    }

    // A container with no scrollable area is not a scroll container, so it is scrollable nowhere.
    if (!scrollableArea)
        return { false, false, false, false };

    auto scrollPosition = scrollableArea->scrollPosition();
    auto minimumScrollPosition = scrollableArea->minimumScrollPosition();
    auto maximumScrollPosition = scrollableArea->maximumScrollPosition();

    // Top, right, bottom, left.
    return {
        canScrollVertically && scrollPosition.y() > minimumScrollPosition.y(),
        canScrollHorizontally && scrollPosition.x() < maximumScrollPosition.x(),
        canScrollVertically && scrollPosition.y() < maximumScrollPosition.y(),
        canScrollHorizontally && scrollPosition.x() > minimumScrollPosition.x()
    };
}

// https://drafts.csswg.org/css-conditional-5/#updating-scroll-state
// Reading live scroll state during style resolution would allow layout cycles, since a scroll-state
// query can change style, which changes layout, which changes the scroll state. Instead the state is
// snapshotted here, after layout, and that snapshot is what evaluation sees until the next snapshot.
void DocumentScope::updateScrollStateSnapshots()
{
    CheckedPtr renderView = m_document->renderView();
    if (!renderView)
        return;

    auto previousScrollStates = WTF::move(m_queryContainerScrollStatesOnLastUpdate);
    m_queryContainerScrollStatesOnLastUpdate.clear();

    Vector<CheckedPtr<Element>> containersToInvalidate;

    for (auto& containerRenderer : renderView->scrollStateQueryBoxes()) {
        CheckedPtr containerElement = containerRenderer.element();
        if (!containerElement)
            continue;

        auto scrollState = ScrollState { scrollableEdges(containerRenderer) };

        // Keyed on the container itself, ::before/::after included, since evaluation looks the state
        // up by the container it is querying. Folding a pseudo-element into its host here would make
        // the two share one entry and read each other's state.
        auto it = previousScrollStates.find(*containerElement);
        // A container seen for the first time is always invalidated, even when its state matches the
        // default that evaluation falls back to: the first style resolution ran before this container
        // had a renderer, so its queries evaluated to false and need resolving again.
        bool changed = it == previousScrollStates.end() || it->value != scrollState;

        m_queryContainerScrollStatesOnLastUpdate.add(*containerElement, scrollState);

        if (!changed)
            continue;

        // Invalidation uses real elements, replace ::before/::after with its host.
        if (auto* pseudoElement = dynamicDowncast<PseudoElement>(containerElement.get()))
            containerElement = pseudoElement->hostElement();

        if (containerElement)
            containersToInvalidate.append(containerElement);
    }

    for (auto& toInvalidate : containersToInvalidate)
        toInvalidate->invalidateForQueryContainerChange();
}

auto DocumentScope::scrollStateSnapshotFor(const Element& element) const -> ScrollState
{
    return m_queryContainerScrollStatesOnLastUpdate.get(element);
}

bool DocumentScope::invalidateForAnchorDependencies(LayoutDependencyUpdateContext& context)
{
    if (!m_document->renderView())
        return false;

    auto previousAnchorPositions = WTF::move(m_anchorPositionsOnLastUpdate);
    m_anchorPositionsOnLastUpdate.clear();

    Vector<CheckedRef<Element>> anchoredElementsToInvalidate;

    if (m_document->renderView()->anchors().isEmptyIgnoringNullReferences())
        return false;

    auto anchorMap = AnchorPositionEvaluator::makeAnchorPositionedForAnchorMap(m_anchorPositionedToAnchorMap);

    auto makeAnchorPosition = [&](const RenderBoxModelObject& anchorRenderer) {
        AnchorPosition result;
        result.absoluteRect = anchorRenderer.absoluteBoundingBoxRect();
        // Include containing block sizes as anchor function insets may be computed against any side and if they change
        // we need to invalidate.
        for (auto* containingBlock = anchorRenderer.containingBlock(); containingBlock; containingBlock = containingBlock->containingBlock()) {
            if (containingBlock->canContainAbsolutelyPositionedObjects())
                result.containingBlockSizes.append(containingBlock->contentBoxSize());
        }
        return result;
    };

    for (auto& anchorRenderer : m_document->renderView()->anchors()) {
        auto anchorPosition = makeAnchorPosition(anchorRenderer);
        m_anchorPositionsOnLastUpdate.add(anchorRenderer, anchorPosition);

        auto it = previousAnchorPositions.find(anchorRenderer);
        bool changed = it == previousAnchorPositions.end() || it->value != anchorPosition;
        if (!changed)
            continue;

        auto anchoredElements = anchorMap.getOptional(anchorRenderer);
        if (!anchoredElements)
            continue;

        for (auto& anchoredElement : *anchoredElements) {
            if (!context.invalidatedAnchorPositioned.add(anchoredElement.get()).isNewEntry)
                continue;
            anchoredElementsToInvalidate.append(anchoredElement);
        }
    }

    for (auto& toInvalidate : anchoredElementsToInvalidate) {
        CheckedPtr renderer = toInvalidate->renderer();
        if (renderer && AnchorPositionEvaluator::isLayoutTimeAnchorPositioned(renderer->style()))
            renderer->setNeedsLayout();
        toInvalidate->invalidateForAnchorRectChange();
    }

    return !anchoredElementsToInvalidate.isEmpty();
}

bool DocumentScope::invalidateForPositionTryFallbacks(LayoutDependencyUpdateContext& context)
{
    if (!m_document->renderView())
        return false;

    bool invalidated = false;

    for (auto& box : m_document->renderView()->positionTryBoxes()) {
        if (!AnchorPositionEvaluator::overflowsInsetModifiedContainingBlock(box))
            continue;

        CheckedPtr element = box.element();
        if (auto* pseudoElement = dynamicDowncast<PseudoElement>(element.get()))
            element = pseudoElement->hostElement();

        if (element) {
            if (!context.invalidatedAnchorPositioned.add(*element).isNewEntry)
                continue;
            element->invalidateForAnchorRectChange();
            invalidated = true;
        }
    }

    return invalidated;
}

MatchResultCache& DocumentScope::matchResultCache()
{
    if (!m_matchResultCache)
        m_matchResultCache = makeUnique<MatchResultCache>();
    return *m_matchResultCache;
}

EnvironmentVariables& DocumentScope::environmentVariables() const
{
    if (!m_environmentVariables) {
        auto& thisScope = const_cast<DocumentScope&>(*this);
        thisScope.m_environmentVariables = makeUnique<EnvironmentVariables>(m_document.get());
    }
    return *m_environmentVariables;
}

void DocumentScope::updateAnchorPositioningStateAfterStyleResolution()
{
    if (CheckedPtr renderView = m_document->renderView())
        AnchorPositionEvaluator::updateScrollAdjustments(*renderView); // Is this necessary? Or will the combination of layout and scroll invalidation handle it sufficiently?

    m_anchorPositionedToAnchorMap.removeIf([](auto& elementAndState) {
        return elementAndState.value.anchors.isEmpty();
    });
}

std::optional<size_t> DocumentScope::lastSuccessfulPositionOptionIndexFor(const Styleable& styleable)
{
    return m_lastSuccessfulPositionOptionIndexes.getOptional(styleable);
}

void DocumentScope::setLastSuccessfulPositionOptionIndexMap(HashMap<WeakStyleable, size_t>&& map)
{
    m_lastSuccessfulPositionOptionIndexes = WTF::move(map);
}

void DocumentScope::forgetLastSuccessfulPositionOptionIndex(const Styleable& styleable)
{
    m_lastSuccessfulPositionOptionIndexes.remove(styleable);
}

}
}
