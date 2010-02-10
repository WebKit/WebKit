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

#ifndef ToolsAgent_h
#define ToolsAgent_h

#include "DevToolsRPC.h"

namespace WebKit {

// Tools agent provides API for enabling / disabling other agents as well as
// API for auxiliary UI functions such as dom elements highlighting.
#define TOOLS_AGENT_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, METHOD4, METHOD5) \
    /* Dispatches given function on the InspectorController object */ \
    METHOD3(dispatchOnInspectorController, int /* call_id */, \
        String /* function_name */, String /* json_args */) \
    \
    /* Dispatches given function on the InjectedScript object */ \
    METHOD5(dispatchOnInjectedScript, int /* call_id */, \
        int /* injected_script_id */, String /* function_name */, \
        String /* json_args */, bool /* async */)

DEFINE_RPC_CLASS(ToolsAgent, TOOLS_AGENT_STRUCT)

#define TOOLS_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, METHOD4, METHOD5) \
    /* Updates focused node on the client. */ \
    METHOD1(frameNavigate, String /* url */) \
    \
    /* Response to the DispatchOn*. */ \
    METHOD3(didDispatchOn, int /* call_id */, String /* result */, String /* exception */) \
    \
    /* Sends InspectorFrontend message to be dispatched on client. */ \
    METHOD1(dispatchOnClient, String /* data */)

DEFINE_RPC_CLASS(ToolsAgentDelegate, TOOLS_AGENT_DELEGATE_STRUCT)

} // namespace WebKit

#endif
