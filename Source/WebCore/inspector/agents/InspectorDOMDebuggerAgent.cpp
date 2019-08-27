/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorDOMDebuggerAgent.h"

#include "Event.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "JSEvent.h"
#include "RegisteredEventListener.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/JSONValues.h>

namespace {

enum DOMBreakpointType {
    SubtreeModified,
    AttributeModified,
    NodeRemoved,
    DOMBreakpointTypesCount
};

const uint32_t inheritableDOMBreakpointTypesMask = (1 << SubtreeModified);
const int domBreakpointDerivedTypeShift = 16;

}


namespace WebCore {

using namespace Inspector;

InspectorDOMDebuggerAgent::InspectorDOMDebuggerAgent(WebAgentContext& context, InspectorDebuggerAgent* debuggerAgent)
    : InspectorAgentBase("DOMDebugger"_s, context)
    , m_backendDispatcher(Inspector::DOMDebuggerBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_debuggerAgent(debuggerAgent)
{
    m_debuggerAgent->addListener(*this);
}

InspectorDOMDebuggerAgent::~InspectorDOMDebuggerAgent() = default;

// Browser debugger agent enabled only when JS debugger is enabled.
void InspectorDOMDebuggerAgent::debuggerWasEnabled()
{
    m_instrumentingAgents.setInspectorDOMDebuggerAgent(this);
}

void InspectorDOMDebuggerAgent::debuggerWasDisabled()
{
    disable();
}

void InspectorDOMDebuggerAgent::disable()
{
    m_instrumentingAgents.setInspectorDOMDebuggerAgent(nullptr);
    m_domBreakpoints.clear();
    m_listenerBreakpoints.clear();
    m_urlBreakpoints.clear();
    m_pauseOnAllAnimationFramesEnabled = false;
    m_pauseOnAllIntervalsEnabled = false;
    m_pauseOnAllListenersEnabled = false;
    m_pauseOnAllTimeoutsEnabled = false;
    m_pauseOnAllURLsEnabled = false;
}

void InspectorDOMDebuggerAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorDOMDebuggerAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

void InspectorDOMDebuggerAgent::discardAgent()
{
    m_debuggerAgent->removeListener(*this);
    m_debuggerAgent = nullptr;
}

void InspectorDOMDebuggerAgent::frameDocumentUpdated(Frame& frame)
{
    if (!frame.isMainFrame())
        return;

    m_domBreakpoints.clear();
}

void InspectorDOMDebuggerAgent::setEventBreakpoint(ErrorString& errorString, const String& breakpointTypeString, const String* eventName)
{
    if (breakpointTypeString.isEmpty()) {
        errorString = "breakpointType is empty"_s;
        return;
    }

    auto breakpointType = Inspector::Protocol::InspectorHelpers::parseEnumValueFromString<Inspector::Protocol::DOMDebugger::EventBreakpointType>(breakpointTypeString);
    if (!breakpointType) {
        errorString = makeString("Unknown breakpointType: "_s, breakpointTypeString);
        return;
    }

    if (eventName && !eventName->isEmpty()) {
        if (breakpointType.value() == Inspector::Protocol::DOMDebugger::EventBreakpointType::Listener) {
            if (!m_listenerBreakpoints.add(*eventName))
                errorString = "Breakpoint with eventName already exists"_s;
            return;
        }

        errorString = "Unexpected eventName"_s;
        return;
    }

    switch (breakpointType.value()) {
    case Inspector::Protocol::DOMDebugger::EventBreakpointType::AnimationFrame:
        if (m_pauseOnAllAnimationFramesEnabled)
            errorString = "Breakpoint for AnimationFrame already exists"_s;
        m_pauseOnAllAnimationFramesEnabled = true;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Interval:
        if (m_pauseOnAllIntervalsEnabled)
            errorString = "Breakpoint for Interval already exists"_s;
        m_pauseOnAllIntervalsEnabled = true;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Listener:
        if (m_pauseOnAllListenersEnabled)
            errorString = "Breakpoint for Listener already exists"_s;
        m_pauseOnAllListenersEnabled = true;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Timeout:
        if (m_pauseOnAllTimeoutsEnabled)
            errorString = "Breakpoint for Timeout already exists"_s;
        m_pauseOnAllTimeoutsEnabled = true;
        break;
    }
}

void InspectorDOMDebuggerAgent::removeEventBreakpoint(ErrorString& errorString, const String& breakpointTypeString, const String* eventName)
{
    if (breakpointTypeString.isEmpty()) {
        errorString = "breakpointType is empty"_s;
        return;
    }

    auto breakpointType = Inspector::Protocol::InspectorHelpers::parseEnumValueFromString<Inspector::Protocol::DOMDebugger::EventBreakpointType>(breakpointTypeString);
    if (!breakpointType) {
        errorString = makeString("Unknown breakpointType: "_s, breakpointTypeString);
        return;
    }

    if (eventName && !eventName->isEmpty()) {
        if (breakpointType.value() == Inspector::Protocol::DOMDebugger::EventBreakpointType::Listener) {
            if (!m_listenerBreakpoints.remove(*eventName))
                errorString = "Breakpoint for given eventName missing"_s;
            return;
        }

        errorString = "Unexpected eventName"_s;
        return;
    }

    switch (breakpointType.value()) {
    case Inspector::Protocol::DOMDebugger::EventBreakpointType::AnimationFrame:
        if (!m_pauseOnAllAnimationFramesEnabled)
            errorString = "Breakpoint for AnimationFrame missing"_s;
        m_pauseOnAllAnimationFramesEnabled = false;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Interval:
        if (!m_pauseOnAllIntervalsEnabled)
            errorString = "Breakpoint for Intervals missing"_s;
        m_pauseOnAllIntervalsEnabled = false;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Listener:
        if (!m_pauseOnAllListenersEnabled)
            errorString = "Breakpoint for Listeners missing"_s;
        m_pauseOnAllListenersEnabled = false;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Timeout:
        if (!m_pauseOnAllTimeoutsEnabled)
            errorString = "Breakpoint for Timeouts missing"_s;
        m_pauseOnAllTimeoutsEnabled = false;
        break;
    }
}

void InspectorDOMDebuggerAgent::willInvalidateStyleAttr(Element& element)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    if (hasBreakpoint(&element, AttributeModified)) {
        Ref<JSON::Object> eventData = JSON::Object::create();
        descriptionForDOMEvent(element, AttributeModified, false, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
    }
}

void InspectorDOMDebuggerAgent::didInsertDOMNode(Node& node)
{
    if (m_domBreakpoints.size()) {
        uint32_t mask = m_domBreakpoints.get(InspectorDOMAgent::innerParentNode(&node));
        uint32_t inheritableTypesMask = (mask | (mask >> domBreakpointDerivedTypeShift)) & inheritableDOMBreakpointTypesMask;
        if (inheritableTypesMask)
            updateSubtreeBreakpoints(&node, inheritableTypesMask, true);
    }
}

void InspectorDOMDebuggerAgent::didRemoveDOMNode(Node& node)
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

static int domTypeForName(ErrorString& errorString, const String& typeString)
{
    if (typeString == "subtree-modified")
        return SubtreeModified;
    if (typeString == "attribute-modified")
        return AttributeModified;
    if (typeString == "node-removed")
        return NodeRemoved;
    errorString = makeString("Unknown type: ", typeString);
    return -1;
}

static String domTypeName(int type)
{
    switch (type) {
    case SubtreeModified: return "subtree-modified"_s;
    case AttributeModified: return "attribute-modified"_s;
    case NodeRemoved: return "node-removed"_s;
    default: break;
    }
    return emptyString();
}

void InspectorDOMDebuggerAgent::setDOMBreakpoint(ErrorString& errorString, int nodeId, const String& typeString)
{
    auto* domAgent = m_instrumentingAgents.inspectorDOMAgent();
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

void InspectorDOMDebuggerAgent::removeDOMBreakpoint(ErrorString& errorString, int nodeId, const String& typeString)
{
    auto* domAgent = m_instrumentingAgents.inspectorDOMAgent();
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

void InspectorDOMDebuggerAgent::willInsertDOMNode(Node& parent)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    if (hasBreakpoint(&parent, SubtreeModified)) {
        Ref<JSON::Object> eventData = JSON::Object::create();
        descriptionForDOMEvent(parent, SubtreeModified, true, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
    }
}

void InspectorDOMDebuggerAgent::willRemoveDOMNode(Node& node)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    Node* parentNode = InspectorDOMAgent::innerParentNode(&node);
    if (hasBreakpoint(&node, NodeRemoved)) {
        Ref<JSON::Object> eventData = JSON::Object::create();
        descriptionForDOMEvent(node, NodeRemoved, false, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
    } else if (parentNode && hasBreakpoint(parentNode, SubtreeModified)) {
        Ref<JSON::Object> eventData = JSON::Object::create();
        descriptionForDOMEvent(node, SubtreeModified, false, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
    }
}

void InspectorDOMDebuggerAgent::willModifyDOMAttr(Element& element)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    if (hasBreakpoint(&element, AttributeModified)) {
        Ref<JSON::Object> eventData = JSON::Object::create();
        descriptionForDOMEvent(element, AttributeModified, false, eventData.get());
        m_debuggerAgent->breakProgram(Inspector::DebuggerFrontendDispatcher::Reason::DOM, WTFMove(eventData));
    }
}

void InspectorDOMDebuggerAgent::descriptionForDOMEvent(Node& target, int breakpointType, bool insertion, JSON::Object& description)
{
    ASSERT(m_debuggerAgent->breakpointsActive());
    ASSERT(hasBreakpoint(&target, breakpointType));

    auto* domAgent = m_instrumentingAgents.inspectorDOMAgent();

    Node* breakpointOwner = &target;
    if ((1 << breakpointType) & inheritableDOMBreakpointTypesMask) {
        if (domAgent) {
            // For inheritable breakpoint types, target node isn't always the same as the node that owns a breakpoint.
            // Target node may be unknown to frontend, so we need to push it first.
            RefPtr<Inspector::Protocol::Runtime::RemoteObject> targetNodeObject = domAgent->resolveNode(&target, InspectorDebuggerAgent::backtraceObjectGroup);
            description.setValue("targetNode", targetNodeObject);
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

bool InspectorDOMDebuggerAgent::hasBreakpoint(Node* node, int type)
{
    uint32_t rootBit = 1 << type;
    uint32_t derivedBit = rootBit << domBreakpointDerivedTypeShift;
    return m_domBreakpoints.get(node) & (rootBit | derivedBit);
}

void InspectorDOMDebuggerAgent::updateSubtreeBreakpoints(Node* node, uint32_t rootMask, bool set)
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

void InspectorDOMDebuggerAgent::willHandleEvent(Event& event, const RegisteredEventListener& registeredEventListener)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    auto state = event.target()->scriptExecutionContext()->execState();
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(state);
    ASSERT(!injectedScript.hasNoValue());
    {
        JSC::JSLockHolder lock(state);

        injectedScript.setEventValue(toJS(state, deprecatedGlobalObjectForPrototype(state), event));
    }

    auto* domAgent = m_instrumentingAgents.inspectorDOMAgent();

    bool shouldPause = m_debuggerAgent->pauseOnNextStatementEnabled() || m_pauseOnAllListenersEnabled || m_listenerBreakpoints.contains(event.type());
    if (!shouldPause && domAgent)
        shouldPause = domAgent->hasBreakpointForEventListener(*event.currentTarget(), event.type(), registeredEventListener.callback(), registeredEventListener.useCapture());
    if (!shouldPause)
        return;

    Ref<JSON::Object> eventData = JSON::Object::create();
    eventData->setString("eventName"_s, event.type());
    if (domAgent) {
        int eventListenerId = domAgent->idForEventListener(*event.currentTarget(), event.type(), registeredEventListener.callback(), registeredEventListener.useCapture());
        if (eventListenerId)
            eventData->setInteger("eventListenerId"_s, eventListenerId);
    }

    m_debuggerAgent->schedulePauseOnNextStatement(Inspector::DebuggerFrontendDispatcher::Reason::Listener, WTFMove(eventData));
}

void InspectorDOMDebuggerAgent::didHandleEvent()
{
    m_injectedScriptManager.clearEventValue();
}

void InspectorDOMDebuggerAgent::willFireTimer(bool oneShot)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    bool shouldPause = m_debuggerAgent->pauseOnNextStatementEnabled() || (oneShot ? m_pauseOnAllTimeoutsEnabled : m_pauseOnAllIntervalsEnabled);
    if (!shouldPause)
        return;

    auto breakReason = oneShot ? Inspector::DebuggerFrontendDispatcher::Reason::Timeout : Inspector::DebuggerFrontendDispatcher::Reason::Interval;
    m_debuggerAgent->schedulePauseOnNextStatement(breakReason, nullptr);
}

void InspectorDOMDebuggerAgent::willFireAnimationFrame()
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    bool shouldPause = m_debuggerAgent->pauseOnNextStatementEnabled() || m_pauseOnAllAnimationFramesEnabled;
    if (!shouldPause)
        return;

    m_debuggerAgent->schedulePauseOnNextStatement(Inspector::DebuggerFrontendDispatcher::Reason::AnimationFrame, nullptr);
}

void InspectorDOMDebuggerAgent::setURLBreakpoint(ErrorString& errorString, const String& url, const bool* optionalIsRegex)
{
    if (url.isEmpty()) {
        if (m_pauseOnAllURLsEnabled)
            errorString = "Breakpoint for all URLs already exists"_s;
        m_pauseOnAllURLsEnabled = true;
        return;
    }

    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    auto result = m_urlBreakpoints.set(url, isRegex ? URLBreakpointType::RegularExpression : URLBreakpointType::Text);
    if (!result.isNewEntry)
        errorString = "Breakpoint for given url already exists"_s;
}

void InspectorDOMDebuggerAgent::removeURLBreakpoint(ErrorString& errorString, const String& url)
{
    if (url.isEmpty()) {
        if (!m_pauseOnAllURLsEnabled)
            errorString = "Breakpoint for all URLs missing"_s;
        m_pauseOnAllURLsEnabled = false;
        return;
    }

    auto result = m_urlBreakpoints.remove(url);
    if (!result)
        errorString = "Breakpoint for given url missing"_s;
}

void InspectorDOMDebuggerAgent::breakOnURLIfNeeded(const String& url, URLBreakpointSource source)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    String breakpointURL;
    if (m_pauseOnAllURLsEnabled)
        breakpointURL = emptyString();
    else {
        for (auto& entry : m_urlBreakpoints) {
            const auto& query = entry.key;
            bool isRegex = entry.value == URLBreakpointType::RegularExpression;
            auto regex = ContentSearchUtilities::createSearchRegex(query, false, isRegex);
            if (regex.match(url) != -1) {
                breakpointURL = query;
                break;
            }
        }
    }

    if (breakpointURL.isNull())
        return;

    Inspector::DebuggerFrontendDispatcher::Reason breakReason;
    if (source == URLBreakpointSource::Fetch)
        breakReason = Inspector::DebuggerFrontendDispatcher::Reason::Fetch;
    else if (source == URLBreakpointSource::XHR)
        breakReason = Inspector::DebuggerFrontendDispatcher::Reason::XHR;
    else {
        ASSERT_NOT_REACHED();
        breakReason = Inspector::DebuggerFrontendDispatcher::Reason::Other;
    }

    Ref<JSON::Object> eventData = JSON::Object::create();
    eventData->setString("breakpointURL", breakpointURL);
    eventData->setString("url", url);
    m_debuggerAgent->breakProgram(breakReason, WTFMove(eventData));
}

void InspectorDOMDebuggerAgent::willSendXMLHttpRequest(const String& url)
{
    breakOnURLIfNeeded(url, URLBreakpointSource::XHR);
}

void InspectorDOMDebuggerAgent::willFetch(const String& url)
{
    breakOnURLIfNeeded(url, URLBreakpointSource::Fetch);
}

} // namespace WebCore
