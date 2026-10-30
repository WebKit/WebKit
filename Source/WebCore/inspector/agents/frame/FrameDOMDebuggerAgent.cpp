/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameDOMDebuggerAgent.h"

#include "CommonVM.h"
#include "Document.h"
#include "Element.h"
#include "FrameDOMAgent.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameInspectorController.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "NodeDocument.h"
#include <JavaScriptCore/CallFrameInlines.h>
#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/StackVisitor.h>
#include <JavaScriptCore/VM.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameDOMDebuggerAgent);

FrameDOMDebuggerAgent::FrameDOMDebuggerAgent(FrameAgentContext& context, InspectorDebuggerAgent* debuggerAgent)
    : InspectorDOMDebuggerAgent(context, debuggerAgent)
    , m_inspectedFrame(context.inspectedFrame)
{
}

FrameDOMDebuggerAgent::~FrameDOMDebuggerAgent() = default;

void FrameDOMDebuggerAgent::enable()
{
    protect(m_instrumentingAgents)->setEnabledFrameDOMDebuggerAgent(this);

    InspectorDOMDebuggerAgent::enable();
}

void FrameDOMDebuggerAgent::disable()
{
    protect(m_instrumentingAgents)->setEnabledFrameDOMDebuggerAgent(nullptr);

    clearDOMBreakpoints();

    InspectorDOMDebuggerAgent::disable();
}

FrameDOMDebuggerAgent::DOMBreakpointMap& FrameDOMDebuggerAgent::domBreakpoints(Inspector::Protocol::DOMDebugger::DOMBreakpointType type)
{
    switch (type) {
    case Inspector::Protocol::DOMDebugger::DOMBreakpointType::SubtreeModified:
        return m_subtreeModifiedBreakpoints;
    case Inspector::Protocol::DOMDebugger::DOMBreakpointType::AttributeModified:
        return m_attributeModifiedBreakpoints;
    case Inspector::Protocol::DOMDebugger::DOMBreakpointType::NodeRemoved:
        return m_nodeRemovedBreakpoints;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void FrameDOMDebuggerAgent::clearDOMBreakpoints()
{
    m_subtreeModifiedBreakpoints.clear();
    m_attributeModifiedBreakpoints.clear();
    m_nodeRemovedBreakpoints.clear();
}

Inspector::Protocol::ErrorStringOr<void> FrameDOMDebuggerAgent::setDOMBreakpoint(Inspector::Protocol::DOM::NodeId nodeId, Inspector::Protocol::DOMDebugger::DOMBreakpointType type, RefPtr<JSON::Object>&& options)
{
    CheckedPtr domAgent = protect(m_instrumentingAgents)->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    Inspector::Protocol::ErrorString errorString;
    RefPtr node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    // FIXME: <https://webkit.org/b/328682> Mutation hooks only reach the agent of the node's own frame, so nodes of nested frames and of template contents could never pause.
    if (node->document().frame() != m_inspectedFrame.ptr())
        return makeUnexpected("Not supported for nodes outside the frame's document"_s);

    auto breakpoint = InspectorDebuggerAgent::debuggerBreakpointFromPayload(errorString, WTF::move(options));
    if (!breakpoint)
        return makeUnexpected(errorString);

    if (!domBreakpoints(type).add(*node, breakpoint.releaseNonNull()))
        return makeUnexpected("Breakpoint for given node and given type already exists"_s);
    return { };
}

Inspector::Protocol::ErrorStringOr<void> FrameDOMDebuggerAgent::removeDOMBreakpoint(Inspector::Protocol::DOM::NodeId nodeId, Inspector::Protocol::DOMDebugger::DOMBreakpointType type)
{
    CheckedPtr domAgent = protect(m_instrumentingAgents)->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    Inspector::Protocol::ErrorString errorString;
    RefPtr node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    if (!domBreakpoints(type).remove(*node))
        return makeUnexpected("Breakpoint for given node and given type missing"_s);
    return { };
}

void FrameDOMDebuggerAgent::frameDocumentUpdated()
{
    clearDOMBreakpoints();
}

// Matching follows PageDOMDebuggerAgent: the breakpoint owner closest to the mutated node wins.

void FrameDOMDebuggerAgent::willInsertDOMNode(Node& parent)
{
    if (m_subtreeModifiedBreakpoints.isEmptyIgnoringNullReferences())
        return;

    for (RefPtr owner = &parent; owner; owner = InspectorDOMAgent::innerParentNode(owner.get())) {
        if (RefPtr breakpoint = m_subtreeModifiedBreakpoints.get(*owner)) {
            breakOnDOMBreakpoint(Inspector::Protocol::DOMDebugger::DOMBreakpointType::SubtreeModified, *owner, breakpoint.releaseNonNull(), nullptr, true);
            return;
        }
    }
}

void FrameDOMDebuggerAgent::willRemoveDOMNode(Node& node)
{
    RefPtr<Node> closestOwner;
    RefPtr<JSC::Breakpoint> closestBreakpoint;
    std::optional<size_t> closestDistance;
    for (auto entry : m_nodeRemovedBreakpoints) {
        size_t distance = 0;
        RefPtr current = &entry.key;
        while (current && current != &node) {
            ++distance;
            current = InspectorDOMAgent::innerParentNode(current.get());
        }
        if (!current)
            continue;

        if (!closestDistance || distance < *closestDistance) {
            closestDistance = distance;
            closestOwner = &entry.key;
            closestBreakpoint = entry.value.ptr();
        }
    }

    if (closestBreakpoint) {
        breakOnDOMBreakpoint(Inspector::Protocol::DOMDebugger::DOMBreakpointType::NodeRemoved, *closestOwner, closestBreakpoint.releaseNonNull(), &node);
        return;
    }

    if (m_subtreeModifiedBreakpoints.isEmptyIgnoringNullReferences())
        return;

    for (RefPtr owner = &node; owner; owner = InspectorDOMAgent::innerParentNode(owner.get())) {
        if (RefPtr breakpoint = m_subtreeModifiedBreakpoints.get(*owner)) {
            breakOnDOMBreakpoint(Inspector::Protocol::DOMDebugger::DOMBreakpointType::SubtreeModified, *owner, breakpoint.releaseNonNull(), &node);
            return;
        }
    }
}

void FrameDOMDebuggerAgent::didRemoveDOMNode(Node& node)
{
    auto isInRemovedSubtree = [node = Ref { node }](auto& entry) {
        return node->contains(entry.key);
    };
    m_subtreeModifiedBreakpoints.removeIf(isInRemovedSubtree);
    m_attributeModifiedBreakpoints.removeIf(isInRemovedSubtree);
    m_nodeRemovedBreakpoints.removeIf(isInRemovedSubtree);
}

void FrameDOMDebuggerAgent::willModifyDOMAttr(Element& element)
{
    if (m_attributeModifiedBreakpoints.isEmptyIgnoringNullReferences())
        return;

    if (RefPtr breakpoint = m_attributeModifiedBreakpoints.get(element))
        breakOnDOMBreakpoint(Inspector::Protocol::DOMDebugger::DOMBreakpointType::AttributeModified, element, breakpoint.releaseNonNull());
}

// The debugger pauses on the top call frame, and shows the first frame with script. A DOM method called by script of another
// frame runs in the realm of the node's frame, with the other frame's script below it.
bool FrameDOMDebuggerAgent::canPauseRunningScript()
{
    auto* debugger = protect(m_inspectedFrame)->inspectorController().debugger();
    auto& vm = commonVM();
    auto* topCallFrame = vm.topCallFrame;
    if (!debugger || !topCallFrame || !debugger->isAttached(topCallFrame->lexicalGlobalObject(vm)))
        return false;

    bool attached = false;
    JSC::StackVisitor::visit(topCallFrame, vm, [&](auto& visitor) {
        if (!visitor->codeBlock())
            return IterationStatus::Continue;
        attached = debugger->isAttached(visitor->codeBlock()->globalObject());
        return IterationStatus::Done;
    });
    return attached;
}

void FrameDOMDebuggerAgent::breakOnDOMBreakpoint(Inspector::Protocol::DOMDebugger::DOMBreakpointType type, Node& breakpointOwner, Ref<JSC::Breakpoint>&& breakpoint, Node* targetNode, bool insertion)
{
    CheckedPtr debuggerAgent = m_debuggerAgent;
    if (!debuggerAgent || !debuggerAgent->breakpointsActive())
        return;

    // FIXME: <https://webkit.org/b/328682> Script of another frame in this process can also mutate this frame's nodes, but
    // this frame's debugger cannot pause it.
    if (!canPauseRunningScript())
        return;

    auto pauseData = JSON::Object::create();
    pauseData->setString("type"_s, Inspector::Protocol::Helpers::getEnumConstantValue(type));
    pauseData->setBoolean("frameTarget"_s, true);
    if (CheckedPtr domAgent = protect(m_instrumentingAgents)->persistentFrameDOMAgent()) {
        if (auto nodeId = domAgent->pushNodePathToFrontend(&breakpointOwner))
            pauseData->setInteger("nodeId"_s, nodeId);
        if (targetNode && targetNode != &breakpointOwner) {
            if (auto targetNodeId = domAgent->pushNodePathToFrontend(targetNode))
                pauseData->setInteger("targetNodeId"_s, targetNodeId);
        }
    }
    if (insertion)
        pauseData->setBoolean("insertion"_s, true);

    debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTF::move(pauseData), WTF::move(breakpoint));
}

} // namespace WebCore
