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
    , renderTreePosition(*document.renderView())
    , change(change)
{
}

TreeResolver::Parent::Parent(Element& element, RenderStyle& style, RenderTreePosition renderTreePosition, Change change)
    : element(&element)
    , style(style)
    , renderTreePosition(renderTreePosition)
    , change(change)
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
        RenderStyle* shadowHostStyle = scope().shadowRoot ? scope().shadowRoot->host()->renderStyle() : nullptr;
        if (auto customStyle = element.resolveCustomStyle(inheritedStyle, shadowHostStyle)) {
            Style::commitRelationsToDocument(WTFMove(customStyle->relations));
            return WTFMove(customStyle->renderStyle);
        }
    }

    if (auto* sharingElement = scope().sharingResolver.resolve(element))
        return *sharingElement->renderStyle();

    auto elementStyle = scope().styleResolver.styleForElement(element, &inheritedStyle, MatchAllRules, nullptr, &scope().selectorFilter);

    Style::commitRelationsToDocument(WTFMove(elementStyle.relations));
    return WTFMove(elementStyle.renderStyle);
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

void TreeResolver::createRenderer(Element& element, RenderTreePosition& renderTreePosition, RefPtr<RenderStyle>&& resolvedStyle)
{
    ASSERT(shouldCreateRenderer(element, renderTreePosition.parent()));
    ASSERT(resolvedStyle);

    RenderNamedFlowThread* parentFlowRenderer = 0;
#if ENABLE(CSS_REGIONS)
    parentFlowRenderer = moveToFlowThreadIfNeeded(element, *resolvedStyle);
#endif

    if (!element.rendererIsNeeded(*resolvedStyle))
        return;

    renderTreePosition.computeNextSibling(element);

    RenderTreePosition insertionPosition = parentFlowRenderer
        ? RenderTreePosition(*parentFlowRenderer, parentFlowRenderer->nextRendererForElement(element))
        : renderTreePosition;

    RenderElement* newRenderer = element.createElementRenderer(resolvedStyle.releaseNonNull(), insertionPosition).leakPtr();
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
    // FIXME: This is not correct with shadow trees. This should be done with ComposedTreeIterator.
    ASSERT(!current.renderer());
    bool elementNeedingStyleRecalcAffectsNextSiblingElementStyle = false;
    for (auto& child : childrenOfType<Element>(current)) {
        ASSERT(!child.renderer());
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
    ASSERT(shouldCreateRenderer(slot, renderTreePosition.parent()));

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
    ASSERT(!current.renderer());

    PostResolutionCallbackDisabler callbackDisabler(m_document);
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    bool shouldCallCreateRenderer = shouldCreateRenderer(current, renderTreePosition.parent());

    RefPtr<RenderStyle> style = resolvedStyle;
    if (!style)
        style = styleForElement(current, inheritedStyle);

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    if (is<HTMLSlotElement>(current)) {
        if (shouldCallCreateRenderer && current.rendererIsNeeded(*style))
            createRenderTreeForSlotAssignees(downcast<HTMLSlotElement>(current), inheritedStyle, renderTreePosition);
        return;
    }
#endif

    if (current.hasCustomStyleResolveCallbacks())
        current.willAttachRenderers();

    if (shouldCallCreateRenderer)
        createRenderer(current, renderTreePosition, style.releaseNonNull());

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
    current.setRenderer(nullptr);

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

Change TreeResolver::resolveElement(Element& current)
{
    Change localChange = Detach;
    RefPtr<RenderStyle> newStyle;
    RefPtr<RenderStyle> currentStyle = current.renderStyle();

    if (currentStyle && current.styleChangeType() != ReconstructRenderTree) {
        Ref<RenderStyle> style(styleForElement(current, parent().style));
        newStyle = style.ptr();
        localChange = determineChange(*currentStyle, style);
    }
    if (localChange == Detach) {
        if (current.renderer() || current.isNamedFlowContentNode())
            detachRenderTree(current, ReattachDetach);
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
        else if (is<HTMLSlotElement>(current))
            detachRenderTree(current, ReattachDetach);
#endif
        createRenderTreeRecursively(current, parent().style, parent().renderTreePosition, newStyle.release());
        invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(current);

        return Detach;
    }

    if (RenderElement* renderer = current.renderer()) {
        if (localChange != NoChange || pseudoStyleCacheIsInvalid(renderer, newStyle.get()) || (parent().change == Force && renderer->requiresForcedStyleRecalcPropagation()) || current.styleChangeType() == SyntheticStyleChange)
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
    if (parent().change == Force || current.styleChangeType() >= FullStyleChange)
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

void TreeResolver::resolveBeforeOrAfterPseudoElement(Element& current, Change change, PseudoId pseudoId, RenderTreePosition& renderTreePosition)
{
    if (!current.renderer())
        return;
    PseudoElement* existingPseudoElement = beforeOrAfterPseudoElement(current, pseudoId);
    if (!existingPseudoElement) {
        createRenderTreeForBeforeOrAfterPseudoElement(current, pseudoId, renderTreePosition);
        return;
    }

    if (existingPseudoElement->renderer())
        renderTreePosition.invalidateNextSibling(*existingPseudoElement->renderer());

    if (change == NoChange && !existingPseudoElement->needsStyleRecalc())
        return;

    if (needsPseudoElement(current, pseudoId)) {
        auto change = resolveElement(*existingPseudoElement);
        existingPseudoElement->didRecalcStyle(change);
        existingPseudoElement->clearNeedsStyleRecalc();
    } else
        clearBeforeOrAfterPseudoElement(current, pseudoId);
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

void TreeResolver::pushParent(Element& element, RenderStyle& style, RenderTreePosition renderTreePosition, Change change)
{
    scope().selectorFilter.pushParent(&element);

    Parent parent(element, style, renderTreePosition, change);

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

    resolveBeforeOrAfterPseudoElement(element, change, BEFORE, renderTreePosition);
}

void TreeResolver::popParent()
{
    auto& parentElement = *parent().element;

    resolveBeforeOrAfterPseudoElement(parentElement, parent().change, AFTER, parent().renderTreePosition);

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

        if (auto* existingRenderer = node.renderer())
            parent.renderTreePosition.invalidateNextSibling(*existingRenderer);

        if (is<Text>(node)) {
            if (node.needsStyleRecalc())
                resolveTextNode(downcast<Text>(node), parent.renderTreePosition);
            it.traverseNextSkippingChildren();
            continue;
        }

        auto& element = downcast<Element>(node);

        // FIXME: We should deal with this during style invalidation.
        bool affectedByPreviousSibling = element.styleIsAffectedByPreviousSibling() && parent.elementNeedingStyleRecalcAffectsNextSiblingElementStyle;
        if (element.needsStyleRecalc() || parent.elementNeedingStyleRecalcAffectsNextSiblingElementStyle)
            parent.elementNeedingStyleRecalcAffectsNextSiblingElementStyle = element.affectsNextSiblingElementStyle();

        Change change = NoChange;

        bool shouldResolve = parent.change >= Inherit || element.needsStyleRecalc() || affectedByPreviousSibling;
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
            change = resolveElement(element);

            element.clearNeedsStyleRecalc();

            if (element.hasCustomStyleResolveCallbacks())
                element.didRecalcStyle(change);

            if (change == Detach) {
                it.traverseNextSkippingChildren();
                continue;
            }

            if (affectedByPreviousSibling)
                change = Force;
        }

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
        if (is<HTMLSlotElement>(element)) {
            // FIXME: We should compute style for the slot and use it as parent style.
            // FIXME: This should be display:contents check.
            // Duplicate the style and render tree position from the current context.
            pushParent(element, parent.style.get(), parent.renderTreePosition, change);
            it.traverseNext();
            continue;
        }
#endif
        auto* renderer = element.renderer();
        if (!renderer) {
            resetStyleForNonRenderedDescendants(element);
            element.clearChildNeedsStyleRecalc();
        }

        bool shouldIterateChildren = renderer && (element.childNeedsStyleRecalc() || change != NoChange);
        if (!shouldIterateChildren) {
            it.traverseNextSkippingChildren();
            continue;
        }

        pushParent(element, renderer->style(), RenderTreePosition(*renderer), change);

        it.traverseNext();
    }

    popParentsToDepth(1);
}

void TreeResolver::resolve(Change change)
{
    auto& renderView = *m_document.renderView();

    Element* documentElement = m_document.documentElement();
    if (!documentElement)
        return;
    if (change != Force && !documentElement->childNeedsStyleRecalc() && !documentElement->needsStyleRecalc())
        return;

    m_scopeStack.append(adoptRef(*new Scope(m_document)));

    // Pseudo element removal and similar may only work with these flags still set. Reset them after the style recalc.
    renderView.setUsesFirstLineRules(renderView.usesFirstLineRules() || scope().styleResolver.usesFirstLineRules());
    renderView.setUsesFirstLetterRules(renderView.usesFirstLetterRules() || scope().styleResolver.usesFirstLetterRules());

    m_parentStack.append(Parent(m_document, change));

    resolveComposedTree();

    renderView.setUsesFirstLineRules(scope().styleResolver.usesFirstLineRules());
    renderView.setUsesFirstLetterRules(scope().styleResolver.usesFirstLetterRules());

    m_parentStack.clear();
    m_scopeStack.clear();
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
