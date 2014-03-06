/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSConsoleClient.h"

#if ENABLE(INSPECTOR)

#include "InspectorConsoleAgent.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"

using namespace JSC;

namespace Inspector {

JSConsoleClient::JSConsoleClient(InspectorConsoleAgent* consoleAgent)
    : ConsoleClient()
    , m_consoleAgent(consoleAgent)
{
}

void JSConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::ExecState* exec, PassRefPtr<ScriptArguments> prpArguments)
{
    RefPtr<ScriptArguments> arguments = prpArguments;

#if !LOG_DISABLED
    ConsoleClient::printConsoleMessageWithArguments(MessageSource::ConsoleAPI, type, level, exec, arguments);
#endif

    String message;
    arguments->getFirstArgumentAsString(message);
    m_consoleAgent->addMessageToConsole(MessageSource::ConsoleAPI, type, level, message, exec, arguments.release());
}

void JSConsoleClient::count(ExecState* exec, PassRefPtr<ScriptArguments> arguments)
{
    m_consoleAgent->count(exec, arguments);
}

void JSConsoleClient::profile(ExecState*, const String&)
{
    // FIXME: JSContext inspection needs a profiler.
    warnUnimplemented(ASCIILiteral("console.profile"));
}

void JSConsoleClient::profileEnd(ExecState*, const String&)
{
    // FIXME: JSContext inspection needs a profiler.
    // Already warned in profile(), we do not need to warn again.
}

void JSConsoleClient::time(ExecState*, const String& title)
{
    m_consoleAgent->startTiming(title);
}

void JSConsoleClient::timeEnd(ExecState* exec, const String& title)
{
    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(exec, 1));
    m_consoleAgent->stopTiming(title, callStack.release());
}

void JSConsoleClient::timeStamp(ExecState*, PassRefPtr<ScriptArguments>)
{
    // FIXME: JSContext inspection needs a timeline.
    warnUnimplemented(ASCIILiteral("console.timeStamp"));
}

void JSConsoleClient::warnUnimplemented(const String& method)
{
    String message = method + " is currently ignored in JavaScript context inspection.";
    m_consoleAgent->addMessageToConsole(MessageSource::ConsoleAPI, MessageType::Log, MessageLevel::Warning, message, nullptr, nullptr);
}

} // namespace Inspector

#endif
