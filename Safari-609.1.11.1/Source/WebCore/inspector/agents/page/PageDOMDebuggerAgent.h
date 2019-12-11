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

#include "InspectorDOMDebuggerAgent.h"

namespace WebCore {

class Element;
class Frame;
class Node;

typedef String ErrorString;

class PageDOMDebuggerAgent final : public InspectorDOMDebuggerAgent {
public:
    PageDOMDebuggerAgent(PageAgentContext&, Inspector::InspectorDebuggerAgent*);
    ~PageDOMDebuggerAgent() override;

    bool enabled() const override;

    // DOMDebuggerBackendDispatcherHandler
    void setDOMBreakpoint(ErrorString&, int nodeId, const String& type) override;
    void removeDOMBreakpoint(ErrorString&, int nodeId, const String& type) override;

    // InspectorInstrumentation
    void frameDocumentUpdated(Frame&);
    void willInsertDOMNode(Node& parent);
    void didInsertDOMNode(Node&);
    void willRemoveDOMNode(Node&);
    void didRemoveDOMNode(Node&);
    void willModifyDOMAttr(Element&);
    void willInvalidateStyleAttr(Element&);
    void willFireAnimationFrame();

private:
    void enable() override;
    void disable() override;

    void setAnimationFrameBreakpoint(ErrorString&, bool enabled) override;

    void descriptionForDOMEvent(Node&, int breakpointType, bool insertion, JSON::Object& description);
    void updateSubtreeBreakpoints(Node*, uint32_t rootMask, bool set);
    bool checkSubtreeForNodeRemoved(Node*);
    bool hasBreakpoint(Node*, int type);

    HashMap<Node*, uint32_t> m_domBreakpoints;
    bool m_pauseOnAllAnimationFramesEnabled { false };
};

} // namespace WebCore
