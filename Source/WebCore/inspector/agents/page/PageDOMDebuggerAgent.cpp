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
#include <wtf/Optional.h>

namespace WebCore {

using namespace Inspector;

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

    m_domSubtreeModifiedBreakpoints.clear();
    m_domAttributeModifiedBreakpoints.clear();
    m_domNodeRemovedBreakpoints.clear();

    m_pauseOnAllAnimationFramesBreakpoint = nullptr;

    InspectorDOMDebuggerAgent::disable();
}

Protocol::ErrorStringOr<void> PageDOMDebuggerAgent::setDOMBreakpoint(Protocol::DOM::NodeId nodeId, Protocol::DOMDebugger::DOMBreakpointType type, RefPtr<JSON::Object>&& options)
{
    Protocol::ErrorString errorString;

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    Node* node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    auto breakpoint = InspectorDebuggerAgent::debuggerBreakpointFromPayload(errorString, WTFMove(options));
    if (!breakpoint)
        return makeUnexpected(errorString);

    switch (type) {
    case Protocol::DOMDebugger::DOMBreakpointType::SubtreeModified:
        if (!m_domSubtreeModifiedBreakpoints.add(node, breakpoint.releaseNonNull()))
            return makeUnexpected("Breakpoint for given node and given type already exists"_s);
        return { };

    case Protocol::DOMDebugger::DOMBreakpointType::AttributeModified:
        if (!m_domAttributeModifiedBreakpoints.add(node, breakpoint.releaseNonNull()))
            return makeUnexpected("Breakpoint for given node and given type already exists"_s);
        return { };

    case Protocol::DOMDebugger::DOMBreakpointType::NodeRemoved:
        if (!m_domNodeRemovedBreakpoints.add(node, breakpoint.releaseNonNull()))
            return makeUnexpected("Breakpoint for given node and given type already exists"_s);
        return { };
    }

    ASSERT_NOT_REACHED();
    return makeUnexpected("Not supported");
}

Protocol::ErrorStringOr<void> PageDOMDebuggerAgent::removeDOMBreakpoint(Protocol::DOM::NodeId nodeId, Protocol::DOMDebugger::DOMBreakpointType type)
{
    Protocol::ErrorString errorString;

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    Node* node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    switch (type) {
    case Protocol::DOMDebugger::DOMBreakpointType::SubtreeModified:
        if (!m_domSubtreeModifiedBreakpoints.remove(node))
            return makeUnexpected("Breakpoint for given node and given type missing"_s);
        return { };

    case Protocol::DOMDebugger::DOMBreakpointType::AttributeModified:
        if (!m_domAttributeModifiedBreakpoints.remove(node))
            return makeUnexpected("Breakpoint for given node and given type missing"_s);
        return { };

    case Protocol::DOMDebugger::DOMBreakpointType::NodeRemoved:
        if (!m_domNodeRemovedBreakpoints.remove(node))
            return makeUnexpected("Breakpoint for given node and given type missing"_s);
        return { };
    }

    ASSERT_NOT_REACHED();
    return makeUnexpected("Not supported");
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

    m_domSubtreeModifiedBreakpoints.clear();
    m_domAttributeModifiedBreakpoints.clear();
    m_domNodeRemovedBreakpoints.clear();
}


static Optional<size_t> calculateDistance(Node& child, Node& ancestor)
{
    size_t distance = 0;

    auto* current = &child;
    while (current != &ancestor) {
        ++distance;

        current = InspectorDOMAgent::innerParentNode(current);
        if (!current)
            return WTF::nullopt;
    }

    return distance;
}

void PageDOMDebuggerAgent::willInsertDOMNode(Node& parent)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    if (m_domSubtreeModifiedBreakpoints.isEmpty())
        return;

    Optional<size_t> closestDistance;
    RefPtr<JSC::Breakpoint> closestBreakpoint;
    Node* closestBreakpointOwner = nullptr;

    for (auto [breakpointOwner, breakpoint] : m_domSubtreeModifiedBreakpoints) {
        auto distance = calculateDistance(parent, *breakpointOwner);
        if (!distance)
            continue;

        if (!closestDistance || distance < closestDistance) {
            closestDistance = distance;
            closestBreakpoint = breakpoint.copyRef();
            closestBreakpointOwner = breakpointOwner;
        }
    }

    if (!closestBreakpoint)
        return;

    ASSERT(closestBreakpointOwner);

    auto pauseData = buildPauseDataForDOMBreakpoint(Protocol::DOMDebugger::DOMBreakpointType::SubtreeModified, *closestBreakpointOwner);
    pauseData->setBoolean("insertion", true);
    // FIXME: <https://webkit.org/b/213499> Web Inspector: allow DOM nodes to be instrumented at any point, regardless of whether the main document has also been instrumented
    // Include the new child node ID so the frontend can show the node that's about to be inserted.
    m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(pauseData), WTFMove(closestBreakpoint));
}

void PageDOMDebuggerAgent::willRemoveDOMNode(Node& node)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    if (m_domNodeRemovedBreakpoints.isEmpty() && m_domSubtreeModifiedBreakpoints.isEmpty())
        return;

    Optional<size_t> closestDistance;
    RefPtr<JSC::Breakpoint> closestBreakpoint;
    Optional<Protocol::DOMDebugger::DOMBreakpointType> closestBreakpointType;
    Node* closestBreakpointOwner = nullptr;

    for (auto [breakpointOwner, breakpoint] : m_domNodeRemovedBreakpoints) {
        auto distance = calculateDistance(*breakpointOwner, node);
        if (!distance)
            continue;

        if (!closestDistance || distance < closestDistance) {
            closestDistance = distance;
            closestBreakpoint = breakpoint.copyRef();
            closestBreakpointType = Protocol::DOMDebugger::DOMBreakpointType::NodeRemoved;
            closestBreakpointOwner = breakpointOwner;
        }
    }

    if (!closestBreakpoint) {
        for (auto [breakpointOwner, breakpoint] : m_domSubtreeModifiedBreakpoints) {
            auto distance = calculateDistance(node, *breakpointOwner);
            if (!distance)
                continue;

            if (!closestDistance || distance < closestDistance) {
                closestDistance = distance;
                closestBreakpoint = breakpoint.copyRef();
                closestBreakpointType = Protocol::DOMDebugger::DOMBreakpointType::SubtreeModified;
                closestBreakpointOwner = breakpointOwner;
            }
        }
    }

    if (!closestBreakpoint)
        return;

    ASSERT(closestBreakpointType);
    ASSERT(closestBreakpointOwner);

    auto pauseData = buildPauseDataForDOMBreakpoint(*closestBreakpointType, *closestBreakpointOwner);
    if (auto* domAgent = m_instrumentingAgents.persistentDOMAgent()) {
        if (&node != closestBreakpointOwner) {
            if (auto targetNodeId = domAgent->pushNodeToFrontend(&node))
                pauseData->setInteger("targetNodeId", targetNodeId);
        }
    }
    m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(pauseData), WTFMove(closestBreakpoint));
}

void PageDOMDebuggerAgent::didRemoveDOMNode(Node& node)
{
    auto nodeContainsBreakpointOwner = [&] (auto& entry) {
        return node.contains(entry.key);
    };
    m_domSubtreeModifiedBreakpoints.removeIf(nodeContainsBreakpointOwner);
    m_domAttributeModifiedBreakpoints.removeIf(nodeContainsBreakpointOwner);
    m_domNodeRemovedBreakpoints.removeIf(nodeContainsBreakpointOwner);
}

void PageDOMDebuggerAgent::willModifyDOMAttr(Element& element)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    auto it = m_domAttributeModifiedBreakpoints.find(&element);
    if (it == m_domAttributeModifiedBreakpoints.end())
        return;

    auto pauseData = buildPauseDataForDOMBreakpoint(Protocol::DOMDebugger::DOMBreakpointType::AttributeModified, element);
    m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(pauseData), it->value.copyRef());
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

    auto it = m_domAttributeModifiedBreakpoints.find(&element);
    if (it == m_domAttributeModifiedBreakpoints.end())
        return;

    auto pauseData = buildPauseDataForDOMBreakpoint(Protocol::DOMDebugger::DOMBreakpointType::AttributeModified, element);
    m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(pauseData), it->value.copyRef());
}

bool PageDOMDebuggerAgent::setAnimationFrameBreakpoint(Protocol::ErrorString& errorString, RefPtr<JSC::Breakpoint>&& breakpoint)
{
    if (!m_pauseOnAllAnimationFramesBreakpoint == !breakpoint) {
        errorString = m_pauseOnAllAnimationFramesBreakpoint ? "Breakpoint for AnimationFrame already exists"_s : "Breakpoint for AnimationFrame missing"_s;
        return false;
    }

    m_pauseOnAllAnimationFramesBreakpoint = WTFMove(breakpoint);
    return true;
}

Ref<JSON::Object> PageDOMDebuggerAgent::buildPauseDataForDOMBreakpoint(Protocol::DOMDebugger::DOMBreakpointType breakpointType, Node& breakpointOwner)
{
    ASSERT(m_debuggerAgent->breakpointsActive());
    ASSERT(m_domSubtreeModifiedBreakpoints.contains(&breakpointOwner) || m_domAttributeModifiedBreakpoints.contains(&breakpointOwner) || m_domNodeRemovedBreakpoints.contains(&breakpointOwner));

    auto pauseData = JSON::Object::create();
    pauseData->setString("type", Protocol::Helpers::getEnumConstantValue(breakpointType));
    if (auto* domAgent = m_instrumentingAgents.persistentDOMAgent()) {
        if (auto breakpointOwnerNodeId = domAgent->pushNodeToFrontend(&breakpointOwner))
            pauseData->setInteger("nodeId", breakpointOwnerNodeId);
    }
    return pauseData;
}

} // namespace WebCore
