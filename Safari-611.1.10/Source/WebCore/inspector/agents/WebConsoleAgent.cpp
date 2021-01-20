/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#include "CommandLineAPIHost.h"
#include "DOMWindow.h"
#include "Logging.h"
#include "ResourceError.h"
#include "ResourceResponse.h"
#include "ScriptState.h"
#include "WebInjectedScriptManager.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <wtf/text/StringBuilder.h>


namespace WebCore {

using namespace Inspector;

WebConsoleAgent::WebConsoleAgent(WebAgentContext& context)
    : InspectorConsoleAgent(context)
{
}

WebConsoleAgent::~WebConsoleAgent() = default;

void WebConsoleAgent::frameWindowDiscarded(DOMWindow* window)
{
    for (auto& message : m_consoleMessages) {
        JSC::JSGlobalObject* lexicalGlobalObject = message->globalObject();
        if (!lexicalGlobalObject)
            continue;
        if (domWindowFromExecState(lexicalGlobalObject) != window)
            continue;
        message->clear();
    }

    static_cast<WebInjectedScriptManager&>(m_injectedScriptManager).discardInjectedScriptsFor(window);
}

void WebConsoleAgent::didReceiveResponse(unsigned long requestIdentifier, const ResourceResponse& response)
{
    if (response.httpStatusCode() >= 400) {
        String message = makeString("Failed to load resource: the server responded with a status of ", response.httpStatusCode(), " (", ScriptArguments::truncateStringForConsoleMessage(response.httpStatusText()), ')');
        addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::Network, MessageType::Log, MessageLevel::Error, message, response.url().string(), 0, 0, nullptr, requestIdentifier));
    }
}

void WebConsoleAgent::didFailLoading(unsigned long requestIdentifier, const ResourceError& error)
{
    // Report failures only.
    if (error.isCancellation())
        return;

    StringBuilder message;
    message.appendLiteral("Failed to load resource");
    if (!error.localizedDescription().isEmpty()) {
        message.appendLiteral(": ");
        message.append(error.localizedDescription());
    }

    addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::Network, MessageType::Log, MessageLevel::Error, message.toString(), error.failingURL().string(), 0, 0, nullptr, requestIdentifier));
}

} // namespace WebCore
