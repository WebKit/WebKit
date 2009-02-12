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

static CounterNode* counter(RenderObject*, const AtomicString& counterName, bool alwaysCreateCounter);

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

static CounterNode* lastDescendant(CounterNode* node)
{
    CounterNode* last = node->lastChild();
    if (!last)
        return 0;

    while (CounterNode* lastChild = last->lastChild())
        last = lastChild;

    return last;
}

static CounterNode* previousInPreOrder(CounterNode* node)
{
    CounterNode* previous = node->previousSibling();
    if (!previous)
        return node->parent();

    while (CounterNode* lastChild = previous->lastChild())
        previous = lastChild;

    return previous;
}

static bool planCounter(RenderObject* object, const AtomicString& counterName, bool& isReset, int& value)
{
    ASSERT(object);

    // Real text nodes don't have their own style so they can't have counters.
    // We can't even look at their styles or we'll see extra resets and increments!
    if (object->isText() && !object->isBR())
        return false;

    RenderStyle* style = object->style();
    ASSERT(style);

    if (const CounterDirectiveMap* directivesMap = style->counterDirectives()) {
        CounterDirectives directives = directivesMap->get(counterName.impl());
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

    if (counterName == "list-item") {
        if (object->isListItem()) {
            if (static_cast<RenderListItem*>(object)->hasExplicitValue()) {
                value = static_cast<RenderListItem*>(object)->explicitValue();
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

static bool findPlaceForCounter(RenderObject* object, const AtomicString& counterName,
    bool isReset, CounterNode*& parent, CounterNode*& previousSibling)
{
    // Find the appropriate previous sibling for insertion into the parent node
    // by searching in render tree order for a child of the counter.
    parent = 0;
    previousSibling = 0;
    RenderObject* resetCandidate = isReset ? object->parent() : previousSiblingOrParent(object);
    RenderObject* prevCounterCandidate = object;
    CounterNode* candidateCounter = 0;
    while ((prevCounterCandidate = prevCounterCandidate->previousInPreOrder())) {
        CounterNode* c = counter(prevCounterCandidate, counterName, false);
        if (prevCounterCandidate == resetCandidate) {
            if (!candidateCounter)
                candidateCounter = c;
            if (candidateCounter) {
                if (candidateCounter->isReset()) {
                    parent = candidateCounter;
                    previousSibling = 0;
                } else {
                    parent = candidateCounter->parent();
                    previousSibling = candidateCounter;
                }
                return true;
            }
            resetCandidate = previousSiblingOrParent(resetCandidate);
        } else if (c) {
            if (c->isReset())
                candidateCounter = 0;
            else if (!candidateCounter)
                candidateCounter = c;
        }
    }

    return false;
}

static CounterNode* counter(RenderObject* object, const AtomicString& counterName, bool alwaysCreateCounter)
{
    ASSERT(object);

    if (object->m_hasCounterNodeMap)
        if (CounterMap* nodeMap = counterMaps().get(object))
            if (CounterNode* node = nodeMap->get(counterName.impl()))
                return node;

    bool isReset = false;
    int value = 0;
    if (!planCounter(object, counterName, isReset, value) && !alwaysCreateCounter)
        return 0;

    CounterNode* newParent = 0;
    CounterNode* newPreviousSibling = 0;
    CounterNode* newNode;
    if (findPlaceForCounter(object, counterName, isReset, newParent, newPreviousSibling)) {
        newNode = new CounterNode(object, isReset, value);
        newParent->insertAfter(newNode, newPreviousSibling);
    } else {
        // Make a reset node for counters that aren't inside an existing reset node.
        newNode = new CounterNode(object, true, value);
    }

    CounterMap* nodeMap;
    if (object->m_hasCounterNodeMap)
        nodeMap = counterMaps().get(object);
    else {
        nodeMap = new CounterMap;
        counterMaps().set(object, nodeMap);
        object->m_hasCounterNodeMap = true;
    }
    nodeMap->set(counterName.impl(), newNode);

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
        m_counterNode = counter(parent(), m_counter.identifier(), true);

    CounterNode* child = m_counterNode;
    int value = child->isReset() ? child->value() : child->countInParent();

    String text = listMarkerText(m_counter.listStyle(), value);

    if (!m_counter.separator().isNull()) {
        if (!child->isReset())
            child = child->parent();
        while (CounterNode* parent = child->parent()) {
            text = listMarkerText(m_counter.listStyle(), child->countInParent())
                + m_counter.separator() + text;
            child = parent;
        }
    }

    return text.impl();
}

void RenderCounter::calcPrefWidths(int lead)
{
    setTextInternal(originalText());
    RenderText::calcPrefWidths(lead);
}

void RenderCounter::invalidate()
{
    m_counterNode = 0;
    setNeedsLayoutAndPrefWidthsRecalc();
}

static void destroyCounterNodeChildren(AtomicStringImpl* identifier, CounterNode* node)
{
    CounterNode* previous;
    for (CounterNode* child = lastDescendant(node); child && child != node; child = previous) {
        previous = previousInPreOrder(child);
        child->parent()->removeChild(child);
        ASSERT(counterMaps().get(child->renderer())->get(identifier) == child);
        counterMaps().get(child->renderer())->remove(identifier);
        if (!child->renderer()->documentBeingDestroyed()) {
            RenderObjectChildList* children = child->renderer()->virtualChildren();
            if (children)
                children->invalidateCounters(child->renderer());
        }
        delete child;
    }
}

void RenderCounter::destroyCounterNodes(RenderObject* object)
{
    CounterMaps& maps = counterMaps();
    CounterMap* map = maps.get(object);
    if (!map)
        return;
    maps.remove(object);

    CounterMap::const_iterator end = map->end();
    for (CounterMap::const_iterator it = map->begin(); it != end; ++it) {
        CounterNode* node = it->second;
        destroyCounterNodeChildren(it->first.get(), node);
        if (CounterNode* parent = node->parent())
            parent->removeChild(node);
        delete node;
    }

    delete map;
}

} // namespace WebCore
