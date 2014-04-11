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
#include "PageDebuggerAgent.h"

#if ENABLE(INSPECTOR)

#include "CachedResource.h"
#include "InspectorOverlay.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "PageConsole.h"
#include "PageScriptDebugServer.h"
#include "ScriptState.h"
#include <inspector/InjectedScript.h>
#include <inspector/InjectedScriptManager.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>

using namespace Inspector;

namespace WebCore {

PageDebuggerAgent::PageDebuggerAgent(InjectedScriptManager* injectedScriptManager, InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorOverlay* overlay)
    : WebDebuggerAgent(injectedScriptManager, instrumentingAgents)
    , m_pageAgent(pageAgent)
    , m_overlay(overlay)
    , m_scriptDebugServer(*pageAgent->page())
{
}

void PageDebuggerAgent::enable()
{
    WebDebuggerAgent::enable();
    m_instrumentingAgents->setPageDebuggerAgent(this);
}

void PageDebuggerAgent::disable(bool isBeingDestroyed)
{
    WebDebuggerAgent::disable(isBeingDestroyed);
    m_instrumentingAgents->setPageDebuggerAgent(nullptr);
}

String PageDebuggerAgent::sourceMapURLForScript(const Script& script)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(String, sourceMapHTTPHeader, (ASCIILiteral("SourceMap")));
    DEPRECATED_DEFINE_STATIC_LOCAL(String, sourceMapHTTPHeaderDeprecated, (ASCIILiteral("X-SourceMap")));

    if (!script.url.isEmpty()) {
        CachedResource* resource = m_pageAgent->cachedResource(m_pageAgent->mainFrame(), URL(ParsedURLString, script.url));
        if (resource) {
            String sourceMapHeader = resource->response().httpHeaderField(sourceMapHTTPHeader);
            if (!sourceMapHeader.isEmpty())
                return sourceMapHeader;

            sourceMapHeader = resource->response().httpHeaderField(sourceMapHTTPHeaderDeprecated);
            if (!sourceMapHeader.isEmpty())
                return sourceMapHeader;
        }
    }

    return InspectorDebuggerAgent::sourceMapURLForScript(script);
}

void PageDebuggerAgent::startListeningScriptDebugServer()
{
    scriptDebugServer().addListener(this);
}

void PageDebuggerAgent::stopListeningScriptDebugServer(bool isBeingDestroyed)
{
    scriptDebugServer().removeListener(this, isBeingDestroyed);
}

PageScriptDebugServer& PageDebuggerAgent::scriptDebugServer()
{
    return m_scriptDebugServer;
}

void PageDebuggerAgent::muteConsole()
{
    PageConsole::mute();
}

void PageDebuggerAgent::unmuteConsole()
{
    PageConsole::unmute();
}

void PageDebuggerAgent::breakpointActionLog(JSC::ExecState* exec, const String& message)
{
    m_pageAgent->page()->console().addMessage(MessageSource::JS, MessageLevel::Log, message, createScriptCallStack(exec, ScriptCallStack::maxCallStackSizeToCapture));
}

InjectedScript PageDebuggerAgent::injectedScriptForEval(ErrorString* errorString, const int* executionContextId)
{
    if (!executionContextId) {
        JSC::ExecState* scriptState = mainWorldExecState(m_pageAgent->mainFrame());
        return injectedScriptManager()->injectedScriptFor(scriptState);
    }

    InjectedScript injectedScript = injectedScriptManager()->injectedScriptForId(*executionContextId);
    if (injectedScript.hasNoValue())
        *errorString = ASCIILiteral("Execution context with given id not found.");

    return injectedScript;
}

void PageDebuggerAgent::setOverlayMessage(ErrorString*, const String* message)
{
    m_overlay->setPausedInDebuggerMessage(message);
}

void PageDebuggerAgent::didClearMainFrameWindowObject()
{
    didClearGlobalObject();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
