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
#include "InspectorConsoleAgent.h"

#if ENABLE(INSPECTOR)

#include "ConsoleMessage.h"
#include "InjectedScriptManager.h"
#include "ScriptArguments.h"
#include "ScriptCallFrame.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "ScriptObject.h"
#include <wtf/CurrentTime.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

static const unsigned maximumConsoleMessages = 1000;
static const int expireConsoleMessagesStep = 100;

InspectorConsoleAgent::InspectorConsoleAgent(InjectedScriptManager* injectedScriptManager)
    : InspectorAgentBase(ASCIILiteral("Console"))
    , m_injectedScriptManager(injectedScriptManager)
    , m_previousMessage(nullptr)
    , m_expiredConsoleMessageCount(0)
    , m_enabled(false)
{
}

InspectorConsoleAgent::~InspectorConsoleAgent()
{
}

void InspectorConsoleAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorConsoleFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorConsoleBackendDispatcher::create(backendDispatcher, this);
}

void InspectorConsoleAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    String errorString;
    disable(&errorString);
}

void InspectorConsoleAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;

    m_enabled = true;

    if (m_expiredConsoleMessageCount) {
        ConsoleMessage expiredMessage(!isWorkerAgent(), MessageSource::Other, MessageType::Log, MessageLevel::Warning, String::format("%d console messages are not shown.", m_expiredConsoleMessageCount));
        expiredMessage.addToFrontend(m_frontendDispatcher.get(), m_injectedScriptManager, false);
    }

    size_t messageCount = m_consoleMessages.size();
    for (size_t i = 0; i < messageCount; ++i)
        m_consoleMessages[i]->addToFrontend(m_frontendDispatcher.get(), m_injectedScriptManager, false);
}

void InspectorConsoleAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;

    m_enabled = false;
}

void InspectorConsoleAgent::clearMessages(ErrorString*)
{
    m_consoleMessages.clear();
    m_expiredConsoleMessageCount = 0;
    m_previousMessage = nullptr;

    m_injectedScriptManager->releaseObjectGroup(ASCIILiteral("console"));

    if (m_frontendDispatcher && m_enabled)
        m_frontendDispatcher->messagesCleared();
}

void InspectorConsoleAgent::reset()
{
    ErrorString error;
    clearMessages(&error);

    m_times.clear();
    m_counts.clear();
}

void InspectorConsoleAgent::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtr<ScriptCallStack> callStack, unsigned long requestIdentifier)
{
    if (!m_injectedScriptManager->inspectorEnvironment().developerExtrasEnabled())
        return;

    if (type == MessageType::Clear) {
        ErrorString error;
        clearMessages(&error);
    }

    addConsoleMessage(adoptPtr(new ConsoleMessage(!isWorkerAgent(), source, type, level, message, callStack, requestIdentifier)));
}

void InspectorConsoleAgent::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, const String& message, JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments, unsigned long requestIdentifier)
{
    if (!m_injectedScriptManager->inspectorEnvironment().developerExtrasEnabled())
        return;

    if (type == MessageType::Clear) {
        ErrorString error;
        clearMessages(&error);
    }

    addConsoleMessage(adoptPtr(new ConsoleMessage(!isWorkerAgent(), source, type, level, message, arguments, state, requestIdentifier)));
}

void InspectorConsoleAgent::addMessageToConsole(MessageSource source, MessageType type, MessageLevel level, const String& message, const String& scriptID, unsigned lineNumber, unsigned columnNumber, JSC::ExecState* state, unsigned long requestIdentifier)
{
    if (!m_injectedScriptManager->inspectorEnvironment().developerExtrasEnabled())
        return;

    if (type == MessageType::Clear) {
        ErrorString error;
        clearMessages(&error);
    }

    bool canGenerateCallStack = !isWorkerAgent() && m_frontendDispatcher;
    addConsoleMessage(adoptPtr(new ConsoleMessage(canGenerateCallStack, source, type, level, message, scriptID, lineNumber, columnNumber, state, requestIdentifier)));
}

Vector<unsigned> InspectorConsoleAgent::consoleMessageArgumentCounts() const
{
    Vector<unsigned> result(m_consoleMessages.size());
    for (size_t i = 0; i < m_consoleMessages.size(); i++)
        result[i] = m_consoleMessages[i]->argumentCount();
    return result;
}

void InspectorConsoleAgent::startTiming(const String& title)
{
    // Follow Firebug's behavior of requiring a title that is not null or
    // undefined for timing functions
    if (title.isNull())
        return;

    m_times.add(title, monotonicallyIncreasingTime());
}

void InspectorConsoleAgent::stopTiming(const String& title, PassRefPtr<ScriptCallStack> callStack)
{
    // Follow Firebug's behavior of requiring a title that is not null or
    // undefined for timing functions
    if (title.isNull())
        return;

    HashMap<String, double>::iterator it = m_times.find(title);
    if (it == m_times.end())
        return;

    double startTime = it->value;
    m_times.remove(it);

    double elapsed = monotonicallyIncreasingTime() - startTime;
    String message = title + String::format(": %.3fms", elapsed * 1000);
    addMessageToConsole(MessageSource::ConsoleAPI, MessageType::Timing, MessageLevel::Debug, message, callStack);
}

void InspectorConsoleAgent::count(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(state));
    const ScriptCallFrame& lastCaller = callStack->at(0);
    // Follow Firebug's behavior of counting with null and undefined title in
    // the same bucket as no argument
    String title;
    arguments->getFirstArgumentAsString(title);
    String identifier = title + '@' + lastCaller.sourceURL() + ':' + String::number(lastCaller.lineNumber());

    HashMap<String, unsigned>::iterator it = m_counts.find(identifier);
    int count;
    if (it == m_counts.end())
        count = 1;
    else {
        count = it->value + 1;
        m_counts.remove(it);
    }

    m_counts.add(identifier, count);

    String message = title + ": " + String::number(count);
    addMessageToConsole(MessageSource::ConsoleAPI, MessageType::Log, MessageLevel::Debug, message, callStack);
}

static bool isGroupMessage(MessageType type)
{
    return type == MessageType::StartGroup
        || type == MessageType::StartGroupCollapsed
        || type == MessageType::EndGroup;
}

void InspectorConsoleAgent::addConsoleMessage(PassOwnPtr<ConsoleMessage> consoleMessage)
{
    ASSERT(m_injectedScriptManager->inspectorEnvironment().developerExtrasEnabled());
    ASSERT_ARG(consoleMessage, consoleMessage);

    if (m_previousMessage && !isGroupMessage(m_previousMessage->type()) && m_previousMessage->isEqual(consoleMessage.get())) {
        m_previousMessage->incrementCount();
        if (m_frontendDispatcher && m_enabled)
            m_previousMessage->updateRepeatCountInConsole(m_frontendDispatcher.get());
    } else {
        m_previousMessage = consoleMessage.get();
        m_consoleMessages.append(consoleMessage);
        if (m_frontendDispatcher && m_enabled)
            m_previousMessage->addToFrontend(m_frontendDispatcher.get(), m_injectedScriptManager, true);
    }

    if (!m_frontendDispatcher && m_consoleMessages.size() >= maximumConsoleMessages) {
        m_expiredConsoleMessageCount += expireConsoleMessagesStep;
        m_consoleMessages.remove(0, expireConsoleMessagesStep);
    }
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
