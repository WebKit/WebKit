/**
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderStyle.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

typedef HashMap<RefPtr<AtomicStringImpl>, CounterNode*> CounterMap;
typedef HashMap<const RenderObject*, CounterMap*> CounterMaps;

static CounterNode* makeCounterNode(RenderObject*, const AtomicString& identifier, bool alwaysCreateCounter);

static CounterMaps& counterMaps()
{
    DEFINE_STATIC_LOCAL(CounterMaps, staticCounterMaps, ());
    return staticCounterMaps;
}

static inline RenderObject* previousSiblingOrParent(RenderObject* object)
{
    if (RenderObject* sibling = object->previousSibling())
        return sibling;
    return object->parent();
}

static bool planCounter(RenderObject* object, const AtomicString& identifier, bool& isReset, int& value)
{
    ASSERT(object);

    // Real text nodes don't have their own style so they can't have counters.
    // We can't even look at their styles or we'll see extra resets and increments!
    if (object->isText() && !object->isBR())
        return false;

    RenderStyle* style = object->style();
    ASSERT(style);

    if (const CounterDirectiveMap* directivesMap = style->counterDirectives()) {
        CounterDirectives directives = directivesMap->get(identifier.impl());
        if (directives.m_reset) {
            value = directives.m_resetValue;
            if (directives.m_increment)
                value += directives.m_incrementValue;
            isReset = true;
            return true;
        }
        if (directives.m_increment) {
            value = directives.m_incrementValue;
            isReset = false;
            return true;
        }
    }

    if (identifier == "list-item") {
        if (object->isListItem()) {
            if (toRenderListItem(object)->hasExplicitValue()) {
                value = toRenderListItem(object)->explicitValue();
                isReset = true;
                return true;
            }
            value = 1;
            isReset = false;
            return true;
        }
        if (Node* e = object->node()) {
            if (e->hasTagName(olTag)) {
                value = static_cast<HTMLOListElement*>(e)->start();
                isReset = true;
                return true;
            }
            if (e->hasTagName(ulTag) || e->hasTagName(menuTag) || e->hasTagName(dirTag)) {
                value = 0;
                isReset = true;
                return true;
            }
        }
    }

    return false;
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

static bool findPlaceForCounter(RenderObject* counterOwner, const AtomicString& identifier, bool isReset, CounterNode*& parent, CounterNode*& previousSibling)
{
    // We cannot stop searching for counters with the same identifier before we also
    // check this renderer, because it may affect the positioning in the tree of our counter.
    RenderObject* searchEndRenderer = previousSiblingOrParent(counterOwner);
    // We check renderers in preOrder from the renderer that our counter is attached to
    // towards the begining of the document for counters with the same identifier as the one
    // we are trying to find a place for. This is the next renderer to be checked.
    RenderObject* currentRenderer = counterOwner->previousInPreOrder();
    previousSibling = 0;
    while (currentRenderer) {
        // A sibling without a parent means that the counter node tree was not constructed correctly so we stop
        // traversing. In the future RenderCounter should handle RenderObjects that are not connected to the
        // render tree at counter node creation. See bug 43812.
        if (previousSibling && !previousSibling->parent())
            return false;
        CounterNode* currentCounter = makeCounterNode(currentRenderer, identifier, false);
        if (searchEndRenderer == currentRenderer) {
            // We may be at the end of our search.
            if (currentCounter) {
                // We have a suitable counter on the EndSearchRenderer.
                if (previousSibling) { // But we already found another counter that we come after.
                    if (currentCounter->actsAsReset()) {
                        // We found a reset counter that is on a renderer that is a sibling of ours or a parent.
                        if (isReset && currentRenderer->parent() == counterOwner->parent()) {
                            // We are also a reset counter and the previous reset was on a sibling renderer
                            // hence we are the next sibling of that counter if that reset is not a root or
                            // we are a root node if that reset is a root.
                            parent = currentCounter->parent();
                            previousSibling = parent ? currentCounter : 0;
                            return parent;
                        }
                        // We are not a reset node or the previous reset must be on an ancestor of our renderer
                        // hence we must be a child of that reset counter.
                        parent = currentCounter;
                        ASSERT(previousSibling->parent() == currentCounter);
                        return true;
                    }
                    // CurrentCounter, the counter at the EndSearchRenderer, is not reset.
                    if (!isReset || currentRenderer->parent() != counterOwner->parent()) {
                        // If the node we are placing is not reset or we have found a counter that is attached
                        // to an ancestor of the placed counter's renderer we know we are a sibling of that node.
                        ASSERT(currentCounter->parent() == previousSibling->parent());
                        parent = currentCounter->parent();
                        return true;
                    }
                } else { 
                    // We are at the potential end of the search, but we had no previous sibling candidate
                    // In this case we follow pretty much the same logic as above but no ASSERTs about 
                    // previousSibling, and when we are a sibling of the end counter we must set previousSibling
                    // to currentCounter.
                    if (currentCounter->actsAsReset()) {
                        if (isReset && currentRenderer->parent() == counterOwner->parent()) {
                            parent = currentCounter->parent();
                            previousSibling = currentCounter;
                            return parent;
                        }
                        parent = currentCounter;
                        return true;
                    }
                    if (!isReset || currentRenderer->parent() != counterOwner->parent()) {
                        parent = currentCounter->parent();
                        previousSibling = currentCounter;
                        return true;
                    }
                    previousSibling = currentCounter;
                }
            }
            // We come here if the previous sibling or parent of our renderer had no 
            // good counter, or we are a reset node and the counter on the previous sibling
            // of our renderer was not a reset counter.
            // Set a new goal for the end of the search.
            searchEndRenderer = previousSiblingOrParent(currentRenderer);
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
                        currentRenderer = currentRenderer->parent();
                        continue;
                    }
                } else
                    previousSibling = currentCounter;
                currentRenderer = previousSiblingOrParent(currentRenderer);
                continue;
            }
        }
        // This function is designed so that the same test is not done twice in an iteration, except for this one
        // which may be done twice in some cases. Rearranging the decision points though, to accommodate this 
        // performance improvement would create more code duplication than is worthwhile in my oppinion and may further
        // impede the readability of this already complex algorithm.
        if (previousSibling)
            currentRenderer = previousSiblingOrParent(currentRenderer);
        else
            currentRenderer = currentRenderer->previousInPreOrder();
    }
    return false;
}

static CounterNode* makeCounterNode(RenderObject* object, const AtomicString& identifier, bool alwaysCreateCounter)
{
    ASSERT(object);

    if (object->m_hasCounterNodeMap)
        if (CounterMap* nodeMap = counterMaps().get(object))
            if (CounterNode* node = nodeMap->get(identifier.impl()))
                return node;

    bool isReset = false;
    int value = 0;
    if (!planCounter(object, identifier, isReset, value) && !alwaysCreateCounter)
        return 0;

    CounterNode* newParent = 0;
    CounterNode* newPreviousSibling = 0;
    CounterNode* newNode = new CounterNode(object, isReset, value);
    if (findPlaceForCounter(object, identifier, isReset, newParent, newPreviousSibling))
        newParent->insertAfter(newNode, newPreviousSibling, identifier);
    CounterMap* nodeMap;
    if (object->m_hasCounterNodeMap)
        nodeMap = counterMaps().get(object);
    else {
        nodeMap = new CounterMap;
        counterMaps().set(object, nodeMap);
        object->m_hasCounterNodeMap = true;
    }
    nodeMap->set(identifier.impl(), newNode);
    if (newNode->parent() || !object->nextInPreOrder(object->parent()))
        return newNode;
    // Checking if some nodes that were previously counter tree root nodes
    // should become children of this node now.
    CounterMaps& maps = counterMaps();
    RenderObject* stayWithin = object->parent();
    for (RenderObject* currentRenderer = object->nextInPreOrder(stayWithin); currentRenderer; currentRenderer = currentRenderer->nextInPreOrder(stayWithin)) {
        if (!currentRenderer->m_hasCounterNodeMap)
            continue;
        CounterNode* currentCounter = maps.get(currentRenderer)->get(identifier.impl());
        if (!currentCounter)
            continue;
        if (currentCounter->parent()) {
            ASSERT(newNode->firstChild());
            if (currentRenderer->lastChild())
                currentRenderer = currentRenderer->lastChild();
            continue;
        }
        if (stayWithin != currentRenderer->parent() || !currentCounter->hasResetType())
            newNode->insertAfter(currentCounter, newNode->lastChild(), identifier);
        if (currentRenderer->lastChild())
            currentRenderer = currentRenderer->lastChild();
    }
    return newNode;
}

RenderCounter::RenderCounter(Document* node, const CounterContent& counter)
    : RenderText(node, StringImpl::empty())
    , m_counter(counter)
    , m_counterNode(0)
{
}

const char* RenderCounter::renderName() const
{
    return "RenderCounter";
}

bool RenderCounter::isCounter() const
{
    return true;
}

PassRefPtr<StringImpl> RenderCounter::originalText() const
{
    if (!parent())
        return 0;

    if (!m_counterNode)
        m_counterNode = makeCounterNode(parent(), m_counter.identifier(), true);

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

    return text.impl();
}

void RenderCounter::computePreferredLogicalWidths(int lead)
{
    setTextInternal(originalText());
    RenderText::computePreferredLogicalWidths(lead);
}

void RenderCounter::invalidate(const AtomicString& identifier)
{
    if (m_counter.identifier() != identifier)
        return;
    m_counterNode = 0;
    setNeedsLayoutAndPrefWidthsRecalc();
}

static void destroyCounterNodeWithoutMapRemoval(const AtomicString& identifier, CounterNode* node)
{
    CounterNode* previous;
    for (CounterNode* child = node->lastDescendant(); child && child != node; child = previous) {
        previous = child->previousInPreOrder();
        child->parent()->removeChild(child, identifier);
        ASSERT(counterMaps().get(child->renderer())->get(identifier.impl()) == child);
        counterMaps().get(child->renderer())->remove(identifier.impl());
        if (!child->renderer()->documentBeingDestroyed()) {
            RenderObjectChildList* children = child->renderer()->virtualChildren();
            if (children)
                children->invalidateCounters(child->renderer(), identifier);
        }
        delete child;
    }
    RenderObject* renderer = node->renderer();
    if (!renderer->documentBeingDestroyed()) {
        if (RenderObjectChildList* children = renderer->virtualChildren())
            children->invalidateCounters(renderer, identifier);
    }
    if (CounterNode* parent = node->parent())
        parent->removeChild(node, identifier);
    delete node;
}

void RenderCounter::destroyCounterNodes(RenderObject* renderer)
{
    CounterMaps& maps = counterMaps();
    CounterMaps::iterator mapsIterator = maps.find(renderer);
    if (mapsIterator == maps.end())
        return;
    CounterMap* map = mapsIterator->second;
    CounterMap::const_iterator end = map->end();
    for (CounterMap::const_iterator it = map->begin(); it != end; ++it) {
        AtomicString identifier(it->first.get());
        destroyCounterNodeWithoutMapRemoval(identifier, it->second);
    }
    maps.remove(mapsIterator);
    delete map;
    renderer->m_hasCounterNodeMap = false;
}

void RenderCounter::destroyCounterNode(RenderObject* renderer, const AtomicString& identifier)
{
    CounterMap* map = counterMaps().get(renderer);
    if (!map)
        return;
    CounterMap::iterator mapIterator = map->find(identifier.impl());
    if (mapIterator == map->end())
        return;
    destroyCounterNodeWithoutMapRemoval(identifier, mapIterator->second);
    map->remove(mapIterator);
    // We do not delete "map" here even if empty because we expect to reuse
    // it soon. In order for a renderer to lose all its counters permanently,
    // a style change for the renderer involving removal of all counter
    // directives must occur, in which case, RenderCounter::destroyCounterNodes()
    // must be called.
    // The destruction of the Renderer (possibly caused by the removal of its 
    // associated DOM node) is the other case that leads to the permanent
    // destruction of all counters attached to a Renderer. In this case
    // RenderCounter::destroyCounterNodes() must be and is now called, too.
    // RenderCounter::destroyCounterNodes() handles destruction of the counter
    // map associated with a renderer, so there is no risk in leaking the map.
}

static void updateCounters(RenderObject* renderer)
{
    ASSERT(renderer->style());
    const CounterDirectiveMap* directiveMap = renderer->style()->counterDirectives();
    if (!directiveMap)
        return;
    CounterDirectiveMap::const_iterator end = directiveMap->end();
    if (!renderer->m_hasCounterNodeMap) {
        for (CounterDirectiveMap::const_iterator it = directiveMap->begin(); it != end; ++it)
            makeCounterNode(renderer, AtomicString(it->first.get()), false);
        return;
    }
    CounterMap* counterMap = counterMaps().get(renderer);
    ASSERT(counterMap);
    for (CounterDirectiveMap::const_iterator it = directiveMap->begin(); it != end; ++it) {
        CounterNode* node = counterMap->get(it->first.get());
        if (!node) {
            makeCounterNode(renderer, AtomicString(it->first.get()), false);
            continue;
        }
        CounterNode* newParent = 0;
        CounterNode* newPreviousSibling;
        findPlaceForCounter(renderer, AtomicString(it->first.get()), node->hasResetType(), newParent, newPreviousSibling);
        CounterNode* parent = node->parent();
        if (newParent == parent && newPreviousSibling == node->previousSibling())
            continue;
        if (parent)
            parent->removeChild(node, it->first.get());
        if (newParent)
            newParent->insertAfter(node, newPreviousSibling, it->first.get());
    }
}

void RenderCounter::rendererSubtreeAttached(RenderObject* renderer)
{
    for (RenderObject* descendant = renderer; descendant; descendant = descendant->nextInPreOrder(renderer))
        updateCounters(descendant);
}

void RenderCounter::rendererStyleChanged(RenderObject* renderer, const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    const CounterDirectiveMap* newCounterDirectives;
    const CounterDirectiveMap* oldCounterDirectives;
    if (oldStyle && (oldCounterDirectives = oldStyle->counterDirectives())) {
        if (newStyle && (newCounterDirectives = newStyle->counterDirectives())) {
            CounterDirectiveMap::const_iterator newMapEnd = newCounterDirectives->end();
            CounterDirectiveMap::const_iterator oldMapEnd = oldCounterDirectives->end();
            for (CounterDirectiveMap::const_iterator it = newCounterDirectives->begin(); it != newMapEnd; ++it) {
                CounterDirectiveMap::const_iterator oldMapIt = oldCounterDirectives->find(it->first);
                if (oldMapIt != oldMapEnd) {
                    if (oldMapIt->second == it->second)
                        continue;
                    RenderCounter::destroyCounterNode(renderer, it->first.get());
                }
                // We must create this node here, because the changed node may be a node with no display such as
                // as those created by the increment or reset directives and the re-layout that will happen will
                // not catch the change if the node had no children.
                makeCounterNode(renderer, it->first.get(), false);
            }
            // Destroying old counters that do not exist in the new counterDirective map.
            for (CounterDirectiveMap::const_iterator it = oldCounterDirectives->begin(); it !=oldMapEnd; ++it) {
                if (!newCounterDirectives->contains(it->first))
                    RenderCounter::destroyCounterNode(renderer, it->first.get());
            }
        } else {
            if (renderer->m_hasCounterNodeMap)
                RenderCounter::destroyCounterNodes(renderer);
        }
    } else if (newStyle && (newCounterDirectives = newStyle->counterDirectives())) {
        CounterDirectiveMap::const_iterator newMapEnd = newCounterDirectives->end();
        for (CounterDirectiveMap::const_iterator it = newCounterDirectives->begin(); it != newMapEnd; ++it) {
            // We must create this node here, because the added node may be a node with no display such as
            // as those created by the increment or reset directives and the re-layout that will happen will
            // not catch the change if the node had no children.
            makeCounterNode(renderer, it->first.get(), false);
        }
    }
}

} // namespace WebCore
