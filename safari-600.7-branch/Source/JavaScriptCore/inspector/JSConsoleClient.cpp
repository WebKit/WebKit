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
#include "InspectorProfilerAgent.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace JSC;

namespace Inspector {

static bool sLogToSystemConsole = false;

bool JSConsoleClient::logToSystemConsole()
{
    return sLogToSystemConsole;
}

void JSConsoleClient::setLogToSystemConsole(bool shouldLog)
{
    sLogToSystemConsole = shouldLog;
}

void JSConsoleClient::initializeLogToSystemConsole()
{
#if !LOG_DISABLED
    sLogToSystemConsole = true;
#elif USE(CF)
    Boolean keyExistsAndHasValidFormat = false;
    Boolean preference = CFPreferencesGetAppBooleanValue(CFSTR("JavaScriptCoreOutputConsoleMessagesToSystemConsole"), kCFPreferencesCurrentApplication, &keyExistsAndHasValidFormat);
    if (keyExistsAndHasValidFormat)
        sLogToSystemConsole = preference;
#endif
}

JSConsoleClient::JSConsoleClient(InspectorConsoleAgent* consoleAgent, InspectorProfilerAgent* profilerAgent)
    : ConsoleClient()
    , m_consoleAgent(consoleAgent)
    , m_profilerAgent(profilerAgent)
{
    static std::once_flag initializeLogging;
    std::call_once(initializeLogging, []{
        JSConsoleClient::initializeLogToSystemConsole();
    });
}

void JSConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::ExecState* exec, PassRefPtr<ScriptArguments> prpArguments)
{
    RefPtr<ScriptArguments> arguments = prpArguments;

    if (JSConsoleClient::logToSystemConsole())
        ConsoleClient::printConsoleMessageWithArguments(MessageSource::ConsoleAPI, type, level, exec, arguments);

    String message;
    arguments->getFirstArgumentAsString(message);
    m_consoleAgent->addMessageToConsole(MessageSource::ConsoleAPI, type, level, message, exec, arguments.release());
}

void JSConsoleClient::count(ExecState* exec, PassRefPtr<ScriptArguments> arguments)
{
    m_consoleAgent->count(exec, arguments);
}

void JSConsoleClient::profile(JSC::ExecState* exec, const String& title)
{
    if (!m_profilerAgent->enabled())
        return;

    String resolvedTitle = m_profilerAgent->startProfiling(title);

    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(exec, 1));
    m_consoleAgent->addMessageToConsole(MessageSource::ConsoleAPI, MessageType::Profile, MessageLevel::Debug, resolvedTitle, callStack);
}

void JSConsoleClient::profileEnd(JSC::ExecState* exec, const String& title)
{
    if (!m_profilerAgent->enabled())
        return;

    RefPtr<JSC::Profile> profile = m_profilerAgent->stopProfiling(title);
    if (!profile)
        return;

    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(exec, 1));
    String message = makeString(profile->title(), '#', String::number(profile->uid()));
    m_consoleAgent->addMessageToConsole(MessageSource::ConsoleAPI, MessageType::Profile, MessageLevel::Debug, message, callStack);
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
