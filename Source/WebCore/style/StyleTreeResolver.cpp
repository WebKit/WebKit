/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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

#include "CSSFontSelector.h"
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "HTMLBodyElement.h"
#include "HTMLInputElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLProgressElement.h"
#include "HTMLSlotElement.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "Quirks.h"
#include "RenderElement.h"
#include "RenderStyleSetters.h"
#include "RenderView.h"
#include "ResolvedStyle.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleAdjuster.h"
#include "StyleBuilder.h"
#include "StyleFontSizeFunctions.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "Text.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "WebAnimationTypes.h"
#include "WebAnimationUtilities.h"

namespace WebCore {

namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(TreeResolverScope);

TreeResolver::TreeResolver(Document& document, std::unique_ptr<Update> update)
    : m_document(document)
    , m_update(WTFMove(update))
{
}

TreeResolver::~TreeResolver() = default;

TreeResolver::Scope::Scope(Document& document)
    : resolver(document.styleScope().resolver())
    , sharingResolver(document, resolver->ruleSets(), selectorMatchingState)
{
    document.setIsResolvingTreeStyle(true);

    // Ensure all shadow tree resolvers exist so their construction doesn't depend on traversal.
    for (auto& shadowRoot : document.inDocumentShadowRoots())
        const_cast<ShadowRoot&>(shadowRoot).styleScope().resolver();
}

TreeResolver::Scope::Scope(ShadowRoot& shadowRoot, Scope& enclosingScope)
    : resolver(shadowRoot.styleScope().resolver())
    , sharingResolver(shadowRoot.documentScope(), resolver->ruleSets(), selectorMatchingState)
    , shadowRoot(&shadowRoot)
    , enclosingScope(&enclosingScope)
{
    selectorMatchingState.queryContainers = enclosingScope.selectorMatchingState.queryContainers;
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

TreeResolver::Parent::Parent(Element& element, const RenderStyle& style, Change change, DescendantsToResolve descendantsToResolve)
    : element(&element)
    , style(style)
    , change(change)
    , descendantsToResolve(descendantsToResolve)
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

ResolvedStyle TreeResolver::styleForStyleable(const Styleable& styleable, ResolutionType resolutionType, const ResolutionContext& resolutionContext)
{
    if (resolutionType == ResolutionType::AnimationOnly && styleable.lastStyleChangeEventStyle() && !styleable.hasPropertiesOverridenAfterAnimation())
        return { RenderStyle::clonePtr(*styleable.lastStyleChangeEventStyle()) };

    auto& element = styleable.element;

    if (element.hasCustomStyleResolveCallbacks()) {
        RenderStyle* shadowHostStyle = scope().shadowRoot ? m_update->elementStyle(*scope().shadowRoot->host()) : nullptr;
        if (auto customStyle = element.resolveCustomStyle(resolutionContext, shadowHostStyle)) {
            if (customStyle->relations)
                commitRelations(WTFMove(customStyle->relations), *m_update);

            return WTFMove(*customStyle);
        }
    }

    if (resolutionType == ResolutionType::FastPathInherit) {
        // If the only reason we are computing the style is that some parent inherited properties changed, we can just copy them.
        auto style = RenderStyle::clonePtr(*existingStyle(element));
        style->fastPathInheritFrom(parent().style);
        return { WTFMove(style) };
    }

    if (auto style = scope().sharingResolver.resolve(styleable, *m_update))
        return { WTFMove(style) };

    auto elementStyle = scope().resolver->styleForElement(element, resolutionContext);

    if (elementStyle.relations)
        commitRelations(WTFMove(elementStyle.relations), *m_update);

    return elementStyle;
}

static void resetStyleForNonRenderedDescendants(Element& current)
{
    auto descendants = descendantsOfType<Element>(current);
    for (auto it = descendants.begin(); it != descendants.end();) {
        if (it->needsStyleRecalc()) {
            it->resetComputedStyle();
            it->resetStyleRelations();
            it->setHasValidStyle();
        }

        if (it->childNeedsStyleRecalc()) {
            it->clearChildNeedsStyleRecalc();
            it.traverseNext();
        } else
            it.traverseNextSkippingChildren();
    }
    current.clearChildNeedsStyleRecalc();
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

auto TreeResolver::computeDescendantsToResolve(Change change, Validity validity, DescendantsToResolve parentDescendantsToResolve) -> DescendantsToResolve
{
    if (parentDescendantsToResolve == DescendantsToResolve::All)
        return DescendantsToResolve::All;
    if (validity >= Validity::SubtreeInvalid)
        return DescendantsToResolve::All;
    switch (change) {
    case Change::None:
        return DescendantsToResolve::None;
    case Change::NonInherited:
        return DescendantsToResolve::ChildrenWithExplicitInherit;
    case Change::FastPathInherited:
    case Change::Inherited:
        return DescendantsToResolve::Children;
    case Change::Descendants:
    case Change::Renderer:
        return DescendantsToResolve::All;
    };
    ASSERT_NOT_REACHED();
    return DescendantsToResolve::None;
};

static bool styleChangeAffectsRelativeUnits(const RenderStyle& style, const RenderStyle* existingStyle)
{
    if (!existingStyle)
        return true;
    return existingStyle->fontCascade() != style.fontCascade()
        || existingStyle->computedLineHeight() != style.computedLineHeight();
}

auto TreeResolver::resolveElement(Element& element, const RenderStyle* existingStyle, ResolutionType resolutionType) -> std::pair<ElementUpdate, DescendantsToResolve>
{
    if (m_didSeePendingStylesheet && !element.renderOrDisplayContentsStyle() && !m_document.isIgnoringPendingStylesheets()) {
        m_document.setHasNodesWithMissingStyle();
        return { };
    }

    if (!element.rendererIsEverNeeded() && !element.hasDisplayContents())
        return { };

    if (resolutionType == ResolutionType::RebuildUsingExisting) {
        return {
            ElementUpdate { RenderStyle::clonePtr(*existingStyle), Change::Renderer },
            DescendantsToResolve::RebuildAllUsingExisting
        };
    }

    auto resolutionContext = makeResolutionContext();

    Styleable styleable { element, PseudoId::None };
    auto resolvedStyle = styleForStyleable(styleable, resolutionType, resolutionContext);

    if (!affectsRenderedSubtree(element, *resolvedStyle.style))
        return { };

    auto update = createAnimatedElementUpdate(WTFMove(resolvedStyle), styleable, parent().change, resolutionContext);
    auto descendantsToResolve = computeDescendantsToResolve(update.change, element.styleValidity(), parent().descendantsToResolve);

    if (&element == m_document.documentElement()) {
        if (styleChangeAffectsRelativeUnits(*update.style, existingStyle)) {
            // "rem" units are relative to the document element's font size so we need to recompute everything.
            scope().resolver->invalidateMatchedDeclarationsCache();
            descendantsToResolve = DescendantsToResolve::All;
        }
    }

    // This is needed for resolving color:-webkit-text for subsequent elements.
    // FIXME: We shouldn't mutate document when resolving style.
    if (&element == m_document.body())
        m_document.setTextColor(update.style->visitedDependentColor(CSSPropertyColor));

    // FIXME: These elements should not change renderer based on appearance property.
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element); (input && input->isSearchField())
        || element.hasTagName(HTMLNames::meterTag)
        || is<HTMLProgressElement>(element)) {
        if (existingStyle && update.style->effectiveAppearance() != existingStyle->effectiveAppearance()) {
            update.change = Change::Renderer;
            descendantsToResolve = DescendantsToResolve::All;
        }
    }

    auto resolveAndAddPseudoElementStyle = [&](PseudoId pseudoId) {
        auto pseudoElementUpdate = resolvePseudoElement(element, pseudoId, update);
        auto pseudoElementChange = [&] {
            if (pseudoElementUpdate) {
                if (pseudoId == PseudoId::WebKitScrollbar)
                    return pseudoElementUpdate->change;
                return pseudoElementUpdate->change == Change::None ? Change::None : Change::NonInherited;
            }
            if (!existingStyle || !existingStyle->getCachedPseudoStyle(pseudoId))
                return Change::None;
            // If ::first-letter goes aways rebuild the renderers.
            return pseudoId == PseudoId::FirstLetter ? Change::Renderer : Change::NonInherited;
        }();
        update.change = std::max(update.change, pseudoElementChange);
        if (!pseudoElementUpdate)
            return pseudoElementChange;
        if (pseudoElementUpdate->recompositeLayer)
            update.recompositeLayer = true;
        update.style->addCachedPseudoStyle(WTFMove(pseudoElementUpdate->style));
        return pseudoElementUpdate->change;
    };
    
    if (resolveAndAddPseudoElementStyle(PseudoId::FirstLine) != Change::None)
        descendantsToResolve = DescendantsToResolve::All;
    if (resolveAndAddPseudoElementStyle(PseudoId::FirstLetter) != Change::None)
        descendantsToResolve = DescendantsToResolve::All;
    if (resolveAndAddPseudoElementStyle(PseudoId::WebKitScrollbar) != Change::None)
        descendantsToResolve = DescendantsToResolve::All;

    resolveAndAddPseudoElementStyle(PseudoId::Marker);
    resolveAndAddPseudoElementStyle(PseudoId::Before);
    resolveAndAddPseudoElementStyle(PseudoId::After);
    resolveAndAddPseudoElementStyle(PseudoId::Backdrop);
    resolveAndAddPseudoElementStyle(PseudoId::ViewTransition);

#if ENABLE(TOUCH_ACTION_REGIONS)
    // FIXME: Track this exactly.
    if (update.style->touchActions() != TouchAction::Auto && !m_document.quirks().shouldDisablePointerEventsQuirk())
        m_document.setMayHaveElementsWithNonAutoTouchAction();
#endif
#if ENABLE(EDITABLE_REGION)
    if (update.style->effectiveUserModify() != UserModify::ReadOnly)
        m_document.setMayHaveEditableElements();
#endif

    return { WTFMove(update), descendantsToResolve };
}

inline bool supportsFirstLineAndLetterPseudoElement(const RenderStyle& style)
{
    auto display = style.display();
    return display == DisplayType::Block
        || display == DisplayType::ListItem
        || display == DisplayType::InlineBlock
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption
        || display == DisplayType::FlowRoot;
};

std::optional<ElementUpdate> TreeResolver::resolvePseudoElement(Element& element, PseudoId pseudoId, const ElementUpdate& elementUpdate)
{
    if (elementUpdate.style->display() == DisplayType::None)
        return { };
    if (pseudoId == PseudoId::Backdrop && !element.isInTopLayer())
        return { };
    if (pseudoId == PseudoId::Marker && elementUpdate.style->display() != DisplayType::ListItem)
        return { };
    if (pseudoId == PseudoId::FirstLine && !scope().resolver->usesFirstLineRules())
        return { };
    if (pseudoId == PseudoId::FirstLetter && !scope().resolver->usesFirstLetterRules())
        return { };
    if (pseudoId == PseudoId::WebKitScrollbar && elementUpdate.style->overflowX() != Overflow::Scroll && elementUpdate.style->overflowY() != Overflow::Scroll)
        return { };
    if (pseudoId == PseudoId::ViewTransition && (!element.document().hasViewTransitionPseudoElementTree() || &element != element.document().documentElement()))
        return { };

    if (!elementUpdate.style->hasPseudoStyle(pseudoId))
        return resolveAncestorPseudoElement(element, pseudoId, elementUpdate);

    if ((pseudoId == PseudoId::FirstLine || pseudoId == PseudoId::FirstLetter) && !supportsFirstLineAndLetterPseudoElement(*elementUpdate.style))
        return { };

    auto resolutionContext = makeResolutionContextForPseudoElement(elementUpdate, pseudoId);

    auto resolvedStyle = scope().resolver->styleForPseudoElement(element, { pseudoId }, resolutionContext);
    if (!resolvedStyle)
        return { };

    auto animatedUpdate = createAnimatedElementUpdate(WTFMove(*resolvedStyle), { element, pseudoId }, elementUpdate.change, resolutionContext);

    if (pseudoId == PseudoId::Before || pseudoId == PseudoId::After) {
        if (scope().resolver->usesFirstLineRules()) {
            // ::first-line can inherit to ::before/::after
            if (auto firstLineContext = makeResolutionContextForInheritedFirstLine(elementUpdate, *elementUpdate.style)) {
                auto firstLineStyle = scope().resolver->styleForPseudoElement(element, { pseudoId }, *firstLineContext);
                firstLineStyle->style->setStyleType(PseudoId::FirstLine);
                animatedUpdate.style->addCachedPseudoStyle(WTFMove(firstLineStyle->style));
            }
        }
        if (scope().resolver->usesFirstLetterRules()) {
            auto beforeAfterContext = makeResolutionContextForPseudoElement(animatedUpdate, PseudoId::FirstLetter);
            if (auto firstLetterStyle = resolveAncestorFirstLetterPseudoElement(element, elementUpdate, beforeAfterContext))
                animatedUpdate.style->addCachedPseudoStyle(WTFMove(firstLetterStyle->style));
        }
    }

    return animatedUpdate;
}

std::optional<ElementUpdate> TreeResolver::resolveAncestorPseudoElement(Element& element, PseudoId pseudoId, const ElementUpdate& elementUpdate)
{
    ASSERT(!elementUpdate.style->hasPseudoStyle(pseudoId));

    auto pseudoElementStyle = [&]() -> std::optional<ResolvedStyle> {
        // ::first-line and ::first-letter defined on an ancestor element may need to be resolved for the current element.
        if (pseudoId == PseudoId::FirstLine)
            return resolveAncestorFirstLinePseudoElement(element, elementUpdate);
        if (pseudoId == PseudoId::FirstLetter) {
            auto resolutionContext = makeResolutionContextForPseudoElement(elementUpdate, PseudoId::FirstLetter);
            return resolveAncestorFirstLetterPseudoElement(element, elementUpdate, resolutionContext);
        }
        return { };
    }();

    if (!pseudoElementStyle)
        return { };

    auto* oldStyle = element.renderOrDisplayContentsStyle(pseudoId);
    auto change = oldStyle ? determineChange(*oldStyle, *pseudoElementStyle->style) : Change::Renderer;
    auto resolutionContext = makeResolutionContextForPseudoElement(elementUpdate, pseudoId);

    return createAnimatedElementUpdate(WTFMove(*pseudoElementStyle), { element, pseudoId }, change, resolutionContext);
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
        elementStyle.style->setStyleType(PseudoId::FirstLine);

        return elementStyle;
    }

    auto findFirstLineElementForBlock = [&]() -> Element* {
        if (!isChildInBlockFormattingContext(*elementUpdate.style))
            return nullptr;

        // ::first-line is only propagated to the first block.
        if (parent().resolvedFirstLineAndLetterChild)
            return nullptr;

        for (auto& parent : makeReversedRange(m_parentStack)) {
            if (parent.style.display() == DisplayType::Contents)
                continue;
            if (!supportsFirstLineAndLetterPseudoElement(parent.style))
                return nullptr;
            if (parent.style.hasPseudoStyle(PseudoId::FirstLine))
                return parent.element;
            if (!isChildInBlockFormattingContext(parent.style))
                return nullptr;
        }
        return nullptr;
    };

    auto firstLineElement = findFirstLineElementForBlock();
    if (!firstLineElement)
        return { };

    auto resolutionContext = makeResolutionContextForPseudoElement(elementUpdate, PseudoId::FirstLine);
    // Can't use the cached state since the element being resolved is not the current one.
    resolutionContext.selectorMatchingState = nullptr;

    return scope().resolver->styleForPseudoElement(*firstLineElement, { PseudoId::FirstLine }, resolutionContext);
}

std::optional<ResolvedStyle> TreeResolver::resolveAncestorFirstLetterPseudoElement(Element& element, const ElementUpdate& elementUpdate, ResolutionContext& resolutionContext)
{
    auto findFirstLetterElement = [&]() -> Element* {
        if (elementUpdate.style->hasPseudoStyle(PseudoId::FirstLetter) && supportsFirstLineAndLetterPseudoElement(*elementUpdate.style))
            return &element;

        // ::first-letter is only propagated to the first box.
        if (parent().resolvedFirstLineAndLetterChild)
            return nullptr;

        bool skipInlines = elementUpdate.style->display() == DisplayType::Inline;
        if (!skipInlines && !isChildInBlockFormattingContext(*elementUpdate.style))
            return nullptr;

        for (auto& parent : makeReversedRange(m_parentStack)) {
            if (parent.style.display() == DisplayType::Contents)
                continue;
            if (skipInlines && parent.style.display() == DisplayType::Inline)
                continue;
            skipInlines = false;

            if (!supportsFirstLineAndLetterPseudoElement(parent.style))
                return nullptr;
            if (parent.style.hasPseudoStyle(PseudoId::FirstLetter))
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

    return scope().resolver->styleForPseudoElement(*firstLetterElement, { PseudoId::FirstLetter }, resolutionContext);
}

ResolutionContext TreeResolver::makeResolutionContext()
{
    return {
        &parent().style,
        parentBoxStyle(),
        m_documentElementStyle.get(),
        &scope().selectorMatchingState
    };
}

ResolutionContext TreeResolver::makeResolutionContextForPseudoElement(const ElementUpdate& elementUpdate, PseudoId pseudoId)
{
    auto parentStyle = [&]() -> const RenderStyle* {
        if (pseudoId == PseudoId::FirstLetter) {
            if (auto* firstLineStyle = elementUpdate.style->getCachedPseudoStyle(PseudoId::FirstLine))
                return firstLineStyle;
        }
        return elementUpdate.style.get();
    };

    return {
        parentStyle(),
        parentBoxStyleForPseudoElement(elementUpdate),
        m_documentElementStyle.get(),
        &scope().selectorMatchingState
    };
}

std::optional<ResolutionContext> TreeResolver::makeResolutionContextForInheritedFirstLine(const ElementUpdate& elementUpdate, const RenderStyle& inheritStyle)
{
    auto parentFirstLineStyle = inheritStyle.getCachedPseudoStyle(PseudoId::FirstLine);
    if (!parentFirstLineStyle)
        return { };

    // First line style for inlines is made by inheriting from parent first line style.
    return ResolutionContext {
        parentFirstLineStyle,
        parentBoxStyleForPseudoElement(elementUpdate),
        m_documentElementStyle.get(),
        &scope().selectorMatchingState
    };
}

auto TreeResolver::boxGeneratingParent() const -> const Parent*
{
    // 'display: contents' doesn't generate boxes.
    for (auto& parent : makeReversedRange(m_parentStack)) {
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

ElementUpdate TreeResolver::createAnimatedElementUpdate(ResolvedStyle&& resolvedStyle, const Styleable& styleable, Change parentChange, const ResolutionContext& resolutionContext)
{
    auto& element = styleable.element;
    auto& document = element.document();
    auto* oldStyle = element.renderOrDisplayContentsStyle(styleable.pseudoId);

    auto updateAnimations = [&] {
        if (document.backForwardCacheState() != Document::NotInBackForwardCache || document.printing())
            return;

        if (oldStyle && (oldStyle->hasTransitions() || resolvedStyle.style->hasTransitions()))
            styleable.updateCSSTransitions(*oldStyle, *resolvedStyle.style);

        // The order in which CSS Transitions and CSS Animations are updated matters since CSS Transitions define the after-change style
        // to use CSS Animations as defined in the previous style change event. As such, we update CSS Animations after CSS Transitions
        // such that when CSS Transitions are updated the CSS Animations data is the same as during the previous style change event.
        if ((oldStyle && oldStyle->hasAnimations()) || resolvedStyle.style->hasAnimations())
            styleable.updateCSSAnimations(oldStyle, *resolvedStyle.style, resolutionContext);
    };

    auto applyAnimations = [&]() -> std::pair<std::unique_ptr<RenderStyle>, OptionSet<AnimationImpact>> {
        if (!styleable.hasKeyframeEffects()) {
            styleable.setLastStyleChangeEventStyle(nullptr);
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

        Adjuster adjuster(document, *resolutionContext.parentStyle, resolutionContext.parentBoxStyle, styleable.pseudoId == PseudoId::None ? &element : nullptr);
        adjuster.adjustAnimatedStyle(*animatedStyle, animationImpact);

        return { WTFMove(animatedStyle), animationImpact };
    };

    // FIXME: Something like this is also needed for viewport units.
    if (oldStyle && parent().needsUpdateQueryContainerDependentStyle)
        styleable.queryContainerDidChange();

    // First, we need to make sure that any new CSS animation occuring on this element has a matching WebAnimation
    // on the document timeline.
    updateAnimations();

    // Now we can update all Web animations, which will include CSS Animations as well
    // as animations created via the JS API.
    auto [newStyle, animationImpact] = applyAnimations();

    // Deduplication speeds up equality comparisons as the properties inherit to descendants.
    // FIXME: There should be a more general mechanism for this.
    if (oldStyle)
        newStyle->deduplicateCustomProperties(*oldStyle);

    auto change = oldStyle ? determineChange(*oldStyle, *newStyle) : Change::Renderer;

    if (element.hasInvalidRenderer() || parentChange == Change::Renderer)
        change = Change::Renderer;

    bool shouldRecompositeLayer = animationImpact.contains(AnimationImpact::RequiresRecomposite) || element.styleResolutionShouldRecompositeLayer();
    return { WTFMove(newStyle), change, shouldRecompositeLayer };
}

HashSet<AnimatableCSSProperty> TreeResolver::applyCascadeAfterAnimation(RenderStyle& animatedStyle, const HashSet<AnimatableCSSProperty>& animatedProperties, bool isTransition, const MatchResult& matchResult, const Element& element, const ResolutionContext& resolutionContext)
{
    auto builderContext = BuilderContext {
        m_document,
        *resolutionContext.parentStyle,
        resolutionContext.documentElementStyle,
        &element
    };

    auto styleBuilder = Builder {
        animatedStyle,
        WTFMove(builderContext),
        matchResult,
        CascadeLevel::Author,
        isTransition ? PropertyCascade::PropertyType::AfterTransition : PropertyCascade::PropertyType::AfterAnimation,
        &animatedProperties
    };

    styleBuilder.applyAllProperties();

    return styleBuilder.overriddenAnimatedProperties();
}

void TreeResolver::pushParent(Element& element, const RenderStyle& style, Change change, DescendantsToResolve descendantsToResolve)
{
    scope().selectorMatchingState.selectorFilter.pushParent(&element);
    if (style.containerType() != ContainerType::Normal)
        scope().selectorMatchingState.queryContainers.append(element);

    Parent parent(element, style, change, descendantsToResolve);

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

    if (parent().didPushScope)
        popScope();

    scope().selectorMatchingState.selectorFilter.popParent();

    auto& queryContainers = scope().selectorMatchingState.queryContainers;
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


auto TreeResolver::determineResolutionType(const Element& element, const RenderStyle* existingStyle, DescendantsToResolve parentDescendantsToResolve, Change parentChange) -> std::optional<ResolutionType>
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
    }

    if (combinedValidity > Validity::Valid)
        return ResolutionType::Full;

    switch (parentDescendantsToResolve) {
    case DescendantsToResolve::None:
        return { };
    case DescendantsToResolve::RebuildAllUsingExisting:
        return existingStyle ? ResolutionType::RebuildUsingExisting : ResolutionType::Full;
    case DescendantsToResolve::Children:
        if (parentChange == Change::FastPathInherited) {
            if (existingStyle && !existingStyle->disallowsFastPathInheritance())
                return ResolutionType::FastPathInherit;
        }
        return ResolutionType::Full;
    case DescendantsToResolve::All:
        return ResolutionType::Full;
    case DescendantsToResolve::ChildrenWithExplicitInherit:
        if (existingStyle && existingStyle->hasExplicitlyInheritedProperties())
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
            if ((text->hasInvalidRenderer() && parent.change != Change::Renderer) || parent.style.display() == DisplayType::Contents) {
                TextUpdate textUpdate;
                textUpdate.inheritedDisplayContentsStyle = createInheritedDisplayContentsStyleIfNeeded(parent.style, parentBoxStyle());

                m_update->addText(*text, parent.element, WTFMove(textUpdate));
            }

            if (!text->containsOnlyASCIIWhitespace())
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

        auto change = Change::None;
        auto descendantsToResolve = DescendantsToResolve::None;

        auto resolutionType = determineResolutionType(element, style, parent.descendantsToResolve, parent.change);
        if (resolutionType) {
            element.resetComputedStyle();
            element.resetStyleRelations();

            if (element.hasCustomStyleResolveCallbacks())
                element.willRecalcStyle(parent.change);

            auto [elementUpdate, elementDescendantsToResolve] = resolveElement(element, style, *resolutionType);

            if (element.hasCustomStyleResolveCallbacks())
                element.didRecalcStyle(elementUpdate.change);

            style = elementUpdate.style.get();
            change = elementUpdate.change;
            descendantsToResolve = elementDescendantsToResolve;

            if (style) {
                m_update->addElement(element, parent.element, WTFMove(elementUpdate));

                if (&element == m_document.documentElement())
                    m_documentElementStyle = RenderStyle::clonePtr(*style);
            }
            clearNeedsStyleResolution(element);
        }

        if (!style)
            resetStyleForNonRenderedDescendants(element);

        auto queryContainerAction = updateStateForQueryContainer(element, style, change, descendantsToResolve);

        bool shouldIterateChildren = [&] {
            // display::none, no need to resolve descendants.
            if (!style)
                return false;
            // Style resolution will be resumed after the container has been resolved.
            if (queryContainerAction == QueryContainerAction::Resolve)
                return false;
            return element.childNeedsStyleRecalc() || descendantsToResolve != DescendantsToResolve::None;
        }();


        if (!m_didSeePendingStylesheet)
            m_didSeePendingStylesheet = hasLoadingStylesheet(m_document.styleScope(), element, !shouldIterateChildren);

        if (!parent.resolvedFirstLineAndLetterChild && style && generatesBox(*style) && supportsFirstLineAndLetterPseudoElement(*style))
            parent.resolvedFirstLineAndLetterChild = true;

        if (!shouldIterateChildren) {
            it.traverseNextSkippingChildren();
            continue;
        }
        
        resetDescendantStyleRelations(element, descendantsToResolve);

        pushParent(element, *style, change, descendantsToResolve);

        it.traverseNext();
    }

    popParentsToDepth(1);
}

const RenderStyle* TreeResolver::existingStyle(const Element& element)
{
    auto* style = element.renderOrDisplayContentsStyle();

    if (style && &element == m_document.documentElement()) {
        // Document element style may have got adjusted based on body style but we don't want to inherit those adjustments.
        m_documentElementStyle = Adjuster::restoreUsedDocumentElementStyleToComputed(*style);
        if (m_documentElementStyle)
            style = m_documentElementStyle.get();
    }

    return style;
}

auto TreeResolver::updateStateForQueryContainer(Element& element, const RenderStyle* style, Change& change, DescendantsToResolve& descendantsToResolve) -> QueryContainerAction
{
    if (!style)
        return QueryContainerAction::None;

    auto tryRestoreState = [&](auto& state) {
        if (!state)
            return;
        change = std::max(change, state->change);
        descendantsToResolve = std::max(descendantsToResolve, state->descendantsToResolve);
        state = { };
    };

    if (auto it = m_queryContainerStates.find(element); it != m_queryContainerStates.end()) {
        tryRestoreState(it->value);
        return QueryContainerAction::Continue;
    }

    auto* existingStyle = element.renderOrDisplayContentsStyle();
    if (style->containerType() != ContainerType::Normal || (existingStyle && existingStyle->containerType() != ContainerType::Normal)) {
        // If any of the queries use font-size relative units then a font size change may affect their evaluation.
        if (styleChangeAffectsRelativeUnits(*style, existingStyle))
            descendantsToResolve = DescendantsToResolve::All;
        m_queryContainerStates.add(element, QueryContainerState { change, descendantsToResolve });
        m_hasUnresolvedQueryContainers = true;
        return QueryContainerAction::Resolve;
    }

    return QueryContainerAction::None;
}

std::unique_ptr<Update> TreeResolver::resolve()
{
    m_hasUnresolvedQueryContainers = false;

    Element* documentElement = m_document.documentElement();
    if (!documentElement) {
        m_document.styleScope().resolver();
        return nullptr;
    }

    if (!documentElement->childNeedsStyleRecalc() && !documentElement->needsStyleRecalc())
        return WTFMove(m_update);

    m_didSeePendingStylesheet = m_document.styleScope().hasPendingSheetsBeforeBody();

    if (!m_update)
        m_update = makeUnique<Update>(m_document);
    m_scopeStack.append(adoptRef(*new Scope(m_document)));
    m_parentStack.append(Parent(m_document));

    resolveComposedTree();

    ASSERT(m_scopeStack.size() == 1);
    ASSERT(m_parentStack.size() == 1);
    m_parentStack.clear();
    popScope();

    if (m_hasUnresolvedQueryContainers) {
        for (auto& containerAndState : m_queryContainerStates) {
            // Ensure that resumed resolution reaches the container.
            if (containerAndState.value)
                containerAndState.key->invalidateForResumingQueryContainerResolution();
        }
    }

    if (m_update->roots().isEmpty())
        return { };

    Adjuster::propagateToDocumentElementAndInitialContainingBlock(*m_update, m_document);

    return WTFMove(m_update);
}

static Vector<Function<void ()>>& postResolutionCallbackQueue()
{
    static NeverDestroyed<Vector<Function<void ()>>> vector;
    return vector;
}

static Vector<RefPtr<LocalFrame>>& memoryCacheClientCallsResumeQueue()
{
    static NeverDestroyed<Vector<RefPtr<LocalFrame>>> vector;
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

    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame()))
        memoryCacheClientCallsResumeQueue().append(localMainFrame);
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
            if (auto* page = queue[i]->page())
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
