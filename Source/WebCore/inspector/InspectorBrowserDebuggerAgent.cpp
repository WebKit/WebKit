/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "InspectorBrowserDebuggerAgent.h"

#if ENABLE(INSPECTOR) && ENABLE(JAVASCRIPT_DEBUGGER)

#include "HTMLElement.h"
#include "InspectorAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorState.h"
#include <wtf/text/CString.h>

namespace {

enum DOMBreakpointType {
    SubtreeModified = 0,
    AttributeModified,
    NodeRemoved,
    DOMBreakpointTypesCount
};

static const char* const domNativeBreakpointType = "DOM";
static const char* const eventListenerNativeBreakpointType = "EventListener";
static const char* const xhrNativeBreakpointType = "XHR";

const uint32_t inheritableDOMBreakpointTypesMask = (1 << SubtreeModified);
const int domBreakpointDerivedTypeShift = 16;

}

namespace WebCore {

namespace BrowserDebuggerAgentState {
static const char browserBreakpoints[] = "browserBreakpoints";
}

PassOwnPtr<InspectorBrowserDebuggerAgent> InspectorBrowserDebuggerAgent::create(InspectorAgent* inspectorAgent, bool eraseStickyBreakpoints)
{
    return adoptPtr(new InspectorBrowserDebuggerAgent(inspectorAgent, eraseStickyBreakpoints));
}

InspectorBrowserDebuggerAgent::InspectorBrowserDebuggerAgent(InspectorAgent* inspectorAgent, bool eraseStickyBreakpoints)
    : m_inspectorAgent(inspectorAgent)
    , m_hasXHRBreakpointWithEmptyURL(false)
{
    if (eraseStickyBreakpoints)
        inspectorAgent->state()->setObject(BrowserDebuggerAgentState::browserBreakpoints, InspectorObject::create());
}

InspectorBrowserDebuggerAgent::~InspectorBrowserDebuggerAgent()
{
}

void InspectorBrowserDebuggerAgent::setAllBrowserBreakpoints(PassRefPtr<InspectorObject> breakpoints)
{
    m_inspectorAgent->state()->setObject(BrowserDebuggerAgentState::browserBreakpoints, breakpoints);
    inspectedURLChanged(m_inspectorAgent->inspectedURLWithoutFragment());
}

void InspectorBrowserDebuggerAgent::inspectedURLChanged(const String& url)
{
    m_eventListenerBreakpoints.clear();
    m_XHRBreakpoints.clear();
    m_hasXHRBreakpointWithEmptyURL = false;

    RefPtr<InspectorObject> allBreakpoints = m_inspectorAgent->state()->getObject(BrowserDebuggerAgentState::browserBreakpoints);
    RefPtr<InspectorArray> breakpoints = allBreakpoints->getArray(url);
    if (!breakpoints)
        return;
    for (unsigned i = 0; i < breakpoints->length(); ++i)
        restoreStickyBreakpoint(breakpoints->get(i)->asObject());
}

void InspectorBrowserDebuggerAgent::restoreStickyBreakpoint(PassRefPtr<InspectorObject> breakpoint)
{
    if (!breakpoint)
        return;
    String type;
    if (!breakpoint->getString("type", &type))
        return;
    bool enabled;
    if (!breakpoint->getBoolean("enabled", &enabled))
        return;
    RefPtr<InspectorObject> condition = breakpoint->getObject("condition");
    if (!condition)
        return;

    if (type == eventListenerNativeBreakpointType) {
        if (!enabled)
            return;
        String eventName;
        if (!condition->getString("eventName", &eventName))
            return;
        setEventListenerBreakpoint(eventName);
    } else if (type == xhrNativeBreakpointType) {
        if (!enabled)
            return;
        String url;
        if (!condition->getString("url", &url))
            return;
        setXHRBreakpoint(url);
    }
}

void InspectorBrowserDebuggerAgent::discardBindings()
{
    m_domBreakpoints.clear();
}

void InspectorBrowserDebuggerAgent::setEventListenerBreakpoint(const String& eventName)
{
    m_eventListenerBreakpoints.add(eventName);
}

void InspectorBrowserDebuggerAgent::removeEventListenerBreakpoint(const String& eventName)
{
    m_eventListenerBreakpoints.remove(eventName);
}

void InspectorBrowserDebuggerAgent::didInsertDOMNode(Node* node)
{
    if (m_domBreakpoints.size()) {
        uint32_t mask = m_domBreakpoints.get(InspectorDOMAgent::innerParentNode(node));
        uint32_t inheritableTypesMask = (mask | (mask >> domBreakpointDerivedTypeShift)) & inheritableDOMBreakpointTypesMask;
        if (inheritableTypesMask)
            updateSubtreeBreakpoints(node, inheritableTypesMask, true);
    }
}

void InspectorBrowserDebuggerAgent::didRemoveDOMNode(Node* node)
{
    if (m_domBreakpoints.size()) {
        // Remove subtree breakpoints.
        m_domBreakpoints.remove(node);
        Vector<Node*> stack(1, InspectorDOMAgent::innerFirstChild(node));
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

void InspectorBrowserDebuggerAgent::setDOMBreakpoint(long nodeId, long type)
{
    Node* node = m_inspectorAgent->domAgent()->nodeForId(nodeId);
    if (!node)
        return;

    uint32_t rootBit = 1 << type;
    m_domBreakpoints.set(node, m_domBreakpoints.get(node) | rootBit);
    if (rootBit & inheritableDOMBreakpointTypesMask) {
        for (Node* child = InspectorDOMAgent::innerFirstChild(node); child; child = InspectorDOMAgent::innerNextSibling(child))
            updateSubtreeBreakpoints(child, rootBit, true);
    }
}

void InspectorBrowserDebuggerAgent::removeDOMBreakpoint(long nodeId, long type)
{
    Node* node = m_inspectorAgent->domAgent()->nodeForId(nodeId);
    if (!node)
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

void InspectorBrowserDebuggerAgent::willInsertDOMNode(Node*, Node* parent)
{
    InspectorDebuggerAgent* debuggerAgent = m_inspectorAgent->debuggerAgent();
    if (!debuggerAgent)
        return;

    if (hasBreakpoint(parent, SubtreeModified)) {
        RefPtr<InspectorObject> eventData = InspectorObject::create();
        descriptionForDOMEvent(parent, SubtreeModified, true, eventData.get());
        eventData->setString("breakpointType", domNativeBreakpointType);
        debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData.release());
    }
}

void InspectorBrowserDebuggerAgent::willRemoveDOMNode(Node* node)
{
    InspectorDebuggerAgent* debuggerAgent = m_inspectorAgent->debuggerAgent();
    if (!debuggerAgent)
        return;

    if (hasBreakpoint(node, NodeRemoved)) {
        RefPtr<InspectorObject> eventData = InspectorObject::create();
        descriptionForDOMEvent(node, NodeRemoved, false, eventData.get());
        eventData->setString("breakpointType", domNativeBreakpointType);
        debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData.release());
    } else if (hasBreakpoint(InspectorDOMAgent::innerParentNode(node), SubtreeModified)) {
        RefPtr<InspectorObject> eventData = InspectorObject::create();
        descriptionForDOMEvent(node, SubtreeModified, false, eventData.get());
        eventData->setString("breakpointType", domNativeBreakpointType);
        debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData.release());
    }
}

void InspectorBrowserDebuggerAgent::willModifyDOMAttr(Element* element)
{
    InspectorDebuggerAgent* debuggerAgent = m_inspectorAgent->debuggerAgent();
    if (!debuggerAgent)
        return;

    if (hasBreakpoint(element, AttributeModified)) {
        RefPtr<InspectorObject> eventData = InspectorObject::create();
        descriptionForDOMEvent(element, AttributeModified, false, eventData.get());
        eventData->setString("breakpointType", domNativeBreakpointType);
        debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData.release());
    }
}

void InspectorBrowserDebuggerAgent::descriptionForDOMEvent(Node* target, long breakpointType, bool insertion, InspectorObject* description)
{
    ASSERT(hasBreakpoint(target, breakpointType));

    Node* breakpointOwner = target;
    if ((1 << breakpointType) & inheritableDOMBreakpointTypesMask) {
        // For inheritable breakpoint types, target node isn't always the same as the node that owns a breakpoint.
        // Target node may be unknown to frontend, so we need to push it first.
        long targetNodeId = m_inspectorAgent->domAgent()->pushNodePathToFrontend(target);
        ASSERT(targetNodeId);
        description->setNumber("targetNodeId", targetNodeId);

        // Find breakpoint owner node.
        if (!insertion)
            breakpointOwner = InspectorDOMAgent::innerParentNode(target);
        ASSERT(breakpointOwner);
        while (!(m_domBreakpoints.get(breakpointOwner) & (1 << breakpointType))) {
            breakpointOwner = InspectorDOMAgent::innerParentNode(breakpointOwner);
            ASSERT(breakpointOwner);
        }

        if (breakpointType == SubtreeModified)
            description->setBoolean("insertion", insertion);
    }

    long breakpointOwnerNodeId = m_inspectorAgent->domAgent()->pushNodePathToFrontend(breakpointOwner);
    ASSERT(breakpointOwnerNodeId);
    description->setNumber("nodeId", breakpointOwnerNodeId);
    description->setNumber("type", breakpointType);
}

bool InspectorBrowserDebuggerAgent::hasBreakpoint(Node* node, long type)
{
    uint32_t rootBit = 1 << type;
    uint32_t derivedBit = rootBit << domBreakpointDerivedTypeShift;
    return m_domBreakpoints.get(node) & (rootBit | derivedBit);
}

void InspectorBrowserDebuggerAgent::updateSubtreeBreakpoints(Node* node, uint32_t rootMask, bool set)
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

void InspectorBrowserDebuggerAgent::pauseOnNativeEventIfNeeded(const String& categoryType, const String& eventName, bool synchronous)
{
    InspectorDebuggerAgent* debuggerAgent = m_inspectorAgent->debuggerAgent();
    if (!debuggerAgent)
        return;

    String fullEventName = String::format("%s:%s", categoryType.utf8().data(), eventName.utf8().data());
    if (!m_eventListenerBreakpoints.contains(fullEventName))
        return;

    RefPtr<InspectorObject> eventData = InspectorObject::create();
    eventData->setString("breakpointType", eventListenerNativeBreakpointType);
    eventData->setString("eventName", fullEventName);
    if (synchronous)
        debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData.release());
    else
        debuggerAgent->schedulePauseOnNextStatement(NativeBreakpointDebuggerEventType, eventData.release());
}

void InspectorBrowserDebuggerAgent::setXHRBreakpoint(const String& url)
{
    if (url.isEmpty())
        m_hasXHRBreakpointWithEmptyURL = true;
    else
        m_XHRBreakpoints.add(url);
}

void InspectorBrowserDebuggerAgent::removeXHRBreakpoint(const String& url)
{
    if (url.isEmpty())
        m_hasXHRBreakpointWithEmptyURL = false;
    else
        m_XHRBreakpoints.remove(url);
}

void InspectorBrowserDebuggerAgent::willSendXMLHttpRequest(const String& url)
{
    InspectorDebuggerAgent* debuggerAgent = m_inspectorAgent->debuggerAgent();
    if (!debuggerAgent)
        return;

    String breakpointURL;
    if (m_hasXHRBreakpointWithEmptyURL)
        breakpointURL = "";
    else {
        for (HashSet<String>::iterator it = m_XHRBreakpoints.begin(); it != m_XHRBreakpoints.end(); ++it) {
            if (url.contains(*it)) {
                breakpointURL = *it;
                break;
            }
        }
    }

    if (breakpointURL.isNull())
        return;

    RefPtr<InspectorObject> eventData = InspectorObject::create();
    eventData->setString("breakpointType", xhrNativeBreakpointType);
    eventData->setString("breakpointURL", breakpointURL);
    eventData->setString("url", url);
    debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData.release());
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(JAVASCRIPT_DEBUGGER)
