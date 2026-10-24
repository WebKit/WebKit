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

#pragma once

#include "InspectorDOMDebuggerAgent.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class Element;
class LocalFrame;
class Node;
class WeakPtrImplWithEventTargetData;

class FrameDOMDebuggerAgent final : public InspectorDOMDebuggerAgent {
    WTF_MAKE_TZONE_ALLOCATED(FrameDOMDebuggerAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FrameDOMDebuggerAgent);
public:
    FrameDOMDebuggerAgent(FrameAgentContext&, Inspector::InspectorDebuggerAgent*);
    ~FrameDOMDebuggerAgent();

    // DOMDebuggerBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> setDOMBreakpoint(Inspector::Protocol::DOM::NodeId, Inspector::Protocol::DOMDebugger::DOMBreakpointType, RefPtr<JSON::Object>&& options) final;
    Inspector::Protocol::ErrorStringOr<void> removeDOMBreakpoint(Inspector::Protocol::DOM::NodeId, Inspector::Protocol::DOMDebugger::DOMBreakpointType) final;

    // InspectorInstrumentation
    void frameDocumentUpdated();
    void willInsertDOMNode(Node& parent);
    void willRemoveDOMNode(Node&);
    void didRemoveDOMNode(Node&);
    void willModifyDOMAttr(Element&);

private:
    void enable() final;
    void disable() final;

    // Weak keys, since this agent is not told when a node is destroyed.
    using DOMBreakpointMap = WeakHashMap<Node, Ref<JSC::Breakpoint>, WeakPtrImplWithEventTargetData>;
    DOMBreakpointMap& domBreakpoints(Inspector::Protocol::DOMDebugger::DOMBreakpointType);
    void clearDOMBreakpoints();
    bool canPauseRunningScript();
    void breakOnDOMBreakpoint(Inspector::Protocol::DOMDebugger::DOMBreakpointType, Node& breakpointOwner, Ref<JSC::Breakpoint>&&, Node* targetNode = nullptr, bool insertion = false);

    WeakRef<LocalFrame> m_inspectedFrame;
    DOMBreakpointMap m_subtreeModifiedBreakpoints;
    DOMBreakpointMap m_attributeModifiedBreakpoints;
    DOMBreakpointMap m_nodeRemovedBreakpoints;
};

} // namespace WebCore
