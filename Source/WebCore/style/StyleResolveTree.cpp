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
#include "StyleResolveTree.h"

#include "AXObjectCache.h"
#include "AnimationController.h"
#include "CSSFontSelector.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "FlowThreadController.h"
#include "InsertionPoint.h"
#include "LoaderStrategy.h"
#include "NodeRenderStyle.h"
#include "NodeRenderingTraversal.h"
#include "NodeTraversal.h"
#include "PlatformStrategies.h"
#include "RenderFullScreen.h"
#include "RenderNamedFlowThread.h"
#include "RenderText.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResourceLoadScheduler.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleResolveForDocument.h"
#include "StyleResolver.h"
#include "Text.h"

#if PLATFORM(IOS)
#include "CSSFontSelector.h"
#include "WKContentObservation.h"
#endif

namespace WebCore {

namespace Style {

enum DetachType { NormalDetach, ReattachDetach };

class RenderTreePosition {
public:
    RenderTreePosition(RenderElement* parent);
    RenderTreePosition(RenderElement* parent, RenderObject* nextSibling);

    RenderElement* parent() { return m_parent; }

    void insert(RenderObject&);
    bool canInsert(RenderElement&) const;
    bool canInsert(RenderText&) const;

    void computeNextSibling(const Node&, ContainerNode& renderingParentNode);
    void invalidateNextSibling(const RenderObject&);

private:
    RenderElement* m_parent;
    RenderObject* m_nextSibling;
    bool m_hasValidNextSibling;
#if !ASSERT_DISABLED
    unsigned m_assertionLimitCounter;
#endif
};

static void attachRenderTree(Element&, ContainerNode& renderingParentNode, RenderTreePosition&, PassRefPtr<RenderStyle>);
static void attachTextRenderer(Text&, ContainerNode& renderingParentNode, RenderTreePosition&);
static void detachRenderTree(Element&, DetachType);
static void resolveTextNode(Text&, ContainerNode& renderingParentNode, RenderTreePosition&);
static void resolveTree(Element&, ContainerNode& renderingParentNode, RenderTreePosition&, Change);

Change determineChange(const RenderStyle* s1, const RenderStyle* s2)
{
    if (!s1 || !s2)
        return Detach;
    if (s1->display() != s2->display())
        return Detach;
    if (s1->hasPseudoStyle(FIRST_LETTER) != s2->hasPseudoStyle(FIRST_LETTER))
        return Detach;
    // We just detach if a renderer acquires or loses a column-span, since spanning elements
    // typically won't contain much content.
    if (s1->columnSpan() != s2->columnSpan())
        return Detach;
    if (!s1->contentDataEquivalent(s2))
        return Detach;
    // When text-combine property has been changed, we need to prepare a separate renderer object.
    // When text-combine is on, we use RenderCombineText, otherwise RenderText.
    // https://bugs.webkit.org/show_bug.cgi?id=55069
    if (s1->hasTextCombine() != s2->hasTextCombine())
        return Detach;
    // We need to reattach the node, so that it is moved to the correct RenderFlowThread.
    if (s1->flowThread() != s2->flowThread())
        return Detach;
    // When the region thread has changed, we need to prepare a separate render region object.
    if (s1->regionThread() != s2->regionThread())
        return Detach;

    if (*s1 != *s2) {
        if (s1->inheritedNotEqual(s2))
            return Inherit;
        if (s1->hasExplicitlyInheritedProperties() || s2->hasExplicitlyInheritedProperties())
            return Inherit;

        return NoInherit;
    }
    // If the pseudoStyles have changed, we want any StyleChange that is not NoChange
    // because setStyle will do the right thing with anything else.
    if (s1->hasAnyPublicPseudoStyles()) {
        for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < FIRST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
            if (s1->hasPseudoStyle(pseudoId)) {
                RenderStyle* ps2 = s2->getCachedPseudoStyle(pseudoId);
                if (!ps2)
                    return NoInherit;
                RenderStyle* ps1 = s1->getCachedPseudoStyle(pseudoId);
                if (!ps1 || *ps1 != *ps2)
                    return NoInherit;
            }
        }
    }

    return NoChange;
}

static bool isRendererReparented(const RenderObject* renderer)
{
    if (!renderer->node()->isElementNode())
        return false;
    if (!renderer->style().flowThread().isEmpty())
        return true;
    return false;
}

static RenderObject* nextSiblingRenderer(const Node& node, const ContainerNode& renderingParentNode)
{
    if (!renderingParentNode.isElementNode())
        return nullptr;
    const Element& renderingParentElement = toElement(renderingParentNode);
    // Avoid an O(N^2) problem with this function by not checking for
    // nextRenderer() when the parent element hasn't attached yet.
    // FIXME: Why would we get here anyway if parent is not attached?
    if (!renderingParentElement.renderer())
        return nullptr;
    if (node.isAfterPseudoElement())
        return nullptr;
    Node* sibling = node.isBeforePseudoElement() ? NodeRenderingTraversal::firstChild(&renderingParentNode) : NodeRenderingTraversal::nextSibling(&node);
    for (; sibling; sibling = NodeRenderingTraversal::nextSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }
    if (PseudoElement* after = renderingParentElement.afterPseudoElement())
        return after->renderer();
    return nullptr;
}

RenderTreePosition::RenderTreePosition(RenderElement* parent)
    : m_parent(parent)
    , m_nextSibling(nullptr)
    , m_hasValidNextSibling(false)
#if !ASSERT_DISABLED
    , m_assertionLimitCounter(0)
#endif
{
}

RenderTreePosition::RenderTreePosition(RenderElement* parent, RenderObject* nextSibling)
    : m_parent(parent)
    , m_nextSibling(nextSibling)
    , m_hasValidNextSibling(true)
#if !ASSERT_DISABLED
    , m_assertionLimitCounter(0)
#endif
{
}

bool RenderTreePosition::canInsert(RenderElement& renderer) const
{
    ASSERT(m_parent);
    ASSERT(!renderer.parent());
    return m_parent->isChildAllowed(renderer, renderer.style());
}

bool RenderTreePosition::canInsert(RenderText& renderer) const
{
    ASSERT(m_parent);
    ASSERT(!renderer.parent());
    return m_parent->isChildAllowed(renderer, m_parent->style());
}

void RenderTreePosition::insert(RenderObject& renderer)
{
    ASSERT(m_parent);
    ASSERT(m_hasValidNextSibling);
    m_parent->addChild(&renderer, m_nextSibling);
}

void RenderTreePosition::computeNextSibling(const Node& node, ContainerNode& renderingParentNode)
{
    ASSERT(!node.renderer());
    if (m_hasValidNextSibling) {
        // Stop validating at some point so the assert doesn't make us O(N^2) on debug builds.
        ASSERT(++m_assertionLimitCounter > 20 || nextSiblingRenderer(node, renderingParentNode) == m_nextSibling);
        return;
    }
    m_nextSibling = nextSiblingRenderer(node, renderingParentNode);
    m_hasValidNextSibling = true;
}

void RenderTreePosition::invalidateNextSibling(const RenderObject& siblingRenderer)
{
    if (!m_hasValidNextSibling)
        return;
    if (m_nextSibling == &siblingRenderer)
        m_hasValidNextSibling = false;
}

static bool shouldCreateRenderer(const Element& element, const ContainerNode& renderingParent)
{
    if (!element.document().shouldCreateRenderers())
        return false;
    RenderObject* parentRenderer = renderingParent.renderer();
    if (!parentRenderer)
        return false;
    if (!parentRenderer->canHaveChildren() && !(element.isPseudoElement() && parentRenderer->canHaveGeneratedChildren()))
        return false;
    if (!renderingParent.childShouldCreateRenderer(element))
        return false;
    return true;
}

static PassRef<RenderStyle> styleForElement(Element& element, ContainerNode& renderingParentNode)
{
    RenderStyle* parentStyle = renderingParentNode.renderStyle();
    if (element.hasCustomStyleResolveCallbacks() && parentStyle) {
        if (RefPtr<RenderStyle> style = element.customStyleForRenderer(*parentStyle))
            return style.releaseNonNull();
    }
    return element.document().ensureStyleResolver().styleForElement(&element, parentStyle);
}

// Check the specific case of elements that are children of regions but are flowed into a flow thread themselves.
static bool elementInsideRegionNeedsRenderer(Element& element, ContainerNode& renderingParentNode, RefPtr<RenderStyle>& style)
{
#if ENABLE(CSS_REGIONS)
    // The parent of a region should always be an element.
    const RenderElement* parentRenderer = renderingParentNode.renderer();

    bool parentIsRegion = parentRenderer && !parentRenderer->canHaveChildren() && parentRenderer->isRenderNamedFlowFragmentContainer();
    bool parentIsNonRenderedInsideRegion = !parentRenderer && element.parentElement() && element.parentElement()->isInsideRegion();
    if (!parentIsRegion && !parentIsNonRenderedInsideRegion)
        return false;

    if (!style)
        style = styleForElement(element, renderingParentNode);

    // Children of this element will only be allowed to be flowed into other flow-threads if display is NOT none.
    if (element.rendererIsNeeded(*style))
        element.setIsInsideRegion(true);

    if (element.shouldMoveToFlowThread(*style))
        return true;
#else
    UNUSED_PARAM(element);
    UNUSED_PARAM(renderingParentNode);
    UNUSED_PARAM(style);
#endif
    return false;
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

static void createRendererIfNeeded(Element& element, ContainerNode& renderingParentNode, RenderTreePosition& renderTreePosition, PassRefPtr<RenderStyle> resolvedStyle)
{
    ASSERT(!element.renderer());

    RefPtr<RenderStyle> style = resolvedStyle;

    element.setIsInsideRegion(false);

    if (!shouldCreateRenderer(element, renderingParentNode) && !elementInsideRegionNeedsRenderer(element, renderingParentNode, style))
        return;

    if (!style)
        style = styleForElement(element, renderingParentNode);

    RenderNamedFlowThread* parentFlowRenderer = 0;
#if ENABLE(CSS_REGIONS)
    parentFlowRenderer = moveToFlowThreadIfNeeded(element, *style);
#endif

    if (!element.rendererIsNeeded(*style))
        return;

    renderTreePosition.computeNextSibling(element, renderingParentNode);

    RenderTreePosition insertionPosition = parentFlowRenderer
        ? RenderTreePosition(parentFlowRenderer, parentFlowRenderer->nextRendererForElement(element))
        : renderTreePosition;

    RenderElement* newRenderer = element.createElementRenderer(style.releaseNonNull()).leakPtr();
    if (!newRenderer)
        return;
    if (!insertionPosition.canInsert(*newRenderer)) {
        newRenderer->destroy();
        return;
    }

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(insertionPosition.parent()->flowThreadState());

    // Code below updateAnimations() can depend on Element::renderer() already being set.
    element.setRenderer(newRenderer);

    // FIXME: There's probably a better way to factor this.
    // This just does what setAnimatedStyle() does, except with setStyleInternal() instead of setStyle().
    newRenderer->setStyleInternal(newRenderer->animation().updateAnimations(*newRenderer, newRenderer->style()));

    newRenderer->initializeStyle();

#if ENABLE(FULLSCREEN_API)
    Document& document = element.document();
    if (document.webkitIsFullScreen() && document.webkitCurrentFullScreenElement() == &element) {
        newRenderer = RenderFullScreen::wrapRenderer(newRenderer, insertionPosition.parent(), document);
        if (!newRenderer)
            return;
    }
#endif
    // Note: Adding newRenderer instead of renderer(). renderer() may be a child of newRenderer.
    insertionPosition.insert(*newRenderer);
}

static RenderObject* previousSiblingRenderer(const Text& textNode)
{
    if (textNode.renderer())
        return textNode.renderer()->previousSibling();
    for (Node* sibling = NodeRenderingTraversal::previousSibling(&textNode); sibling; sibling = NodeRenderingTraversal::previousSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }
    if (PseudoElement* before = textNode.parentElement()->beforePseudoElement())
        return before->renderer();
    return nullptr;
}

static void invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(Node& current)
{
    if (isInsertionPoint(current))
        return;
    // This function finds sibling text renderers where the results of textRendererIsNeeded may have changed as a result of
    // the current node gaining or losing the renderer. This can only affect white space text nodes.
    for (Node* sibling = current.nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (sibling->needsStyleRecalc())
            return;
        if (sibling->isElementNode()) {
            // Text renderers beyond rendered elements can't be affected.
            if (!sibling->renderer() || isRendererReparented(sibling->renderer()))
                continue;
            return;
        }
        if (!sibling->isTextNode())
            continue;
        Text& textSibling = toText(*sibling);
        if (!textSibling.containsOnlyWhitespace())
            continue;
        textSibling.setNeedsStyleRecalc();
    }
}

static bool textRendererIsNeeded(const Text& textNode, ContainerNode& renderingParentNode)
{
    if (!renderingParentNode.renderer())
        return false;
    RenderElement& parentRenderer = *renderingParentNode.renderer();
    if (!parentRenderer.canHaveChildren())
        return false;
    if (!renderingParentNode.childShouldCreateRenderer(textNode))
        return false;

    if (textNode.isEditingText())
        return true;
    if (!textNode.length())
        return false;
    if (parentRenderer.style().display() == NONE)
        return false;
    if (!textNode.containsOnlyWhitespace())
        return true;
    // This text node has nothing but white space. We may still need a renderer in some cases.
    if (parentRenderer.isTable() || parentRenderer.isTableRow() || parentRenderer.isTableSection() || parentRenderer.isRenderTableCol() || parentRenderer.isFrameSet())
        return false;
    if (parentRenderer.style().preserveNewline()) // pre/pre-wrap/pre-line always make renderers.
        return true;

    RenderObject* previousRenderer = previousSiblingRenderer(textNode);
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
        RenderObject* nextRenderer = nextSiblingRenderer(textNode, *textNode.parentNode());
        if (!first || nextRenderer == first) {
            // Whitespace at the start of a block just goes away. Don't even make a render object for this text.
            return false;
        }
    }
    return true;
}

static void createTextRendererIfNeeded(Text& textNode, ContainerNode& renderingParentNode, RenderTreePosition& renderTreePosition)
{
    ASSERT(!textNode.renderer());

    if (!textRendererIsNeeded(textNode, renderingParentNode))
        return;

    auto newRenderer = textNode.createTextRenderer(*renderingParentNode.renderStyle());
    ASSERT(newRenderer);

    renderTreePosition.computeNextSibling(textNode, renderingParentNode);

    if (!renderTreePosition.canInsert(*newRenderer))
        return;

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(renderTreePosition.parent()->flowThreadState());

    textNode.setRenderer(newRenderer.get());
    // Parent takes care of the animations, no need to call setAnimatableStyle.
    renderTreePosition.insert(*newRenderer.leakPtr());
}

void attachTextRenderer(Text& textNode, ContainerNode& renderingParentNode, RenderTreePosition& renderTreePosition)
{
    createTextRendererIfNeeded(textNode, renderingParentNode, renderTreePosition);

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
    ContainerNode* renderingParentNode = NodeRenderingTraversal::parent(&textNode);
    if (!renderingParentNode)
        return;

    bool hadRenderer = textNode.renderer();

    RenderTreePosition renderTreePosition(renderingParentNode->renderer());
    resolveTextNode(textNode, *renderingParentNode, renderTreePosition);

    if (hadRenderer && textNode.renderer())
        textNode.renderer()->setTextWithOffset(textNode.dataImpl(), offsetOfReplacedData, lengthOfReplacedData);
}

static void attachChildren(ContainerNode& current, ContainerNode& renderingParentNode, RenderTreePosition& renderTreePosition)
{
    for (Node* child = current.firstChild(); child; child = child->nextSibling()) {
        ASSERT((!child->renderer() || child->inNamedFlow()) || current.shadowRoot());
        if (child->renderer()) {
            renderTreePosition.invalidateNextSibling(*child->renderer());
            continue;
        }
        if (child->isTextNode()) {
            attachTextRenderer(*toText(child), renderingParentNode, renderTreePosition);
            continue;
        }
        if (child->isElementNode())
            attachRenderTree(*toElement(child), renderingParentNode, renderTreePosition, nullptr);
    }
}

static void attachDistributedChildren(InsertionPoint& insertionPoint, ContainerNode& renderingParentNode, RenderTreePosition& renderTreePosition)
{
    if (ShadowRoot* shadowRoot = insertionPoint.containingShadowRoot())
        ContentDistributor::ensureDistribution(shadowRoot);

    for (Node* current = insertionPoint.firstDistributed(); current; current = insertionPoint.nextDistributedTo(current)) {
        if (current->renderer())
            renderTreePosition.invalidateNextSibling(*current->renderer());
        if (current->isTextNode()) {
            if (current->renderer())
                continue;
            attachTextRenderer(*toText(current), renderingParentNode, renderTreePosition);
            continue;
        }
        if (current->isElementNode()) {
            Element& currentElement = toElement(*current);
            if (currentElement.renderer())
                detachRenderTree(currentElement);
            attachRenderTree(currentElement, renderingParentNode, renderTreePosition, nullptr);
        }
    }
    // Use actual children as fallback content.
    if (!insertionPoint.hasDistribution())
        attachChildren(insertionPoint, renderingParentNode, renderTreePosition);
}

static void attachShadowRoot(ShadowRoot& shadowRoot)
{
    ASSERT(shadowRoot.hostElement());

    RenderTreePosition renderTreePosition(shadowRoot.hostElement()->renderer());
    attachChildren(shadowRoot, *shadowRoot.hostElement(), renderTreePosition);

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

static void setBeforeOrAfterPseudoElement(Element& current, PassRefPtr<PseudoElement> pseudoElement, PseudoId pseudoId)
{
    ASSERT(pseudoId == BEFORE || pseudoId == AFTER);
    if (pseudoId == BEFORE) {
        current.setBeforePseudoElement(pseudoElement);
        return;
    }
    current.setAfterPseudoElement(pseudoElement);
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

static bool needsPseudoElement(Element& current, PseudoId pseudoId)
{
    if (!current.document().styleSheetCollection().usesBeforeAfterRules())
        return false;
    if (!current.renderer() || !current.renderer()->canHaveGeneratedChildren())
        return false;
    if (current.isPseudoElement())
        return false;
    if (!pseudoElementRendererIsNeeded(current.renderer()->getCachedPseudoStyle(pseudoId)))
        return false;
    return true;
}

static void attachBeforeOrAfterPseudoElementIfNeeded(Element& current, PseudoId pseudoId, RenderTreePosition& renderTreePosition)
{
    if (!needsPseudoElement(current, pseudoId))
        return;
    RefPtr<PseudoElement> pseudoElement = PseudoElement::create(current, pseudoId);
    setBeforeOrAfterPseudoElement(current, pseudoElement, pseudoId);
    attachRenderTree(*pseudoElement, current, renderTreePosition, nullptr);
}

static void attachRenderTree(Element& current, ContainerNode& renderingParentNode, RenderTreePosition& renderTreePosition, PassRefPtr<RenderStyle> resolvedStyle)
{
    PostResolutionCallbackDisabler callbackDisabler(current.document());
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    if (current.hasCustomStyleResolveCallbacks())
        current.willAttachRenderers();

    createRendererIfNeeded(current, renderingParentNode, renderTreePosition, resolvedStyle);

    if (current.parentElement() && current.parentElement()->isInCanvasSubtree())
        current.setIsInCanvasSubtree(true);

    StyleResolverParentPusher parentPusher(&current);

    RenderTreePosition childRenderTreePosition(current.renderer());
    attachBeforeOrAfterPseudoElementIfNeeded(current, BEFORE, childRenderTreePosition);

    if (ShadowRoot* shadowRoot = current.shadowRoot()) {
        parentPusher.push();
        attachShadowRoot(*shadowRoot);
    } else if (current.firstChild())
        parentPusher.push();

    if (isInsertionPoint(current))
        attachDistributedChildren(toInsertionPoint(current), renderingParentNode, renderTreePosition);
    else
        attachChildren(current, current, childRenderTreePosition);

    current.clearNeedsStyleRecalc();
    current.clearChildNeedsStyleRecalc();

    if (AXObjectCache* cache = current.document().axObjectCache())
        cache->updateCacheAfterNodeIsAttached(&current);

    attachBeforeOrAfterPseudoElementIfNeeded(current, AFTER, childRenderTreePosition);

    current.updateFocusAppearanceAfterAttachIfNeeded();
    
    if (current.hasCustomStyleResolveCallbacks())
        current.didAttachRenderers();
}

static void detachDistributedChildren(InsertionPoint& insertionPoint)
{
    for (Node* current = insertionPoint.firstDistributed(); current; current = insertionPoint.nextDistributedTo(current)) {
        if (current->isTextNode()) {
            detachTextRenderer(*toText(current));
            continue;
        }
        if (current->isElementNode())
            detachRenderTree(*toElement(current));
    }
}

static void detachChildren(ContainerNode& current, DetachType detachType)
{
    if (isInsertionPoint(current))
        detachDistributedChildren(toInsertionPoint(current));

    for (Node* child = current.firstChild(); child; child = child->nextSibling()) {
        if (child->isTextNode()) {
            Style::detachTextRenderer(*toText(child));
            continue;
        }
        if (child->isElementNode())
            detachRenderTree(*toElement(child), detachType);
    }
    current.clearChildNeedsStyleRecalc();
}

static void detachShadowRoot(ShadowRoot& shadowRoot, DetachType detachType)
{
    detachChildren(shadowRoot, detachType);
}

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

    if (ShadowRoot* shadowRoot = current.shadowRoot())
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

    size_t cacheSize = pseudoStyleCache->size();
    for (size_t i = 0; i < cacheSize; ++i) {
        RefPtr<RenderStyle> newPseudoStyle;
        PseudoId pseudoId = pseudoStyleCache->at(i)->styleType();
        if (pseudoId == FIRST_LINE || pseudoId == FIRST_LINE_INHERITED)
            newPseudoStyle = renderer->uncachedFirstLineStyle(newStyle);
        else
            newPseudoStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(pseudoId), newStyle, newStyle);
        if (!newPseudoStyle)
            return true;
        if (*newPseudoStyle != *pseudoStyleCache->at(i)) {
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

static Change resolveLocal(Element& current, ContainerNode& renderingParentNode, RenderTreePosition& renderTreePosition, Change inheritedChange)
{
    Change localChange = Detach;
    RefPtr<RenderStyle> newStyle;
    RefPtr<RenderStyle> currentStyle = current.renderStyle();

    Document& document = current.document();
    if (currentStyle && current.styleChangeType() != ReconstructRenderTree) {
        newStyle = styleForElement(current, renderingParentNode);
        localChange = determineChange(currentStyle.get(), newStyle.get());
    }
    if (localChange == Detach) {
        if (current.renderer() || current.inNamedFlow())
            detachRenderTree(current, ReattachDetach);
        attachRenderTree(current, renderingParentNode, renderTreePosition, newStyle.release());
        invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(current);

        return Detach;
    }

    if (RenderElement* renderer = current.renderer()) {
        if (localChange != NoChange || pseudoStyleCacheIsInvalid(renderer, newStyle.get()) || (inheritedChange == Force && renderer->requiresForcedStyleRecalcPropagation()) || current.styleChangeType() == SyntheticStyleChange)
            renderer->setAnimatableStyle(*newStyle);
        else if (current.needsStyleRecalc()) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.
            renderer->setStyleInternal(*newStyle);
        }
    }

    // If "rem" units are used anywhere in the document, and if the document element's font size changes, then go ahead and force font updating
    // all the way down the tree. This is simpler than having to maintain a cache of objects (and such font size changes should be rare anyway).
    if (document.styleSheetCollection().usesRemUnits() && document.documentElement() == &current && localChange != NoChange && currentStyle && newStyle && currentStyle->fontSize() != newStyle->fontSize()) {
        // Cached RenderStyles may depend on the re units.
        if (StyleResolver* styleResolver = document.styleResolverIfExists())
            styleResolver->invalidateMatchedPropertiesCache();
        return Force;
    }
    if (inheritedChange == Force)
        return Force;
    if (current.styleChangeType() >= FullStyleChange)
        return Force;

    return localChange;
}

void resolveTextNode(Text& text, ContainerNode& renderingParentNode, RenderTreePosition& renderTreePosition)
{
    text.clearNeedsStyleRecalc();

    bool hasRenderer = text.renderer();
    bool needsRenderer = textRendererIsNeeded(text, renderingParentNode);
    if (hasRenderer) {
        if (needsRenderer)
            return;
        detachTextRenderer(text);
        invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(text);
        return;
    }
    if (!needsRenderer)
        return;
    attachTextRenderer(text, renderingParentNode, renderTreePosition);
    invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(text);
}

static void resolveShadowTree(ShadowRoot& shadowRoot, Element& host, Style::Change change)
{
    ASSERT(shadowRoot.hostElement() == &host);
    RenderTreePosition renderTreePosition(host.renderer());
    for (Node* child = shadowRoot.firstChild(); child; child = child->nextSibling()) {
        if (child->renderer())
            renderTreePosition.invalidateNextSibling(*child->renderer());
        if (child->isTextNode() && child->needsStyleRecalc()) {
            resolveTextNode(*toText(child), host, renderTreePosition);
            continue;
        }
        if (child->isElementNode())
            resolveTree(*toElement(child), host, renderTreePosition, change);
    }

    shadowRoot.clearNeedsStyleRecalc();
    shadowRoot.clearChildNeedsStyleRecalc();
}

static void updateBeforeOrAfterPseudoElement(Element& current, Change change, PseudoId pseudoId, RenderTreePosition& renderTreePosition)
{
    if (PseudoElement* existingPseudoElement = beforeOrAfterPseudoElement(current, pseudoId)) {
        if (needsPseudoElement(current, pseudoId))
            resolveTree(*existingPseudoElement, current, renderTreePosition, current.needsStyleRecalc() ? Force : change);
        else
            clearBeforeOrAfterPseudoElement(current, pseudoId);
        return;
    }
    attachBeforeOrAfterPseudoElementIfNeeded(current, pseudoId, renderTreePosition);
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

void resolveTree(Element& current, ContainerNode& renderingParentNode, RenderTreePosition& renderTreePosition, Change change)
{
    ASSERT(change != Detach);

    if (current.hasCustomStyleResolveCallbacks()) {
        if (!current.willRecalcStyle(change))
            return;
    }

    bool hasParentStyle = renderingParentNode.renderStyle();
    bool hasDirectAdjacentRules = current.childrenAffectedByDirectAdjacentRules();
    bool hasIndirectAdjacentRules = current.childrenAffectedByForwardPositionalRules();

#if PLATFORM(IOS)
    CheckForVisibilityChangeOnRecalcStyle checkForVisibilityChange(&current, current.renderStyle());
#endif

    if (change > NoChange || current.needsStyleRecalc())
        current.resetComputedStyle();

    if (hasParentStyle && (change >= Inherit || current.needsStyleRecalc()))
        change = resolveLocal(current, renderingParentNode, renderTreePosition, change);

    if (change != Detach) {
        StyleResolverParentPusher parentPusher(&current);

        if (ShadowRoot* shadowRoot = current.shadowRoot()) {
            if (change >= Inherit || shadowRoot->childNeedsStyleRecalc() || shadowRoot->needsStyleRecalc()) {
                parentPusher.push();
                resolveShadowTree(*shadowRoot, current, change);
            }
        }

        RenderTreePosition childRenderTreePosition(current.renderer());
        updateBeforeOrAfterPseudoElement(current, change, BEFORE, childRenderTreePosition);

        // FIXME: This check is good enough for :hover + foo, but it is not good enough for :hover + foo + bar.
        // For now we will just worry about the common case, since it's a lot trickier to get the second case right
        // without doing way too much re-resolution.
        bool forceCheckOfNextElementSibling = false;
        bool forceCheckOfAnyElementSibling = false;
        for (Node* child = current.firstChild(); child; child = child->nextSibling()) {
            if (child->renderer())
                childRenderTreePosition.invalidateNextSibling(*child->renderer());
            if (child->isTextNode() && child->needsStyleRecalc()) {
                resolveTextNode(*toText(child), current, childRenderTreePosition);
                continue;
            }
            if (!child->isElementNode())
                continue;
            Element* childElement = toElement(child);
            bool childRulesChanged = childElement->needsStyleRecalc() && childElement->styleChangeType() == FullStyleChange;
            if ((forceCheckOfNextElementSibling || forceCheckOfAnyElementSibling))
                childElement->setNeedsStyleRecalc();
            if (change >= Inherit || childElement->childNeedsStyleRecalc() || childElement->needsStyleRecalc()) {
                parentPusher.push();
                resolveTree(*childElement, current, childRenderTreePosition, change);
            }
            forceCheckOfNextElementSibling = childRulesChanged && hasDirectAdjacentRules;
            forceCheckOfAnyElementSibling = forceCheckOfAnyElementSibling || (childRulesChanged && hasIndirectAdjacentRules);
        }

        updateBeforeOrAfterPseudoElement(current, change, AFTER, childRenderTreePosition);
    }

    current.clearNeedsStyleRecalc();
    current.clearChildNeedsStyleRecalc();
    
    if (current.hasCustomStyleResolveCallbacks())
        current.didRecalcStyle(change);
}

void resolveTree(Document& document, Change change)
{
    if (change == Force) {
        auto documentStyle = resolveForDocument(document);

        // Inserting the pictograph font at the end of the font fallback list is done by the
        // font selector, so set a font selector if needed.
        if (Settings* settings = document.settings()) {
            StyleResolver* styleResolver = document.styleResolverIfExists();
            if (settings->fontFallbackPrefersPictographs() && styleResolver)
                documentStyle.get().font().update(styleResolver->fontSelector());
        }

        Style::Change documentChange = determineChange(&documentStyle.get(), &document.renderView()->style());
        if (documentChange != NoChange)
            document.renderView()->setStyle(std::move(documentStyle));
        else
            documentStyle.dropRef();
    }

    Element* documentElement = document.documentElement();
    if (!documentElement)
        return;
    if (change < Inherit && !documentElement->childNeedsStyleRecalc() && !documentElement->needsStyleRecalc())
        return;
    RenderTreePosition renderTreePosition(document.renderView());
    resolveTree(*documentElement, document, renderTreePosition, change);
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

    RefPtr<Document> protectedDocument = &document;
    postResolutionCallbackQueue().append([protectedDocument]{
        // FIXME: If the document becomes unassociated with the page during style resolution
        // then this won't work and the memory cache client calls will be permanently disabled.
        if (Page* page = protectedDocument->page())
            page->setMemoryCacheClientCallsEnabled(true);
    });
}

static unsigned resolutionNestingDepth;

PostResolutionCallbackDisabler::PostResolutionCallbackDisabler(Document& document)
{
    ++resolutionNestingDepth;

    if (resolutionNestingDepth == 1)
        platformStrategies()->loaderStrategy()->resourceLoadScheduler()->suspendPendingRequests();

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

        platformStrategies()->loaderStrategy()->resourceLoadScheduler()->resumePendingRequests();
    }

    --resolutionNestingDepth;
}

bool postResolutionCallbacksAreSuspended()
{
    return resolutionNestingDepth;
}

}
}
