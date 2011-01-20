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

#ifndef DebuggerAgentManager_h
#define DebuggerAgentManager_h

#include "WebCString.h"
#include "WebDevToolsAgent.h"
#include <v8-debug.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {
class Page;
class PageGroupLoadDeferrer;
}

namespace WebKit {

class DebuggerAgentImpl;
class DictionaryValue;
class WebFrameImpl;
class WebViewImpl;

// There is single v8 instance per render process. Also there may be several
// RenderViews and consequently devtools agents in the process that want to talk
// to the v8 debugger. This class coordinates communication between the debug
// agents and v8 debugger. It will set debug output handler as long as at least
// one debugger agent is attached and remove it when last debugger agent is
// detached. When message is received from debugger it will route it to the
// right debugger agent if there is one otherwise the message will be ignored.
//
// v8 may send a message(e.g. exception event) after which it
// would expect some actions from the handler. If there is no appropriate
// debugger agent to handle such messages the manager will perform the action
// itself, otherwise v8 may hang waiting for the action.
class DebuggerAgentManager {
    WTF_MAKE_NONCOPYABLE(DebuggerAgentManager);
public:
    static void debugAttach(DebuggerAgentImpl* debuggerAgent);
    static void debugDetach(DebuggerAgentImpl* debuggerAgent);
    static void pauseScript();
    static void executeDebuggerCommand(const WTF::String& command, int callerId);
    static void setMessageLoopDispatchHandler(WebDevToolsAgent::MessageLoopDispatchHandler handler);
    static void setExposeV8DebuggerProtocol(bool);

    // Sets |hostId| as the frame context data. This id is used to filter scripts
    // related to the inspected page.
    static void setHostId(WebFrameImpl* webframe, int hostId);

    static void onWebViewClosed(WebViewImpl* webview);

    static void onNavigate();

private:
    DebuggerAgentManager();
    ~DebuggerAgentManager();

    static void debugHostDispatchHandler();
    static void onV8DebugMessage(const v8::Debug::Message& message);
    static void sendCommandToV8(const WTF::String& cmd,
                                v8::Debug::ClientData* data);
    static void sendContinueCommandToV8();

    static DebuggerAgentImpl* findAgentForCurrentV8Context();
    static DebuggerAgentImpl* debuggerAgentForHostId(int hostId);

    typedef HashMap<int, DebuggerAgentImpl*> AttachedAgentsMap;
    static AttachedAgentsMap* s_attachedAgentsMap;

    static WebDevToolsAgent::MessageLoopDispatchHandler s_messageLoopDispatchHandler;
    static bool s_inHostDispatchHandler;
    typedef HashMap<WebViewImpl*, WebCore::PageGroupLoadDeferrer*> DeferrersMap;
    static DeferrersMap s_pageDeferrers;

    static bool s_exposeV8DebuggerProtocol;
};

} // namespace WebKit

#endif
