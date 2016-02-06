/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004-2010, 2012-2014 Apple Inc. All rights reserved.
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
#include "AnimationController.h"
#include "AuthorStyleSheets.h"
#include "CSSFontSelector.h"
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "FlowThreadController.h"
#include "HTMLSlotElement.h"
#include "InspectorInstrumentation.h"
#include "LoaderStrategy.h"
#include "MainFrame.h"
#include "NodeRenderStyle.h"
#include "NodeTraversal.h"
#include "PlatformStrategies.h"
#include "RenderFullScreen.h"
#include "RenderNamedFlowThread.h"
#include "RenderText.h"
#include "RenderTreePosition.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include "Text.h"

#if PLATFORM(IOS)
#include "WKContentObservation.h"
#endif

namespace WebCore {

namespace Style {

enum DetachType { NormalDetach, ReattachDetach };

static void attachTextRenderer(Text&, RenderTreePosition&);
static void detachRenderTree(Element&, DetachType);
static void resolveTextNode(Text&, RenderTreePosition&);

class SelectorFilterPusher {
public:
    enum PushMode { Push, NoPush };
    SelectorFilterPusher(SelectorFilter& selectorFilter, Element& parent, PushMode pushMode = Push)
        : m_selectorFilter(selectorFilter)
        , m_parent(parent)
    {
        if (pushMode == Push)
            push();
    }
    void push()
    {
        if (m_didPush)
            return;
        m_didPush = true;
        m_selectorFilter.pushParent(&m_parent);
    }
    ~SelectorFilterPusher()
    {
        if (!m_didPush)
            return;
        m_selectorFilter.popParent();
    }
    
private:
    SelectorFilter& m_selectorFilter;
    Element& m_parent;
    bool m_didPush { false };
};


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

    m_scopeStack.append(adoptRef(*new Scope(document)));
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

static bool shouldCreateRenderer(const Element& element, const RenderElement& parentRenderer)
{
    if (!element.document().shouldCreateRenderers())
        return false;
    if (!parentRenderer.canHaveChildren() && !(element.isPseudoElement() && parentRenderer.canHaveGeneratedChildren()))
        return false;
    if (parentRenderer.element() && !parentRenderer.element()->childShouldCreateRenderer(element))
        return false;
    return true;
}

Ref<RenderStyle> TreeResolver::styleForElement(Element& element, RenderStyle& inheritedStyle)
{
    if (!m_document.haveStylesheetsLoaded() && !element.renderer()) {
        m_document.setHasNodesWithPlaceholderStyle();
        return *placeholderStyle;
    }

    if (element.hasCustomStyleResolveCallbacks()) {
        if (RefPtr<RenderStyle> style = element.customStyleForRenderer(inheritedStyle))
            return style.releaseNonNull();
    }

    if (auto* sharingElement = scope().sharingResolver.resolve(element))
        return *sharingElement->renderStyle();

    return scope().styleResolver.styleForElement(element, &inheritedStyle, MatchAllRules, nullptr, &scope().selectorFilter);
}

#if ENABLE(CSS_REGIONS)
static RenderNamedFlowThread* moveToFlowThreadIfNeeded(Element& element, const RenderStyle& style)
{
    if (!element.shouldMoveToFlowThread(style))
        return 0;

    FlowThreadController& flowThreadController = element.document().renderView()->flowThreadController();
    RenderNamedFlowThread& parentFlowRenderer = flowThreadController.ensureRenderFlowThreadWithName(style.flowThread());
    flowThreadController.registerNamedFlowContentElement(element, parentFlowRenderer);
    return &parentFlowRenderer;
}
#endif

void TreeResolver::createRenderer(Element& element, RenderStyle& inheritedStyle, RenderTreePosition& renderTreePosition, RefPtr<RenderStyle>&& resolvedStyle)
{
    ASSERT(!element.renderer());

    RefPtr<RenderStyle> style = resolvedStyle;

    if (!shouldCreateRenderer(element, renderTreePosition.parent()))
        return;

    if (!style)
        style = styleForElement(element, inheritedStyle);

    RenderNamedFlowThread* parentFlowRenderer = 0;
#if ENABLE(CSS_REGIONS)
    parentFlowRenderer = moveToFlowThreadIfNeeded(element, *style);
#endif

    if (!element.rendererIsNeeded(*style))
        return;

    renderTreePosition.computeNextSibling(element);

    RenderTreePosition insertionPosition = parentFlowRenderer
        ? RenderTreePosition(*parentFlowRenderer, parentFlowRenderer->nextRendererForElement(element))
        : renderTreePosition;

    RenderElement* newRenderer = element.createElementRenderer(style.releaseNonNull(), insertionPosition).leakPtr();
    if (!newRenderer)
        return;
    if (!insertionPosition.canInsert(*newRenderer)) {
        newRenderer->destroy();
        return;
    }

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(insertionPosition.parent().flowThreadState());

    // Code below updateAnimations() can depend on Element::renderer() already being set.
    element.setRenderer(newRenderer);

    // FIXME: There's probably a better way to factor this.
    // This just does what setAnimatedStyle() does, except with setStyleInternal() instead of setStyle().
    Ref<RenderStyle> animatedStyle = newRenderer->style();
    newRenderer->animation().updateAnimations(*newRenderer, animatedStyle, animatedStyle);
    newRenderer->setStyleInternal(WTFMove(animatedStyle));

    newRenderer->initializeStyle();

#if ENABLE(FULLSCREEN_API)
    if (m_document.webkitIsFullScreen() && m_document.webkitCurrentFullScreenElement() == &element) {
        newRenderer = RenderFullScreen::wrapRenderer(newRenderer, &insertionPosition.parent(), m_document);
        if (!newRenderer)
            return;
    }
#endif
    // Note: Adding newRenderer instead of renderer(). renderer() may be a child of newRenderer.
    insertionPosition.insert(*newRenderer);
}

static void invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(Node& current)
{
    // FIXME: This needs to traverse in composed tree order.

    // This function finds sibling text renderers where the results of textRendererIsNeeded may have changed as a result of
    // the current node gaining or losing the renderer. This can only affect white space text nodes.
    for (Node* sibling = current.nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (sibling->needsStyleRecalc())
            return;
        if (is<Element>(*sibling)) {
            // Text renderers beyond rendered elements can't be affected.
            if (!sibling->renderer() || RenderTreePosition::isRendererReparented(*sibling->renderer()))
                continue;
            return;
        }
        if (!is<Text>(*sibling))
            continue;
        Text& textSibling = downcast<Text>(*sibling);
        if (!textSibling.containsOnlyWhitespace())
            continue;
        textSibling.setNeedsStyleRecalc();
    }
}

static bool textRendererIsNeeded(const Text& textNode, const RenderTreePosition& renderTreePosition)
{
    const RenderElement& parentRenderer = renderTreePosition.parent();
    if (!parentRenderer.canHaveChildren())
        return false;
    if (parentRenderer.element() && !parentRenderer.element()->childShouldCreateRenderer(textNode))
        return false;
    if (textNode.isEditingText())
        return true;
    if (!textNode.length())
        return false;
    if (!textNode.containsOnlyWhitespace())
        return true;
    // This text node has nothing but white space. We may still need a renderer in some cases.
    if (parentRenderer.isTable() || parentRenderer.isTableRow() || parentRenderer.isTableSection() || parentRenderer.isRenderTableCol() || parentRenderer.isFrameSet())
        return false;
    if (parentRenderer.style().preserveNewline()) // pre/pre-wrap/pre-line always make renderers.
        return true;

    RenderObject* previousRenderer = renderTreePosition.previousSiblingRenderer(textNode);
    if (previousRenderer && previousRenderer->isBR()) // <span><br/> <br/></span>
        return false;

    if (parentRenderer.isRenderInline()) {
        // <span><div/> <div/></span>
        if (previousRenderer && !previousRenderer->isInline())
            return false;
    } else {
        if (parentRenderer.isRenderBlock() && !parentRenderer.childrenInline() && (!previousRenderer || !previousRenderer->isInline()))
            return false;
        
        RenderObject* first = parentRenderer.firstChild();
        while (first && first->isFloatingOrOutOfFlowPositioned())
            first = first->nextSibling();
        RenderObject* nextRenderer = renderTreePosition.nextSiblingRenderer(textNode);
        if (!first || nextRenderer == first) {
            // Whitespace at the start of a block just goes away. Don't even make a render object for this text.
            return false;
        }
    }
    return true;
}

static void createTextRendererIfNeeded(Text& textNode, RenderTreePosition& renderTreePosition)
{
    ASSERT(!textNode.renderer());

    if (!textRendererIsNeeded(textNode, renderTreePosition))
        return;

    auto newRenderer = textNode.createTextRenderer(renderTreePosition.parent().style());
    ASSERT(newRenderer);

    renderTreePosition.computeNextSibling(textNode);

    if (!renderTreePosition.canInsert(*newRenderer))
        return;

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(renderTreePosition.parent().flowThreadState());

    textNode.setRenderer(newRenderer.get());
    // Parent takes care of the animations, no need to call setAnimatableStyle.
    renderTreePosition.insert(*newRenderer.leakPtr());
}

void attachTextRenderer(Text& textNode, RenderTreePosition& renderTreePosition)
{
    createTextRendererIfNeeded(textNode, renderTreePosition);

    textNode.clearNeedsStyleRecalc();
}

void detachTextRenderer(Text& textNode)
{
    if (textNode.renderer())
        textNode.renderer()->destroyAndCleanupAnonymousWrappers();
    textNode.setRenderer(0);
}

void updateTextRendererAfterContentChange(Text& textNode, unsigned offsetOfReplacedData, unsigned lengthOfReplacedData)
{
    auto* renderingParentNode = composedTreeAncestors(textNode).first();
    if (!renderingParentNode || !renderingParentNode->renderer())
        return;

    bool hadRenderer = textNode.renderer();

    RenderTreePosition renderTreePosition(*renderingParentNode->renderer());
    resolveTextNode(textNode, renderTreePosition);

    if (hadRenderer && textNode.renderer())
        textNode.renderer()->setTextWithOffset(textNode.data(), offsetOfReplacedData, lengthOfReplacedData);
}

void TreeResolver::createRenderTreeForChildren(ContainerNode& current, RenderStyle& inheritedStyle, RenderTreePosition& renderTreePosition)
{
    for (Node* child = current.firstChild(); child; child = child->nextSibling()) {
        ASSERT((!child->renderer() || child->isNamedFlowContentNode()) || current.shadowRoot());
        if (child->renderer()) {
            renderTreePosition.invalidateNextSibling(*child->renderer());
            continue;
        }
        if (is<Text>(*child)) {
            attachTextRenderer(downcast<Text>(*child), renderTreePosition);
            continue;
        }
        if (is<Element>(*child))
            createRenderTreeRecursively(downcast<Element>(*child), inheritedStyle, renderTreePosition, nullptr);
    }
}

void TreeResolver::createRenderTreeForShadowRoot(ShadowRoot& shadowRoot)
{
    ASSERT(shadowRoot.host());
    ASSERT(shadowRoot.host()->renderer());

    pushScope(shadowRoot);

    auto& renderer = *shadowRoot.host()->renderer();
    RenderTreePosition renderTreePosition(renderer);
    createRenderTreeForChildren(shadowRoot, renderer.style(), renderTreePosition);

    popScope();

    shadowRoot.clearNeedsStyleRecalc();
    shadowRoot.clearChildNeedsStyleRecalc();
}

static PseudoElement* beforeOrAfterPseudoElement(Element& current, PseudoId pseudoId)
{
    ASSERT(pseudoId == BEFORE || pseudoId == AFTER);
    if (pseudoId == BEFORE)
        return current.beforePseudoElement();
    return current.afterPseudoElement();
}

static void setBeforeOrAfterPseudoElement(Element& current, Ref<PseudoElement>&& pseudoElement, PseudoId pseudoId)
{
    ASSERT(pseudoId == BEFORE || pseudoId == AFTER);
    if (pseudoId == BEFORE) {
        current.setBeforePseudoElement(WTFMove(pseudoElement));
        return;
    }
    current.setAfterPseudoElement(WTFMove(pseudoElement));
}

static void clearBeforeOrAfterPseudoElement(Element& current, PseudoId pseudoId)
{
    ASSERT(pseudoId == BEFORE || pseudoId == AFTER);
    if (pseudoId == BEFORE) {
        current.clearBeforePseudoElement();
        return;
    }
    current.clearAfterPseudoElement();
}

static void resetStyleForNonRenderedDescendants(Element& current)
{
    ASSERT(!current.renderer());
    bool elementNeedingStyleRecalcAffectsNextSiblingElementStyle = false;
    for (auto& child : childrenOfType<Element>(current)) {
        ASSERT(!child.renderer());
        if (elementNeedingStyleRecalcAffectsNextSiblingElementStyle) {
            if (child.styleIsAffectedByPreviousSibling())
                child.setNeedsStyleRecalc();
            elementNeedingStyleRecalcAffectsNextSiblingElementStyle = child.affectsNextSiblingElementStyle();
        }

        if (child.needsStyleRecalc()) {
            child.resetComputedStyle();
            child.clearNeedsStyleRecalc();
            elementNeedingStyleRecalcAffectsNextSiblingElementStyle = child.affectsNextSiblingElementStyle();
        }

        if (child.childNeedsStyleRecalc()) {
            resetStyleForNonRenderedDescendants(child);
            child.clearChildNeedsStyleRecalc();
        }
    }
}

static bool needsPseudoElement(Element& current, PseudoId pseudoId)
{
    if (!current.renderer() || !current.renderer()->canHaveGeneratedChildren())
        return false;
    if (current.isPseudoElement())
        return false;
    if (!pseudoElementRendererIsNeeded(current.renderer()->getCachedPseudoStyle(pseudoId)))
        return false;
    return true;
}

void TreeResolver::createRenderTreeForBeforeOrAfterPseudoElement(Element& current, PseudoId pseudoId, RenderTreePosition& renderTreePosition)
{
    if (!needsPseudoElement(current, pseudoId))
        return;
    Ref<PseudoElement> pseudoElement = PseudoElement::create(current, pseudoId);
    InspectorInstrumentation::pseudoElementCreated(m_document.page(), pseudoElement.get());
    setBeforeOrAfterPseudoElement(current, pseudoElement.copyRef(), pseudoId);
    createRenderTreeRecursively(pseudoElement.get(), *current.renderStyle(), renderTreePosition, nullptr);
}

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
void TreeResolver::createRenderTreeForSlotAssignees(HTMLSlotElement& slot, RenderStyle& inheritedStyle, RenderTreePosition& renderTreePosition)
{
    if (auto* assignedNodes = slot.assignedNodes()) {
        pushEnclosingScope();
        for (auto* child : *assignedNodes) {
            if (is<Text>(*child))
                attachTextRenderer(downcast<Text>(*child), renderTreePosition);
            else if (is<Element>(*child))
                createRenderTreeRecursively(downcast<Element>(*child), inheritedStyle, renderTreePosition, nullptr);
        }
        popScope();
    } else {
        SelectorFilterPusher selectorFilterPusher(scope().selectorFilter, slot);
        createRenderTreeForChildren(slot, inheritedStyle, renderTreePosition);
    }

    slot.clearNeedsStyleRecalc();
    slot.clearChildNeedsStyleRecalc();
}
#endif

void TreeResolver::createRenderTreeRecursively(Element& current, RenderStyle& inheritedStyle, RenderTreePosition& renderTreePosition, RefPtr<RenderStyle>&& resolvedStyle)
{
    PostResolutionCallbackDisabler callbackDisabler(m_document);
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    if (is<HTMLSlotElement>(current)) {
        createRenderTreeForSlotAssignees(downcast<HTMLSlotElement>(current), inheritedStyle, renderTreePosition);
        return;
    }
#endif

    if (current.hasCustomStyleResolveCallbacks())
        current.willAttachRenderers();

    createRenderer(current, inheritedStyle, renderTreePosition, WTFMove(resolvedStyle));

    if (auto* renderer = current.renderer()) {
        SelectorFilterPusher selectorFilterPusher(scope().selectorFilter, current, SelectorFilterPusher::NoPush);

        RenderTreePosition childRenderTreePosition(*renderer);
        createRenderTreeForBeforeOrAfterPseudoElement(current, BEFORE, childRenderTreePosition);

        auto* shadowRoot = current.shadowRoot();
        if (shadowRoot) {
            selectorFilterPusher.push();
            createRenderTreeForShadowRoot(*shadowRoot);
        } else if (current.firstChild())
            selectorFilterPusher.push();

        bool skipChildren = shadowRoot;
        if (!skipChildren)
            createRenderTreeForChildren(current, renderer->style(), childRenderTreePosition);

        if (AXObjectCache* cache = m_document.axObjectCache())
            cache->updateCacheAfterNodeIsAttached(&current);

        createRenderTreeForBeforeOrAfterPseudoElement(current, AFTER, childRenderTreePosition);

        current.updateFocusAppearanceAfterAttachIfNeeded();
    } else
        resetStyleForNonRenderedDescendants(current);

    current.clearNeedsStyleRecalc();
    current.clearChildNeedsStyleRecalc();

    if (current.hasCustomStyleResolveCallbacks())
        current.didAttachRenderers();
}

static void detachChildren(ContainerNode& current, DetachType detachType)
{
    for (Node* child = current.firstChild(); child; child = child->nextSibling()) {
        if (is<Text>(*child))
            detachTextRenderer(downcast<Text>(*child));
        else if (is<Element>(*child))
            detachRenderTree(downcast<Element>(*child), detachType);
    }
    current.clearChildNeedsStyleRecalc();
}

static void detachShadowRoot(ShadowRoot& shadowRoot, DetachType detachType)
{
    detachChildren(shadowRoot, detachType);
}

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
static void detachSlotAssignees(HTMLSlotElement& slot, DetachType detachType)
{
    ASSERT(!slot.renderer());
    if (auto* assignedNodes = slot.assignedNodes()) {
        for (auto* child : *assignedNodes) {
            if (is<Text>(*child))
                detachTextRenderer(downcast<Text>(*child));
            else if (is<Element>(*child))
                detachRenderTree(downcast<Element>(*child), detachType);
        }
    } else
        detachChildren(slot, detachType);

    slot.clearNeedsStyleRecalc();
    slot.clearChildNeedsStyleRecalc();
}
#endif

static void detachRenderTree(Element& current, DetachType detachType)
{
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    if (current.hasCustomStyleResolveCallbacks())
        current.willDetachRenderers();

    current.clearStyleDerivedDataBeforeDetachingRenderer();

    // Do not remove the element's hovered and active status
    // if performing a reattach.
    if (detachType != ReattachDetach)
        current.clearHoverAndActiveStatusBeforeDetachingRenderer();

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    if (is<HTMLSlotElement>(current))
        detachSlotAssignees(downcast<HTMLSlotElement>(current), detachType);
#endif
    else if (ShadowRoot* shadowRoot = current.shadowRoot())
        detachShadowRoot(*shadowRoot, detachType);

    detachChildren(current, detachType);

    if (current.renderer())
        current.renderer()->destroyAndCleanupAnonymousWrappers();
    current.setRenderer(0);

    if (current.hasCustomStyleResolveCallbacks())
        current.didDetachRenderers();
}

static bool pseudoStyleCacheIsInvalid(RenderElement* renderer, RenderStyle* newStyle)
{
    const RenderStyle& currentStyle = renderer->style();

    const PseudoStyleCache* pseudoStyleCache = currentStyle.cachedPseudoStyles();
    if (!pseudoStyleCache)
        return false;

    for (auto& cache : *pseudoStyleCache) {
        RefPtr<RenderStyle> newPseudoStyle;
        PseudoId pseudoId = cache->styleType();
        if (pseudoId == FIRST_LINE || pseudoId == FIRST_LINE_INHERITED)
            newPseudoStyle = renderer->uncachedFirstLineStyle(newStyle);
        else
            newPseudoStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(pseudoId), newStyle, newStyle);
        if (!newPseudoStyle)
            return true;
        if (*newPseudoStyle != *cache) {
            if (pseudoId < FIRST_INTERNAL_PSEUDOID)
                newStyle->setHasPseudoStyle(pseudoId);
            newStyle->addCachedPseudoStyle(newPseudoStyle);
            if (pseudoId == FIRST_LINE || pseudoId == FIRST_LINE_INHERITED) {
                // FIXME: We should do an actual diff to determine whether a repaint vs. layout
                // is needed, but for now just assume a layout will be required. The diff code
                // in RenderObject::setStyle would need to be factored out so that it could be reused.
                renderer->setNeedsLayoutAndPrefWidthsRecalc();
            }
            return true;
        }
    }
    return false;
}

Change TreeResolver::resolveLocally(Element& current, RenderStyle& inheritedStyle, RenderTreePosition& renderTreePosition, Change inheritedChange)
{
    Change localChange = Detach;
    RefPtr<RenderStyle> newStyle;
    RefPtr<RenderStyle> currentStyle = current.renderStyle();

    if (currentStyle && current.styleChangeType() != ReconstructRenderTree) {
        Ref<RenderStyle> style(styleForElement(current, inheritedStyle));
        newStyle = style.ptr();
        localChange = determineChange(*currentStyle, style);
    }
    if (localChange == Detach) {
        if (current.renderer() || current.isNamedFlowContentNode())
            detachRenderTree(current, ReattachDetach);
        createRenderTreeRecursively(current, inheritedStyle, renderTreePosition, newStyle.release());
        invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(current);

        return Detach;
    }

    if (RenderElement* renderer = current.renderer()) {
        if (localChange != NoChange || pseudoStyleCacheIsInvalid(renderer, newStyle.get()) || (inheritedChange == Force && renderer->requiresForcedStyleRecalcPropagation()) || current.styleChangeType() == SyntheticStyleChange)
            renderer->setAnimatableStyle(*newStyle, current.styleChangeType() == SyntheticStyleChange ? StyleDifferenceRecompositeLayer : StyleDifferenceEqual);
        else if (current.needsStyleRecalc()) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.
            renderer->setStyleInternal(*newStyle);
        }
    }

    // If "rem" units are used anywhere in the document, and if the document element's font size changes, then force font updating
    // all the way down the tree. This is simpler than having to maintain a cache of objects (and such font size changes should be rare anyway).
    if (m_document.authorStyleSheets().usesRemUnits() && m_document.documentElement() == &current && localChange != NoChange && currentStyle && newStyle && currentStyle->fontSize() != newStyle->fontSize()) {
        // Cached RenderStyles may depend on the re units.
        scope().styleResolver.invalidateMatchedPropertiesCache();
        return Force;
    }
    if (inheritedChange == Force)
        return Force;
    if (current.styleChangeType() >= FullStyleChange)
        return Force;

    return localChange;
}

void resolveTextNode(Text& text, RenderTreePosition& renderTreePosition)
{
    text.clearNeedsStyleRecalc();

    bool hasRenderer = text.renderer();
    bool needsRenderer = textRendererIsNeeded(text, renderTreePosition);
    if (hasRenderer) {
        if (needsRenderer)
            return;
        detachTextRenderer(text);
        invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(text);
        return;
    }
    if (!needsRenderer)
        return;
    attachTextRenderer(text, renderTreePosition);
    invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(text);
}

void TreeResolver::resolveChildAtShadowBoundary(Node& child, RenderStyle& inheritedStyle, RenderTreePosition& renderTreePosition, Style::Change change)
{
    if (auto* renderer = child.renderer())
        renderTreePosition.invalidateNextSibling(*renderer);

    if (is<Text>(child) && child.needsStyleRecalc()) {
        resolveTextNode(downcast<Text>(child), renderTreePosition);
        return;
    }
    if (is<Element>(child))
        resolveRecursively(downcast<Element>(child), inheritedStyle, renderTreePosition, change);
}

void TreeResolver::resolveShadowTree(Style::Change change, RenderStyle& inheritedStyle)
{
    ASSERT(scope().shadowRoot);
    auto& host = *scope().shadowRoot->host();
    ASSERT(host.renderer());
    if (scope().shadowRoot->styleChangeType() >= FullStyleChange)
        change = Force;
    RenderTreePosition renderTreePosition(*host.renderer());
    for (auto* child = scope().shadowRoot->firstChild(); child; child = child->nextSibling())
        resolveChildAtShadowBoundary(*child, inheritedStyle, renderTreePosition, change);

    scope().shadowRoot->clearNeedsStyleRecalc();
    scope().shadowRoot->clearChildNeedsStyleRecalc();
}

void TreeResolver::resolveBeforeOrAfterPseudoElement(Element& current, Change change, PseudoId pseudoId, RenderTreePosition& renderTreePosition)
{
    ASSERT(current.renderer());
    if (PseudoElement* existingPseudoElement = beforeOrAfterPseudoElement(current, pseudoId)) {
        if (existingPseudoElement->renderer())
            renderTreePosition.invalidateNextSibling(*existingPseudoElement->renderer());

        if (needsPseudoElement(current, pseudoId))
            resolveRecursively(*existingPseudoElement, current.renderer()->style(), renderTreePosition, current.needsStyleRecalc() ? Force : change);
        else
            clearBeforeOrAfterPseudoElement(current, pseudoId);
        return;
    }
    createRenderTreeForBeforeOrAfterPseudoElement(current, pseudoId, renderTreePosition);
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
        , m_previousImplicitVisibility(WKObservingContentChanges() && WKContentChange() != WKContentVisibilityChange ? elementImplicitVisibility(element) : VISIBLE)
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

void TreeResolver::resolveChildren(Element& current, RenderStyle& inheritedStyle, Change change, RenderTreePosition& childRenderTreePosition)
{
    SelectorFilterPusher selectorFilterPusher(scope().selectorFilter, current, SelectorFilterPusher::NoPush);

    bool elementNeedingStyleRecalcAffectsNextSiblingElementStyle = false;
    for (Node* child = current.firstChild(); child; child = child->nextSibling()) {
        if (RenderObject* childRenderer = child->renderer())
            childRenderTreePosition.invalidateNextSibling(*childRenderer);
        if (is<Text>(*child) && child->needsStyleRecalc()) {
            resolveTextNode(downcast<Text>(*child), childRenderTreePosition);
            continue;
        }
        if (!is<Element>(*child))
            continue;

        Element& childElement = downcast<Element>(*child);
        if (elementNeedingStyleRecalcAffectsNextSiblingElementStyle) {
            if (childElement.styleIsAffectedByPreviousSibling())
                childElement.setNeedsStyleRecalc();
            elementNeedingStyleRecalcAffectsNextSiblingElementStyle = childElement.affectsNextSiblingElementStyle();
        } else if (childElement.needsStyleRecalc())
            elementNeedingStyleRecalcAffectsNextSiblingElementStyle = childElement.affectsNextSiblingElementStyle();
        if (change >= Inherit || childElement.childNeedsStyleRecalc() || childElement.needsStyleRecalc()) {
            selectorFilterPusher.push();
            resolveRecursively(childElement, inheritedStyle, childRenderTreePosition, change);
        }
    }
}

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
void TreeResolver::resolveSlotAssignees(HTMLSlotElement& slot, RenderStyle& inheritedStyle, RenderTreePosition& renderTreePosition, Change change)
{
    if (auto* assignedNodes = slot.assignedNodes()) {
        pushEnclosingScope();
        for (auto* child : *assignedNodes)
            resolveChildAtShadowBoundary(*child, inheritedStyle, renderTreePosition, change);
        popScope();
    } else
        resolveChildren(slot, inheritedStyle, change, renderTreePosition);

    slot.clearNeedsStyleRecalc();
    slot.clearChildNeedsStyleRecalc();
}
#endif

void TreeResolver::resolveRecursively(Element& current, RenderStyle& inheritedStyle, RenderTreePosition& renderTreePosition, Change change)
{
    ASSERT(change != Detach);

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    if (is<HTMLSlotElement>(current)) {
        resolveSlotAssignees(downcast<HTMLSlotElement>(current), inheritedStyle, renderTreePosition, change);
        return;
    }
#endif

    if (current.hasCustomStyleResolveCallbacks()) {
        if (!current.willRecalcStyle(change))
            return;
    }

#if PLATFORM(IOS)
    CheckForVisibilityChangeOnRecalcStyle checkForVisibilityChange(&current, current.renderStyle());
#endif

    if (change > NoChange || current.needsStyleRecalc())
        current.resetComputedStyle();

    if (change >= Inherit || current.needsStyleRecalc())
        change = resolveLocally(current, inheritedStyle, renderTreePosition, change);

    auto* renderer = current.renderer();

    if (change != Detach && renderer) {
        auto* shadowRoot = current.shadowRoot();
        if (shadowRoot && (change >= Inherit || shadowRoot->childNeedsStyleRecalc() || shadowRoot->needsStyleRecalc())) {
            SelectorFilterPusher selectorFilterPusher(scope().selectorFilter, current);

            pushScope(*shadowRoot);
            resolveShadowTree(change, renderer->style());
            popScope();
        }

        RenderTreePosition childRenderTreePosition(*renderer);
        resolveBeforeOrAfterPseudoElement(current, change, BEFORE, childRenderTreePosition);

        bool skipChildren = shadowRoot;
        if (!skipChildren)
            resolveChildren(current, renderer->style(), change, childRenderTreePosition);

        resolveBeforeOrAfterPseudoElement(current, change, AFTER, childRenderTreePosition);
    }
    if (change != Detach && !renderer)
        resetStyleForNonRenderedDescendants(current);

    current.clearNeedsStyleRecalc();
    current.clearChildNeedsStyleRecalc();
    
    if (current.hasCustomStyleResolveCallbacks())
        current.didRecalcStyle(change);
}

void TreeResolver::resolve(Change change)
{
    ASSERT(!scope().shadowRoot);

    auto& renderView = *m_document.renderView();

    Element* documentElement = m_document.documentElement();
    if (!documentElement)
        return;
    if (change != Force && !documentElement->childNeedsStyleRecalc() && !documentElement->needsStyleRecalc())
        return;

    // Pseudo element removal and similar may only work with these flags still set. Reset them after the style recalc.
    renderView.setUsesFirstLineRules(renderView.usesFirstLineRules() || scope().styleResolver.usesFirstLineRules());
    renderView.setUsesFirstLetterRules(renderView.usesFirstLetterRules() || scope().styleResolver.usesFirstLetterRules());

    RenderTreePosition renderTreePosition(renderView);
    resolveRecursively(*documentElement, *m_document.renderStyle(), renderTreePosition, change);

    renderView.setUsesFirstLineRules(scope().styleResolver.usesFirstLineRules());
    renderView.setUsesFirstLetterRules(scope().styleResolver.usesFirstLetterRules());
}

void detachRenderTree(Element& element)
{
    detachRenderTree(element, NormalDetach);
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
