/*
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "RenderCounter.h"

#include "CounterNode.h"
#include "Document.h"
#include "Element.h"
#include "ElementTraversal.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "PseudoElement.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(TREE_DEBUGGING)
#include <stdio.h>
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderCounter);

using CounterMap = HashMap<AtomicString, Ref<CounterNode>>;
using CounterMaps = HashMap<const RenderElement*, std::unique_ptr<CounterMap>>;

static CounterNode* makeCounterNode(RenderElement&, const AtomicString& identifier, bool alwaysCreateCounter);

static CounterMaps& counterMaps()
{
    static NeverDestroyed<CounterMaps> staticCounterMaps;
    return staticCounterMaps;
}

// This function processes the renderer tree in the order of the DOM tree
// including pseudo elements as defined in CSS 2.1.
static RenderElement* previousInPreOrder(const RenderElement& renderer)
{
    ASSERT(renderer.element());
    Element* previous = ElementTraversal::previousIncludingPseudo(*renderer.element());
    while (previous && !previous->renderer())
        previous = ElementTraversal::previousIncludingPseudo(*previous);
    return previous ? previous->renderer() : 0;
}

static inline Element* parentOrPseudoHostElement(const RenderElement& renderer)
{
    if (renderer.isPseudoElement())
        return renderer.generatingElement();
    return renderer.element() ? renderer.element()->parentElement() : nullptr;
}

// This function processes the renderer tree in the order of the DOM tree
// including pseudo elements as defined in CSS 2.1.
static RenderElement* previousSiblingOrParent(const RenderElement& renderer)
{
    ASSERT(renderer.element());
    Element* previous = ElementTraversal::pseudoAwarePreviousSibling(*renderer.element());
    while (previous && !previous->renderer())
        previous = ElementTraversal::pseudoAwarePreviousSibling(*previous);
    if (previous)
        return previous->renderer();
    previous = parentOrPseudoHostElement(renderer);
    return previous ? previous->renderer() : nullptr;
}

static inline bool areRenderersElementsSiblings(const RenderElement& first, const RenderElement& second)
{
    return parentOrPseudoHostElement(first) == parentOrPseudoHostElement(second);
}

// This function processes the renderer tree in the order of the DOM tree
// including pseudo elements as defined in CSS 2.1.
static RenderElement* nextInPreOrder(const RenderElement& renderer, const Element* stayWithin, bool skipDescendants = false)
{
    ASSERT(renderer.element());
    Element& self = *renderer.element();
    Element* next = skipDescendants ? ElementTraversal::nextIncludingPseudoSkippingChildren(self, stayWithin) : ElementTraversal::nextIncludingPseudo(self, stayWithin);
    while (next && !next->renderer())
        next = skipDescendants ? ElementTraversal::nextIncludingPseudoSkippingChildren(*next, stayWithin) : ElementTraversal::nextIncludingPseudo(*next, stayWithin);
    return next ? next->renderer() : nullptr;
}

static CounterDirectives listItemCounterDirectives(RenderElement& renderer)
{
    if (is<RenderListItem>(renderer)) {
        auto& item = downcast<RenderListItem>(renderer);
        if (auto explicitValue = item.explicitValue())
            return { *explicitValue, WTF::nullopt };
        return { WTF::nullopt, item.isInReversedOrderedList() ? -1 : 1 };
    }
    if (auto element = renderer.element()) {
        if (is<HTMLOListElement>(*element)) {
            auto& list = downcast<HTMLOListElement>(*element);
            return { list.start(), list.isReversed() ? 1 : -1 };
        }
        if (isHTMLListElement(*element))
            return { 0, WTF::nullopt };
    }
    return { };
}

struct CounterPlan {
    bool isReset;
    int value;
};

static Optional<CounterPlan> planCounter(RenderElement& renderer, const AtomicString& identifier)
{
    // We must have a generating node or else we cannot have a counter.
    Element* generatingElement = renderer.generatingElement();
    if (!generatingElement)
        return WTF::nullopt;

    auto& style = renderer.style();

    switch (style.styleType()) {
    case PseudoId::None:
        // Sometimes elements have more then one renderer. Only the first one gets the counter
        // LayoutTests/http/tests/css/counter-crash.html
        if (generatingElement->renderer() != &renderer)
            return WTF::nullopt;
        break;
    case PseudoId::Before:
    case PseudoId::After:
        break;
    default:
        return WTF::nullopt; // Counters are forbidden from all other pseudo elements.
    }

    CounterDirectives directives;

    if (auto map = style.counterDirectives())
        directives = map->get(identifier);

    if (identifier == "list-item") {
        auto itemDirectives = listItemCounterDirectives(renderer);
        if (!directives.resetValue)
            directives.resetValue = itemDirectives.resetValue;
        if (!directives.incrementValue)
            directives.incrementValue = itemDirectives.incrementValue;
    }

    if (directives.resetValue)
        return CounterPlan { true, saturatedAddition(*directives.resetValue, directives.incrementValue.value_or(0)) };
    if (directives.incrementValue)
        return CounterPlan { false, *directives.incrementValue };
    return WTF::nullopt;
}

// - Finds the insertion point for the counter described by counterOwner, isReset and 
// identifier in the CounterNode tree for identifier and sets parent and
// previousSibling accordingly.
// - The function returns true if the counter whose insertion point is searched is NOT
// the root of the tree.
// - The root of the tree is a counter reference that is not in the scope of any other
// counter with the same identifier.
// - All the counter references with the same identifier as this one that are in
// children or subsequent siblings of the renderer that owns the root of the tree
// form the rest of of the nodes of the tree.
// - The root of the tree is always a reset type reference.
// - A subtree rooted at any reset node in the tree is equivalent to all counter 
// references that are in the scope of the counter or nested counter defined by that
// reset node.
// - Non-reset CounterNodes cannot have descendants.

struct CounterInsertionPoint {
    RefPtr<CounterNode> parent;
    RefPtr<CounterNode> previousSibling;
};

static CounterInsertionPoint findPlaceForCounter(RenderElement& counterOwner, const AtomicString& identifier, bool isReset)
{
    // We cannot stop searching for counters with the same identifier before we also
    // check this renderer, because it may affect the positioning in the tree of our counter.
    RenderElement* searchEndRenderer = previousSiblingOrParent(counterOwner);
    // We check renderers in preOrder from the renderer that our counter is attached to
    // towards the begining of the document for counters with the same identifier as the one
    // we are trying to find a place for. This is the next renderer to be checked.
    RenderElement* currentRenderer = previousInPreOrder(counterOwner);
    RefPtr<CounterNode> previousSibling;

    while (currentRenderer) {
        auto currentCounter = makeCounterNode(*currentRenderer, identifier, false);
        if (searchEndRenderer == currentRenderer) {
            // We may be at the end of our search.
            if (currentCounter) {
                // We have a suitable counter on the EndSearchRenderer.
                if (previousSibling) {
                    // But we already found another counter that we come after.
                    if (currentCounter->actsAsReset()) {
                        // We found a reset counter that is on a renderer that is a sibling of ours or a parent.
                        if (isReset && areRenderersElementsSiblings(*currentRenderer, counterOwner)) {
                            // We are also a reset counter and the previous reset was on a sibling renderer
                            // hence we are the next sibling of that counter if that reset is not a root or
                            // we are a root node if that reset is a root.
                            auto* parent = currentCounter->parent();
                            return { parent, parent ? currentCounter : nullptr };
                        }
                        // We are not a reset node or the previous reset must be on an ancestor of our owner renderer
                        // hence we must be a child of that reset counter.
                        // In some cases renders can be reparented (ex. nodes inside a table but not in a column or row).
                        // In these cases the identified previousSibling will be invalid as its parent is different from
                        // our identified parent.
                        if (previousSibling->parent() != currentCounter)
                            previousSibling = nullptr;
                        return { currentCounter, WTFMove(previousSibling) };
                    }
                    // CurrentCounter, the counter at the EndSearchRenderer, is not reset.
                    if (!isReset || !areRenderersElementsSiblings(*currentRenderer, counterOwner)) {
                        // If the node we are placing is not reset or we have found a counter that is attached
                        // to an ancestor of the placed counter's owner renderer we know we are a sibling of that node.
                        if (currentCounter->parent() != previousSibling->parent())
                            return { };
                        return { currentCounter->parent(), WTFMove(previousSibling) };
                    }
                } else { 
                    // We are at the potential end of the search, but we had no previous sibling candidate
                    // In this case we follow pretty much the same logic as above but no ASSERTs about 
                    // previousSibling, and when we are a sibling of the end counter we must set previousSibling
                    // to currentCounter.
                    if (currentCounter->actsAsReset()) {
                        if (isReset && areRenderersElementsSiblings(*currentRenderer, counterOwner))
                            return { currentCounter->parent(), currentCounter };
                        return { currentCounter, WTFMove(previousSibling) };
                    }
                    if (!isReset || !areRenderersElementsSiblings(*currentRenderer, counterOwner))
                        return { currentCounter->parent(), currentCounter };
                    previousSibling = currentCounter;
                }
            }
            // We come here if the previous sibling or parent of our owner renderer had no
            // good counter, or we are a reset node and the counter on the previous sibling
            // of our owner renderer was not a reset counter.
            // Set a new goal for the end of the search.
            searchEndRenderer = previousSiblingOrParent(*currentRenderer);
        } else {
            // We are searching descendants of a previous sibling of the renderer that the
            // counter being placed is attached to.
            if (currentCounter) {
                // We found a suitable counter.
                if (previousSibling) {
                    // Since we had a suitable previous counter before, we should only consider this one as our 
                    // previousSibling if it is a reset counter and hence the current previousSibling is its child.
                    if (currentCounter->actsAsReset()) {
                        previousSibling = currentCounter;
                        // We are no longer interested in previous siblings of the currentRenderer or their children
                        // as counters they may have attached cannot be the previous sibling of the counter we are placing.
                        currentRenderer = parentOrPseudoHostElement(*currentRenderer)->renderer();
                        continue;
                    }
                } else
                    previousSibling = currentCounter;
                currentRenderer = previousSiblingOrParent(*currentRenderer);
                continue;
            }
        }
        // This function is designed so that the same test is not done twice in an iteration, except for this one
        // which may be done twice in some cases. Rearranging the decision points though, to accommodate this 
        // performance improvement would create more code duplication than is worthwhile in my oppinion and may further
        // impede the readability of this already complex algorithm.
        if (previousSibling)
            currentRenderer = previousSiblingOrParent(*currentRenderer);
        else
            currentRenderer = previousInPreOrder(*currentRenderer);
    }
    return { };
}

static CounterNode* makeCounterNode(RenderElement& renderer, const AtomicString& identifier, bool alwaysCreateCounter)
{
    if (renderer.hasCounterNodeMap()) {
        ASSERT(counterMaps().contains(&renderer));
        if (auto* node = counterMaps().find(&renderer)->value->get(identifier))
            return node;
    }

    auto plan = planCounter(renderer, identifier);
    if (!plan && !alwaysCreateCounter)
        return nullptr;

    auto& maps = counterMaps();

    auto newNode = CounterNode::create(renderer, plan && plan->isReset, plan ? plan->value : 0);

    auto place = findPlaceForCounter(renderer, identifier, plan && plan->isReset);
    if (place.parent)
        place.parent->insertAfter(newNode, place.previousSibling.get(), identifier);

    maps.add(&renderer, std::make_unique<CounterMap>()).iterator->value->add(identifier, newNode.copyRef());
    renderer.setHasCounterNodeMap(true);

    if (newNode->parent())
        return newNode.ptr();

    // Check if some nodes that were previously root nodes should become children of this node now.
    auto* currentRenderer = &renderer;
    auto* stayWithin = parentOrPseudoHostElement(renderer);
    bool skipDescendants = false;
    while ((currentRenderer = nextInPreOrder(*currentRenderer, stayWithin, skipDescendants))) {
        skipDescendants = false;
        if (!currentRenderer->hasCounterNodeMap())
            continue;
        auto* currentCounter = maps.find(currentRenderer)->value->get(identifier);
        if (!currentCounter)
            continue;
        skipDescendants = true;
        if (currentCounter->parent())
            continue;
        if (stayWithin == parentOrPseudoHostElement(*currentRenderer) && currentCounter->hasResetType())
            break;
        newNode->insertAfter(*currentCounter, newNode->lastChild(), identifier);
    }

    return newNode.ptr();
}

RenderCounter::RenderCounter(Document& document, const CounterContent& counter)
    : RenderText(document, emptyString())
    , m_counter(counter)
{
    view().addRenderCounter();
}

RenderCounter::~RenderCounter()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderCounter::willBeDestroyed()
{
    view().removeRenderCounter();

    if (m_counterNode) {
        m_counterNode->removeRenderer(*this);
        ASSERT(!m_counterNode);
    }
    
    RenderText::willBeDestroyed();
}

const char* RenderCounter::renderName() const
{
    return "RenderCounter";
}

bool RenderCounter::isCounter() const
{
    return true;
}

String RenderCounter::originalText() const
{
    if (!m_counterNode) {
        RenderElement* beforeAfterContainer = parent();
        while (true) {
            if (!beforeAfterContainer)
                return String();
            if (!beforeAfterContainer->isAnonymous() && !beforeAfterContainer->isPseudoElement())
                return String(); // RenderCounters are restricted to before and after pseudo elements
            PseudoId containerStyle = beforeAfterContainer->style().styleType();
            if ((containerStyle == PseudoId::Before) || (containerStyle == PseudoId::After))
                break;
            beforeAfterContainer = beforeAfterContainer->parent();
        }
        makeCounterNode(*beforeAfterContainer, m_counter.identifier(), true)->addRenderer(const_cast<RenderCounter&>(*this));
        ASSERT(m_counterNode);
    }
    CounterNode* child = m_counterNode;
    int value = child->actsAsReset() ? child->value() : child->countInParent();

    String text = listMarkerText(m_counter.listStyle(), value);

    if (!m_counter.separator().isNull()) {
        if (!child->actsAsReset())
            child = child->parent();
        while (CounterNode* parent = child->parent()) {
            text = listMarkerText(m_counter.listStyle(), child->countInParent())
                + m_counter.separator() + text;
            child = parent;
        }
    }

    return text;
}

void RenderCounter::updateCounter()
{
    computePreferredLogicalWidths(0);
}

void RenderCounter::computePreferredLogicalWidths(float lead)
{
#ifndef NDEBUG
    // FIXME: We shouldn't be modifying the tree in computePreferredLogicalWidths.
    // Instead, we should properly hook the appropriate changes in the DOM and modify
    // the render tree then. When that's done, we also won't need to override
    // computePreferredLogicalWidths at all.
    // https://bugs.webkit.org/show_bug.cgi?id=104829
    SetLayoutNeededForbiddenScope layoutForbiddenScope(this, false);
#endif

    setRenderedText(originalText());

    RenderText::computePreferredLogicalWidths(lead);
}

static void destroyCounterNodeWithoutMapRemoval(const AtomicString& identifier, CounterNode& node)
{
    RefPtr<CounterNode> previous;
    for (RefPtr<CounterNode> child = node.lastDescendant(); child && child != &node; child = WTFMove(previous)) {
        previous = child->previousInPreOrder();
        child->parent()->removeChild(*child);
        ASSERT(counterMaps().find(&child->owner())->value->get(identifier) == child);
        counterMaps().find(&child->owner())->value->remove(identifier);
    }
    if (auto* parent = node.parent())
        parent->removeChild(node);
}

void RenderCounter::destroyCounterNodes(RenderElement& owner)
{
    ASSERT(owner.hasCounterNodeMap());
    auto& maps = counterMaps();
    ASSERT(maps.contains(&owner));
    auto counterMap = maps.take(&owner);
    for (auto& counterMapEntry : *counterMap)
        destroyCounterNodeWithoutMapRemoval(counterMapEntry.key, counterMapEntry.value);
    owner.setHasCounterNodeMap(false);
}

void RenderCounter::destroyCounterNode(RenderElement& owner, const AtomicString& identifier)
{
    auto map = counterMaps().find(&owner);
    if (map == counterMaps().end())
        return;
    auto node = map->value->take(identifier);
    if (!node)
        return;
    destroyCounterNodeWithoutMapRemoval(identifier, *node);
    // We do not delete the map here even if it is now empty because we expect to
    // reuse it. In order for a renderer to lose all its counters permanently,
    // a style change for the renderer involving removal of all counter
    // directives must occur, in which case, RenderCounter::destroyCounterNodes()
    // will be called.
}

void RenderCounter::rendererRemovedFromTree(RenderElement& renderer)
{
    if (!renderer.view().hasRenderCounters())
        return;
    RenderObject* currentRenderer = renderer.lastLeafChild();
    if (!currentRenderer)
        currentRenderer = &renderer;
    while (true) {
        if (is<RenderElement>(*currentRenderer)) {
            auto& counterNodeRenderer = downcast<RenderElement>(*currentRenderer);
            if (counterNodeRenderer.hasCounterNodeMap())
                destroyCounterNodes(counterNodeRenderer);
        }
        if (currentRenderer == &renderer)
            break;
        currentRenderer = currentRenderer->previousInPreOrder();
    }
}

static void updateCounters(RenderElement& renderer)
{
    auto* directiveMap = renderer.style().counterDirectives();
    if (!directiveMap)
        return;
    if (!renderer.hasCounterNodeMap()) {
        for (auto& key : directiveMap->keys())
            makeCounterNode(renderer, key, false);
        return;
    }
    ASSERT(counterMaps().contains(&renderer));
    auto* counterMap = counterMaps().find(&renderer)->value.get();
    for (auto& key : directiveMap->keys()) {
        RefPtr<CounterNode> node = counterMap->get(key);
        if (!node) {
            makeCounterNode(renderer, key, false);
            continue;
        }
        auto place = findPlaceForCounter(renderer, key, node->hasResetType());
        if (node != counterMap->get(key))
            continue;
        CounterNode* parent = node->parent();
        if (place.parent == parent && place.previousSibling == node->previousSibling())
            continue;
        if (parent)
            parent->removeChild(*node);
        if (place.parent)
            place.parent->insertAfter(*node, place.previousSibling.get(), key);
    }
}

void RenderCounter::rendererSubtreeAttached(RenderElement& renderer)
{
    if (!renderer.view().hasRenderCounters())
        return;
    Element* element = renderer.element();
    if (element && !element->isPseudoElement())
        element = element->parentElement();
    else
        element = renderer.generatingElement();
    if (element && !element->renderer())
        return; // No need to update if the parent is not attached yet
    for (RenderObject* descendant = &renderer; descendant; descendant = descendant->nextInPreOrder(&renderer)) {
        if (is<RenderElement>(*descendant))
            updateCounters(downcast<RenderElement>(*descendant));
    }
}

void RenderCounter::rendererStyleChanged(RenderElement& renderer, const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    Element* element = renderer.generatingElement();
    if (!element || !element->renderer())
        return; // cannot have generated content or if it can have, it will be handled during attaching

    const CounterDirectiveMap* newCounterDirectives;
    const CounterDirectiveMap* oldCounterDirectives;
    if (oldStyle && (oldCounterDirectives = oldStyle->counterDirectives())) {
        if (newStyle && (newCounterDirectives = newStyle->counterDirectives())) {
            for (auto& keyValue : *newCounterDirectives) {
                auto existingEntry = oldCounterDirectives->find(keyValue.key);
                if (existingEntry != oldCounterDirectives->end()) {
                    if (existingEntry->value == keyValue.value)
                        continue;
                    RenderCounter::destroyCounterNode(renderer, keyValue.key);
                }
                // We must create this node here, because the changed node may be a node with no display such as
                // as those created by the increment or reset directives and the re-layout that will happen will
                // not catch the change if the node had no children.
                makeCounterNode(renderer, keyValue.key, false);
            }
            // Destroying old counters that do not exist in the new counterDirective map.
            for (auto& key : oldCounterDirectives->keys()) {
                if (!newCounterDirectives->contains(key))
                    RenderCounter::destroyCounterNode(renderer, key);
            }
        } else {
            if (renderer.hasCounterNodeMap())
                RenderCounter::destroyCounterNodes(renderer);
        }
    } else {
        if (newStyle && (newCounterDirectives = newStyle->counterDirectives())) {
            for (auto& key : newCounterDirectives->keys()) {
                // We must create this node here, because the added node may be a node with no display such as
                // as those created by the increment or reset directives and the re-layout that will happen will
                // not catch the change if the node had no children.
                makeCounterNode(renderer, key, false);
            }
        }
    }
}

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)

void showCounterRendererTree(const WebCore::RenderObject* renderer, const char* counterName)
{
    if (!renderer)
        return;
    auto* root = renderer;
    while (root->parent())
        root = root->parent();

    AtomicString identifier(counterName);
    for (auto* current = root; current; current = current->nextInPreOrder()) {
        if (!is<WebCore::RenderElement>(*current))
            continue;
        fprintf(stderr, "%c", (current == renderer) ? '*' : ' ');
        for (auto* ancestor = current; ancestor && ancestor != root; ancestor = ancestor->parent())
            fprintf(stderr, "    ");
        fprintf(stderr, "%p N:%p P:%p PS:%p NS:%p C:%p\n",
            current, current->node(), current->parent(), current->previousSibling(),
            current->nextSibling(), downcast<WebCore::RenderElement>(*current).hasCounterNodeMap() ?
            counterName ? WebCore::counterMaps().find(downcast<WebCore::RenderElement>(current))->value->get(identifier) : (WebCore::CounterNode*)1 : (WebCore::CounterNode*)0);
    }
    fflush(stderr);
}

#endif // NDEBUG
