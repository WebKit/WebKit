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
#include "PageDOMDebuggerAgent.h"

#include "Element.h"
#include "Frame.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "Node.h"

namespace WebCore {

using namespace Inspector;

enum DOMBreakpointType {
    SubtreeModified,
    AttributeModified,
    NodeRemoved,
};

const uint32_t inheritableDOMBreakpointTypesMask = (1 << SubtreeModified);
const int domBreakpointDerivedTypeShift = 16;

static int domTypeForName(ErrorString& errorString, const String& typeString)
{
    if (typeString == "subtree-modified")
        return SubtreeModified;
    if (typeString == "attribute-modified")
        return AttributeModified;
    if (typeString == "node-removed")
        return NodeRemoved;
    errorString = makeString("Unknown DOM breakpoint type: ", typeString);
    return -1;
}

static String domTypeName(int type)
{
    switch (type) {
    case SubtreeModified:
        return "subtree-modified"_s;
    case AttributeModified:
        return "attribute-modified"_s;
    case NodeRemoved:
        return "node-removed"_s;
    }
    return emptyString();
}

PageDOMDebuggerAgent::PageDOMDebuggerAgent(PageAgentContext& context, InspectorDebuggerAgent* debuggerAgent)
    : InspectorDOMDebuggerAgent(context, debuggerAgent)
{
}

PageDOMDebuggerAgent::~PageDOMDebuggerAgent() = default;

bool PageDOMDebuggerAgent::enabled() const
{
    return m_instrumentingAgents.enabledPageDOMDebuggerAgent() == this && InspectorDOMDebuggerAgent::enabled();
}

void PageDOMDebuggerAgent::enable()
{
    m_instrumentingAgents.setEnabledPageDOMDebuggerAgent(this);

    InspectorDOMDebuggerAgent::enable();
}

void PageDOMDebuggerAgent::disable()
{
    m_instrumentingAgents.setEnabledPageDOMDebuggerAgent(nullptr);

    m_domBreakpoints.clear();

    m_pauseOnAllAnimationFramesBreakpoint = nullptr;

    InspectorDOMDebuggerAgent::disable();
}

void PageDOMDebuggerAgent::setDOMBreakpoint(ErrorString& errorString, int nodeId, const String& typeString)
{
    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent) {
        errorString = "DOM domain must be enabled"_s;
        return;
    }

    Node* node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    int type = domTypeForName(errorString, typeString);
    if (type == -1)
        return;

    uint32_t rootBit = 1 << type;
    m_domBreakpoints.set(node, m_domBreakpoints.get(node) | rootBit);
    if (rootBit & inheritableDOMBreakpointTypesMask) {
        for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
            updateSubtreeBreakpoints(child, rootBit, true);
    }
}

void PageDOMDebuggerAgent::removeDOMBreakpoint(ErrorString& errorString, int nodeId, const String& typeString)
{
    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent) {
        errorString = "DOM domain must be enabled"_s;
        return;
    }

    Node* node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    int type = domTypeForName(errorString, typeString);
    if (type == -1)
        return;

    uint32_t rootBit = 1 << type;
    uint32_t mask = m_domBreakpoints.get(node) & ~rootBit;
    if (mask)
        m_domBreakpoints.set(node, mask);
    else
        m_domBreakpoints.remove(node);

    if ((rootBit & inheritableDOMBreakpointTypesMask) && !(mask & (rootBit << domBreakpointDerivedTypeShift))) {
        for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
            updateSubtreeBreakpoints(child, rootBit, false);
    }
}

void PageDOMDebuggerAgent::mainFrameNavigated()
{
    InspectorDOMDebuggerAgent::mainFrameNavigated();

    if (m_pauseOnAllAnimationFramesBreakpoint)
        m_pauseOnAllAnimationFramesBreakpoint->resetHitCount();
}

void PageDOMDebuggerAgent::frameDocumentUpdated(Frame& frame)
{
    if (!frame.isMainFrame())
        return;

    m_domBreakpoints.clear();
}

void PageDOMDebuggerAgent::willInsertDOMNode(Node& parent)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    if (hasBreakpoint(&parent, SubtreeModified)) {
        Ref<JSON::Object> eventData = JSON::Object::create();
        descriptionForDOMEvent(parent, SubtreeModified, true, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
    }
}

void PageDOMDebuggerAgent::didInsertDOMNode(Node& node)
{
    if (m_domBreakpoints.size()) {
        uint32_t mask = m_domBreakpoints.get(InspectorDOMAgent::innerParentNode(&node));
        uint32_t inheritableTypesMask = (mask | (mask >> domBreakpointDerivedTypeShift)) & inheritableDOMBreakpointTypesMask;
        if (inheritableTypesMask)
            updateSubtreeBreakpoints(&node, inheritableTypesMask, true);
    }
}

void PageDOMDebuggerAgent::willRemoveDOMNode(Node& node)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    if (hasBreakpoint(&node, NodeRemoved)) {
        auto eventData = JSON::Object::create();
        descriptionForDOMEvent(node, NodeRemoved, false, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
        return;
    }

    uint32_t rootBit = 1 << NodeRemoved;
    uint32_t derivedBit = rootBit << domBreakpointDerivedTypeShift;
    uint32_t matchBit = rootBit | derivedBit;
    for (auto& [nodeWithBreakpoint, breakpointTypes] : m_domBreakpoints) {
        if (node.contains(nodeWithBreakpoint) && (breakpointTypes & matchBit)) {
            auto eventData = JSON::Object::create();
            descriptionForDOMEvent(*nodeWithBreakpoint, NodeRemoved, false, eventData.get());
            if (auto* domAgent = m_instrumentingAgents.persistentDOMAgent())
                eventData->setInteger("targetNodeId"_s, domAgent->pushNodeToFrontend(&node));
            m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
            return;
        }
    }

    auto* parentNode = InspectorDOMAgent::innerParentNode(&node);
    if (parentNode && hasBreakpoint(parentNode, SubtreeModified)) {
        auto eventData = JSON::Object::create();
        descriptionForDOMEvent(node, SubtreeModified, false, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
        return;
    }
}

void PageDOMDebuggerAgent::didRemoveDOMNode(Node& node)
{
    if (m_domBreakpoints.size()) {
        // Remove subtree breakpoints.
        m_domBreakpoints.remove(&node);
        Vector<Node*> stack(1, InspectorDOMAgent::innerFirstChild(&node));
        do {
            Node* node = stack.last();
            stack.removeLast();
            if (!node)
                continue;
            m_domBreakpoints.remove(node);
            stack.append(InspectorDOMAgent::innerFirstChild(node));
            stack.append(InspectorDOMAgent::innerNextSibling(node));
        } while (!stack.isEmpty());
    }
}

void PageDOMDebuggerAgent::willModifyDOMAttr(Element& element)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    if (hasBreakpoint(&element, AttributeModified)) {
        Ref<JSON::Object> eventData = JSON::Object::create();
        descriptionForDOMEvent(element, AttributeModified, false, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
    }
}

void PageDOMDebuggerAgent::willFireAnimationFrame()
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    auto breakpoint = m_pauseOnAllAnimationFramesBreakpoint;
    if (!breakpoint)
        return;

    m_debuggerAgent->schedulePauseForSpecialBreakpoint(*breakpoint, Inspector::DebuggerFrontendDispatcher::Reason::AnimationFrame);
}

void PageDOMDebuggerAgent::didFireAnimationFrame()
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    auto breakpoint = m_pauseOnAllAnimationFramesBreakpoint;
    if (!breakpoint)
        return;

    m_debuggerAgent->cancelPauseForSpecialBreakpoint(*breakpoint);
}

void PageDOMDebuggerAgent::willInvalidateStyleAttr(Element& element)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    if (hasBreakpoint(&element, AttributeModified)) {
        Ref<JSON::Object> eventData = JSON::Object::create();
        descriptionForDOMEvent(element, AttributeModified, false, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
    }
}

void PageDOMDebuggerAgent::setAnimationFrameBreakpoint(ErrorString& errorString, RefPtr<JSC::Breakpoint>&& breakpoint)
{
    if (!m_pauseOnAllAnimationFramesBreakpoint == !breakpoint) {
        errorString = m_pauseOnAllAnimationFramesBreakpoint ? "Breakpoint for AnimationFrame already exists"_s : "Breakpoint for AnimationFrame missing"_s;
        return;
    }

    m_pauseOnAllAnimationFramesBreakpoint = WTFMove(breakpoint);
}

void PageDOMDebuggerAgent::descriptionForDOMEvent(Node& target, int breakpointType, bool insertion, JSON::Object& description)
{
    ASSERT(m_debuggerAgent->breakpointsActive());
    ASSERT(hasBreakpoint(&target, breakpointType));

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();

    Node* breakpointOwner = &target;
    if ((1 << breakpointType) & inheritableDOMBreakpointTypesMask) {
        if (domAgent) {
            // For inheritable breakpoint types, target node isn't always the same as the node that owns a breakpoint.
            // Target node may be unknown to frontend, so we need to push it first.
            description.setInteger("targetNodeId"_s, domAgent->pushNodeToFrontend(&target));
        }

        // Find breakpoint owner node.
        if (!insertion)
            breakpointOwner = InspectorDOMAgent::innerParentNode(&target);
        ASSERT(breakpointOwner);
        while (!(m_domBreakpoints.get(breakpointOwner) & (1 << breakpointType))) {
            Node* parentNode = InspectorDOMAgent::innerParentNode(breakpointOwner);
            if (!parentNode)
                break;
            breakpointOwner = parentNode;
        }

        if (breakpointType == SubtreeModified)
            description.setBoolean("insertion", insertion);
    }

    if (domAgent) {
        int breakpointOwnerNodeId = domAgent->boundNodeId(breakpointOwner);
        ASSERT(breakpointOwnerNodeId);
        description.setInteger("nodeId", breakpointOwnerNodeId);
    }

    description.setString("type", domTypeName(breakpointType));
}

void PageDOMDebuggerAgent::updateSubtreeBreakpoints(Node* node, uint32_t rootMask, bool set)
{
    uint32_t oldMask = m_domBreakpoints.get(node);
    uint32_t derivedMask = rootMask << domBreakpointDerivedTypeShift;
    uint32_t newMask = set ? oldMask | derivedMask : oldMask & ~derivedMask;
    if (newMask)
        m_domBreakpoints.set(node, newMask);
    else
        m_domBreakpoints.remove(node);

    uint32_t newRootMask = rootMask & ~newMask;
    if (!newRootMask)
        return;

    for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
        updateSubtreeBreakpoints(child, newRootMask, set);
}

bool PageDOMDebuggerAgent::hasBreakpoint(Node* node, int type)
{
    uint32_t rootBit = 1 << type;
    uint32_t derivedBit = rootBit << domBreakpointDerivedTypeShift;
    return m_domBreakpoints.get(node) & (rootBit | derivedBit);
}

} // namespace WebCore
