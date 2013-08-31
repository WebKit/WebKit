/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
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
#include "Element.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "ElementTraversal.h"
#include "FlowThreadController.h"
#include "NodeRenderStyle.h"
#include "NodeRenderingTraversal.h"
#include "NodeTraversal.h"
#include "RenderFullScreen.h"
#include "RenderNamedFlowThread.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "RenderWidget.h"
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

static void attachRenderTree(Element&, RenderStyle* resolvedStyle);
static void detachRenderTree(Element&, DetachType);

Change determineChange(const RenderStyle* s1, const RenderStyle* s2, Settings* settings)
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
    if (settings->regionBasedColumnsEnabled()) {
        bool specifiesColumns1 = !s1->hasAutoColumnCount() || !s1->hasAutoColumnWidth();
        bool specifiesColumns2 = !s2->hasAutoColumnCount() || !s2->hasAutoColumnWidth();
        if (specifiesColumns1 != specifiesColumns2)
            return Detach;
    }
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
    if (renderer->style() && !renderer->style()->flowThread().isEmpty())
        return true;
    return false;
}

static RenderObject* nextSiblingRenderer(const Element& element, const ContainerNode* renderingParentNode)
{
    // Avoid an O(N^2) problem with this function by not checking for
    // nextRenderer() when the parent element hasn't attached yet.
    // FIXME: Why would we get here anyway if parent is not attached?
    if (renderingParentNode && !renderingParentNode->attached())
        return 0;
    for (Node* sibling = NodeRenderingTraversal::nextSibling(&element); sibling; sibling = NodeRenderingTraversal::nextSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }
    return 0;
}

static bool shouldCreateRenderer(const Element& element, const ContainerNode* renderingParent)
{
    if (!element.document().shouldCreateRenderers())
        return false;
    if (!renderingParent)
        return false;
    RenderObject* parentRenderer = renderingParent->renderer();
    if (!parentRenderer)
        return false;
    if (!parentRenderer->canHaveChildren() && !(element.isPseudoElement() && parentRenderer->canHaveGeneratedChildren()))
        return false;
    if (!renderingParent->childShouldCreateRenderer(&element))
        return false;
    return true;
}

// Check the specific case of elements that are children of regions but are flowed into a flow thread themselves.
static bool elementInsideRegionNeedsRenderer(Element& element, const ContainerNode* renderingParentNode, RefPtr<RenderStyle>& style)
{
#if ENABLE(CSS_REGIONS)
    const RenderObject* parentRenderer = renderingParentNode ? renderingParentNode->renderer() : 0;

    bool parentIsRegion = parentRenderer && !parentRenderer->canHaveChildren() && parentRenderer->isRenderRegion();
    bool parentIsNonRenderedInsideRegion = !parentRenderer && element.parentElement() && element.parentElement()->isInsideRegion();
    if (!parentIsRegion && !parentIsNonRenderedInsideRegion)
        return false;

    if (!style)
        style = element.styleForRenderer();

    // Children of this element will only be allowed to be flowed into other flow-threads if display is NOT none.
    if (element.rendererIsNeeded(*style))
        element.setIsInsideRegion(true);

    if (element.shouldMoveToFlowThread(*style))
        return true;
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
    flowThreadController.registerNamedFlowContentNode(&element, &parentFlowRenderer);
    return &parentFlowRenderer;
}
#endif

static void createRendererIfNeeded(Element& element, RenderStyle* resolvedStyle)
{
    ASSERT(!element.renderer());

    Document& document = element.document();
    ContainerNode* renderingParentNode = NodeRenderingTraversal::parent(&element);

    RefPtr<RenderStyle> style = resolvedStyle;

    element.setIsInsideRegion(false);

    if (!shouldCreateRenderer(element, renderingParentNode) && !elementInsideRegionNeedsRenderer(element, renderingParentNode, style))
        return;

    if (!style)
        style = element.styleForRenderer();

    RenderNamedFlowThread* parentFlowRenderer = 0;
#if ENABLE(CSS_REGIONS)
    parentFlowRenderer = moveToFlowThreadIfNeeded(element, *style);
#endif

    if (!element.rendererIsNeeded(*style))
        return;

    RenderObject* parentRenderer;
    RenderObject* nextRenderer;
    if (parentFlowRenderer) {
        parentRenderer = parentFlowRenderer;
        nextRenderer = parentFlowRenderer->nextRendererForNode(&element);
    } else {
        parentRenderer = renderingParentNode->renderer();
        nextRenderer = nextSiblingRenderer(element, renderingParentNode);
    }

    RenderObject* newRenderer = element.createRenderer(document.renderArena(), style.get());
    if (!newRenderer)
        return;
    if (!parentRenderer->isChildAllowed(newRenderer, style.get())) {
        newRenderer->destroy();
        return;
    }

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(parentRenderer->flowThreadState());

    element.setRenderer(newRenderer);
    newRenderer->setAnimatableStyle(style.release()); // setAnimatableStyle() can depend on renderer() already being set.

#if ENABLE(FULLSCREEN_API)
    if (document.webkitIsFullScreen() && document.webkitCurrentFullScreenElement() == &element) {
        newRenderer = RenderFullScreen::wrapRenderer(newRenderer, parentRenderer, &document);
        if (!newRenderer)
            return;
    }
#endif
    // Note: Adding newRenderer instead of renderer(). renderer() may be a child of newRenderer.
    parentRenderer->addChild(newRenderer, nextRenderer);
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
    return 0;
}

static RenderObject* nextSiblingRenderer(const Text& textNode)
{
    if (textNode.renderer())
        return textNode.renderer()->nextSibling();
    for (Node* sibling = NodeRenderingTraversal::nextSibling(&textNode); sibling; sibling = NodeRenderingTraversal::nextSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }
    return 0;
}

static void createTextRenderersForSiblingsAfterAttachIfNeeded(Node* sibling)
{
    ASSERT(sibling->previousSibling());
    ASSERT(sibling->previousSibling()->renderer());
    ASSERT(!sibling->renderer());
    ASSERT(sibling->attached());
    // If this node got a renderer it may be the previousRenderer() of sibling text nodes and thus affect the
    // result of Text::textRendererIsNeeded() for those nodes.
    for (; sibling; sibling = sibling->nextSibling()) {
        if (sibling->renderer())
            break;
        if (!sibling->attached())
            break; // Assume this means none of the following siblings are attached.
        if (!sibling->isTextNode())
            continue;
        ASSERT(!sibling->renderer());
        attachTextRenderer(*toText(sibling));
        // If we again decided not to create a renderer for next, we can bail out the loop,
        // because it won't affect the result of Text::textRendererIsNeeded() for the rest
        // of sibling nodes.
        if (!sibling->renderer())
            break;
    }
}

static bool textRendererIsNeeded(const Text& textNode, const RenderObject& parentRenderer, const RenderStyle& style)
{
    if (textNode.isEditingText())
        return true;
    if (!textNode.length())
        return false;
    if (style.display() == NONE)
        return false;
    if (!textNode.containsOnlyWhitespace())
        return true;
    // This text node has nothing but white space. We may still need a renderer in some cases.
    if (parentRenderer.isTable() || parentRenderer.isTableRow() || parentRenderer.isTableSection() || parentRenderer.isRenderTableCol() || parentRenderer.isFrameSet())
        return false;
    if (style.preserveNewline()) // pre/pre-wrap/pre-line always make renderers.
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
        RenderObject* nextRenderer = nextSiblingRenderer(textNode);
        if (!first || nextRenderer == first) {
            // Whitespace at the start of a block just goes away. Don't even make a render object for this text.
            return false;
        }
    }
    return true;
}

static void createTextRendererIfNeeded(Text& textNode)
{
    ASSERT(!textNode.renderer());

    ContainerNode* renderingParentNode = NodeRenderingTraversal::parent(&textNode);
    if (!renderingParentNode)
        return;
    RenderObject* parentRenderer = renderingParentNode->renderer();
    if (!parentRenderer || !parentRenderer->canHaveChildren())
        return;
    if (!renderingParentNode->childShouldCreateRenderer(&textNode))
        return;

    Document& document = textNode.document();
    RefPtr<RenderStyle> style;
    bool resetStyleInheritance = textNode.parentNode()->isShadowRoot() && toShadowRoot(textNode.parentNode())->resetStyleInheritance();
    if (resetStyleInheritance)
        style = document.ensureStyleResolver().defaultStyleForElement();
    else
        style = parentRenderer->style();

    if (!textRendererIsNeeded(textNode, *parentRenderer, *style))
        return;
    RenderText* newRenderer = textNode.createTextRenderer(document.renderArena(), style.get());
    if (!newRenderer)
        return;
    if (!parentRenderer->isChildAllowed(newRenderer, style.get())) {
        newRenderer->destroy();
        return;
    }

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(parentRenderer->flowThreadState());

    RenderObject* nextRenderer = nextSiblingRenderer(textNode);
    textNode.setRenderer(newRenderer);
    // Parent takes care of the animations, no need to call setAnimatableStyle.
    newRenderer->setStyle(style.release());
    parentRenderer->addChild(newRenderer, nextRenderer);

    Node* sibling = textNode.nextSibling();
    if (sibling && !sibling->renderer() && sibling->attached())
        createTextRenderersForSiblingsAfterAttachIfNeeded(sibling);
}

void attachTextRenderer(Text& textNode)
{
    createTextRendererIfNeeded(textNode);

    textNode.setAttached(true);
    textNode.clearNeedsStyleRecalc();
}

void detachTextRenderer(Text& textNode)
{
    if (textNode.renderer())
        textNode.renderer()->destroyAndCleanupAnonymousWrappers();
    textNode.setRenderer(0);
    textNode.setAttached(false);
}

void updateTextRendererAfterContentChange(Text& textNode, unsigned offsetOfReplacedData, unsigned lengthOfReplacedData)
{
    if (!textNode.attached())
        return;
    RenderText* textRenderer = toRenderText(textNode.renderer());
    if (!textRenderer) {
        attachTextRenderer(textNode);
        return;
    }
    RenderObject* parentRenderer = NodeRenderingTraversal::parent(&textNode)->renderer();
    if (!textRendererIsNeeded(textNode, *parentRenderer, *textRenderer->style())) {
        detachTextRenderer(textNode);
        attachTextRenderer(textNode);
        return;
    }
    textRenderer->setTextWithOffset(textNode.dataImpl(), offsetOfReplacedData, lengthOfReplacedData);
}

#ifndef NDEBUG
static bool childAttachedAllowedWhenAttachingChildren(ContainerNode& node)
{
    if (node.isShadowRoot())
        return true;
    if (node.isInsertionPoint())
        return true;
    if (node.isElementNode() && toElement(&node)->shadowRoot())
        return true;
    return false;
}
#endif

static void attachChildren(ContainerNode& current)
{
    for (Node* child = current.firstChild(); child; child = child->nextSibling()) {
        ASSERT(!child->attached() || childAttachedAllowedWhenAttachingChildren(current));
        if (child->attached())
            continue;
        if (child->isTextNode()) {
            attachTextRenderer(*toText(child));
            continue;
        }
        if (child->isElementNode())
            attachRenderTree(*toElement(child), nullptr);
    }
}

static void attachShadowRoot(ShadowRoot& shadowRoot)
{
    if (shadowRoot.attached())
        return;
    StyleResolver& styleResolver = shadowRoot.document().ensureStyleResolver();
    styleResolver.pushParentShadowRoot(&shadowRoot);

    attachChildren(shadowRoot);

    styleResolver.popParentShadowRoot(&shadowRoot);

    shadowRoot.clearNeedsStyleRecalc();
    shadowRoot.setAttached(true);
}

static void attachRenderTree(Element& current, RenderStyle* resolvedStyle)
{
    PostAttachCallbackDisabler callbackDisabler(current);
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    if (current.hasCustomStyleResolveCallbacks())
        current.willAttachRenderers();

    createRendererIfNeeded(current, resolvedStyle);

    if (current.parentElement() && current.parentElement()->isInCanvasSubtree())
        current.setIsInCanvasSubtree(true);

    current.updateBeforePseudoElement(NoChange);

    StyleResolverParentPusher parentPusher(&current);

    // When a shadow root exists, it does the work of attaching the children.
    if (ShadowRoot* shadowRoot = current.shadowRoot()) {
        parentPusher.push();
        attachShadowRoot(*shadowRoot);
    } else if (current.firstChild())
        parentPusher.push();

    attachChildren(current);

    Node* sibling = current.nextSibling();
    if (current.renderer() && sibling && !sibling->renderer() && sibling->attached())
        createTextRenderersForSiblingsAfterAttachIfNeeded(sibling);

    current.setAttached(true);
    current.clearNeedsStyleRecalc();

    if (AXObjectCache* cache = current.document().axObjectCache())
        cache->updateCacheAfterNodeIsAttached(&current);

    current.updateAfterPseudoElement(NoChange);

    current.updateFocusAppearanceAfterAttachIfNeeded();
    
    if (current.hasCustomStyleResolveCallbacks())
        current.didAttachRenderers();
}

static void detachChildren(ContainerNode& current, DetachType detachType)
{
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
    if (!shadowRoot.attached())
        return;
    detachChildren(shadowRoot, detachType);

    shadowRoot.setAttached(false);
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

    current.setAttached(false);

    if (current.hasCustomStyleResolveCallbacks())
        current.didDetachRenderers();
}

static bool pseudoStyleCacheIsInvalid(RenderObject* renderer, RenderStyle* newStyle)
{
    const RenderStyle* currentStyle = renderer->style();

    const PseudoStyleCache* pseudoStyleCache = currentStyle->cachedPseudoStyles();
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

static Change resolveLocal(Element& current, Change inheritedChange)
{
    Change localChange = Detach;
    RefPtr<RenderStyle> newStyle;
    RefPtr<RenderStyle> currentStyle = current.renderStyle();

    Document& document = current.document();
    if (currentStyle) {
        newStyle = current.styleForRenderer();
        localChange = determineChange(currentStyle.get(), newStyle.get(), document.settings());
    }
    if (localChange == Detach) {
        if (current.attached())
            detachRenderTree(current, ReattachDetach);
        attachRenderTree(current, newStyle.get());
        return Detach;
    }

    if (RenderObject* renderer = current.renderer()) {
        if (localChange != NoChange || pseudoStyleCacheIsInvalid(renderer, newStyle.get()) || (inheritedChange == Force && renderer->requiresForcedStyleRecalcPropagation()) || current.styleChangeType() == SyntheticStyleChange)
            renderer->setAnimatableStyle(newStyle.get());
        else if (current.needsStyleRecalc()) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.
            renderer->setStyleInternal(newStyle.get());
        }
    }

    // If "rem" units are used anywhere in the document, and if the document element's font size changes, then go ahead and force font updating
    // all the way down the tree. This is simpler than having to maintain a cache of objects (and such font size changes should be rare anyway).
    if (document.styleSheetCollection()->usesRemUnits() && document.documentElement() == &current && localChange != NoChange && currentStyle && newStyle && currentStyle->fontSize() != newStyle->fontSize()) {
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

static void updateTextStyle(Text& text, RenderStyle* parentElementStyle, Style::Change change)
{
    RenderText* renderer = toRenderText(text.renderer());

    if (change != Style::NoChange && renderer)
        renderer->setStyle(parentElementStyle);

    if (!text.needsStyleRecalc())
        return;
    if (renderer)
        renderer->setText(text.dataImpl());
    else
        attachTextRenderer(text);
    text.clearNeedsStyleRecalc();
}

static void resolveShadowTree(ShadowRoot* shadowRoot, RenderStyle* parentElementStyle, Style::Change change)
{
    if (!shadowRoot)
        return;
    StyleResolver& styleResolver = shadowRoot->document().ensureStyleResolver();
    styleResolver.pushParentShadowRoot(shadowRoot);

    for (Node* child = shadowRoot->firstChild(); child; child = child->nextSibling()) {
        if (child->isTextNode()) {
            // Current user agent ShadowRoots don't have immediate text children so this branch is never actually taken.
            updateTextStyle(*toText(child), parentElementStyle, change);
            continue;
        }
        resolveTree(*toElement(child), change);
    }

    styleResolver.popParentShadowRoot(shadowRoot);
    shadowRoot->clearNeedsStyleRecalc();
    shadowRoot->clearChildNeedsStyleRecalc();
}

#if PLATFORM(IOS)
static EVisibility elementImplicitVisibility(const Element* element)
{
    RenderObject* renderer = element->renderer();
    if (!renderer)
        return VISIBLE;

    RenderStyle* style = renderer->style();
    if (!style)
        return VISIBLE;

    Length width(style->width());
    Length height(style->height());
    if ((width.isFixed() && width.value() <= 0) || (height.isFixed() && height.value() <= 0))
        return HIDDEN;

    Length top(style->top());
    Length left(style->left());
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

void resolveTree(Element& current, Change change)
{
    ASSERT(change != Detach);

    if (current.hasCustomStyleResolveCallbacks()) {
        if (!current.willRecalcStyle(change))
            return;
    }

    ContainerNode* renderingParentNode = NodeRenderingTraversal::parent(&current);
    bool hasParentStyle = renderingParentNode && renderingParentNode->renderStyle();
    bool hasDirectAdjacentRules = current.childrenAffectedByDirectAdjacentRules();
    bool hasIndirectAdjacentRules = current.childrenAffectedByForwardPositionalRules();

#if PLATFORM(IOS)
    CheckForVisibilityChangeOnRecalcStyle checkForVisibilityChange(current, current->renderStyle());
#endif

    if (change > NoChange || current.needsStyleRecalc())
        current.resetComputedStyle();

    if (hasParentStyle && (change >= Inherit || current.needsStyleRecalc()))
        change = resolveLocal(current, change);

    if (change != Detach) {
        StyleResolverParentPusher parentPusher(&current);

        RenderStyle* currentStyle = current.renderStyle();

        if (ShadowRoot* shadowRoot = current.shadowRoot()) {
            if (change >= Inherit || shadowRoot->childNeedsStyleRecalc() || shadowRoot->needsStyleRecalc()) {
                parentPusher.push();
                resolveShadowTree(shadowRoot, currentStyle, change);
            }
        }

        current.updateBeforePseudoElement(change);

        // FIXME: This check is good enough for :hover + foo, but it is not good enough for :hover + foo + bar.
        // For now we will just worry about the common case, since it's a lot trickier to get the second case right
        // without doing way too much re-resolution.
        bool forceCheckOfNextElementSibling = false;
        bool forceCheckOfAnyElementSibling = false;
        for (Node* child = current.firstChild(); child; child = child->nextSibling()) {
            if (child->isTextNode()) {
                updateTextStyle(*toText(child), currentStyle, change);
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
                resolveTree(*childElement, change);
            }
            forceCheckOfNextElementSibling = childRulesChanged && hasDirectAdjacentRules;
            forceCheckOfAnyElementSibling = forceCheckOfAnyElementSibling || (childRulesChanged && hasIndirectAdjacentRules);
        }

        current.updateAfterPseudoElement(change);
    }

    current.clearNeedsStyleRecalc();
    current.clearChildNeedsStyleRecalc();
    
    if (current.hasCustomStyleResolveCallbacks())
        current.didRecalcStyle(change);
}

void resolveTree(Document& document, Change change)
{
    bool resolveRootStyle = change == Force || (document.shouldDisplaySeamlesslyWithParent() && change >= Inherit);
    if (resolveRootStyle) {
        RefPtr<RenderStyle> documentStyle = resolveForDocument(document);

#if PLATFORM(IOS)
        // Inserting the pictograph font at the end of the font fallback list is done by the
        // font selector, so set a font selector if needed.
        if (Settings* settings = document.settings()) {
            StyleResolver* styleResolver = document.styleResolverIfExists();
            if (settings->fontFallbackPrefersPictographs() && styleResolver)
                documentStyle->font().update(styleResolver->fontSelector());
        }
#endif

        Style::Change documentChange = determineChange(documentStyle.get(), document.renderer()->style(), document.settings());
        if (documentChange != NoChange)
            document.renderer()->setStyle(documentStyle.release());
    }

    Element* documentElement = document.documentElement();
    if (!documentElement)
        return;
    if (change < Inherit && !documentElement->childNeedsStyleRecalc() && !documentElement->needsStyleRecalc())
        return;
    resolveTree(*documentElement, change);
}

void attachRenderTree(Element& element)
{
    attachRenderTree(element, nullptr);
}

void detachRenderTree(Element& element)
{
    detachRenderTree(element, NormalDetach);
}

void detachRenderTreeInReattachMode(Element& element)
{
    detachRenderTree(element, ReattachDetach);
}

void reattachRenderTree(Element& current)
{
    if (current.attached())
        detachRenderTree(current, ReattachDetach);
    attachRenderTree(current, nullptr);
}

}
}
