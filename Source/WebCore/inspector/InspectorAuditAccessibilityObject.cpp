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
#include "AccessibilityObjectInterface.h"
#include "ContainerNode.h"
#include "Document.h"
#include "ElementIterator.h"
#include "HTMLNames.h"
#include "SpaceSplitString.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

#define ERROR_IF_NO_ACTIVE_AUDIT() \
    if (!m_auditAgent.hasActiveAudit()) \
        return Exception { NotAllowedError, "Cannot be called outside of a Web Inspector Audit"_s };

InspectorAuditAccessibilityObject::InspectorAuditAccessibilityObject(InspectorAuditAgent& auditAgent)
    : m_auditAgent(auditAgent)
{
}

static AXCoreObject* accessiblityObjectForNode(Node& node)
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

    for (Element& element : descendantsOfType<Element>(is<ContainerNode>(container) ? downcast<ContainerNode>(*container) : document)) {
        if (AXCoreObject* axObject = accessiblityObjectForNode(element)) {
            if (axObject->computedRoleString() == role)
                nodes.append(element);
        }
    }

    return nodes;
}

ExceptionOr<RefPtr<Node>> InspectorAuditAccessibilityObject::getActiveDescendant(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    if (AXCoreObject* axObject = accessiblityObjectForNode(node)) {
        if (AXCoreObject* activeDescendant = axObject->activeDescendant())
            return activeDescendant->node();
    }

    return nullptr;
}

static void addChildren(AXCoreObject& parentObject, Vector<RefPtr<Node>>& childNodes)
{
    for (const auto& childObject : parentObject.children()) {
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

    if (AXCoreObject* axObject = accessiblityObjectForNode(node)) {
        Vector<RefPtr<Node>> childNodes;
        addChildren(*axObject, childNodes);
        result = WTFMove(childNodes);
    }

    return result;
}

ExceptionOr<Optional<InspectorAuditAccessibilityObject::ComputedProperties>> InspectorAuditAccessibilityObject::getComputedProperties(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Optional<InspectorAuditAccessibilityObject::ComputedProperties> result;

    if (AXCoreObject* axObject = accessiblityObjectForNode(node)) {
        ComputedProperties computedProperties;

        AXCoreObject* current = axObject;
        while (current && (!computedProperties.busy || !computedProperties.busy.value())) {
            computedProperties.busy = current->isBusy();
            current = current->parentObject();
        }

        if (axObject->supportsChecked()) {
            AccessibilityButtonState checkValue = axObject->checkboxOrRadioValue();
            if (checkValue == AccessibilityButtonState::On)
                computedProperties.checked = "true"_s;
            else if (checkValue == AccessibilityButtonState::Mixed)
                computedProperties.checked = "mixed"_s;
            else if (axObject->isChecked())
                computedProperties.checked = "true"_s;
            else
                computedProperties.checked = "false"_s;
        }

        switch (axObject->currentState()) {
        case AccessibilityCurrentState::False:
            computedProperties.currentState = "false"_s;
            break;
        case AccessibilityCurrentState::True:
            computedProperties.currentState = "true"_s;
            break;
        case AccessibilityCurrentState::Page:
            computedProperties.currentState = "page"_s;
            break;
        case AccessibilityCurrentState::Step:
            computedProperties.currentState = "step"_s;
            break;
        case AccessibilityCurrentState::Location:
            computedProperties.currentState = "location"_s;
            break;
        case AccessibilityCurrentState::Date:
            computedProperties.currentState = "date"_s;
            break;
        case AccessibilityCurrentState::Time:
            computedProperties.currentState = "time"_s;
            break;
        }

        computedProperties.disabled = !axObject->isEnabled();

        if (axObject->supportsExpanded())
            computedProperties.expanded = axObject->isExpanded();

        if (is<Element>(node) && axObject->canSetFocusAttribute())
            computedProperties.focused = axObject->isFocused();

        computedProperties.headingLevel = axObject->headingLevel();
        computedProperties.hidden = axObject->isAXHidden() || axObject->isDOMHidden();
        computedProperties.hierarchicalLevel = axObject->hierarchicalLevel();
        computedProperties.ignored = axObject->accessibilityIsIgnored();
        computedProperties.ignoredByDefault = axObject->accessibilityIsIgnoredByDefault();

        String invalidValue = axObject->invalidStatus();
        if (invalidValue == "false")
            computedProperties.invalidStatus = "false"_s;
        else if (invalidValue == "grammar")
            computedProperties.invalidStatus = "grammar"_s;
        else if (invalidValue == "spelling")
            computedProperties.invalidStatus = "spelling"_s;
        else
            computedProperties.invalidStatus = "true"_s;

        computedProperties.isPopUpButton = axObject->isPopUpButton() || axObject->hasPopup();
        computedProperties.label = axObject->computedLabel();

        if (axObject->supportsLiveRegion()) {
            computedProperties.liveRegionAtomic = axObject->liveRegionAtomic();

            String ariaRelevantAttrValue = axObject->liveRegionRelevant();
            if (!ariaRelevantAttrValue.isEmpty()) {
                Vector<String> liveRegionRelevant;
                String ariaRelevantAdditions = "additions";
                String ariaRelevantRemovals = "removals";
                String ariaRelevantText = "text";

                const auto& values = SpaceSplitString(ariaRelevantAttrValue, true);
                if (values.contains("all")) {
                    liveRegionRelevant.append(ariaRelevantAdditions);
                    liveRegionRelevant.append(ariaRelevantRemovals);
                    liveRegionRelevant.append(ariaRelevantText);
                } else {
                    if (values.contains(ariaRelevantAdditions))
                        liveRegionRelevant.append(ariaRelevantAdditions);
                    if (values.contains(ariaRelevantRemovals))
                        liveRegionRelevant.append(ariaRelevantRemovals);
                    if (values.contains(ariaRelevantText))
                        liveRegionRelevant.append(ariaRelevantText);
                }
                computedProperties.liveRegionRelevant = liveRegionRelevant;
            }

            computedProperties.liveRegionStatus = axObject->liveRegionStatus();
        }

        computedProperties.pressed = axObject->pressedIsPresent() && axObject->isPressed();

        if (axObject->isTextControl())
            computedProperties.readonly = !axObject->canSetValueAttribute();

        if (axObject->supportsRequiredAttribute())
            computedProperties.required = axObject->isRequired();

        computedProperties.role = axObject->computedRoleString();
        computedProperties.selected = axObject->isSelected();

        result = computedProperties;
    }

    return result;
}

ExceptionOr<Optional<Vector<RefPtr<Node>>>> InspectorAuditAccessibilityObject::getControlledNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Optional<Vector<RefPtr<Node>>> result;

    if (AXCoreObject* axObject = accessiblityObjectForNode(node)) {
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

    if (AXCoreObject* axObject = accessiblityObjectForNode(node)) {
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

    if (AXCoreObject* axObject = accessiblityObjectForNode(node)) {
        if (is<AccessibilityNodeObject>(axObject))
            return downcast<AccessibilityNodeObject>(axObject)->mouseButtonListener(MouseButtonListenerResultFilter::IncludeBodyElement);
    }

    return nullptr;
}

ExceptionOr<Optional<Vector<RefPtr<Node>>>> InspectorAuditAccessibilityObject::getOwnedNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Optional<Vector<RefPtr<Node>>> result;

    if (AXCoreObject* axObject = accessiblityObjectForNode(node)) {
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

    if (AXCoreObject* axObject = accessiblityObjectForNode(node)) {
        if (AXCoreObject* parentObject = axObject->parentObjectUnignored())
            return parentObject->node();
    }

    return nullptr;
}

ExceptionOr<Optional<Vector<RefPtr<Node>>>> InspectorAuditAccessibilityObject::getSelectedChildNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Optional<Vector<RefPtr<Node>>> result;

    if (AXCoreObject* axObject = accessiblityObjectForNode(node)) {
        Vector<RefPtr<Node>> selectedChildNodes;

        AXCoreObject::AccessibilityChildrenVector selectedChildren;
        axObject->selectedChildren(selectedChildren);
        for (auto& selectedChildObject : selectedChildren) {
            if (Node* selectedChildNode = selectedChildObject->node())
                selectedChildNodes.append(selectedChildNode);
        }

        result = WTFMove(selectedChildNodes);
    }

    return result;
}

} // namespace WebCore
