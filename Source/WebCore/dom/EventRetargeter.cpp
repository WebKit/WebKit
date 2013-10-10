/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "EventRetargeter.h"

#include "ContainerNode.h"
#include "EventContext.h"
#include "EventDispatcher.h"
#include "FocusEvent.h"
#include "MouseEvent.h"
#include "ShadowRoot.h"
#include "Touch.h"
#include "TouchEvent.h"
#include "TouchList.h"
#include "TreeScope.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

static inline bool shouldEventCrossShadowBoundary(Event& event, ShadowRoot& shadowRoot, EventTarget& target)
{
    Node* targetNode = target.toNode();
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO)
    // Video-only full screen is a mode where we use the shadow DOM as an implementation
    // detail that should not be detectable by the web content.
    if (targetNode) {
        if (Element* element = targetNode->document().webkitCurrentFullScreenElement()) {
            // FIXME: We assume that if the full screen element is a media element that it's
            // the video-only full screen. Both here and elsewhere. But that is probably wrong.
            if (element->isMediaElement() && shadowRoot.hostElement() == element)
                return false;
        }
    }
#endif

    // WebKit never allowed selectstart event to cross the the shadow DOM boundary.
    // Changing this breaks existing sites.
    // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
    const AtomicString& eventType = event.type();
    bool targetIsInShadowRoot = targetNode && targetNode->treeScope().rootNode() == &shadowRoot;
    return !targetIsInShadowRoot
        || !(eventType == eventNames().abortEvent
            || eventType == eventNames().changeEvent
            || eventType == eventNames().errorEvent
            || eventType == eventNames().loadEvent
            || eventType == eventNames().resetEvent
            || eventType == eventNames().resizeEvent
            || eventType == eventNames().scrollEvent
            || eventType == eventNames().selectEvent
            || eventType == eventNames().selectstartEvent);
}

static Node* nodeOrHostIfPseudoElement(Node* node)
{
    return node->isPseudoElement() ? toPseudoElement(node)->hostElement() : node;
}

EventPath::EventPath(Node& targetNode, Event& event)
    : m_origin(&targetNode)
{
    bool inDocument = targetNode.inDocument();
    bool isSVGElement = targetNode.isSVGElement();
    bool isMouseOrFocusEvent = event.isMouseEvent() || event.isFocusEvent();
#if ENABLE(TOUCH_EVENTS)
    bool isTouchEvent = event.isTouchEvent();
#endif
    EventTarget* target = 0;

    Node* node = nodeOrHostIfPseudoElement(&targetNode);
    while (node) {
        if (!target || !isSVGElement) // FIXME: This code doesn't make sense once we've climbed out of the SVG subtree in a HTML document.
            target = &eventTargetRespectingTargetRules(*node);
        for (; node; node = node->parentNode()) {
            EventTarget& currentTarget = eventTargetRespectingTargetRules(*node);
            if (isMouseOrFocusEvent)
                m_path.append(std::make_unique<MouseOrFocusEventContext>(node, &currentTarget, target));
#if ENABLE(TOUCH_EVENTS)
            else if (isTouchEvent)
                m_path.append(std::make_unique<TouchEventContext>(node, &currentTarget, target));
#endif
            else
                m_path.append(std::make_unique<EventContext>(node, &currentTarget, target));
            if (!inDocument)
                return;
            if (node->isShadowRoot())
                break;
        }
        if (!node || !shouldEventCrossShadowBoundary(event, *toShadowRoot(node), *target))
            return;
        node = toShadowRoot(node)->hostElement();
    }
}

static void buildRelatedNodeMap(const Node* relatedNode, HashMap<TreeScope*, Node*>& relatedNodeMap)
{
    Node* relatedNodeInCurrentTree = 0;
    TreeScope* lastTreeScope = 0;
    for (Node* node = nodeOrHostIfPseudoElement(const_cast<Node*>(relatedNode)); node; node = node->parentOrShadowHostNode()) {
        if (!relatedNodeInCurrentTree)
            relatedNodeInCurrentTree = node;
        TreeScope* scope = &node->treeScope();
        // Skips adding a node to the map if treeScope does not change. Just for the performance optimization.
        if (scope != lastTreeScope)
            relatedNodeMap.add(scope, relatedNodeInCurrentTree);
        lastTreeScope = scope;
        if (node->isShadowRoot()) {
            ASSERT(relatedNodeInCurrentTree);
            relatedNodeInCurrentTree = 0;
        }
    }
}

static Node* addRelatedNodeForUnmapedTreeScopes(TreeScope* scope, HashMap<TreeScope*, Node*>& relatedNodeMap)
{
    Node* relatedNode = 0;
    TreeScope* endScope = 0;
    for (TreeScope* currentScope = scope; currentScope; currentScope = currentScope->parentTreeScope()) {
        auto result = relatedNodeMap.find(currentScope);
        if (result != relatedNodeMap.end()) {
            relatedNode = result->value;
            endScope = currentScope;
            break;
        }
    }
    for (TreeScope* currentScope = scope; currentScope != endScope; currentScope = currentScope->parentTreeScope())
        relatedNodeMap.add(currentScope, relatedNode);
    return relatedNode;
}

enum EventWithRelatedTargetDispatchBehavior {
    StopAtBoundaryIfNeeded,
    DoesNotStopAtBoundary
};
static size_t calculateAdjustedNodes(const Node* node, const Node* relatedNode, EventWithRelatedTargetDispatchBehavior eventWithRelatedTargetDispatchBehavior, const EventPath& eventPath, Vector<RefPtr<Node>>& adjustedNodes)
{
    HashMap<TreeScope*, Node*> relatedNodeMap;
    buildRelatedNodeMap(relatedNode, relatedNodeMap);

    // Synthetic mouse events can have a relatedTarget which is identical to the target.
    bool targetIsIdenticalToToRelatedTarget = (node == relatedNode);

    TreeScope* lastTreeScope = 0;
    Node* adjustedNode = 0;
    size_t eventPathSize = eventPath.size();
    for (size_t i = 0; i < eventPathSize; i++) {
        const EventContext& context = eventPath.contextAt(i);
        TreeScope* scope = &context.node()->treeScope();

        // Re-use the previous adjustedRelatedTarget if treeScope does not change. Just for the performance optimization.
        if (scope != lastTreeScope)
            adjustedNode = addRelatedNodeForUnmapedTreeScopes(scope, relatedNodeMap);
        adjustedNodes.append(adjustedNode);

        lastTreeScope = scope;
        if (eventWithRelatedTargetDispatchBehavior == DoesNotStopAtBoundary)
            continue;
        if (targetIsIdenticalToToRelatedTarget) {
            if (node->treeScope().rootNode() == context.node())
                return i + 1;
        } else if (context.target() == adjustedNode) {
            // Event dispatching should be stopped here.
            adjustedNodes.shrink(adjustedNodes.size() - 1);
            return i;
        }
    }
    return eventPathSize;
}

#if ENABLE(TOUCH_EVENTS)
static void updateTouchListsInEventPath(Vector<RefPtr<TouchList>>& touchListsInEventPath, const Node* node, const TouchList* touchList, const EventPath& eventPath)
{
    if (!touchList)
        return;
    size_t eventPathSize = eventPath.size();
    ASSERT(touchListsInEventPath.size() == eventPathSize);
    for (size_t i = 0; i < touchList->length(); ++i) {
        const Touch& touch = *touchList->item(i);

        Vector<RefPtr<Node>> adjustedNodes;
        calculateAdjustedNodes(node, touch.target()->toNode(), DoesNotStopAtBoundary, eventPath, adjustedNodes);

        ASSERT(adjustedNodes.size() == eventPathSize);
        for (size_t j = 0; j < eventPathSize; ++j)
            touchListsInEventPath[j]->append(touch.cloneWithNewTarget(adjustedNodes[j].get()));
    }
}

void EventPath::updateTouchLists(const TouchEvent& touchEvent)
{
    size_t eventPathSize = m_path.size();

    Vector<RefPtr<TouchList>> touchesInEventPath(eventPathSize);
    Vector<RefPtr<TouchList>> targetTouchesInEventPath(eventPathSize);
    Vector<RefPtr<TouchList>> changedTouchesInEventPath(eventPathSize);

    for (size_t i = 0; i < eventPathSize; ++i) {
        TouchEventContext& context = *toTouchEventContext(m_path[i].get());
        touchesInEventPath[i] = context.touches();
        targetTouchesInEventPath[i] = context.targetTouches();
        changedTouchesInEventPath[i] = context.changedTouches();
    }

    updateTouchListsInEventPath(touchesInEventPath, m_origin.get(), touchEvent.touches(), *this);
    updateTouchListsInEventPath(targetTouchesInEventPath, m_origin.get(), touchEvent.targetTouches(), *this);
    updateTouchListsInEventPath(changedTouchesInEventPath, m_origin.get(), touchEvent.changedTouches(), *this);
}
#endif

void EventPath::setRelatedTarget(EventTarget& relatedTarget)
{
    Node* relatedNode = relatedTarget.toNode();
    if (!relatedNode)
        return;
    Vector<RefPtr<Node>> adjustedNodes;
    m_path.shrink(calculateAdjustedNodes(m_origin.get(), relatedNode, StopAtBoundaryIfNeeded, *this, adjustedNodes));
    ASSERT(adjustedNodes.size() <= m_path.size());
    for (size_t i = 0; i < adjustedNodes.size(); ++i)
        toMouseOrFocusEventContext(*m_path[i]).setRelatedTarget(adjustedNodes[i]);
}

}
