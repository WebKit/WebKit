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
#include "ElementRareData.h"
#include "ElementTraversal.h"
#include "NodeRenderStyle.h"
#include "NodeRenderingContext.h"
#include "NodeTraversal.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleResolveForDocument.h"
#include "StyleResolver.h"
#include "Text.h"

namespace WebCore {

namespace Style {

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

static void createRendererIfNeeded(Element* element, const AttachContext& context)
{
    NodeRenderingContext(element, context).createRendererForElementIfNeeded();
}

static void attachShadowRoot(ShadowRoot* shadowRoot, const AttachContext& context)
{
    if (shadowRoot->attached())
        return;
    StyleResolver& styleResolver = shadowRoot->document()->ensureStyleResolver();
    styleResolver.pushParentShadowRoot(shadowRoot);

    Style::AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;
    for (Node* child = shadowRoot->firstChild(); child; child = child->nextSibling()) {
        if (child->isTextNode()) {
            toText(child)->attachText();
            continue;
        }
        if (child->isElementNode())
            attachRenderTree(toElement(child), childrenContext);
    }
    styleResolver.popParentShadowRoot(shadowRoot);

    shadowRoot->clearNeedsStyleRecalc();
    shadowRoot->setAttached(true);
}

#ifndef NDEBUG
static bool childAttachedAllowedWhenAttachingChildren(ContainerNode* node)
{
    if (node->isShadowRoot())
        return true;
    if (node->isInsertionPoint())
        return true;
    if (node->isElementNode() && toElement(node)->shadowRoot())
        return true;
    return false;
}
#endif

static void attachChildren(Element* current, const AttachContext& context)
{
    AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (Node* child = current->firstChild(); child; child = child->nextSibling()) {
        ASSERT(!child->attached() || childAttachedAllowedWhenAttachingChildren(current));
        if (child->attached())
            continue;
        if (child->isTextNode()) {
            toText(child)->attachText();
            continue;
        }
        if (child->isElementNode())
            attachRenderTree(toElement(child), childrenContext);
    }
}

void attachRenderTree(Element* current, const AttachContext& context)
{
    PostAttachCallbackDisabler callbackDisabler(current);
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    if (current->hasCustomStyleResolveCallbacks())
        current->willAttachRenderers();

    createRendererIfNeeded(current, context);

    if (current->parentElement() && current->parentElement()->isInCanvasSubtree())
        current->setIsInCanvasSubtree(true);

    current->updatePseudoElement(BEFORE);

    StyleResolverParentPusher parentPusher(current);

    // When a shadow root exists, it does the work of attaching the children.
    if (ShadowRoot* shadowRoot = current->shadowRoot()) {
        parentPusher.push();
        attachShadowRoot(shadowRoot, context);
    } else if (current->firstChild())
        parentPusher.push();

    attachChildren(current, context);

    Node* sibling = current->nextSibling();
    if (current->renderer() && sibling && !sibling->renderer() && sibling->attached())
        Text::createTextRenderersForSiblingsAfterAttachIfNeeded(sibling);

    current->setAttached(true);
    current->clearNeedsStyleRecalc();

    if (Document* document = current->document()) {
        if (AXObjectCache* cache = document->axObjectCache())
            cache->updateCacheAfterNodeIsAttached(current);
    }

    current->updatePseudoElement(AFTER);

    current->updateFocusAppearanceAfterAttachIfNeeded();
    
    if (current->hasCustomStyleResolveCallbacks())
        current->didAttachRenderers();
}

static void detachShadowRoot(ShadowRoot* shadowRoot, const AttachContext& context)
{
    if (!shadowRoot->attached())
        return;
    Style::AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;
    for (Node* child = shadowRoot->firstChild(); child; child = child->nextSibling()) {
        if (child->isTextNode()) {
            toText(child)->detachText();
            continue;
        }
        if (child->isElementNode())
            detachRenderTree(toElement(child), context);
    }
    shadowRoot->clearChildNeedsStyleRecalc();
    shadowRoot->setAttached(false);
}

static void detachChildren(Element* current, const AttachContext& context)
{
    AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (Node* child = current->firstChild(); child; child = child->nextSibling()) {
        if (child->isTextNode()) {
            toText(child)->detachText();
            continue;
        }
        if (child->isElementNode())
            detachRenderTree(toElement(child), childrenContext);
    }
    current->clearChildNeedsStyleRecalc();
}

void detachRenderTree(Element* current, const AttachContext& context)
{
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    if (current->hasCustomStyleResolveCallbacks())
        current->willDetachRenderers();

    current->clearStyleDerivedDataBeforeDetachingRenderer();

    // Do not remove the element's hovered and active status
    // if performing a reattach.
    if (!context.performingReattach)
        current->clearHoverAndActiveStatusBeforeDetachingRenderer();

    if (ShadowRoot* shadowRoot = current->shadowRoot())
        detachShadowRoot(shadowRoot, context);

    detachChildren(current, context);

    if (current->renderer())
        current->renderer()->destroyAndCleanupAnonymousWrappers();
    current->setRenderer(0);

    current->setAttached(false);

    if (current->hasCustomStyleResolveCallbacks())
        current->didDetachRenderers();
}

void reattachRenderTree(Element* current, const AttachContext& context)
{
    AttachContext reattachContext(context);
    reattachContext.performingReattach = true;

    if (current->attached())
        detachRenderTree(current, reattachContext);

    attachRenderTree(current, reattachContext);
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

static Change resolveLocal(Element* current, Change inheritedChange)
{
    Change localChange = Detach;
    RefPtr<RenderStyle> newStyle;
    RefPtr<RenderStyle> currentStyle = current->renderStyle();

    Document* document = current->document();
    if (currentStyle) {
        newStyle = current->styleForRenderer();
        localChange = determineChange(currentStyle.get(), newStyle.get(), document->settings());
    }
    if (localChange == Detach) {
        AttachContext reattachContext;
        reattachContext.resolvedStyle = newStyle.get();
        reattachRenderTree(current, reattachContext);
        return Detach;
    }

    if (RenderObject* renderer = current->renderer()) {
        if (localChange != NoChange || pseudoStyleCacheIsInvalid(renderer, newStyle.get()) || (inheritedChange == Force && renderer->requiresForcedStyleRecalcPropagation()) || current->styleChangeType() == SyntheticStyleChange)
            renderer->setAnimatableStyle(newStyle.get());
        else if (current->needsStyleRecalc()) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.
            renderer->setStyleInternal(newStyle.get());
        }
    }

    // If "rem" units are used anywhere in the document, and if the document element's font size changes, then go ahead and force font updating
    // all the way down the tree. This is simpler than having to maintain a cache of objects (and such font size changes should be rare anyway).
    if (document->styleSheetCollection()->usesRemUnits() && document->documentElement() == current && localChange != NoChange && currentStyle && newStyle && currentStyle->fontSize() != newStyle->fontSize()) {
        // Cached RenderStyles may depend on the re units.
        if (StyleResolver* styleResolver = document->styleResolverIfExists())
            styleResolver->invalidateMatchedPropertiesCache();
        return Force;
    }
    if (inheritedChange == Force)
        return Force;
    if (current->styleChangeType() >= FullStyleChange)
        return Force;

    return localChange;
}

static void updateTextStyle(Text* text, RenderStyle* parentElementStyle, Style::Change change)
{
    RenderText* renderer = toRenderText(text->renderer());

    if (change != Style::NoChange && renderer)
        renderer->setStyle(parentElementStyle);

    if (!text->needsStyleRecalc())
        return;
    if (renderer)
        renderer->setText(text->dataImpl());
    else
        text->attachText();
    text->clearNeedsStyleRecalc();
}

static void resolveShadowTree(ShadowRoot* shadowRoot, RenderStyle* parentElementStyle, Style::Change change)
{
    if (!shadowRoot)
        return;
    StyleResolver& styleResolver = shadowRoot->document()->ensureStyleResolver();
    styleResolver.pushParentShadowRoot(shadowRoot);

    for (Node* child = shadowRoot->firstChild(); child; child = child->nextSibling()) {
        if (child->isTextNode()) {
            // Current user agent ShadowRoots don't have immediate text children so this branch is never actually taken.
            updateTextStyle(toText(child), parentElementStyle, change);
            continue;
        }
        resolveTree(toElement(child), change);
    }

    styleResolver.popParentShadowRoot(shadowRoot);
    shadowRoot->clearNeedsStyleRecalc();
    shadowRoot->clearChildNeedsStyleRecalc();
}

void resolveTree(Element* current, Change change)
{
    ASSERT(change != Detach);

    if (current->hasCustomStyleResolveCallbacks()) {
        if (!current->willRecalcStyle(change))
            return;
    }

    bool hasParentStyle = current->parentNodeForRenderingAndStyle() && current->parentNodeForRenderingAndStyle()->renderStyle();
    bool hasDirectAdjacentRules = current->childrenAffectedByDirectAdjacentRules();
    bool hasIndirectAdjacentRules = current->childrenAffectedByForwardPositionalRules();

    if (change > NoChange || current->needsStyleRecalc())
        current->resetComputedStyle();

    if (hasParentStyle && (change >= Inherit || current->needsStyleRecalc()))
        change = resolveLocal(current, change);

    if (change != Detach) {
        StyleResolverParentPusher parentPusher(current);

        RenderStyle* currentStyle = current->renderStyle();

        if (ShadowRoot* shadowRoot = current->shadowRoot()) {
            if (change >= Inherit || shadowRoot->childNeedsStyleRecalc() || shadowRoot->needsStyleRecalc()) {
                parentPusher.push();
                resolveShadowTree(shadowRoot, currentStyle, change);
            }
        }

        current->updatePseudoElement(BEFORE, change);

        // FIXME: This check is good enough for :hover + foo, but it is not good enough for :hover + foo + bar.
        // For now we will just worry about the common case, since it's a lot trickier to get the second case right
        // without doing way too much re-resolution.
        bool forceCheckOfNextElementSibling = false;
        bool forceCheckOfAnyElementSibling = false;
        for (Node* child = current->firstChild(); child; child = child->nextSibling()) {
            if (child->isTextNode()) {
                updateTextStyle(toText(child), currentStyle, change);
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
                resolveTree(childElement, change);
            }
            forceCheckOfNextElementSibling = childRulesChanged && hasDirectAdjacentRules;
            forceCheckOfAnyElementSibling = forceCheckOfAnyElementSibling || (childRulesChanged && hasIndirectAdjacentRules);
        }

        current->updatePseudoElement(AFTER, change);
    }

    current->clearNeedsStyleRecalc();
    current->clearChildNeedsStyleRecalc();
    
    if (current->hasCustomStyleResolveCallbacks())
        current->didRecalcStyle(change);
}

void resolveTree(Document* document, Change change)
{
    bool resolveRootStyle = change == Force || (document->shouldDisplaySeamlesslyWithParent() && change >= Inherit);
    if (resolveRootStyle) {
        RefPtr<RenderStyle> documentStyle = resolveForDocument(document);

        Style::Change documentChange = determineChange(documentStyle.get(), document->renderer()->style(), document->settings());
        if (documentChange != NoChange)
            document->renderer()->setStyle(documentStyle.release());
    }

    for (Element* child = ElementTraversal::firstWithin(document); child; child = ElementTraversal::nextSibling(child)) {
        if (change < Inherit && !child->childNeedsStyleRecalc() && !child->needsStyleRecalc())
            continue;
        resolveTree(child, change);
    }
}

}
}
