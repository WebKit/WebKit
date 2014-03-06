/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "WebConsoleAgent.h"

#if ENABLE(INSPECTOR)

#include "CommandLineAPIHost.h"
#include "DOMWindow.h"
#include "ResourceError.h"
#include "ResourceResponse.h"
#include "ScriptProfiler.h"
#include "ScriptState.h"
#include "WebInjectedScriptManager.h"
#include <inspector/ConsoleMessage.h>
#include <runtime/JSCInlines.h>
#include <wtf/text/StringBuilder.h>

using namespace Inspector;

namespace WebCore {

WebConsoleAgent::WebConsoleAgent(WebInjectedScriptManager* injectedScriptManager)
    : InspectorConsoleAgent(injectedScriptManager)
    , m_monitoringXHREnabled(false)
{
}

void WebConsoleAgent::setMonitoringXHREnabled(ErrorString*, bool enabled)
{
    m_monitoringXHREnabled = enabled;
}

void WebConsoleAgent::frameWindowDiscarded(DOMWindow* window)
{
    size_t messageCount = m_consoleMessages.size();
    for (size_t i = 0; i < messageCount; ++i) {
        JSC::ExecState* exec = m_consoleMessages[i]->scriptState();
        if (!exec)
            continue;
        if (domWindowFromExecState(exec) != window)
            continue;
        m_consoleMessages[i]->clear();
    }

    static_cast<WebInjectedScriptManager*>(m_injectedScriptManager)->discardInjectedScriptsFor(window);
}

void WebConsoleAgent::didFinishXHRLoading(unsigned long requestIdentifier, const String& url, const String& sendURL, unsigned sendLineNumber, unsigned sendColumnNumber)
{
    if (!m_injectedScriptManager->inspectorEnvironment().developerExtrasEnabled())
        return;

    if (m_frontendDispatcher && m_monitoringXHREnabled) {
        String message = "XHR finished loading: \"" + url + "\".";
        addMessageToConsole(MessageSource::Network, MessageType::Log, MessageLevel::Debug, message, sendURL, sendLineNumber, sendColumnNumber, nullptr, requestIdentifier);
    }
}

void WebConsoleAgent::didReceiveResponse(unsigned long requestIdentifier, const ResourceResponse& response)
{
    if (!m_injectedScriptManager->inspectorEnvironment().developerExtrasEnabled())
        return;

    if (response.httpStatusCode() >= 400) {
        String message = "Failed to load resource: the server responded with a status of " + String::number(response.httpStatusCode()) + " (" + response.httpStatusText() + ')';
        addMessageToConsole(MessageSource::Network, MessageType::Log, MessageLevel::Error, message, response.url().string(), 0, 0, nullptr, requestIdentifier);
    }
}

void WebConsoleAgent::didFailLoading(unsigned long requestIdentifier, const ResourceError& error)
{
    if (!m_injectedScriptManager->inspectorEnvironment().developerExtrasEnabled())
        return;

    // Report failures only.
    if (error.isCancellation())
        return;

    StringBuilder message;
    message.appendLiteral("Failed to load resource");
    if (!error.localizedDescription().isEmpty()) {
        message.appendLiteral(": ");
        message.append(error.localizedDescription());
    }

    addMessageToConsole(MessageSource::Network, MessageType::Log, MessageLevel::Error, message.toString(), error.failingURL(), 0, 0, nullptr, requestIdentifier);
}

class InspectableHeapObject final : public CommandLineAPIHost::InspectableObject {
public:
    explicit InspectableHeapObject(int heapObjectId)
        : m_heapObjectId(heapObjectId)
    {
    }

    virtual Deprecated::ScriptValue get(JSC::ExecState*) override
    {
        return ScriptProfiler::objectByHeapObjectId(m_heapObjectId);
    }

private:
    int m_heapObjectId;
};

void WebConsoleAgent::addInspectedHeapObject(ErrorString*, int inspectedHeapObjectId)
{
    if (CommandLineAPIHost* commandLineAPIHost = static_cast<WebInjectedScriptManager*>(m_injectedScriptManager)->commandLineAPIHost())
        commandLineAPIHost->addInspectedObject(std::make_unique<InspectableHeapObject>(inspectedHeapObjectId));
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
