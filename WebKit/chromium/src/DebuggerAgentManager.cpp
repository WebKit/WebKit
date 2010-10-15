/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "DebuggerAgentManager.h"

#include "DebuggerAgentImpl.h"
#include "Frame.h"
#include "PageGroupLoadDeferrer.h"
#include "ScriptDebugServer.h"
#include "V8Proxy.h"
#include "WebDevToolsAgentImpl.h"
#include "WebFrameImpl.h"
#include "WebViewImpl.h"
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringConcatenate.h>

namespace WebKit {

WebDevToolsAgent::MessageLoopDispatchHandler DebuggerAgentManager::s_messageLoopDispatchHandler = 0;

bool DebuggerAgentManager::s_inHostDispatchHandler = false;

DebuggerAgentManager::DeferrersMap DebuggerAgentManager::s_pageDeferrers;

bool DebuggerAgentManager::s_exposeV8DebuggerProtocol = false;

namespace {

class CallerIdWrapper : public v8::Debug::ClientData, public Noncopyable {
public:
    CallerIdWrapper() : m_callerIsMananager(true), m_callerId(0) { }
    explicit CallerIdWrapper(int callerId)
        : m_callerIsMananager(false)
        , m_callerId(callerId) { }
    ~CallerIdWrapper() { }
    bool callerIsMananager() const { return m_callerIsMananager; }
    int callerId() const { return m_callerId; }
private:
    bool m_callerIsMananager;
    int m_callerId;
};

} // namespace


void DebuggerAgentManager::debugHostDispatchHandler()
{
    if (!s_messageLoopDispatchHandler || !s_attachedAgentsMap)
        return;

    if (s_inHostDispatchHandler)
        return;

    s_inHostDispatchHandler = true;

    Vector<WebViewImpl*> views;
    // 1. Disable active objects and input events.
    for (AttachedAgentsMap::iterator it = s_attachedAgentsMap->begin(); it != s_attachedAgentsMap->end(); ++it) {
        DebuggerAgentImpl* agent = it->second;
        s_pageDeferrers.set(agent->webView(), new WebCore::PageGroupLoadDeferrer(agent->page(), true));
        views.append(agent->webView());
        agent->webView()->setIgnoreInputEvents(true);
    }

    // 2. Process messages.
    s_messageLoopDispatchHandler();

    // 3. Bring things back.
    for (Vector<WebViewImpl*>::iterator it = views.begin(); it != views.end(); ++it) {
        if (s_pageDeferrers.contains(*it)) {
            // The view was not closed during the dispatch.
            (*it)->setIgnoreInputEvents(false);
        }
    }
    deleteAllValues(s_pageDeferrers);
    s_pageDeferrers.clear();

    s_inHostDispatchHandler = false;
    if (!s_attachedAgentsMap) {
        // Remove handlers if all agents were detached within host dispatch.
        v8::Debug::SetMessageHandler(0);
        v8::Debug::SetHostDispatchHandler(0);
    }
}

DebuggerAgentManager::AttachedAgentsMap* DebuggerAgentManager::s_attachedAgentsMap = 0;

void DebuggerAgentManager::debugAttach(DebuggerAgentImpl* debuggerAgent)
{
    if (!s_exposeV8DebuggerProtocol)
        return;
    if (!s_attachedAgentsMap) {
        s_attachedAgentsMap = new AttachedAgentsMap();
        v8::Debug::SetMessageHandler2(&DebuggerAgentManager::onV8DebugMessage);
        v8::Debug::SetHostDispatchHandler(&DebuggerAgentManager::debugHostDispatchHandler, 100 /* ms */);
    }
    int hostId = debuggerAgent->webdevtoolsAgent()->hostId();
    ASSERT(hostId);
    s_attachedAgentsMap->set(hostId, debuggerAgent);
}

void DebuggerAgentManager::debugDetach(DebuggerAgentImpl* debuggerAgent)
{
    if (!s_exposeV8DebuggerProtocol)
        return;
    if (!s_attachedAgentsMap) {
        ASSERT_NOT_REACHED();
        return;
    }
    int hostId = debuggerAgent->webdevtoolsAgent()->hostId();
    ASSERT(s_attachedAgentsMap->get(hostId) == debuggerAgent);
    bool isOnBreakpoint = (findAgentForCurrentV8Context() == debuggerAgent);
    s_attachedAgentsMap->remove(hostId);

    if (s_attachedAgentsMap->isEmpty()) {
        delete s_attachedAgentsMap;
        s_attachedAgentsMap = 0;
        // Note that we do not empty handlers while in dispatch - we schedule
        // continue and do removal once we are out of the dispatch. Also there is
        // no need to send continue command in this case since removing message
        // handler will cause debugger unload and all breakpoints will be cleared.
        if (!s_inHostDispatchHandler) {
            v8::Debug::SetMessageHandler2(0);
            v8::Debug::SetHostDispatchHandler(0);
        }
    } else {
      // Remove all breakpoints set by the agent.
      String clearBreakpointGroupCmd = makeString(
          "{\"seq\":1,\"type\":\"request\",\"command\":\"clearbreakpointgroup\","
              "\"arguments\":{\"groupId\":", String::number(hostId), "}}");
      sendCommandToV8(clearBreakpointGroupCmd, new CallerIdWrapper());

      if (isOnBreakpoint) {
          // Force continue if detach happened in nessted message loop while
          // debugger was paused on a breakpoint(as long as there are other
          // attached agents v8 will wait for explicit'continue' message).
          sendContinueCommandToV8();
      }
    }
}

void DebuggerAgentManager::onV8DebugMessage(const v8::Debug::Message& message)
{
    v8::HandleScope scope;
    v8::String::Value value(message.GetJSON());
    WTF::String out(reinterpret_cast<const UChar*>(*value), value.length());

    // If callerData is not 0 the message is a response to a debugger command.
    if (v8::Debug::ClientData* callerData = message.GetClientData()) {
        CallerIdWrapper* wrapper = static_cast<CallerIdWrapper*>(callerData);
        if (wrapper->callerIsMananager()) {
            // Just ignore messages sent by this manager.
            return;
        }
        DebuggerAgentImpl* debuggerAgent = debuggerAgentForHostId(wrapper->callerId());
        if (debuggerAgent)
            debuggerAgent->debuggerOutput(out);
        else if (!message.WillStartRunning()) {
            // Autocontinue execution if there is no handler.
            sendContinueCommandToV8();
        }
        return;
    } // Otherwise it's an event message.
    ASSERT(message.IsEvent());

    // Ignore unsupported event types.
    if (message.GetEvent() != v8::AfterCompile && message.GetEvent() != v8::Break && message.GetEvent() != v8::Exception)
        return;

    v8::Handle<v8::Context> context = message.GetEventContext();
    // If the context is from one of the inpected tabs it should have its context
    // data.
    if (context.IsEmpty()) {
        // Unknown context, skip the event.
        return;
    }

    // If the context is from one of the inpected tabs or injected extension
    // scripts it must have hostId in the data field.
    int hostId = WebCore::V8Proxy::contextDebugId(context);
    if (hostId != -1) {
        DebuggerAgentImpl* agent = debuggerAgentForHostId(hostId);
        if (agent) {
            if (agent->autoContinueOnException()
                && message.GetEvent() == v8::Exception) {
                sendContinueCommandToV8();
                return;
            }

            agent->debuggerOutput(out);
            return;
        }
    }

    if (!message.WillStartRunning()) {
        // Autocontinue execution on break and exception  events if there is no
        // handler.
        sendContinueCommandToV8();
    }
}

void DebuggerAgentManager::pauseScript()
{
    v8::Debug::DebugBreak();
}

void DebuggerAgentManager::executeDebuggerCommand(const WTF::String& command, int callerId)
{
    sendCommandToV8(command, new CallerIdWrapper(callerId));
}

void DebuggerAgentManager::setMessageLoopDispatchHandler(WebDevToolsAgent::MessageLoopDispatchHandler handler)
{
    s_messageLoopDispatchHandler = handler;
}

void DebuggerAgentManager::setExposeV8DebuggerProtocol(bool value)
{
    s_exposeV8DebuggerProtocol = value;
    WebCore::ScriptDebugServer::shared().setEnabled(!s_exposeV8DebuggerProtocol);
}

void DebuggerAgentManager::setHostId(WebFrameImpl* webframe, int hostId)
{
    ASSERT(hostId > 0);
    WebCore::V8Proxy* proxy = WebCore::V8Proxy::retrieve(webframe->frame());
    if (proxy)
        proxy->setContextDebugId(hostId);
}

void DebuggerAgentManager::onWebViewClosed(WebViewImpl* webview)
{
    if (s_pageDeferrers.contains(webview)) {
        delete s_pageDeferrers.get(webview);
        s_pageDeferrers.remove(webview);
    }
}

void DebuggerAgentManager::onNavigate()
{
    if (s_inHostDispatchHandler)
        DebuggerAgentManager::sendContinueCommandToV8();
}

void DebuggerAgentManager::sendCommandToV8(const WTF::String& cmd, v8::Debug::ClientData* data)
{
    v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd.characters()), cmd.length(), data);
}

void DebuggerAgentManager::sendContinueCommandToV8()
{
    WTF::String continueCmd("{\"seq\":1,\"type\":\"request\",\"command\":\"continue\"}");
    sendCommandToV8(continueCmd, new CallerIdWrapper());
}

DebuggerAgentImpl* DebuggerAgentManager::findAgentForCurrentV8Context()
{
    if (!s_attachedAgentsMap)
        return 0;
    ASSERT(!s_attachedAgentsMap->isEmpty());

    WebCore::Frame* frame = WebCore::V8Proxy::retrieveFrameForEnteredContext();
    if (!frame)
        return 0;
    WebCore::Page* page = frame->page();
    for (AttachedAgentsMap::iterator it = s_attachedAgentsMap->begin(); it != s_attachedAgentsMap->end(); ++it) {
        if (it->second->page() == page)
            return it->second;
    }
    return 0;
}

DebuggerAgentImpl* DebuggerAgentManager::debuggerAgentForHostId(int hostId)
{
    if (!s_attachedAgentsMap)
        return 0;
    return s_attachedAgentsMap->get(hostId);
}

} // namespace WebKit
