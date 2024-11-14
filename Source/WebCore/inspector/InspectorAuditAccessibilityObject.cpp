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

#include "AXCoreObject.h"
#include "AXObjectCache.h"
#include "AccessibilityNodeObject.h"
#include "ContainerNode.h"
#include "Document.h"
#include "HTMLNames.h"
#include "SpaceSplitString.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

#define ERROR_IF_NO_ACTIVE_AUDIT() \
    if (!m_auditAgent.hasActiveAudit()) \
        return Exception { ExceptionCode::NotAllowedError, "Cannot be called outside of a Web Inspector Audit"_s };

InspectorAuditAccessibilityObject::InspectorAuditAccessibilityObject(InspectorAuditAgent& auditAgent)
    : m_auditAgent(auditAgent)
{
}

static AccessibilityObject* accessibilityObjectForNode(Node& node)
{
    if (!AXObjectCache::accessibilityEnabled())
        AXObjectCache::enableAccessibility();

    if (AXObjectCache* axObjectCache = node.document().axObjectCache())
        return axObjectCache->getOrCreate(node);

    return nullptr;
}

ExceptionOr<Vector<Ref<Node>>> InspectorAuditAccessibilityObject::getElementsByComputedRole(Document& document, const String& role, Node* container)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Vector<Ref<Node>> nodes;

    auto* containerNode = dynamicDowncast<ContainerNode>(container);
    for (Element& element : descendantsOfType<Element>(containerNode ? *containerNode : document)) {
        if (auto* axObject = accessibilityObjectForNode(element)) {
            if (axObject->computedRoleString() == role)
                nodes.append(element);
        }
    }

    return nodes;
}

ExceptionOr<RefPtr<Node>> InspectorAuditAccessibilityObject::getActiveDescendant(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    if (auto* axObject = accessibilityObjectForNode(node)) {
        if (AXCoreObject* activeDescendant = axObject->activeDescendant())
            return activeDescendant->node();
    }

    return nullptr;
}

static void addChildren(AXCoreObject& parentObject, Vector<Ref<Node>>& childNodes)
{
    for (const auto& childObject : parentObject.children()) {
        if (RefPtr childNode = childObject->node())
            childNodes.append(childNode.releaseNonNull());
        else
            addChildren(*childObject, childNodes);
    }
}

ExceptionOr<std::optional<Vector<Ref<Node>>>> InspectorAuditAccessibilityObject::getChildNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    std::optional<Vector<Ref<Node>>> result;

    if (auto* axObject = accessibilityObjectForNode(node)) {
        Vector<Ref<Node>> childNodes;
        addChildren(*axObject, childNodes);
        result = WTFMove(childNodes);
    }

    return result;
}

ExceptionOr<std::optional<InspectorAuditAccessibilityObject::ComputedProperties>> InspectorAuditAccessibilityObject::getComputedProperties(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    std::optional<InspectorAuditAccessibilityObject::ComputedProperties> result;

    if (auto* axObject = accessibilityObjectForNode(node)) {
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
        computedProperties.hidden = axObject->isHidden();
        computedProperties.hierarchicalLevel = axObject->hierarchicalLevel();
        computedProperties.ignored = axObject->isIgnored();
        computedProperties.ignoredByDefault = axObject->isIgnoredByDefault();

        String invalidValue = axObject->invalidStatus();
        if (invalidValue == "false"_s)
            computedProperties.invalidStatus = "false"_s;
        else if (invalidValue == "grammar"_s)
            computedProperties.invalidStatus = "grammar"_s;
        else if (invalidValue == "spelling"_s)
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
                AtomString ariaRelevantAdditions = "additions"_s;
                AtomString ariaRelevantRemovals = "removals"_s;
                AtomString ariaRelevantText = "text"_s;

                const auto& values = SpaceSplitString(AtomString { ariaRelevantAttrValue }, SpaceSplitString::ShouldFoldCase::Yes);
                if (values.contains("all"_s)) {
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

ExceptionOr<std::optional<Vector<Ref<Node>>>> InspectorAuditAccessibilityObject::getControlledNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    std::optional<Vector<Ref<Node>>> result;

    if (auto* axObject = accessibilityObjectForNode(node)) {
        auto controlledElements = axObject->elementsFromAttribute(HTMLNames::aria_controlsAttr);
        result = WTF::map(WTFMove(controlledElements), [](auto&& element) -> Ref<Node> {
            return WTFMove(element);
        });
    }

    return result;
}

ExceptionOr<std::optional<Vector<Ref<Node>>>> InspectorAuditAccessibilityObject::getFlowedNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    std::optional<Vector<Ref<Node>>> result;

    if (auto* axObject = accessibilityObjectForNode(node)) {
        auto flowedElements = axObject->elementsFromAttribute(HTMLNames::aria_flowtoAttr);
        result = WTF::map(WTFMove(flowedElements), [](auto&& element) -> Ref<Node> {
            return WTFMove(element);
        });
    }

    return result;
}

ExceptionOr<RefPtr<Node>> InspectorAuditAccessibilityObject::getMouseEventNode(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    if (auto* axObject = accessibilityObjectForNode(node)) {
        if (auto* clickableObject = axObject->clickableSelfOrAncestor(ClickHandlerFilter::IncludeBody))
            return clickableObject->node();
    }

    return nullptr;
}

ExceptionOr<std::optional<Vector<Ref<Node>>>> InspectorAuditAccessibilityObject::getOwnedNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    std::optional<Vector<Ref<Node>>> result;

    if (auto* axObject = accessibilityObjectForNode(node)) {
        if (axObject->supportsARIAOwns()) {
            auto ownedElements = axObject->elementsFromAttribute(HTMLNames::aria_ownsAttr);
            result = WTF::map(WTFMove(ownedElements), [](auto&& element) -> Ref<Node> {
                return WTFMove(element);
            });
        }
    }

    return result;
}

ExceptionOr<RefPtr<Node>> InspectorAuditAccessibilityObject::getParentNode(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    if (auto* axObject = accessibilityObjectForNode(node)) {
        if (AXCoreObject* parentObject = axObject->parentObjectUnignored())
            return parentObject->node();
    }

    return nullptr;
}

ExceptionOr<std::optional<Vector<Ref<Node>>>> InspectorAuditAccessibilityObject::getSelectedChildNodes(Node& node)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    std::optional<Vector<Ref<Node>>> result;

    if (auto* axObject = accessibilityObjectForNode(node)) {
        Vector<Ref<Node>> selectedChildNodes;

        if (auto selectedChildren = axObject->selectedChildren()) {
            for (auto& selectedChildObject : *selectedChildren) {
                if (RefPtr selectedChildNode = selectedChildObject->node())
                    selectedChildNodes.append(selectedChildNode.releaseNonNull());
            }
        }

        result = WTFMove(selectedChildNodes);
    }

    return result;
}

} // namespace WebCore
