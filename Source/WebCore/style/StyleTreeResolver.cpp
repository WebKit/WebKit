/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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
#include "StyleTreeResolver.h"

#include "AXObjectCache.h"
#include "AnchorPositionEvaluator.h"
#include "CSSFontSelector.h"
#include "CSSPositionTryRule.h"
#include "CSSSerializationContext.h"
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "Document.h"
#include "DocumentPage.h"
#include "DocumentQuirks.h"
#include "DocumentTimeline.h"
#include "DocumentView.h"
#include "EventTarget.h"
#include "HTMLBodyElement.h"
#include "HTMLInputElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLProgressElement.h"
#include "HTMLSlotElement.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "MatchResultCache.h"
#include "NodeInlines.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "PositionTryOrder.h"
#include "PositionedLayoutConstraints.h"
#include "RenderBoxInlines.h"
#include "RenderElement.h"
#include "RenderStyleSetters.h"
#include "RenderView.h"
#include "ResolvedStyle.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleAdjuster.h"
#include "StyleBuilder.h"
#include "StyleFontSizeFunctions.h"
#include "StyleOriginatedTimelinesController.h"
#include "StylePositionTryFallbackTactic.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "Text.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "ViewTransition.h"
#include "WebAnimationTypes.h"
#include "WebAnimationUtilities.h"
#include <ranges>

namespace WebCore {

namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(TreeResolverScope);

TreeResolver::TreeResolver(Document& document, std::unique_ptr<Update> update)
    : m_document(document)
    , m_update(WTFMove(update))
{
}

TreeResolver::~TreeResolver()
{
    m_document->styleScope().updateAnchorPositioningStateAfterStyleResolution();
}

TreeResolver::Scope::Scope(Document& document, Update& update)
    : resolver(document.styleScope().resolver())
{
    document.setIsResolvingTreeStyle(true);

    // Ensure all shadow tree resolvers exist so their construction doesn't depend on traversal.
    for (auto& shadowRoot : document.inDocumentShadowRoots())
        const_cast<ShadowRoot&>(shadowRoot).styleScope().resolver();

    selectorMatchingState.containerQueryEvaluationState.styleUpdate = &update;
}

TreeResolver::Scope::Scope(ShadowRoot& shadowRoot, Scope& enclosingScope)
    : resolver(shadowRoot.styleScope().resolver())
    , shadowRoot(&shadowRoot)
    , enclosingScope(&enclosingScope)
{
    selectorMatchingState.containerQueryEvaluationState = enclosingScope.selectorMatchingState.containerQueryEvaluationState;
}

TreeResolver::Scope::~Scope()
{
    if (!shadowRoot)
        resolver->document().setIsResolvingTreeStyle(false);
}

TreeResolver::Parent::Parent(Document& document)
    : element(nullptr)
    , style(*document.initialContainingBlockStyle())
{
}

TreeResolver::Parent::Parent(Element& element, const RenderStyle& style, OptionSet<Change> changes, DescendantsToResolve descendantsToResolve, IsInDisplayNoneTree isInDisplayNoneTree)
    : element(&element)
    , style(style)
    , changes(changes)
    , descendantsToResolve(descendantsToResolve)
    , isInDisplayNoneTree(isInDisplayNoneTree)
{
}

void TreeResolver::pushScope(ShadowRoot& shadowRoot)
{
    m_scopeStack.append(adoptRef(*new Scope(shadowRoot, scope())));
}

void TreeResolver::pushEnclosingScope()
{
    ASSERT(scope().enclosingScope);
    m_scopeStack.append(*scope().enclosingScope);
}

void TreeResolver::popScope()
{
    return m_scopeStack.removeLast();
}

// Takes an old style (from previous style resolution) and new style (from current
// style resolution), determine if the last successful position option should be
// invalidated. This follows the criterias in the spec:
// https://drafts.csswg.org/css-anchor-position-1/#last-successful-position-option
static bool shouldInvalidateLastSuccessfulPositionOptionIndex(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    if (oldStyle && newStyle) {
        if (oldStyle->positionTryFallbacks() != newStyle->positionTryFallbacks())
            return true;

        if (oldStyle->positionTryOrder() != newStyle->positionTryOrder())
            return true;

        // FIXME: add missing invalidation criterias in the spec.
    }

    return false;
}

ResolvedStyle TreeResolver::styleForStyleable(const Styleable& styleable, ResolutionType resolutionType, const ResolutionContext& resolutionContext, const RenderStyle* existingStyle)
{
    if (resolutionType == ResolutionType::AnimationOnly && styleable.lastStyleChangeEventStyle() && !styleable.hasPropertiesOverridenAfterAnimation())
        return { RenderStyle::clonePtr(*styleable.lastStyleChangeEventStyle()) };

    auto& element = styleable.element;

    if (auto optionStyle = tryChoosePositionOption(styleable, resolutionContext))
        return WTFMove(*optionStyle);

    if (resolutionType == ResolutionType::FastPathInherit) {
        // If the only reason we are computing the style is that some parent inherited properties changed, we can just copy them.
        auto style = RenderStyle::clonePtr(*existingStyle);
        style->fastPathInheritFrom(parent().style);
        m_document->styleScope().matchResultCache().updateForFastPathInherit(element, parent().style);
        return { WTFMove(style) };
    }

    auto unadjustedStyle = [&] {
        if (element.hasCustomStyleResolveCallbacks()) {
            RenderStyle* shadowHostStyle = scope().shadowRoot ? m_update->elementStyle(*scope().shadowRoot->host()) : nullptr;
            if (auto customStyle = element.resolveCustomStyle(resolutionContext, shadowHostStyle))
                return WTFMove(*customStyle);
        }

        if (resolutionType == ResolutionType::FullWithMatchResultCache) {
            if (auto cachedResult = m_document->styleScope().matchResultCache().resultWithCurrentInlineStyle(element)) {
                auto result = scope().resolver->unadjustedStyleForCachedMatchResult(element, resolutionContext, WTFMove(*cachedResult));
                MatchResultCache::update(*cachedResult, *result.style);
                return result;
            }
        }
        auto result = scope().resolver->unadjustedStyleForElement(element, resolutionContext);
        m_document->styleScope().matchResultCache().set(element, result);
        return result;
    }();

    if (unadjustedStyle.relations)
        commitRelations(WTFMove(unadjustedStyle.relations), *m_update);

    auto style = WTFMove(unadjustedStyle.style);

    // Fully custom styles in UA shadow trees that don't originate from selector matching don't need adjusting.
    if (unadjustedStyle.matchResult) {
        Adjuster adjuster(m_document, *resolutionContext.parentStyle, resolutionContext.parentBoxStyle, &element);
        adjuster.adjust(*style);
    }

    ResolvedStyle resolvedStyle {
        .style = WTFMove(style),
        .relations = { },
        .matchResult = WTFMove(unadjustedStyle.matchResult)
    };

    // Invalidate the last successful position option here. This is the only place where
    // we have access to the old and new style, and hence we could determine whether the
    // style changes enough to invalidate.
    if (shouldInvalidateLastSuccessfulPositionOptionIndex(existingStyle, resolvedStyle.style.get()))
        m_document->styleScope().forgetLastSuccessfulPositionOptionIndex(styleable);

    generatePositionOptionsIfNeeded(resolvedStyle, styleable, resolutionContext);

    return resolvedStyle;
}

void TreeResolver::resetStyleForNonRenderedDescendants(Element& subtreeRoot)
{
    auto descendants = composedTreeDescendants(subtreeRoot);
    for (auto it = descendants.begin(); it != descendants.end();) {
        if (auto* element = dynamicDowncast<Element>(*it)) {
            if (element->needsStyleRecalc()) {
                element->resetComputedStyle();
                element->resetStyleRelations();
                element->setHasValidStyle();
            }
        }

        if (it->childNeedsStyleRecalc()) {
            it->clearChildNeedsStyleRecalc();
            it.traverseNext();
        } else
            it.traverseNextSkippingChildren();
    }

    auto nonRenderedElementsWithPositionOptions = [&] () {
        Vector<RefPtr<const Element>> result;
        for (auto& styleable : m_positionOptions.keys()) {
            if (styleable.first->isComposedTreeDescendantOf(subtreeRoot))
                result.append(styleable.first);
        }

        return result;
    }();

    m_positionOptions.removeIf([&nonRenderedElementsWithPositionOptions] (const auto& kv) {
        return nonRenderedElementsWithPositionOptions.contains(kv.key.first);
    });

    subtreeRoot.clearChildNeedsStyleRecalc();
}

static bool affectsRenderedSubtree(Element& element, const RenderStyle& newStyle)
{
    if (newStyle.display() != DisplayType::None)
        return true;
    if (element.renderOrDisplayContentsStyle())
        return true;
    if (element.rendererIsNeeded(newStyle))
        return true;
    return false;
}

auto TreeResolver::computeDescendantsToResolve(const ElementUpdate& update, const RenderStyle* existingStyle, Validity validity) const -> DescendantsToResolve
{
    if (parent().descendantsToResolve == DescendantsToResolve::All)
        return DescendantsToResolve::All;
    if (validity >= Validity::SubtreeInvalid)
        return DescendantsToResolve::All;

    if (update.changes && existingStyle) {
        auto customPropertyInStyleContainerQueryChanged = [&] {
            auto& namesInQueries = scope().resolver->ruleSets().customPropertyNamesInStyleContainerQueries();
            for (auto& name : namesInQueries) {
                // Any descendant may depend on this changed custom property via a style query.
                if (!existingStyle->customPropertyValueEqual(*update.style, name))
                    return true;
            }
            return false;
        }();
        if (customPropertyInStyleContainerQueryChanged)
            return DescendantsToResolve::All;
    }

    if (update.changes.containsAny({ Change::Container, Change::Renderer }))
        return DescendantsToResolve::All;

    if (update.changes.containsAny(inheritedChanges()))
        return DescendantsToResolve::Children;

    if (update.changes.contains(Change::NonInherited))
        return DescendantsToResolve::ChildrenWithExplicitInherit;

    ASSERT(!update.changes);
    return DescendantsToResolve::None;
};

static bool styleChangeAffectsRelativeUnits(const RenderStyle& style, const RenderStyle* existingStyle)
{
    if (!existingStyle)
        return true;
    return !existingStyle->fontCascadeEqual(style)
        || existingStyle->computedLineHeight() != style.computedLineHeight();
}

auto TreeResolver::resolveElement(Element& element, const RenderStyle* existingStyle, ResolutionType resolutionType) -> std::pair<ElementUpdate, DescendantsToResolve>
{
    if (m_didSeePendingStylesheet && !element.renderOrDisplayContentsStyle() && !m_document->isIgnoringPendingStylesheets()) {
        m_document->setHasNodesWithMissingStyle();
        return { };
    }

    if (resolutionType == ResolutionType::RebuildUsingExisting) {
        return {
            ElementUpdate { RenderStyle::clonePtr(*existingStyle), Change::Renderer },
            DescendantsToResolve::RebuildAllUsingExisting
        };
    }

    auto resolutionContext = makeResolutionContext();

    Styleable styleable { element, { } };
    auto resolvedStyle = styleForStyleable(styleable, resolutionType, resolutionContext, existingStyle);

    updateForPositionVisibility(*resolvedStyle.style, styleable);

    auto update = createAnimatedElementUpdate(WTFMove(resolvedStyle), styleable, parent().changes, resolutionContext, parent().isInDisplayNoneTree);

    if (!affectsRenderedSubtree(element, *update.style)) {
        styleable.setLastStyleChangeEventStyle(nullptr);
        if (update.style->display() == DisplayType::None && element.hasDisplayNone())
            return { WTFMove(update), DescendantsToResolve::None };
        return { };
    }

    auto descendantsToResolve = computeDescendantsToResolve(update, existingStyle, element.styleValidity());
    bool isDocumentElement = &element == m_document->documentElement();
    if (isDocumentElement) {
        if (styleChangeAffectsRelativeUnits(*update.style, existingStyle)) {
            // "rem" units are relative to the document element's font size so we need to recompute everything.
            scope().resolver->invalidateMatchedDeclarationsCache();
            descendantsToResolve = DescendantsToResolve::All;
        }
    }

    // This is needed for resolving color:-webkit-text for subsequent elements.
    // FIXME: We shouldn't mutate document when resolving style.
    if (&element == m_document->body())
        m_document->setTextColor(update.style->visitedDependentColor(CSSPropertyColor));

    // FIXME: These elements should not change renderer based on appearance property.
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element); (input && input->isSearchField())
        || is<HTMLMeterElement>(element)
        || is<HTMLProgressElement>(element)) {
        if (existingStyle && update.style->usedAppearance() != existingStyle->usedAppearance()) {
            update.changes.add(Change::Renderer);
            descendantsToResolve = DescendantsToResolve::All;
        }
    }

    auto resolveAndAddPseudoElementStyle = [&](const PseudoElementIdentifier& pseudoElementIdentifier) {
        const RenderStyle* existingPseudoStyle = existingStyle ? existingStyle->getCachedPseudoStyle(pseudoElementIdentifier) : nullptr;
        auto pseudoElementUpdate = resolvePseudoElement(element, pseudoElementIdentifier, update, parent().isInDisplayNoneTree, existingPseudoStyle);

        auto pseudoElementChanges = [&]() -> OptionSet<Change> {
            if (pseudoElementUpdate) {
                if (pseudoElementIdentifier.type == PseudoElementType::WebKitScrollbar)
                    return pseudoElementUpdate->changes;
                if (!pseudoElementUpdate->changes)
                    return { };
                return { Change::NonInherited };
            }
            if (!existingStyle || !existingStyle->getCachedPseudoStyle(pseudoElementIdentifier))
                return { };
            // If ::first-letter goes aways rebuild the renderers.
            if (pseudoElementIdentifier.type == PseudoElementType::FirstLetter)
                return { Change::Renderer };
            return { Change::NonInherited };
        }();
        update.changes |= pseudoElementChanges;
        if (!pseudoElementUpdate)
            return pseudoElementChanges;
        if (pseudoElementUpdate->recompositeLayer)
            update.recompositeLayer = true;
        update.style->addCachedPseudoStyle(WTFMove(pseudoElementUpdate->style));
        return pseudoElementUpdate->changes;
    };
    
    if (resolveAndAddPseudoElementStyle({ PseudoElementType::FirstLine }))
        descendantsToResolve = DescendantsToResolve::All;
    if (resolveAndAddPseudoElementStyle({ PseudoElementType::FirstLetter }))
        descendantsToResolve = DescendantsToResolve::All;
    if (resolveAndAddPseudoElementStyle({ PseudoElementType::WebKitScrollbar }))
        descendantsToResolve = DescendantsToResolve::All;

    resolveAndAddPseudoElementStyle({ PseudoElementType::Marker });
    resolveAndAddPseudoElementStyle({ PseudoElementType::Before });
    resolveAndAddPseudoElementStyle({ PseudoElementType::After });
    resolveAndAddPseudoElementStyle({ PseudoElementType::Backdrop });

    if (isDocumentElement && m_document->hasViewTransitionPseudoElementTree()) {
        resolveAndAddPseudoElementStyle({ PseudoElementType::ViewTransition });

        RefPtr activeViewTransition = m_document->activeViewTransition();
        ASSERT(activeViewTransition);
        for (auto& name : activeViewTransition->namedElements().keys()) {
            resolveAndAddPseudoElementStyle({ PseudoElementType::ViewTransitionGroup, name });
            resolveAndAddPseudoElementStyle({ PseudoElementType::ViewTransitionImagePair, name });
            resolveAndAddPseudoElementStyle({ PseudoElementType::ViewTransitionNew, name });
            resolveAndAddPseudoElementStyle({ PseudoElementType::ViewTransitionOld, name });
        }
    }

#if ENABLE(TOUCH_ACTION_REGIONS)
    // FIXME: Track this exactly.
    if (!update.style->touchAction().isAuto() && !m_document->quirks().shouldDisablePointerEventsQuirk())
        m_document->setMayHaveElementsWithNonAutoTouchAction();
#endif
#if ENABLE(EDITABLE_REGION)
    if (update.style->usedUserModify() != UserModify::ReadOnly)
        m_document->setMayHaveEditableElements();
#endif

    return { WTFMove(update), descendantsToResolve };
}

std::optional<ElementUpdate> TreeResolver::resolvePseudoElement(Element& element, const PseudoElementIdentifier& pseudoElementIdentifier, const ElementUpdate& elementUpdate, IsInDisplayNoneTree isInDisplayNoneTree, const RenderStyle* existingStyle)
{
    if (elementUpdate.style->display() == DisplayType::None)
        return { };

    if (pseudoElementIdentifier.type == PseudoElementType::Backdrop && !element.isInTopLayer())
        return { };
    if (pseudoElementIdentifier.type == PseudoElementType::Marker && elementUpdate.style->display() != DisplayType::ListItem)
        return { };

    auto userAgentShadowTreeEnclosingResolver = [&] -> Resolver* {
        if (element.isInUserAgentShadowTree())
            return scope().enclosingScope->resolver.ptr();
        return nullptr;
    };

    if (pseudoElementIdentifier.type == PseudoElementType::FirstLine && !scope().resolver->usesFirstLineRules()) {
        // For user-agent shadow tree elements, also check the enclosing (document) scope
        // because user-agent pseudo-elements like details::details-content::first-line
        // are defined in the document scope but target elements in the shadow tree
        RefPtr resolver = userAgentShadowTreeEnclosingResolver();
        if (!resolver || !resolver->usesFirstLineRules())
            return { };
    }

    if (pseudoElementIdentifier.type == PseudoElementType::FirstLetter && !scope().resolver->usesFirstLetterRules()) {
        RefPtr resolver = userAgentShadowTreeEnclosingResolver();
        if (!resolver || !resolver->usesFirstLetterRules())
            return { };
    }

    if (pseudoElementIdentifier.type == PseudoElementType::WebKitScrollbar && elementUpdate.style->overflowX() != Overflow::Scroll && elementUpdate.style->overflowY() != Overflow::Scroll)
        return { };

    ASSERT_IMPLIES(pseudoElementIdentifier.type == PseudoElementType::ViewTransition
        || pseudoElementIdentifier.type == PseudoElementType::ViewTransitionGroup
        || pseudoElementIdentifier.type == PseudoElementType::ViewTransitionImagePair
        || pseudoElementIdentifier.type == PseudoElementType::ViewTransitionNew
        || pseudoElementIdentifier.type == PseudoElementType::ViewTransitionOld,
        m_document->hasViewTransitionPseudoElementTree() && &element == m_document->documentElement());

    if (!elementUpdate.style->hasPseudoStyle(pseudoElementIdentifier.type))
        return resolveAncestorPseudoElement(element, pseudoElementIdentifier, elementUpdate);

    if ((pseudoElementIdentifier.type == PseudoElementType::FirstLine || pseudoElementIdentifier.type == PseudoElementType::FirstLetter) && !supportsFirstLineAndLetterPseudoElement(*elementUpdate.style))
        return { };

    auto resolutionContext = makeResolutionContextForPseudoElement(elementUpdate, pseudoElementIdentifier);

    bool pseudoSupportsPositionTry = pseudoElementIdentifier.type == PseudoElementType::Before || pseudoElementIdentifier.type == PseudoElementType::After || pseudoElementIdentifier.type == PseudoElementType::Backdrop;

    Styleable styleable { element, pseudoElementIdentifier };

    auto resolvedStyle = [&] () {
        std::optional<ResolvedStyle> resolvedStyle;

        if (pseudoSupportsPositionTry)
            resolvedStyle = tryChoosePositionOption(styleable, resolutionContext);

        if (!resolvedStyle) {
            resolvedStyle = scope().resolver->styleForPseudoElement(element, pseudoElementIdentifier, resolutionContext);

            if (resolvedStyle && pseudoSupportsPositionTry) {
                if (shouldInvalidateLastSuccessfulPositionOptionIndex(existingStyle, resolvedStyle->style.get()))
                    m_document->styleScope().forgetLastSuccessfulPositionOptionIndex(styleable);

                generatePositionOptionsIfNeeded(*resolvedStyle, styleable, resolutionContext);
            }
        }

        return resolvedStyle;
    }();

    if (!resolvedStyle)
        return { };

    auto animatedUpdate = createAnimatedElementUpdate(WTFMove(*resolvedStyle), styleable, elementUpdate.changes, resolutionContext, isInDisplayNoneTree);

    if (pseudoElementIdentifier.type == PseudoElementType::Before || pseudoElementIdentifier.type == PseudoElementType::After) {
        if (scope().resolver->usesFirstLineRules()) {
            // ::first-line can inherit to ::before/::after
            if (auto firstLineContext = makeResolutionContextForInheritedFirstLine(elementUpdate, *elementUpdate.style)) {
                auto firstLineStyle = scope().resolver->styleForPseudoElement(element, pseudoElementIdentifier, *firstLineContext);
                firstLineStyle->style->setPseudoElementIdentifier({ { PseudoElementType::FirstLine } });
                animatedUpdate.style->addCachedPseudoStyle(WTFMove(firstLineStyle->style));
            }
        }
        if (scope().resolver->usesFirstLetterRules()) {
            auto beforeAfterContext = makeResolutionContextForPseudoElement(animatedUpdate, { PseudoElementType::FirstLetter });
            if (auto firstLetterStyle = resolveAncestorFirstLetterPseudoElement(element, elementUpdate, beforeAfterContext))
                animatedUpdate.style->addCachedPseudoStyle(WTFMove(firstLetterStyle->style));
        }
    }

    return animatedUpdate;
}

std::optional<ElementUpdate> TreeResolver::resolveAncestorPseudoElement(Element& element, const PseudoElementIdentifier& pseudoElementIdentifier, const ElementUpdate& elementUpdate)
{
    ASSERT(!elementUpdate.style->hasPseudoStyle(pseudoElementIdentifier.type));

    auto pseudoElementStyle = [&]() -> std::optional<ResolvedStyle> {
        // ::first-line and ::first-letter defined on an ancestor element may need to be resolved for the current element.
        if (pseudoElementIdentifier.type == PseudoElementType::FirstLine)
            return resolveAncestorFirstLinePseudoElement(element, elementUpdate);
        if (pseudoElementIdentifier.type == PseudoElementType::FirstLetter) {
            auto resolutionContext = makeResolutionContextForPseudoElement(elementUpdate, { PseudoElementType::FirstLetter });
            return resolveAncestorFirstLetterPseudoElement(element, elementUpdate, resolutionContext);
        }
        return { };
    }();

    if (!pseudoElementStyle)
        return { };

    auto* oldStyle = element.renderOrDisplayContentsStyle(pseudoElementIdentifier);
    auto changes = oldStyle ? determineChanges(*oldStyle, *pseudoElementStyle->style) : Change::Renderer;
    auto resolutionContext = makeResolutionContextForPseudoElement(elementUpdate, pseudoElementIdentifier);

    return createAnimatedElementUpdate(WTFMove(*pseudoElementStyle), { element, pseudoElementIdentifier }, changes, resolutionContext);
}

static bool isChildInBlockFormattingContext(const RenderStyle& style)
{
    // FIXME: Incomplete. There should be shared code with layout for this.
    if (style.display() != DisplayType::Block && style.display() != DisplayType::ListItem)
        return false;
    if (style.hasOutOfFlowPosition())
        return false;
    if (style.floating() != Float::None)
        return false;
    if (style.overflowX() != Overflow::Visible || style.overflowY() != Overflow::Visible)
        return false;
    return true;
};

std::optional<ResolvedStyle> TreeResolver::resolveAncestorFirstLinePseudoElement(Element& element, const ElementUpdate& elementUpdate)
{
    if (elementUpdate.style->display() == DisplayType::Inline) {
        auto* parent = boxGeneratingParent();
        if (!parent)
            return { };

        auto resolutionContext = makeResolutionContextForInheritedFirstLine(elementUpdate, parent->style);
        if (!resolutionContext)
            return { };

        auto elementStyle = scope().resolver->styleForElement(element, *resolutionContext);
        elementStyle.style->setPseudoElementIdentifier({ { PseudoElementType::FirstLine } });

        return elementStyle;
    }

    auto findFirstLineElementForBlock = [&]() -> Element* {
        if (!isChildInBlockFormattingContext(*elementUpdate.style))
            return nullptr;

        // ::first-line is only propagated to the first block.
        if (parent().resolvedFirstLineAndLetterChild)
            return nullptr;

        for (auto& parent : m_parentStack | std::views::reverse) {
            if (parent.style.display() == DisplayType::Contents)
                continue;
            if (!supportsFirstLineAndLetterPseudoElement(parent.style))
                return nullptr;
            if (parent.style.hasPseudoStyle(PseudoElementType::FirstLine))
                return parent.element;
            if (!isChildInBlockFormattingContext(parent.style))
                return nullptr;
        }
        return nullptr;
    };

    auto firstLineElement = findFirstLineElementForBlock();
    if (!firstLineElement)
        return { };

    auto resolutionContext = makeResolutionContextForPseudoElement(elementUpdate, { PseudoElementType::FirstLine });
    // Can't use the cached state since the element being resolved is not the current one.
    resolutionContext.selectorMatchingState = nullptr;

    return scope().resolver->styleForPseudoElement(*firstLineElement, { PseudoElementType::FirstLine }, resolutionContext);
}

std::optional<ResolvedStyle> TreeResolver::resolveAncestorFirstLetterPseudoElement(Element& element, const ElementUpdate& elementUpdate, ResolutionContext& resolutionContext)
{
    auto findFirstLetterElement = [&]() -> Element* {
        if (elementUpdate.style->hasPseudoStyle(PseudoElementType::FirstLetter) && supportsFirstLineAndLetterPseudoElement(*elementUpdate.style))
            return &element;

        // ::first-letter is only propagated to the first box.
        if (parent().resolvedFirstLineAndLetterChild)
            return nullptr;

        bool skipInlines = elementUpdate.style->display() == DisplayType::Inline;
        if (!skipInlines && !isChildInBlockFormattingContext(*elementUpdate.style))
            return nullptr;

        for (auto& parent : m_parentStack | std::views::reverse) {
            if (parent.style.display() == DisplayType::Contents)
                continue;
            if (skipInlines && parent.style.display() == DisplayType::Inline)
                continue;
            skipInlines = false;

            if (!supportsFirstLineAndLetterPseudoElement(parent.style))
                return nullptr;
            if (parent.style.hasPseudoStyle(PseudoElementType::FirstLetter))
                return parent.element;
            if (!isChildInBlockFormattingContext(parent.style))
                return nullptr;
        }
        return nullptr;
    };

    auto firstLetterElement = findFirstLetterElement();
    if (!firstLetterElement)
        return { };

    // Can't use the cached state since the element being resolved is not the current one.
    resolutionContext.selectorMatchingState = nullptr;

    return scope().resolver->styleForPseudoElement(*firstLetterElement, { PseudoElementType::FirstLetter }, resolutionContext);
}

ResolutionContext TreeResolver::makeResolutionContext()
{
    return {
        &parent().style,
        parentBoxStyle(),
        documentElementStyle(),
        &scope().selectorMatchingState,
        &m_treeResolutionState
    };
}

ResolutionContext TreeResolver::makeResolutionContextForPseudoElement(const ElementUpdate& elementUpdate, const PseudoElementIdentifier& pseudoElementIdentifier)
{
    auto parentStyle = [&]() -> const RenderStyle* {
        if (auto parentPseudoId = parentPseudoElement(pseudoElementIdentifier.type)) {
            if (auto* parentPseudoStyle = elementUpdate.style->getCachedPseudoStyle({ *parentPseudoId, (*parentPseudoId == PseudoElementType::ViewTransitionGroup || *parentPseudoId == PseudoElementType::ViewTransitionImagePair) ? pseudoElementIdentifier.nameArgument : nullAtom() }))
                return parentPseudoStyle;
        }
        return elementUpdate.style.get();
    };

    return {
        parentStyle(),
        parentBoxStyleForPseudoElement(elementUpdate),
        documentElementStyle(),
        &scope().selectorMatchingState,
        &m_treeResolutionState
    };
}

std::optional<ResolutionContext> TreeResolver::makeResolutionContextForInheritedFirstLine(const ElementUpdate& elementUpdate, const RenderStyle& inheritStyle)
{
    auto parentFirstLineStyle = inheritStyle.getCachedPseudoStyle({ PseudoElementType::FirstLine });
    if (!parentFirstLineStyle)
        return { };

    // First line style for inlines is made by inheriting from parent first line style.
    return ResolutionContext {
        parentFirstLineStyle,
        parentBoxStyleForPseudoElement(elementUpdate),
        documentElementStyle(),
        &scope().selectorMatchingState,
        &m_treeResolutionState
    };
}

const RenderStyle* TreeResolver::documentElementStyle() const
{
    if (m_computedDocumentElementStyle)
        return m_computedDocumentElementStyle.get();

    return m_document->documentElement()->renderStyle();
}

auto TreeResolver::boxGeneratingParent() const -> const Parent*
{
    // 'display: contents' doesn't generate boxes.
    for (auto& parent : m_parentStack | std::views::reverse) {
        if (parent.style.display() == DisplayType::None)
            return nullptr;
        if (parent.style.display() != DisplayType::Contents)
            return &parent;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

const RenderStyle* TreeResolver::parentBoxStyle() const
{
    auto* parent = boxGeneratingParent();
    return parent ? &parent->style : nullptr;
}

const RenderStyle* TreeResolver::parentBoxStyleForPseudoElement(const ElementUpdate& elementUpdate) const
{
    switch (elementUpdate.style->display()) {
    case DisplayType::None:
        return nullptr;
    case DisplayType::Contents:
        return parentBoxStyle();
    default:
        return elementUpdate.style.get();
    }
}

ElementUpdate TreeResolver::createAnimatedElementUpdate(ResolvedStyle&& resolvedStyle, const Styleable& styleable, OptionSet<Change> parentChanges, const ResolutionContext& resolutionContext, IsInDisplayNoneTree isInDisplayNoneTree)
{
    auto& element = styleable.element;
    auto& document = element.document();
    auto* currentStyle = element.renderOrDisplayContentsStyle(styleable.pseudoElementIdentifier);

    std::unique_ptr<RenderStyle> startingStyle;

    auto* oldStyle = [&]() -> const RenderStyle* {
        if (auto* styleBefore = beforeResolutionStyle(element, styleable.pseudoElementIdentifier))
            return styleBefore;

        if (resolvedStyle.style->hasTransitions()) {
            // https://drafts.csswg.org/css-transitions-2/#at-ruledef-starting-style
            // "If an element does not have a before-change style for a given style change event, the starting style is used instead."
            startingStyle = resolveStartingStyle(resolvedStyle, styleable, resolutionContext);
            return startingStyle.get();
        }
        return nullptr;
    }();

    auto unanimatedDisplay = resolvedStyle.style->display();
    auto hasUnresolvedAnchorPosition = this->hasUnresolvedAnchorPosition(styleable);

    WeakStyleOriginatedAnimations newStyleOriginatedAnimations;

    auto updateAnimations = [&] {
        if (document.backForwardCacheState() != Document::NotInBackForwardCache || document.printing())
            return;

        if (hasUnresolvedAnchorPosition)
            return;

        if (oldStyle && (oldStyle->hasTransitions() || resolvedStyle.style->hasTransitions()))
            styleable.updateCSSTransitions(*oldStyle, *resolvedStyle.style, newStyleOriginatedAnimations);

        if ((oldStyle && oldStyle->hasScrollTimelines()) || resolvedStyle.style->hasScrollTimelines())
            styleable.updateCSSScrollTimelines(oldStyle, *resolvedStyle.style);

        if ((oldStyle && oldStyle->hasViewTimelines()) || resolvedStyle.style->hasViewTimelines())
            styleable.updateCSSViewTimelines(oldStyle, *resolvedStyle.style);

        if ((oldStyle && oldStyle->timelineScope().type != NameScope::Type::None) || resolvedStyle.style->timelineScope().type != NameScope::Type::None) {
            CheckedRef styleOriginatedTimelinesController = element.protectedDocument()->ensureStyleOriginatedTimelinesController();
            styleOriginatedTimelinesController->updateNamedTimelineMapForTimelineScope(resolvedStyle.style->timelineScope(), styleable);
        }

        // The order in which CSS Transitions and CSS Animations are updated matters since CSS Transitions define the after-change style
        // to use CSS Animations as defined in the previous style change event. As such, we update CSS Animations after CSS Transitions
        // such that when CSS Transitions are updated the CSS Animations data is the same as during the previous style change event.
        if ((oldStyle && oldStyle->hasAnimations()) || resolvedStyle.style->hasAnimations())
            styleable.updateCSSAnimations(oldStyle, *resolvedStyle.style, resolutionContext, newStyleOriginatedAnimations, isInDisplayNoneTree);
    };

    auto applyAnimations = [&]() -> std::pair<std::unique_ptr<RenderStyle>, OptionSet<AnimationImpact>> {
        if (hasUnresolvedAnchorPosition) {
            auto newStyle = WTFMove(resolvedStyle.style);
            ASSERT(newStyle);

            // When display transitions to none, it keeps its old value throughout the transition,
            // and only switches to none after the transition ends. CSS transition machinery normally
            // handles this, except we skip applying transitions when anchor position hasn't been resolved.
            // This results in display: none until its anchor position is resolved, when it switches back
            // to the old value. To remedy this, we manually patch display to be the old value if:
            // 1. the old style's display is not none, and
            // 2. the new style has display: none and specifies a transition on display.
            if (oldStyle && oldStyle->hasTransitions() && oldStyle->display() != DisplayType::None && styleHasDisplayTransition(*newStyle) && newStyle->display() == DisplayType::None)
                newStyle->setDisplay(oldStyle->display());
            return { WTFMove(newStyle), OptionSet<AnimationImpact> { } };
        }

        if (!styleable.hasKeyframeEffects()) {
            // FIXME: Push after-change style into parent stack instead.
            styleable.setLastStyleChangeEventStyle(resolveAfterChangeStyleForNonAnimated(resolvedStyle, styleable, resolutionContext));
            styleable.setHasPropertiesOverridenAfterAnimation(false);
            return { WTFMove(resolvedStyle.style), OptionSet<AnimationImpact> { } };
        }

        auto previousLastStyleChangeEventStyle = styleable.lastStyleChangeEventStyle() ? RenderStyle::clonePtr(*styleable.lastStyleChangeEventStyle()) : nullptr;
        // Record the style prior to applying animations for this style change event.
        styleable.setLastStyleChangeEventStyle(RenderStyle::clonePtr(*resolvedStyle.style));

        // Apply all keyframe effects to the new style.
        HashSet<AnimatableCSSProperty> animatedProperties;
        auto animatedStyle = RenderStyle::clonePtr(*resolvedStyle.style);

        auto animationImpact = styleable.applyKeyframeEffects(*animatedStyle, animatedProperties, previousLastStyleChangeEventStyle.get(), resolutionContext);

        if (*resolvedStyle.style == *animatedStyle && animationImpact.isEmpty() && previousLastStyleChangeEventStyle)
            return { WTFMove(resolvedStyle.style), animationImpact };

        if (resolvedStyle.matchResult) {
            auto animatedStyleBeforeCascadeApplication = RenderStyle::clonePtr(*animatedStyle);
            // The cascade may override animated properties and have dependencies to them.
            // FIXME: This is wrong if there are both transitions and animations running on the same element.
            auto overriddenAnimatedProperties = applyCascadeAfterAnimation(*animatedStyle, animatedProperties, styleable.hasRunningTransitions(), *resolvedStyle.matchResult, element, resolutionContext);
            ASSERT(styleable.keyframeEffectStack());
            styleable.keyframeEffectStack()->cascadeDidOverrideProperties(overriddenAnimatedProperties, document);
            styleable.setHasPropertiesOverridenAfterAnimation(!overriddenAnimatedProperties.isEmpty());
        }

        Adjuster adjuster(document, *resolutionContext.parentStyle, resolutionContext.parentBoxStyle, !styleable.pseudoElementIdentifier ? &styleable.element : nullptr);
        adjuster.adjustAnimatedStyle(*animatedStyle, animationImpact);

        return { WTFMove(animatedStyle), animationImpact };
    };

    // FIXME: Something like this is also needed for viewport units.
    if (currentStyle && parent().needsUpdateQueryContainerDependentStyle)
        styleable.queryContainerDidChange();

    // First, we need to make sure that any new CSS animation occuring on this element has a matching WebAnimation
    // on the document timeline.
    updateAnimations();

    // Now we can update all Web animations, which will include CSS Animations as well
    // as animations created via the JS API.
    auto [newStyle, animationImpact] = applyAnimations();

    // Deduplication speeds up equality comparisons as the properties inherit to descendants.
    // FIXME: There should be a more general mechanism for this.
    if (currentStyle)
        newStyle->deduplicateCustomProperties(*currentStyle);

    auto changes = currentStyle ? determineChanges(*currentStyle, *newStyle) : Change::Renderer;

    if (element.hasInvalidRenderer() || parentChanges.contains(Change::Renderer))
        changes.add(Change::Renderer);

    collectChangedAnchorNames(*newStyle, currentStyle);

    auto animationsAffectedDisplay = [&, animatedDisplay = newStyle->display()]() {
        auto* keyframeEffectStack = styleable.keyframeEffectStack();
        if (!keyframeEffectStack)
            return false;
        if (unanimatedDisplay != animatedDisplay)
            return true;
        return keyframeEffectStack->containsProperty(CSSPropertyDisplay);
    }();

    if (!affectsRenderedSubtree(styleable.element, *newStyle) && !animationsAffectedDisplay) {
        // If after updating animations we end up not rendering this element or its subtree
        // and the update did not change the "display" value then we should cancel all
        // style-originated animations while ensuring that the new ones are canceled silently,
        // as if they hadn't been created.
        styleable.cancelStyleOriginatedAnimations(newStyleOriginatedAnimations);
    } else if (!newStyleOriginatedAnimations.isEmpty()) {
        // If style-originated animations were not canceled, then we should make sure that
        // the creation of new style-originated animations during this update is known to the
        // document's timeline as animation scheduling was paused for any animation created
        // during this update.
        if (auto* timeline = m_document->existingTimeline())
            timeline->styleOriginatedAnimationsWereCreated();
    }

    if (animationsAffectedDisplay)
        newStyle->setHasDisplayAffectedByAnimations();

    bool shouldRecompositeLayer = animationImpact.contains(AnimationImpact::RequiresRecomposite) || element.styleResolutionShouldRecompositeLayer();

    auto mayNeedRebuildRoot = [&, newStyle = newStyle.get()] {
        if (changes.contains(Change::Renderer))
            return true;
        // We may need to rebuild the tree starting from further up if there is a position property change to clean up continuations.
        if (currentStyle && currentStyle->position() != newStyle->position())
            return true;
        return false;
    }();

    return { WTFMove(newStyle), changes, shouldRecompositeLayer, mayNeedRebuildRoot };
}

std::unique_ptr<RenderStyle> TreeResolver::resolveStartingStyle(const ResolvedStyle& resolvedStyle, const Styleable& styleable, const ResolutionContext& resolutionContext)
{
    if (!resolvedStyle.matchResult || !resolvedStyle.matchResult->usedRuleTypes.contains(UsedRuleType::StartingStyle))
        return nullptr;

    // "Starting style inherits from the parent’s after-change style just like after-change style does."
    auto& parentStyle = parentAfterChangeStyle(styleable, resolutionContext);

    // We now resolve the starting style by applying all rules (including @starting-style ones) again.
    // We could compute it along with the primary style and include it in MatchedPropertiesCache but it is not
    // clear this would be benefitial as it is typically only used once.
    return resolveAgainInDifferentContext(resolvedStyle, styleable, parentStyle, PropertyCascade::startingStylePropertyTypes(), { }, resolutionContext);
}

std::unique_ptr<RenderStyle> TreeResolver::resolveAfterChangeStyleForNonAnimated(const ResolvedStyle& resolvedStyle, const Styleable& styleable, const ResolutionContext& resolutionContext)
{
    // Element may have after-change style differing from the current style in case they are inheriting from a transitioning element.
    // We need after-change style for non-animating elements only in case there @starting-style rules in the subtree.
    if (!scope().resolver->usesStartingStyleRules())
        return nullptr;

    if (!resolvedStyle.matchResult)
        return nullptr;

    if (styleable.pseudoElementIdentifier)
        return nullptr;

    if (!parent().element || !parent().element->lastStyleChangeEventStyle({ }))
        return nullptr;

    // "Likewise, define the after-change style as.. and inheriting from the after-change style of the parent."
    auto& parentStyle = parentAfterChangeStyle(styleable, resolutionContext);
    return resolveAgainInDifferentContext(resolvedStyle, styleable, parentStyle, PropertyCascade::normalPropertyTypes(), { }, resolutionContext);
}

std::unique_ptr<RenderStyle> TreeResolver::resolveAgainInDifferentContext(const ResolvedStyle& resolvedStyle, const Styleable& styleable, const RenderStyle& parentStyle, OptionSet<PropertyCascade::PropertyType> properties, std::optional<BuilderPositionTryFallback>&& positionTryFallback, const ResolutionContext& resolutionContext)
{
    ASSERT(resolvedStyle.matchResult);

    auto newStyle = RenderStyle::createPtr();
    newStyle->inheritFrom(parentStyle);

    newStyle->setPseudoElementIdentifier(resolvedStyle.style->pseudoElementIdentifier());
    newStyle->copyPseudoElementBitsFrom(*resolvedStyle.style);

    auto builderContext = BuilderContext {
        m_document.get(),
        &parentStyle,
        resolutionContext.documentElementStyle,
        &styleable.element,
        &m_treeResolutionState,
        WTFMove(positionTryFallback)
    };

    auto styleBuilder = Builder {
        *newStyle,
        WTFMove(builderContext),
        *resolvedStyle.matchResult,
        { properties }
    };

    styleBuilder.applyAllProperties();

    if (newStyle->display() == DisplayType::None)
        return nullptr;

    Adjuster adjuster(m_document, parentStyle, resolutionContext.parentBoxStyle, !styleable.pseudoElementIdentifier ? &styleable.element : nullptr);
    adjuster.adjust(*newStyle);

    return newStyle;
}

const RenderStyle& TreeResolver::parentAfterChangeStyle(const Styleable& styleable, const ResolutionContext& resolutionContext) const
{
    if (auto* parentElement = !styleable.pseudoElementIdentifier ? parent().element : &styleable.element) {
        if (auto* afterChangeStyle = parentElement->lastStyleChangeEventStyle({ }))
            return *afterChangeStyle;
    }
    return *resolutionContext.parentStyle;
}

HashSet<AnimatableCSSProperty> TreeResolver::applyCascadeAfterAnimation(RenderStyle& animatedStyle, const HashSet<AnimatableCSSProperty>& animatedProperties, bool isTransition, const MatchResult& matchResult, const Element& element, const ResolutionContext& resolutionContext)
{
    auto builderContext = BuilderContext {
        m_document.get(),
        resolutionContext.parentStyle,
        resolutionContext.documentElementStyle,
        &element,
        &m_treeResolutionState
    };

    auto styleBuilder = Builder {
        animatedStyle,
        WTFMove(builderContext),
        matchResult,
        { isTransition ? PropertyCascade::PropertyType::AfterTransition : PropertyCascade::PropertyType::AfterAnimation },
        &animatedProperties
    };

    styleBuilder.applyAllProperties();

    return styleBuilder.overriddenAnimatedProperties();
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void TreeResolver::pushParent(Element& element, const RenderStyle& style, OptionSet<Change> changes, DescendantsToResolve descendantsToResolve, IsInDisplayNoneTree isInDisplayNoneTree, bool didAXUpdateFontSubtree, bool didAXUpdateTextColorSubtree)
#else
void TreeResolver::pushParent(Element& element, const RenderStyle& style, OptionSet<Change> changes, DescendantsToResolve descendantsToResolve, IsInDisplayNoneTree isInDisplayNoneTree)
#endif
{
    scope().selectorMatchingState.selectorFilter.pushParent(&element);
    if (style.containerType() != ContainerType::Normal)
        scope().selectorMatchingState.containerQueryEvaluationState.sizeQueryContainers.append(element);

    Parent parent(element, style, changes, descendantsToResolve, isInDisplayNoneTree);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    parent.didAXUpdateFontSubtree = didAXUpdateFontSubtree;
    parent.didAXUpdateTextColorSubtree = didAXUpdateTextColorSubtree;
#endif

    if (auto* shadowRoot = element.shadowRoot()) {
        pushScope(*shadowRoot);
        parent.didPushScope = true;
    } else if (RefPtr slot = dynamicDowncast<HTMLSlotElement>(element); slot && slot->assignedNodes()) {
        pushEnclosingScope();
        parent.didPushScope = true;
    }

    parent.needsUpdateQueryContainerDependentStyle = m_parentStack.last().needsUpdateQueryContainerDependentStyle || element.needsUpdateQueryContainerDependentStyle();
    element.clearNeedsUpdateQueryContainerDependentStyle();

    m_parentStack.append(WTFMove(parent));
}

void TreeResolver::popParent()
{
    auto& parentElement = *parent().element;

    parentElement.setHasValidStyle();
    parentElement.clearChildNeedsStyleRecalc();

    // FIXME: Push after-change style into parent stack instead.
    if (!parentElement.hasKeyframeEffects({ }))
        parentElement.setLastStyleChangeEventStyle({ }, nullptr);

    if (parent().didPushScope)
        popScope();

    scope().selectorMatchingState.selectorFilter.popParent();

    auto& queryContainers = scope().selectorMatchingState.containerQueryEvaluationState.sizeQueryContainers;
    if (!queryContainers.isEmpty() && queryContainers.last().ptr() == &parentElement)
        queryContainers.removeLast();

    m_parentStack.removeLast();
}

void TreeResolver::popParentsToDepth(unsigned depth)
{
    ASSERT(depth);
    ASSERT(m_parentStack.size() >= depth);

    while (m_parentStack.size() > depth)
        popParent();
}


auto TreeResolver::determineResolutionType(const Element& element, const RenderStyle* existingStyle, DescendantsToResolve parentDescendantsToResolve, OptionSet<Change> parentChanges) -> std::optional<ResolutionType>
{
    auto combinedValidity = [&] {
        auto validity = element.styleValidity();
        if (auto* pseudoElement = element.beforePseudoElement())
            validity = std::max(validity, pseudoElement->styleValidity());
        if (auto* pseudoElement = element.afterPseudoElement())
            validity = std::max(validity, pseudoElement->styleValidity());
        return validity;
    }();

    if (parentDescendantsToResolve == DescendantsToResolve::None) {
        if (combinedValidity == Validity::AnimationInvalid)
            return ResolutionType::AnimationOnly;
        if (combinedValidity == Validity::Valid && element.hasInvalidRenderer())
            return existingStyle ? ResolutionType::RebuildUsingExisting : ResolutionType::Full;
        if (combinedValidity == Validity::InlineStyleInvalid && existingStyle)
            return ResolutionType::FullWithMatchResultCache;
    }

    if (combinedValidity > Validity::Valid)
        return ResolutionType::Full;

    switch (parentDescendantsToResolve) {
    case DescendantsToResolve::None:
        return { };
    case DescendantsToResolve::RebuildAllUsingExisting:
        return existingStyle ? ResolutionType::RebuildUsingExisting : ResolutionType::Full;
    case DescendantsToResolve::Children: {
        auto canFastPathInherit = [&] {
            if (parentChanges.contains(Change::Inherited))
                return false;
            if (!parentChanges.contains(Change::FastPathInherited))
                return false;
            if (!existingStyle || existingStyle->disallowsFastPathInheritance())
                return false;
            // Some non-inherited property changed along with a fast-path property and we may need to inherit it too.
            if (parentChanges.contains(Change::NonInherited) && existingStyle->hasExplicitlyInheritedProperties())
                return false;
            return true;
        };
        return canFastPathInherit() ? ResolutionType::FastPathInherit : ResolutionType::Full;
    }
    case DescendantsToResolve::All:
        return ResolutionType::Full;
    case DescendantsToResolve::ChildrenWithExplicitInherit:
        if (!existingStyle || existingStyle->hasExplicitlyInheritedProperties())
            return ResolutionType::Full;
        return { };
    };
    ASSERT_NOT_REACHED();
    return { };
}

static void clearNeedsStyleResolution(Element& element)
{
    element.setHasValidStyle();
    if (auto* before = element.beforePseudoElement())
        before->setHasValidStyle();
    if (auto* after = element.afterPseudoElement())
        after->setHasValidStyle();
}

static bool hasLoadingStylesheet(const Style::Scope& styleScope, const Element& element, bool checkDescendants)
{
    if (!styleScope.hasPendingSheetsInBody())
        return false;
    if (styleScope.hasPendingSheetInBody(element))
        return true;
    if (!checkDescendants)
        return false;
    for (auto& descendant : descendantsOfType<Element>(element)) {
        if (styleScope.hasPendingSheetInBody(descendant))
            return true;
    };
    return false;
}

static std::unique_ptr<RenderStyle> createInheritedDisplayContentsStyleIfNeeded(const RenderStyle& parentElementStyle, const RenderStyle* parentBoxStyle)
{
    if (parentElementStyle.display() != DisplayType::Contents)
        return nullptr;
    if (parentBoxStyle && parentBoxStyle->inheritedEqual(parentElementStyle))
        return nullptr;
    // Compute style for imaginary unstyled <span> around the text node.
    auto style = RenderStyle::createPtr();
    style->inheritFrom(parentElementStyle);
    return style;
}

void TreeResolver::resetDescendantStyleRelations(Element& element, DescendantsToResolve descendantsToResolve)
{
    switch (descendantsToResolve) {
    case DescendantsToResolve::None:
    case DescendantsToResolve::RebuildAllUsingExisting:
    case DescendantsToResolve::ChildrenWithExplicitInherit:
        break;
    case DescendantsToResolve::Children:
        element.resetChildStyleRelations();
        break;
    case DescendantsToResolve::All:
        element.resetAllDescendantStyleRelations();
        break;
    };
}

void TreeResolver::resolveComposedTree()
{
    ASSERT(m_parentStack.size() == 1);
    ASSERT(m_scopeStack.size() == 1);

    auto descendants = composedTreeDescendants(m_document);
    auto it = descendants.begin();
    auto end = descendants.end();

    while (it != end) {
        popParentsToDepth(it.depth());

        auto& node = *it;
        auto& parent = this->parent();

        ASSERT(node.isConnected());
        ASSERT(node.containingShadowRoot() == scope().shadowRoot);
        ASSERT(node.parentElement() == parent.element || is<ShadowRoot>(node.parentNode()) || node.parentElement()->shadowRoot());

        if (RefPtr text = dynamicDowncast<Text>(node)) {
            auto containsOnlyASCIIWhitespace = text->containsOnlyASCIIWhitespace();
            auto needsTextUpdate = [&] {
                if ((text->hasInvalidRenderer() && parent.changes != Change::Renderer) || parent.style.display() == DisplayType::Contents)
                    return true;
                if (!text->renderer() && containsOnlyASCIIWhitespace && parent.style.preserveNewline()) {
                    // FIXME: This really needs to be done only when parent.style.preserveNewline() changes value.
                    return true;
                }
                return false;
            };
            if (needsTextUpdate()) {
                TextUpdate textUpdate;
                textUpdate.inheritedDisplayContentsStyle = createInheritedDisplayContentsStyleIfNeeded(parent.style, parentBoxStyle());

                m_update->addText(*text, parent.element, WTFMove(textUpdate));
            }

            if (!containsOnlyASCIIWhitespace)
                parent.resolvedFirstLineAndLetterChild = true;

            text->setHasValidStyle();
            it.traverseNextSkippingChildren();
            continue;
        }

        auto& element = downcast<Element>(node);

        if (it.depth() > Settings::defaultMaximumRenderTreeDepth) {
            resetStyleForNonRenderedDescendants(element);
            it.traverseNextSkippingChildren();
            continue;
        }

        auto* style = existingStyle(element);

        auto changes = OptionSet<Change> { };
        auto descendantsToResolve = DescendantsToResolve::None;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        bool didAXUpdateFontSubtree = parent.didAXUpdateFontSubtree;
        bool didAXUpdateTextColorSubtree = parent.didAXUpdateTextColorSubtree;
#endif
        auto resolutionType = determineResolutionType(element, style, parent.descendantsToResolve, parent.changes);
        if (resolutionType) {
            element.resetComputedStyle();

            if (*resolutionType == ResolutionType::Full)
                element.resetStyleRelations();

            if (element.hasCustomStyleResolveCallbacks())
                element.willRecalcStyle(parent.changes);

            auto [elementUpdate, elementDescendantsToResolve] = resolveElement(element, style, *resolutionType);

            if (element.hasCustomStyleResolveCallbacks())
                element.didRecalcStyle(elementUpdate.changes);
            if (CheckedPtr cache = m_document->existingAXObjectCache()) {
                cache->onStyleChange(element, elementUpdate.changes, style, elementUpdate.style.get());
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
                if (!didAXUpdateFontSubtree)
                    didAXUpdateFontSubtree = cache->onFontChange(element, style, elementUpdate.style.get());
                if (!didAXUpdateTextColorSubtree)
                    didAXUpdateTextColorSubtree = cache->onTextColorChange(element, style, elementUpdate.style.get());
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
            }

            style = elementUpdate.style.get();
            changes = elementUpdate.changes;
            descendantsToResolve = elementDescendantsToResolve;

            if (style || element.hasDisplayNone())
                m_update->addElement(element, parent.element, WTFMove(elementUpdate));
            if (style && &element == m_document->documentElement())
                m_computedDocumentElementStyle = RenderStyle::clonePtr(*style);
            clearNeedsStyleResolution(element);
        }

        if (!style)
            resetStyleForNonRenderedDescendants(element);

        auto queryContainerAction = updateStateForQueryContainer(element, style, descendantsToResolve);
        auto anchorPositionedElementAction = updateAnchorPositioningState(element, style);

        resumeDescendantResolutionIfNeeded(element, changes, descendantsToResolve);

        bool shouldIterateChildren = [&] {
            // display::none, no need to resolve descendants.
            if (!style)
                return false;

            // Style resolution will be resumed after the container or anchor-positioned element has been resolved.
            if (queryContainerAction == LayoutInterleavingAction::SkipDescendants || anchorPositionedElementAction == LayoutInterleavingAction::SkipDescendants) {
                deferDescendantResolution(element, changes, descendantsToResolve);
                return false;
            }

            return element.childNeedsStyleRecalc() || descendantsToResolve != DescendantsToResolve::None;
        }();

        if (!m_didSeePendingStylesheet)
            m_didSeePendingStylesheet = hasLoadingStylesheet(m_document->styleScope(), element, !shouldIterateChildren);

        if (!parent.resolvedFirstLineAndLetterChild && style && generatesBox(*style) && supportsFirstLineAndLetterPseudoElement(*style))
            parent.resolvedFirstLineAndLetterChild = true;

        if (!shouldIterateChildren) {
            it.traverseNextSkippingChildren();
            continue;
        }

        resetDescendantStyleRelations(element, descendantsToResolve);

        auto isInDisplayNoneTree = parent.isInDisplayNoneTree == IsInDisplayNoneTree::Yes || !style || style->display() == DisplayType::None;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        pushParent(element, *style, changes, descendantsToResolve, isInDisplayNoneTree ? IsInDisplayNoneTree::Yes : IsInDisplayNoneTree::No, didAXUpdateFontSubtree, didAXUpdateTextColorSubtree);
#else
        pushParent(element, *style, changes, descendantsToResolve, isInDisplayNoneTree ? IsInDisplayNoneTree::Yes : IsInDisplayNoneTree::No);
#endif
        it.traverseNext();
    }

    popParentsToDepth(1);
}

const RenderStyle* TreeResolver::existingStyle(const Element& element)
{
    auto* style = element.renderOrDisplayContentsStyle();

    if (style && &element == m_document->documentElement()) {
        // Document element style may have got adjusted based on body style but we don't want to inherit those adjustments.
        m_computedDocumentElementStyle = Adjuster::restoreUsedDocumentElementStyleToComputed(*style);
        if (m_computedDocumentElementStyle)
            style = m_computedDocumentElementStyle.get();
    }

    return style;
}

void TreeResolver::deferDescendantResolution(Element& element, OptionSet<Change> changes, DescendantsToResolve descendantsToResolve)
{
    m_deferredDescendantResolutionStates.add(element, DeferredDescendantResolutionState {
        .changes = changes,
        .descendantsToResolve = descendantsToResolve
    });
}

void TreeResolver::resumeDescendantResolutionIfNeeded(Element& element, OptionSet<Change>& changes, DescendantsToResolve& descendantsToResolve)
{
    auto it = m_deferredDescendantResolutionStates.find(element);
    if (it == m_deferredDescendantResolutionStates.end())
        return;

    const auto& state = it->value;

    changes |= state.changes;
    descendantsToResolve = std::max(descendantsToResolve, state.descendantsToResolve);

    m_deferredDescendantResolutionStates.remove(it);
}

auto TreeResolver::updateStateForQueryContainer(Element& element, const RenderStyle* style, DescendantsToResolve& descendantsToResolve) -> LayoutInterleavingAction
{
    if (!style)
        return LayoutInterleavingAction::None;

    if (m_queryContainerStates.get(element).invalidated)
        return LayoutInterleavingAction::None;

    auto* existingStyle = element.renderOrDisplayContentsStyle();
    if (style->containerType() != ContainerType::Normal || (existingStyle && existingStyle->containerType() != ContainerType::Normal)) {
        // If any of the queries use font-size relative units then a font size change
        // may affect their evaluation, so force re-evaluating all descendants.
        if (styleChangeAffectsRelativeUnits(*style, existingStyle))
            descendantsToResolve = DescendantsToResolve::All;

        m_queryContainerStates.add(element, QueryContainerState { });

        return LayoutInterleavingAction::SkipDescendants;
    }

    return LayoutInterleavingAction::None;
}

std::unique_ptr<Update> TreeResolver::resolve()
{
    auto didInterleavedLayout = std::exchange(m_needsInterleavedLayout, false);
    if (didInterleavedLayout)
        m_didFirstInterleavedLayout = true;

    Element* documentElement = m_document->documentElement();
    if (!documentElement) {
        m_document->styleScope().resolver();
        return nullptr;
    }

    if (didInterleavedLayout)
        AnchorPositionEvaluator::updateAnchorPositioningStatesAfterInterleavedLayout(m_document, m_treeResolutionState.anchorPositionedStates);

    if (!documentElement->childNeedsStyleRecalc() && !documentElement->needsStyleRecalc())
        return WTFMove(m_update);

    m_didSeePendingStylesheet = m_document->styleScope().hasPendingSheetsBeforeBody();

    if (!m_update)
        m_update = makeUnique<Update>(m_document);
    m_scopeStack.append(adoptRef(*new Scope(m_document, *m_update)));
    m_parentStack.append(Parent(m_document));

    resolveComposedTree();

    ASSERT(m_scopeStack.size() == 1);
    ASSERT(m_parentStack.size() == 1);
    m_parentStack.clear();
    popScope();

    for (auto& [element, state] : m_queryContainerStates) {
        // Ensure that resumed resolution reaches the container.
        if (!state.invalidated) {
            element->invalidateForResumingQueryContainerResolution();
            state.invalidated = true;

            m_needsInterleavedLayout = true;
        }
    }

    for (auto& elementAndState : m_treeResolutionState.anchorPositionedStates) {
        // Ensure that style resolution visits any unresolved anchor-positioned elements.
        if (elementAndState.value->stage < AnchorPositionResolutionStage::Resolved) {
            const_cast<Element&>(*elementAndState.key.first).invalidateForResumingAnchorPositionedElementResolution();
            m_needsInterleavedLayout = true;
            saveBeforeResolutionStyleForInterleaving(*elementAndState.key.first);
        }
    }

    for (auto& [styleable, options] : m_positionOptions) {
        if (!options.chosen) {
            ASSERT(styleable.first);
            const_cast<Element&>(*styleable.first).invalidateForResumingAnchorPositionedElementResolution();
            m_needsInterleavedLayout = true;
            saveBeforeResolutionStyleForInterleaving(*styleable.first);
        }
    }

    if (!m_changedAnchorNames.isEmpty() || m_allAnchorNamesInvalid) {
        // If there are changes to the anchor names then loop through the existing anchors and see if any of them references those names.
        for (auto entry : m_document->styleScope().anchorPositionedToAnchorMap()) {
            CheckedRef anchorPositionedElement = entry.key;
            auto& anchors = entry.value;

            bool anchorPositionedReferencesChangedAnchorNames = [&] {
                if (m_allAnchorNamesInvalid)
                    return true;

                for (auto anchor : anchors.anchors) {
                    if (m_changedAnchorNames.contains(anchor.name.name()))
                        return true;
                }

                return false;
            }();

            if (anchorPositionedReferencesChangedAnchorNames) {
                // Invalidate the anchor-positioned element, so subsequent style resolution rounds would visit it.
                anchorPositionedElement->invalidateForResumingAnchorPositionedElementResolution();

                // Mark that additional style resolution round is needed.
                m_needsInterleavedLayout = true;

                // If the anchor-positioned element is currently being tracked for resolution,
                // reset the resolution stage to FindAnchor. This re-runs anchor resolution to
                // pick up new anchor name changes.
                AnchorPositionedKey anchorPositionedKey { anchorPositionedElement.ptr(), anchors.pseudoElementIdentifier };
                auto stateIt = m_treeResolutionState.anchorPositionedStates.find(anchorPositionedKey);
                if (stateIt != m_treeResolutionState.anchorPositionedStates.end()) {
                    ASSERT(stateIt->value);
                    stateIt->value->stage = AnchorPositionResolutionStage::FindAnchors;
                }
            }
        }

        m_changedAnchorNames = { };
        m_allAnchorNamesInvalid = false;
    }

    if (m_update->roots().isEmpty())
        return { };

    // Ensure we do at least one interleaved layout as any style change may affect existing anchor positions.
    if (!m_didFirstInterleavedLayout && !m_document->styleScope().anchorPositionedToAnchorMap().isEmptyIgnoringNullReferences())
        m_needsInterleavedLayout = true;

    Adjuster::propagateToDocumentElementAndInitialContainingBlock(*m_update, m_document);

    return WTFMove(m_update);
}

auto TreeResolver::updateAnchorPositioningState(Element& element, const RenderStyle* style) -> LayoutInterleavingAction
{
    if (!style)
        return LayoutInterleavingAction::None;

    auto update = [&](const RenderStyle* style) {
        if (!style)
            return;
        AnchorPositionEvaluator::updateAnchorPositionedStateForDefaultAnchorAndPositionVisibility(element, *style, m_treeResolutionState.anchorPositionedStates);
    };

    update(style);
    update(style->getCachedPseudoStyle({ PseudoElementType::Before }));
    update(style->getCachedPseudoStyle({ PseudoElementType::After }));

    auto needsInterleavedLayout = hasUnresolvedAnchorPosition({ element, { } });
    if (needsInterleavedLayout)
        return LayoutInterleavingAction::SkipDescendants;

    return LayoutInterleavingAction::None;
}

static std::optional<LayoutSize> scrollContainerSizeForPositionOptions(const Styleable& anchored)
{
    CheckedPtr anchoredRenderer = anchored.renderer();
    if (!anchoredRenderer)
        return { };
    // Overlay scrollbars can't affect anchor() function resolution so we don't need to save the size.
    CheckedRef containingBlock = *anchoredRenderer->containingBlock();
    if (containingBlock->canUseOverlayScrollbars())
        return { };
    bool isOverflowScroller = containingBlock->isScrollContainerY() || containingBlock->isScrollContainerY();
    if (!containingBlock->isRenderView() && !isOverflowScroller)
        return { };
    return containingBlock->contentBoxSize();
}

void TreeResolver::generatePositionOptionsIfNeeded(const ResolvedStyle& resolvedStyle, const Styleable& styleable, const ResolutionContext& resolutionContext)
{
    // https://drafts.csswg.org/css-anchor-position-1/#fallback-apply

    if (!resolvedStyle.style || resolvedStyle.style->positionTryFallbacks().isNone())
        return;

    if (!resolvedStyle.style->hasOutOfFlowPosition())
        return;

    AnchorPositionedKey positionOptionsKey { styleable.element, styleable.pseudoElementIdentifier };

    if (m_positionOptions.contains(positionOptionsKey))
        return;

    auto generatePositionOptions = [&] {
        ResolvedStyle clonedResolvedStyle {
            .style = RenderStyle::clonePtr(*resolvedStyle.style),
            .relations = { },
            .matchResult = resolvedStyle.matchResult
        };
        PositionOptions options { .originalResolvedStyle = WTFMove(clonedResolvedStyle) };

        auto scrollContainerSizeOnGeneration = scrollContainerSizeForPositionOptions(styleable);
        options.optionStyles.append({ RenderStyle::clonePtr(*resolvedStyle.style), { }, scrollContainerSizeOnGeneration });

        for (auto [i, fallback] : indexedRange(resolvedStyle.style->positionTryFallbacks())) {
            auto optionStyle = generatePositionOption(fallback, options.originalResolvedStyle, styleable, resolutionContext);
            if (!optionStyle)
                continue;
            optionStyle->setUsedPositionOptionIndex(i);

            options.optionStyles.append({ WTFMove(optionStyle), fallback, scrollContainerSizeOnGeneration });
        }

        return options;
    };

    auto options = generatePositionOptions();

    // If the fallbacks contain anchor references we need to resolve the anchors first and regenerate the options.
    if (hasUnresolvedAnchorPosition(styleable))
        return;

    m_positionOptions.add(positionOptionsKey, WTFMove(options));
}

std::unique_ptr<RenderStyle> TreeResolver::generatePositionOption(const PositionTryFallback& fallback, const ResolvedStyle& resolvedStyle, const Styleable& styleable, const ResolutionContext& resolutionContext)
{
    // https://drafts.csswg.org/css-anchor-position-1/#fallback-apply

    if (!resolvedStyle.matchResult)
        return { };

    auto resolveFallbackProperties = [&] -> RefPtr<const StyleProperties> {
        if (fallback.positionArea.properties) {
            ASSERT(!fallback.ruleAndTactics.rule);
            ASSERT(!fallback.ruleAndTactics.tactics);
            return fallback.positionArea.properties;
        }
        if (!fallback.ruleAndTactics.rule)
            return nullptr;

        // "If an at-rule or property defines a name that other CSS constructs can refer to it by, ... it must be defined as a tree-scoped name."
        // https://drafts.csswg.org/css-scoping-1/#shadow-names
        return Style::Scope::resolveTreeScopedReference(styleable.element, *fallback.ruleAndTactics.rule, [](const Style::Scope& scope, const AtomString& name) -> RefPtr<const StyleProperties> {
            auto& ruleSet = scope.resolverIfExists()->ruleSets().authorStyle();
            auto rule = ruleSet.positionTryRuleForName(name);
            if (!rule)
                return nullptr;
            return rule->properties();
        });
    };

    auto builderFallback = BuilderPositionTryFallback {
        .properties = resolveFallbackProperties(),
        .tactics = fallback.ruleAndTactics.tactics->value,
    };

    return resolveAgainInDifferentContext(resolvedStyle, styleable, *resolutionContext.parentStyle, PropertyCascade::normalPropertyTypes(), WTFMove(builderFallback), resolutionContext);
}

const RenderStyle& TreeResolver::PositionOptions::originalStyle() const
{
    ASSERT(optionStyles.size());
    ASSERT(optionStyles[0].style);
    return *optionStyles[0].style;
}

std::unique_ptr<RenderStyle> TreeResolver::PositionOptions::currentOption() const
{
    ASSERT(index < optionStyles.size());
    ASSERT(optionStyles[index].style);
    return RenderStyle::clonePtr(*optionStyles[index].style);
}

void TreeResolver::sortPositionOptionsIfNeeded(PositionOptions& options, const Styleable& styleable)
{
    if (options.sorted)
        return;
    options.sorted = true;

    CheckedPtr box = dynamicDowncast<RenderBox>(styleable.renderer());
    if (!box || !box->isOutOfFlowPositioned())
        return;

    auto order = options.originalStyle().positionTryOrder();
    if (order != PositionTryOrder::Normal && options.optionStyles.size() > 2) {
        // "For each entry in the position options list, apply that position option to the box, and find
        // the specified inset-modified containing block size that results from those styles."
        // https://drafts.csswg.org/css-anchor-position-1/#position-try-order-property
        auto boxAxis = boxAxisForPositionTryOrder(order, options.originalStyle().writingMode());

        struct SortingOption {
            PositionOption option;
            LayoutUnit containingBlockSize;
        };
        Vector<SortingOption> optionsForSorting;
        optionsForSorting.reserveInitialCapacity(options.optionStyles.size());

        for (size_t i = 1; i < options.optionStyles.size(); ++i) {
            auto constraints = PositionedLayoutConstraints { *box, *options.optionStyles[i].style, boxAxis };
            constraints.computeInsets();
            optionsForSorting.append({ WTFMove(options.optionStyles[i]), constraints.insetModifiedContainingSize() });
        }

        // "Stably sort the position options list according to this size, with the largest coming first."
        std::ranges::stable_sort(optionsForSorting, std::ranges::greater { }, &SortingOption::containingBlockSize);

        for (size_t i = 0; i < optionsForSorting.size(); ++i)
            options.optionStyles[i + 1] = WTFMove(optionsForSorting[i].option);
    }

    // If the styleable has a last successful position option...
    if (auto lastSuccessfulIndex = m_document->styleScope().lastSuccessfulPositionOptionIndexFor(styleable)) {
        // ... find which style in options.optionStyles has that index
        auto lastSuccessfulIndexInOptionStyles = options.optionStyles.findIf([lastSuccessfulIndex] (const auto& option) {
            ASSERT(option.style);
            return option.style->usedPositionOptionIndex() == *lastSuccessfulIndex;
        });

        // If there's one, move it to the beginning.
        // (if it's at index zero, do nothing since it's already at the beginning)
        if (lastSuccessfulIndexInOptionStyles && lastSuccessfulIndexInOptionStyles != notFound) {
            auto lastSuccessfulOption = WTFMove(options.optionStyles[lastSuccessfulIndexInOptionStyles]);
            options.optionStyles.removeAt(lastSuccessfulIndexInOptionStyles);
            options.optionStyles.insert(0, WTFMove(lastSuccessfulOption));
        }
    }
}

std::optional<ResolvedStyle> TreeResolver::tryChoosePositionOption(const Styleable& styleable, const ResolutionContext& resolutionContext)
{
    // https://drafts.csswg.org/css-anchor-position-1/#fallback-apply

    AnchorPositionedKey anchorPositionedKey { styleable.element, styleable.pseudoElementIdentifier };

    auto optionIt = m_positionOptions.find(anchorPositionedKey);
    if (optionIt == m_positionOptions.end())
        return { };

    auto& options = optionIt->value;

    sortPositionOptionsIfNeeded(options, styleable);

    auto invalidateQueryContainer = [&] () {
        // Pseudo-elements can't be query containers, so skip invalidating for pseudo-elements.
        if (styleable.pseudoElementIdentifier)
            return;

        auto iter = m_queryContainerStates.find(styleable.element);
        if (iter == m_queryContainerStates.end())
            return;

        iter->value.invalidated = false;
    };

    auto renderer = dynamicDowncast<RenderBox>(styleable.renderer());
    if (!renderer) {
        invalidateQueryContainer();

        options.chosen = true;
        options.index = 0;
        return ResolvedStyle { RenderStyle::clonePtr(options.originalStyle()) };
    }

    // On the first try, we force apply the original style (which _could_ be different from
    // the existing style, since the original style might have a fallback applied to it)
    if (options.isFirstTry) {
        invalidateQueryContainer();

        options.isFirstTry = false;
        options.index = 0;
        return ResolvedStyle { options.currentOption() };
    }

    if (!options.chosen) {
        ASSERT(options.index < options.optionStyles.size());

        auto& option = options.optionStyles[options.index];
        auto newScrollContainerSize = scrollContainerSizeForPositionOptions(styleable);

        // Re-generate the options if a scrollbar change changes the view size. It may affect anchor() function resolution.
        if (option.scrollContainerSizeOnGeneration != newScrollContainerSize) {
            option.scrollContainerSizeOnGeneration = newScrollContainerSize;
            if (option.option)
                option.style = generatePositionOption(*option.option, options.originalResolvedStyle, styleable, resolutionContext);

            return ResolvedStyle { options.currentOption() };
        }
    }

    // We can't test for overflow before the box has been positioned.
    auto* anchorPositionedState = m_treeResolutionState.anchorPositionedStates.get({ &styleable.element, styleable.pseudoElementIdentifier });
    if (anchorPositionedState && anchorPositionedState->stage < AnchorPositionResolutionStage::Positioned)
        return ResolvedStyle { options.currentOption() };

    if (!AnchorPositionEvaluator::overflowsInsetModifiedContainingBlock(*renderer)) {
        // We don't overflow anymore so this is a good style.
        options.chosen = true;
        return ResolvedStyle { options.currentOption() };
    }

    if (options.chosen) {
        // We have already chosen.
        return ResolvedStyle { options.currentOption() };
    }

    // Next option to try if this doesn't work.
    ++options.index;

    invalidateQueryContainer();

    if (options.index >= options.optionStyles.size()) {
        // None of the options worked, return back to the original.
        options.index = 0;
        options.chosen = true;
        return ResolvedStyle { options.currentOption() };
    }

    return ResolvedStyle { options.currentOption() };
}

void TreeResolver::updateForPositionVisibility(RenderStyle& style, const Styleable& styleable)
{
    if (!hasResolvedAnchorPosition(styleable))
        return;

    auto shouldHideAnchorPositioned = [&] {
        CheckedPtr anchored = dynamicDowncast<RenderBox>(styleable.renderer());
        if (!anchored)
            return false;

        if (style.positionVisibility().contains(PositionVisibilityValue::AnchorsVisible)) {
            // "If the box has a default anchor box but that anchor box is invisible or clipped by intervening boxes, the box’s visibility property computes to force-hidden."
            if (AnchorPositionEvaluator::isDefaultAnchorInvisibleOrClippedByInterveningBoxes(*anchored))
                return true;
        }
        if (style.positionVisibility().contains(PositionVisibilityValue::NoOverflow)) {
            if (AnchorPositionEvaluator::overflowsInsetModifiedContainingBlock(*anchored))
                return true;
        }
        if (style.positionVisibility().contains(PositionVisibilityValue::AnchorsValid)) {
            auto* anchorPositionedState = m_treeResolutionState.anchorPositionedStates.get({ &styleable.element, styleable.pseudoElementIdentifier });
            if (anchorPositionedState) {
                for (auto& anchorElement : anchorPositionedState->anchorElements.values()) {
                    if (!anchorElement)
                        return true;
                }
            }
        }
        return false;
    };

    // FIXME: Implement via "visibility: force-hidden".
    if (shouldHideAnchorPositioned())
        style.setIsForceHidden();
}

const RenderStyle* TreeResolver::beforeResolutionStyle(const Element& element, std::optional<PseudoElementIdentifier> pseudo)
{
    auto resolvePseudoStyle = [&](auto* style) -> const RenderStyle* {
        if (!pseudo)
            return style;
        if (!style)
            return nullptr;
        return style->getCachedPseudoStyle(*pseudo);
    };

    auto it = m_savedBeforeResolutionStylesForInterleaving.find(element);
    if (it != m_savedBeforeResolutionStylesForInterleaving.end())
        return resolvePseudoStyle(it->value.get());

    return resolvePseudoStyle(element.renderOrDisplayContentsStyle());
}

void TreeResolver::saveBeforeResolutionStyleForInterleaving(const Element& element)
{
    m_savedBeforeResolutionStylesForInterleaving.ensure(element, [&]() -> std::unique_ptr<RenderStyle> {
        if (auto* style = element.renderOrDisplayContentsStyle())
            return makeUnique<RenderStyle>(RenderStyle::cloneIncludingPseudoElements(*style));
        return { };
    });
}

bool TreeResolver::hasUnresolvedAnchorPosition(const Styleable& styleable) const
{
    auto* anchorPositionedState = m_treeResolutionState.anchorPositionedStates.get({ &styleable.element, styleable.pseudoElementIdentifier });
    if (anchorPositionedState && anchorPositionedState->stage < AnchorPositionResolutionStage::Resolved)
        return true;

    return false;
}

bool TreeResolver::hasResolvedAnchorPosition(const Styleable& styleable) const
{
    auto* anchorPositionedState = m_treeResolutionState.anchorPositionedStates.get({ &styleable.element, styleable.pseudoElementIdentifier });
    if (anchorPositionedState && anchorPositionedState->stage >= AnchorPositionResolutionStage::Resolved)
        return true;

    return false;
}

void TreeResolver::collectChangedAnchorNames(const RenderStyle& newStyle, const RenderStyle* currentStyle)
{
    // A changed anchor name is either a name being added, a name being removed, or a name whose interpretation changes.
    // This may change which elements get anchored to it.

    if (!currentStyle || currentStyle->anchorNames() != newStyle.anchorNames()) {
        // This could check which individual names differ but usually there is just one.
        auto addChanged = [&](auto& style) {
            for (auto& name : style.anchorNames())
                m_changedAnchorNames.add(name.name);
        };
        if (currentStyle)
            addChanged(*currentStyle);
        addChanged(newStyle);
    }

    // Only anchor-names in self and descendants are affected by anchor-scope so we need to check only if there is an existing style.
    if (currentStyle && currentStyle->anchorScope() != newStyle.anchorScope()) {
        auto addChanged = [&](auto& style) {
            switch (style.anchorScope().type) {
            case NameScope::Type::None:
                break;
            case NameScope::Type::All:
                // This affects desdendants too so lets just say all names are invalid.
                m_allAnchorNamesInvalid = true;
                break;
            case NameScope::Type::Ident:
                // A scope change changes interpretation of these names.
                for (auto& name : style.anchorScope().names)
                    m_changedAnchorNames.add(name.value);
                break;
            }
        };
        addChanged(*currentStyle);
        addChanged(newStyle);
    }
}

static Vector<Function<void ()>>& postResolutionCallbackQueue()
{
    static NeverDestroyed<Vector<Function<void ()>>> vector;
    return vector;
}

static Vector<RefPtr<Frame>>& memoryCacheClientCallsResumeQueue()
{
    static NeverDestroyed<Vector<RefPtr<Frame>>> vector;
    return vector;
}

void deprecatedQueuePostResolutionCallback(Function<void()>&& callback)
{
    postResolutionCallbackQueue().append(WTFMove(callback));
}

static void suspendMemoryCacheClientCalls(Document& document)
{
    Page* page = document.page();
    if (!page || !page->areMemoryCacheClientCallsEnabled())
        return;

    page->setMemoryCacheClientCallsEnabled(false);

    if (RefPtr mainFrame = page->mainFrame())
        memoryCacheClientCallsResumeQueue().append(mainFrame);
}

static unsigned resolutionNestingDepth;

PostResolutionCallbackDisabler::PostResolutionCallbackDisabler(Document& document, DrainCallbacks drainCallbacks)
    : m_drainCallbacks(drainCallbacks)
{
    ++resolutionNestingDepth;

    if (resolutionNestingDepth == 1)
        platformStrategies()->loaderStrategy()->suspendPendingRequests();

    // FIXME: It's strange to build this into the disabler.
    suspendMemoryCacheClientCalls(document);
}

PostResolutionCallbackDisabler::~PostResolutionCallbackDisabler()
{
    if (resolutionNestingDepth == 1) {
        if (m_drainCallbacks == DrainCallbacks::Yes) {
            // Get size each time through the loop because a callback can add more callbacks to the end of the queue.
            auto& queue = postResolutionCallbackQueue();
            for (size_t i = 0; i < queue.size(); ++i)
                queue[i]();
            queue.clear();
        }

        auto& queue = memoryCacheClientCallsResumeQueue();
        for (size_t i = 0; i < queue.size(); ++i) {
            if (RefPtr page = queue[i]->page())
                page->setMemoryCacheClientCallsEnabled(true);
        }
        queue.clear();

        platformStrategies()->loaderStrategy()->resumePendingRequests();
    }

    --resolutionNestingDepth;
}

bool postResolutionCallbacksAreSuspended()
{
    return resolutionNestingDepth;
}

}
}
