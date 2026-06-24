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
#include "CSSStyleSheet.h"
#include "ContainerNodeInlines.h"
#include "DocumentInlines.h"
#include "DocumentView.h"
#include "Element.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "InspectorInstrumentation.h"
#include "MatchResultCache.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "RuleSet.h"
#include "ShadowRoot.h"
#include "StyleableInlines.h"
#include "StyleCustomPropertyRegistry.h"
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
        toInvalidate->invalidateForQueryContainerSizeChange();

    return !containersToInvalidate.isEmpty();
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
