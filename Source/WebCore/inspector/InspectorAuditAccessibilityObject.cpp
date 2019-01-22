/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "InspectorAuditAccessibilityObject.h"

#include "AXObjectCache.h"
#include "AccessibilityNodeObject.h"
#include "AccessibilityObject.h"
#include "ContainerNode.h"
#include "Document.h"
#include "Element.h"
#include "ElementDescendantIterator.h"
#include "HTMLNames.h"
#include "Node.h"
#include <wtf/Vector.h>

namespace WebCore {

using namespace Inspector;

#define ERROR_IF_NO_ACTIVE_AUDIT() \
    if (!m_auditAgent.hasActiveAudit()) \
        return Exception { NotAllowedError, "Cannot be called outside of a Web Inspector Audit"_s };

InspectorAuditAccessibilityObject::InspectorAuditAccessibilityObject(InspectorAuditAgent& auditAgent)
    : m_auditAgent(auditAgent)
{
}

static AccessibilityObject* accessiblityObjectForNode(Node& node)
{
    if (!AXObjectCache::accessibilityEnabled())
        AXObjectCache::enableAccessibility();

    if (AXObjectCache* axObjectCache = node.document().axObjectCache())
        return axObjectCache->getOrCreate(&node);

    return nullptr;
}

ExceptionOr<Vector<Ref<Node>>> InspectorAuditAccessibilityObject::getElementsByComputedRole(Document& document, const String& role, Node* container)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Vector<Ref<Node>> nodes;

    for (Element& element : elementDescendants(is<ContainerNode>(container) ? downcast<ContainerNode>(*container) : document)) {
        if (AccessibilityObject* axObject = accessiblityObjectForNode(element)) {
            if (axObject->computedRoleString() == role)
                nodes.append(element);
        }
    }

    return nodes;
}

ExceptionOr<RefPtr<Node>> InspectorAuditAccessibilityObject::getActiveDescendant(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    if (AccessibilityObject* axObject = accessiblityObjectForNode(node)) {
        if (AccessibilityObject* activeDescendant = axObject->activeDescendant())
            return activeDescendant->node();
    }

    return nullptr;
}

static void addChildren(AccessibilityObject& parentObject, Vector<RefPtr<Node>>& childNodes)
{
    for (const RefPtr<AccessibilityObject>& childObject : parentObject.children()) {
        if (Node* childNode = childObject->node())
            childNodes.append(childNode);
        else
            addChildren(*childObject, childNodes);
    }
}

ExceptionOr<Optional<Vector<RefPtr<Node>>>> InspectorAuditAccessibilityObject::getChildNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Optional<Vector<RefPtr<Node>>> result;

    if (AccessibilityObject* axObject = accessiblityObjectForNode(node)) {
        Vector<RefPtr<Node>> childNodes;
        addChildren(*axObject, childNodes);
        result = WTFMove(childNodes);
    }

    return result;
}


ExceptionOr<Optional<Vector<RefPtr<Node>>>> InspectorAuditAccessibilityObject::getControlledNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Optional<Vector<RefPtr<Node>>> result;

    if (AccessibilityObject* axObject = accessiblityObjectForNode(node)) {
        Vector<RefPtr<Node>> controlledNodes;

        Vector<Element*> controlledElements;
        axObject->elementsFromAttribute(controlledElements, HTMLNames::aria_controlsAttr);
        for (Element* controlledElement : controlledElements) {
            if (controlledElement)
                controlledNodes.append(controlledElement);
        }

        result = WTFMove(controlledNodes);
    }

    return result;
}

ExceptionOr<Optional<Vector<RefPtr<Node>>>> InspectorAuditAccessibilityObject::getFlowedNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Optional<Vector<RefPtr<Node>>> result;

    if (AccessibilityObject* axObject = accessiblityObjectForNode(node)) {
        Vector<RefPtr<Node>> flowedNodes;

        Vector<Element*> flowedElements;
        axObject->elementsFromAttribute(flowedElements, HTMLNames::aria_flowtoAttr);
        for (Element* flowedElement : flowedElements) {
            if (flowedElement)
                flowedNodes.append(flowedElement);
        }

        result = WTFMove(flowedNodes);
    }

    return result;
}

ExceptionOr<RefPtr<Node>> InspectorAuditAccessibilityObject::getMouseEventNode(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    if (AccessibilityObject* axObject = accessiblityObjectForNode(node)) {
        if (is<AccessibilityNodeObject>(axObject))
            return downcast<AccessibilityNodeObject>(axObject)->mouseButtonListener(MouseButtonListenerResultFilter::IncludeBodyElement);
    }

    return nullptr;
}

ExceptionOr<Optional<Vector<RefPtr<Node>>>> InspectorAuditAccessibilityObject::getOwnedNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Optional<Vector<RefPtr<Node>>> result;

    if (AccessibilityObject* axObject = accessiblityObjectForNode(node)) {
        if (axObject->supportsARIAOwns()) {
            Vector<RefPtr<Node>> ownedNodes;

            Vector<Element*> ownedElements;
            axObject->elementsFromAttribute(ownedElements, HTMLNames::aria_ownsAttr);
            for (Element* ownedElement : ownedElements) {
                if (ownedElement)
                    ownedNodes.append(ownedElement);
            }

            result = WTFMove(ownedNodes);
        }
    }

    return result;
}

ExceptionOr<RefPtr<Node>> InspectorAuditAccessibilityObject::getParentNode(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    if (AccessibilityObject* axObject = accessiblityObjectForNode(node)) {
        if (AccessibilityObject* parentObject = axObject->parentObjectUnignored())
            return parentObject->node();
    }

    return nullptr;
}

ExceptionOr<Optional<Vector<RefPtr<Node>>>> InspectorAuditAccessibilityObject::getSelectedChildNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Optional<Vector<RefPtr<Node>>> result;

    if (AccessibilityObject* axObject = accessiblityObjectForNode(node)) {
        Vector<RefPtr<Node>> selectedChildNodes;

        AccessibilityObject::AccessibilityChildrenVector selectedChildren;
        axObject->selectedChildren(selectedChildren);
        for (RefPtr<AccessibilityObject>& selectedChildObject : selectedChildren) {
            if (Node* selectedChildNode = selectedChildObject->node())
                selectedChildNodes.append(selectedChildNode);
        }

        result = WTFMove(selectedChildNodes);
    }

    return result;
}

} // namespace WebCore
