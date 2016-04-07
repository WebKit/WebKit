/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004-2010, 2012-2016 Apple Inc. All rights reserved.
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

#include "AuthorStyleSheets.h"
#include "CSSFontSelector.h"
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "ElementIterator.h"
#include "HTMLBodyElement.h"
#include "HTMLSlotElement.h"
#include "LoaderStrategy.h"
#include "MainFrame.h"
#include "NodeRenderStyle.h"
#include "PlatformStrategies.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "Text.h"

#if PLATFORM(IOS)
#include "WKContentObservation.h"
#endif

namespace WebCore {

namespace Style {

static RenderStyle* placeholderStyle;

static void ensurePlaceholderStyle(Document& document)
{
    if (placeholderStyle)
        return;
    placeholderStyle = &RenderStyle::create().leakRef();
    placeholderStyle->setDisplay(NONE);
    placeholderStyle->fontCascade().update(&document.fontSelector());
}

TreeResolver::TreeResolver(Document& document)
    : m_document(document)
{
    ensurePlaceholderStyle(document);
}

TreeResolver::~TreeResolver()
{
}

TreeResolver::Scope::Scope(Document& document)
    : styleResolver(document.ensureStyleResolver())
    , sharingResolver(document, styleResolver.ruleSets(), selectorFilter)
{
}

TreeResolver::Scope::Scope(ShadowRoot& shadowRoot, Scope& enclosingScope)
    : styleResolver(shadowRoot.styleResolver())
    , sharingResolver(shadowRoot.documentScope(), styleResolver.ruleSets(), selectorFilter)
    , shadowRoot(&shadowRoot)
    , enclosingScope(&enclosingScope)
{
}

TreeResolver::Parent::Parent(Document& document, Change change)
    : element(nullptr)
    , style(*document.renderStyle())
    , change(change)
{
}

TreeResolver::Parent::Parent(Element& element, ElementUpdate& update)
    : element(&element)
    , style(*update.style)
    , change(update.change)
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

Ref<RenderStyle> TreeResolver::styleForElement(Element& element, RenderStyle& inheritedStyle)
{
    if (!m_document.haveStylesheetsLoaded() && !element.renderer()) {
        m_document.setHasNodesWithPlaceholderStyle();
        return *placeholderStyle;
    }

    scope().styleResolver.setOverrideDocumentElementStyle(m_documentElementStyle.get());

    if (element.hasCustomStyleResolveCallbacks()) {
        RenderStyle* shadowHostStyle = scope().shadowRoot ? m_update->elementStyle(*scope().shadowRoot->host()) : nullptr;
        if (auto customStyle = element.resolveCustomStyle(inheritedStyle, shadowHostStyle)) {
            if (customStyle->relations)
                commitRelations(WTFMove(customStyle->relations), *m_update);

            return WTFMove(customStyle->renderStyle);
        }
    }

    if (auto style = scope().sharingResolver.resolve(element, *m_update))
        return *style;

    auto elementStyle = scope().styleResolver.styleForElement(element, &inheritedStyle, MatchAllRules, nullptr, &scope().selectorFilter);

    if (elementStyle.relations)
        commitRelations(WTFMove(elementStyle.relations), *m_update);

    return WTFMove(elementStyle.renderStyle);
}

static void resetStyleForNonRenderedDescendants(Element& current)
{
    // FIXME: This is not correct with shadow trees. This should be done with ComposedTreeIterator.
    bool elementNeedingStyleRecalcAffectsNextSiblingElementStyle = false;
    for (auto& child : childrenOfType<Element>(current)) {
        bool affectedByPreviousSibling = child.styleIsAffectedByPreviousSibling() && elementNeedingStyleRecalcAffectsNextSiblingElementStyle;
        if (child.needsStyleRecalc() || elementNeedingStyleRecalcAffectsNextSiblingElementStyle)
            elementNeedingStyleRecalcAffectsNextSiblingElementStyle = child.affectsNextSiblingElementStyle();

        if (child.needsStyleRecalc() || affectedByPreviousSibling) {
            child.resetComputedStyle();
            child.clearNeedsStyleRecalc();
        }

        if (child.childNeedsStyleRecalc()) {
            resetStyleForNonRenderedDescendants(child);
            child.clearChildNeedsStyleRecalc();
        }
    }
}

static bool affectsRenderedSubtree(Element& element, const RenderStyle& newStyle)
{
    if (element.renderer())
        return true;
    if (newStyle.display() != NONE)
        return true;
    if (element.rendererIsNeeded(newStyle))
        return true;
    if (element.shouldMoveToFlowThread(newStyle))
        return true;
    return false;
}

ElementUpdate TreeResolver::resolveElement(Element& element)
{
    auto newStyle = styleForElement(element, parent().style);

    auto* renderer = element.renderer();

    if (!affectsRenderedSubtree(element, newStyle.get()))
        return { };

    ElementUpdate update;

    bool needsNewRenderer = !renderer || element.styleChangeType() == ReconstructRenderTree || parent().change == Detach;
    if (!needsNewRenderer && m_document.frame()->animation().updateAnimations(*renderer, newStyle, newStyle))
        update.isSynthetic = true;

    update.change = needsNewRenderer ? Detach : determineChange(renderer->style(), newStyle);
    update.style = WTFMove(newStyle);

    if (element.styleChangeType() == SyntheticStyleChange)
        update.isSynthetic = true;

    if (&element == m_document.documentElement()) {
        m_documentElementStyle = update.style;

        // If "rem" units are used anywhere in the document, and if the document element's font size changes, then force font updating
        // all the way down the tree. This is simpler than having to maintain a cache of objects (and such font size changes should be rare anyway).
        if (m_document.authorStyleSheets().usesRemUnits() && update.change != NoChange && renderer && renderer->style().fontSize() != update.style->fontSize()) {
            // Cached RenderStyles may depend on the rem units.
            scope().styleResolver.invalidateMatchedPropertiesCache();
            update.change = Force;
        }
    }

    // This is needed for resolving color:-webkit-text for subsequent elements.
    // FIXME: We shouldn't mutate document when resolving style.
    if (&element == m_document.body())
        m_document.setTextColor(update.style->visitedDependentColor(CSSPropertyColor));

    if (update.change != Detach && (parent().change == Force || element.styleChangeType() >= FullStyleChange))
        update.change = Force;

    return update;
}

#if PLATFORM(IOS)
static EVisibility elementImplicitVisibility(const Element* element)
{
    RenderObject* renderer = element->renderer();
    if (!renderer)
        return VISIBLE;

    RenderStyle& style = renderer->style();

    Length width(style.width());
    Length height(style.height());
    if ((width.isFixed() && width.value() <= 0) || (height.isFixed() && height.value() <= 0))
        return HIDDEN;

    Length top(style.top());
    Length left(style.left());
    if (left.isFixed() && width.isFixed() && -left.value() >= width.value())
        return HIDDEN;

    if (top.isFixed() && height.isFixed() && -top.value() >= height.value())
        return HIDDEN;
    return VISIBLE;
}

class CheckForVisibilityChangeOnRecalcStyle {
public:
    CheckForVisibilityChangeOnRecalcStyle(Element* element, RenderStyle* currentStyle)
        : m_element(element)
        , m_previousDisplay(currentStyle ? currentStyle->display() : NONE)
        , m_previousVisibility(currentStyle ? currentStyle->visibility() : HIDDEN)
        , m_previousImplicitVisibility(WKObservingContentChanges() && WKObservedContentChange() != WKContentVisibilityChange ? elementImplicitVisibility(element) : VISIBLE)
    {
    }
    ~CheckForVisibilityChangeOnRecalcStyle()
    {
        if (!WKObservingContentChanges())
            return;
        if (m_element->isInUserAgentShadowTree())
            return;
        RenderStyle* style = m_element->renderStyle();
        if (!style)
            return;
        if ((m_previousDisplay == NONE && style->display() != NONE) || (m_previousVisibility == HIDDEN && style->visibility() != HIDDEN)
            || (m_previousImplicitVisibility == HIDDEN && elementImplicitVisibility(m_element.get()) == VISIBLE))
            WKSetObservedContentChange(WKContentVisibilityChange);
    }
private:
    RefPtr<Element> m_element;
    EDisplay m_previousDisplay;
    EVisibility m_previousVisibility;
    EVisibility m_previousImplicitVisibility;
};
#endif // PLATFORM(IOS)

void TreeResolver::pushParent(Element& element, ElementUpdate& update)
{
    scope().selectorFilter.pushParent(&element);

    Parent parent(element, update);

    if (auto* shadowRoot = element.shadowRoot()) {
        pushScope(*shadowRoot);
        parent.didPushScope = true;
    }
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    else if (is<HTMLSlotElement>(element) && downcast<HTMLSlotElement>(element).assignedNodes()) {
        pushEnclosingScope();
        parent.didPushScope = true;
    }
#endif

    m_parentStack.append(WTFMove(parent));
}

void TreeResolver::popParent()
{
    auto& parentElement = *parent().element;

    parentElement.clearNeedsStyleRecalc();
    parentElement.clearChildNeedsStyleRecalc();

    if (parent().didPushScope)
        popScope();

    scope().selectorFilter.popParent();

    m_parentStack.removeLast();
}

void TreeResolver::popParentsToDepth(unsigned depth)
{
    ASSERT(depth);
    ASSERT(m_parentStack.size() >= depth);

    while (m_parentStack.size() > depth)
        popParent();
}

static bool shouldResolvePseudoElement(PseudoElement* pseudoElement)
{
    if (!pseudoElement)
        return false;
    bool needsStyleRecalc = pseudoElement->needsStyleRecalc();
    pseudoElement->clearNeedsStyleRecalc();
    return needsStyleRecalc;
}

void TreeResolver::resolveComposedTree()
{
    ASSERT(m_parentStack.size() == 1);
    ASSERT(m_scopeStack.size() == 1);

    auto descendants = composedTreeDescendants(m_document);
    auto it = descendants.begin();
    auto end = descendants.end();

    // FIXME: SVG <use> element may cause tree mutations during style recalc.
    it.dropAssertions();

    while (it != end) {
        popParentsToDepth(it.depth());

        auto& node = *it;
        auto& parent = this->parent();

        ASSERT(node.containingShadowRoot() == scope().shadowRoot);
        ASSERT(node.parentElement() == parent.element || is<ShadowRoot>(node.parentNode()) || node.parentElement()->shadowRoot());

        if (is<Text>(node)) {
            auto& text = downcast<Text>(node);
            if (text.styleChangeType() == ReconstructRenderTree && parent.change != Detach)
                m_update->addText(text, parent.element);

            text.clearNeedsStyleRecalc();
            it.traverseNextSkippingChildren();
            continue;
        }

        auto& element = downcast<Element>(node);

        // FIXME: We should deal with this during style invalidation.
        bool affectedByPreviousSibling = element.styleIsAffectedByPreviousSibling() && parent.elementNeedingStyleRecalcAffectsNextSiblingElementStyle;
        if (element.needsStyleRecalc() || parent.elementNeedingStyleRecalcAffectsNextSiblingElementStyle)
            parent.elementNeedingStyleRecalcAffectsNextSiblingElementStyle = element.affectsNextSiblingElementStyle();

        bool shouldResolveForPseudoElement = shouldResolvePseudoElement(element.beforePseudoElement()) || shouldResolvePseudoElement(element.afterPseudoElement());

        ElementUpdate update;
        update.style = element.renderStyle();

        bool shouldResolve = parent.change >= Inherit || element.needsStyleRecalc() || shouldResolveForPseudoElement || affectedByPreviousSibling || element.hasDisplayContents();
        if (shouldResolve) {
#if PLATFORM(IOS)
            CheckForVisibilityChangeOnRecalcStyle checkForVisibilityChange(&element, element.renderStyle());
#endif
            element.resetComputedStyle();

            if (element.hasCustomStyleResolveCallbacks()) {
                if (!element.willRecalcStyle(parent.change)) {
                    it.traverseNextSkippingChildren();
                    continue;
                }
            }

            update = resolveElement(element);

            if (element.hasCustomStyleResolveCallbacks())
                element.didRecalcStyle(update.change);

            if (affectedByPreviousSibling && update.change != Detach)
                update.change = Force;

            if (update.style)
                m_update->addElement(element, parent.element, update);

            element.clearNeedsStyleRecalc();
        }

        if (!update.style) {
            resetStyleForNonRenderedDescendants(element);
            element.clearChildNeedsStyleRecalc();
        }

        bool shouldIterateChildren = update.style && (element.childNeedsStyleRecalc() || update.change != NoChange);
        if (!shouldIterateChildren) {
            it.traverseNextSkippingChildren();
            continue;
        }

        pushParent(element, update);

        it.traverseNext();
    }

    popParentsToDepth(1);
}

std::unique_ptr<const Update> TreeResolver::resolve(Change change)
{
    auto& renderView = *m_document.renderView();

    Element* documentElement = m_document.documentElement();
    if (!documentElement)
        return nullptr;
    if (change != Force && !documentElement->childNeedsStyleRecalc() && !documentElement->needsStyleRecalc())
        return nullptr;

    m_update = std::make_unique<Update>(m_document);
    m_scopeStack.append(adoptRef(*new Scope(m_document)));
    m_parentStack.append(Parent(m_document, change));

    // Pseudo element removal and similar may only work with these flags still set. Reset them after the style recalc.
    renderView.setUsesFirstLineRules(renderView.usesFirstLineRules() || scope().styleResolver.usesFirstLineRules());
    renderView.setUsesFirstLetterRules(renderView.usesFirstLetterRules() || scope().styleResolver.usesFirstLetterRules());

    resolveComposedTree();

    renderView.setUsesFirstLineRules(scope().styleResolver.usesFirstLineRules());
    renderView.setUsesFirstLetterRules(scope().styleResolver.usesFirstLetterRules());

    m_parentStack.clear();
    m_scopeStack.clear();

    if (m_update->roots().isEmpty())
        return { };

    return WTFMove(m_update);
}

static Vector<std::function<void ()>>& postResolutionCallbackQueue()
{
    static NeverDestroyed<Vector<std::function<void ()>>> vector;
    return vector;
}

void queuePostResolutionCallback(std::function<void ()> callback)
{
    postResolutionCallbackQueue().append(callback);
}

static void suspendMemoryCacheClientCalls(Document& document)
{
    Page* page = document.page();
    if (!page || !page->areMemoryCacheClientCallsEnabled())
        return;

    page->setMemoryCacheClientCallsEnabled(false);

    RefPtr<MainFrame> protectedMainFrame = &page->mainFrame();
    postResolutionCallbackQueue().append([protectedMainFrame]{
        if (Page* page = protectedMainFrame->page())
            page->setMemoryCacheClientCallsEnabled(true);
    });
}

static unsigned resolutionNestingDepth;

PostResolutionCallbackDisabler::PostResolutionCallbackDisabler(Document& document)
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
        // Get size each time through the loop because a callback can add more callbacks to the end of the queue.
        auto& queue = postResolutionCallbackQueue();
        for (size_t i = 0; i < queue.size(); ++i)
            queue[i]();
        queue.clear();

        platformStrategies()->loaderStrategy()->resumePendingRequests();
    }

    --resolutionNestingDepth;
}

bool postResolutionCallbacksAreSuspended()
{
    return resolutionNestingDepth;
}

bool isPlaceholderStyle(const RenderStyle& style)
{
    return &style == placeholderStyle;
}

}
}
