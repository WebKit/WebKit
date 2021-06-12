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

#pragma once

#include "ExceptionOr.h"
#include <JavaScriptCore/InspectorAuditAgent.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Document;
class Node;

class InspectorAuditAccessibilityObject : public RefCounted<InspectorAuditAccessibilityObject> {
public:
    static Ref<InspectorAuditAccessibilityObject> create(Inspector::InspectorAuditAgent& auditAgent)
    {
        return adoptRef(*new InspectorAuditAccessibilityObject(auditAgent));
    }

    ExceptionOr<Vector<Ref<Node>>> getElementsByComputedRole(Document&, const String& role, Node* container);

    struct ComputedProperties {
        std::optional<bool> busy;
        String checked;
        String currentState;
        std::optional<bool> disabled;
        std::optional<bool> expanded;
        std::optional<bool> focused;
        std::optional<int> headingLevel;
        std::optional<bool> hidden;
        std::optional<int> hierarchicalLevel;
        std::optional<bool> ignored;
        std::optional<bool> ignoredByDefault;
        String invalidStatus;
        std::optional<bool> isPopUpButton;
        String label;
        std::optional<bool> liveRegionAtomic;
        std::optional<Vector<String>> liveRegionRelevant;
        String liveRegionStatus;
        std::optional<bool> pressed;
        std::optional<bool> readonly;
        std::optional<bool> required;
        String role;
        std::optional<bool> selected;
    };

    ExceptionOr<RefPtr<Node>> getActiveDescendant(Node&);
    ExceptionOr<std::optional<Vector<RefPtr<Node>>>> getChildNodes(Node&);
    ExceptionOr<std::optional<ComputedProperties>> getComputedProperties(Node&);
    ExceptionOr<std::optional<Vector<RefPtr<Node>>>> getControlledNodes(Node&);
    ExceptionOr<std::optional<Vector<RefPtr<Node>>>> getFlowedNodes(Node&);
    ExceptionOr<RefPtr<Node>> getMouseEventNode(Node&);
    ExceptionOr<std::optional<Vector<RefPtr<Node>>>> getOwnedNodes(Node&);
    ExceptionOr<RefPtr<Node>> getParentNode(Node&);
    ExceptionOr<std::optional<Vector<RefPtr<Node>>>> getSelectedChildNodes(Node&);

private:
    explicit InspectorAuditAccessibilityObject(Inspector::InspectorAuditAgent&);

    Inspector::InspectorAuditAgent& m_auditAgent;
};

} // namespace WebCore
