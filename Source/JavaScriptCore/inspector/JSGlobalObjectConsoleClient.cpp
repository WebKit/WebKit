/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#include "JSGlobalObjectConsoleClient.h"

#include "ConsoleMessage.h"
#include "InspectorConsoleAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorHeapAgent.h"
#include "InspectorScriptProfilerAgent.h"
#include "ScriptArguments.h"
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace Inspector {

using namespace JSC;

#if !LOG_DISABLED
static bool sLogToSystemConsole = true;
#else
static bool sLogToSystemConsole = false;
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(JSGlobalObjectConsoleClient);

bool JSGlobalObjectConsoleClient::logToSystemConsole()
{
    return sLogToSystemConsole;
}

void JSGlobalObjectConsoleClient::setLogToSystemConsole(bool shouldLog)
{
    sLogToSystemConsole = shouldLog;
}

JSGlobalObjectConsoleClient::JSGlobalObjectConsoleClient(InspectorConsoleAgent* consoleAgent)
    : ConsoleClient()
    , m_consoleAgent(consoleAgent)
{
}

void JSGlobalObjectConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    if (JSGlobalObjectConsoleClient::logToSystemConsole())
        ConsoleClient::printConsoleMessageWithArguments(MessageSource::ConsoleAPI, type, level, globalObject, arguments.copyRef());

    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    RefPtr<StringImpl> message;
    bool messageIsFirstArgument = arguments->getFirstArgumentAsString(message);

    auto consoleMessage = makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, type, level, message, WTF::move(arguments), globalObject);
    consoleMessage->setMessageIsFirstArgument(messageIsFirstArgument);
    m_consoleAgent->addMessageToConsole(WTF::move(consoleMessage));

    if (type == MessageType::Assert) {
        if (CheckedPtr agent = m_debuggerAgent)
            agent->handleConsoleAssert(message);
    }
}

void JSGlobalObjectConsoleClient::count(JSGlobalObject* globalObject, StringImpl* label)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    m_consoleAgent->count(globalObject, label);
}

void JSGlobalObjectConsoleClient::countReset(JSGlobalObject* globalObject, StringImpl* label)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    m_consoleAgent->countReset(globalObject, label);
}

void JSGlobalObjectConsoleClient::profile(JSC::JSGlobalObject*, StringImpl* title)
{
    if (!m_consoleAgent->enabled()) [[likely]]
        return;

    // Allow duplicate unnamed profiles. Disallow duplicate named profiles.
    if (!isEmpty(title)) {
        for (auto& existingTitle : m_profiles) {
            if (equal(existingTitle, title)) {
                // FIXME: Send an enum to the frontend for localization?
                RefPtr<StringImpl> warning = isEmpty(title) ? "Unnamed Profile already exists"_si : makeString("Profile \""_s, ScriptArguments::truncateStringForConsoleMessage(title), "\" already exists"_s);
                m_consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Profile, MessageLevel::Warning, warning));
                return;
            }
        }
    }

    m_profiles.append(title);
    startConsoleProfile();
}

void JSGlobalObjectConsoleClient::profileEnd(JSC::JSGlobalObject*, StringImpl* title)
{
    if (!m_consoleAgent->enabled()) [[likely]]
        return;

    // Stop profiles in reverse order. If the title is empty, then stop the last profile.
    // Otherwise, match the title of the profile to stop.
    for (ptrdiff_t i = m_profiles.size() - 1; i >= 0; --i) {
        if (isEmpty(title) || equal(m_profiles[i], title)) {
            m_profiles.removeAt(i);
            if (m_profiles.isEmpty())
                stopConsoleProfile();
            return;
        }
    }

    // FIXME: Send an enum to the frontend for localization?
    RefPtr<StringImpl> warning = isEmpty(title) ? "No profiles exist"_si : makeString("Profile \""_s, title, "\" does not exist"_s);
    m_consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::ProfileEnd, MessageLevel::Warning, warning));
}

void JSGlobalObjectConsoleClient::startConsoleProfile()
{
    if (CheckedPtr agent = m_debuggerAgent) {
        m_profileRestoreBreakpointActiveValue = agent->breakpointsActive();
        std::ignore = agent->setBreakpointsActive(false);
    }

    if (CheckedPtr agent = m_scriptProfilerAgent)
        std::ignore = agent->startTracking(true);
}

void JSGlobalObjectConsoleClient::stopConsoleProfile()
{
    if (CheckedPtr agent = m_scriptProfilerAgent)
        std::ignore = agent->stopTracking();

    if (CheckedPtr agent = m_debuggerAgent)
        std::ignore = agent->setBreakpointsActive(m_profileRestoreBreakpointActiveValue);
}

void JSGlobalObjectConsoleClient::takeHeapSnapshot(JSC::JSGlobalObject*, StringImpl* title)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    if (!m_heapAgent)
        return;

    auto result = CheckedRef { *m_heapAgent }->snapshot();
    if (!result)
        return;

    auto [timestamp, snapshotData] = WTF::move(result.value());
    m_consoleAgent->reportHeapSnapshot(timestamp, snapshotData, title);
}

void JSGlobalObjectConsoleClient::time(JSGlobalObject* globalObject, StringImpl* label)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    m_consoleAgent->startTiming(globalObject, label);
}

void JSGlobalObjectConsoleClient::timeLog(JSGlobalObject* globalObject, StringImpl* label, Ref<ScriptArguments>&& arguments)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    m_consoleAgent->logTiming(globalObject, label, WTF::move(arguments));
}

void JSGlobalObjectConsoleClient::timeEnd(JSGlobalObject* globalObject, StringImpl* label)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    m_consoleAgent->stopTiming(globalObject, label);
}

void JSGlobalObjectConsoleClient::timeStamp(JSGlobalObject*, Ref<ScriptArguments>&&)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    warnUnimplemented("console.timeStamp"_si);
}

void JSGlobalObjectConsoleClient::record(JSGlobalObject*, Ref<ScriptArguments>&&)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    warnUnimplemented("console.record"_si);
}

void JSGlobalObjectConsoleClient::recordEnd(JSGlobalObject*, Ref<ScriptArguments>&&)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    warnUnimplemented("console.recordEnd"_si);
}

void JSGlobalObjectConsoleClient::screenshot(JSGlobalObject*, Ref<ScriptArguments>&&)
{
    if (!m_consoleAgent->developerExtrasEnabled()) [[likely]]
        return;

    warnUnimplemented("console.screenshot"_si);
}

void JSGlobalObjectConsoleClient::warnUnimplemented(StringImpl* method)
{
    auto message = makeString(method, " is currently ignored in JavaScript context inspection."_s);
    m_consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Log, MessageLevel::Warning, message));
}

} // namespace Inspector
