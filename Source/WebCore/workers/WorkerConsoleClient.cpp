/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WorkerConsoleClient.h"

#include "InspectorInstrumentation.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerConsoleClient);

WorkerConsoleClient::WorkerConsoleClient(WorkerOrWorkletGlobalScope& globalScope)
    : m_globalScope(globalScope)
{
}

WorkerConsoleClient::~WorkerConsoleClient() = default;

void WorkerConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::JSGlobalObject* exec, Ref<Inspector::ScriptArguments>&& arguments)
{
    String messageText;
    arguments->getFirstArgumentAsString(messageText);
    auto message = makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, type, level, messageText, WTFMove(arguments), exec);
    Ref { m_globalScope.get() }->addConsoleMessage(WTFMove(message));
}

void WorkerConsoleClient::count(JSC::JSGlobalObject* exec, const String& label)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::consoleCount(*worker, exec, label);
}

void WorkerConsoleClient::countReset(JSC::JSGlobalObject* exec, const String& label)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::consoleCountReset(*worker, exec, label);
}

void WorkerConsoleClient::time(JSC::JSGlobalObject* exec, const String& label)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::startConsoleTiming(*worker, exec, label);
}

void WorkerConsoleClient::timeLog(JSC::JSGlobalObject* exec, const String& label, Ref<ScriptArguments>&& arguments)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::logConsoleTiming(*worker, exec, label, WTFMove(arguments));
}

void WorkerConsoleClient::timeEnd(JSC::JSGlobalObject* exec, const String& label)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::stopConsoleTiming(*worker, exec, label);
}

void WorkerConsoleClient::profile(JSC::JSGlobalObject*, const String& title)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::startProfiling(*worker, title);
}

void WorkerConsoleClient::profileEnd(JSC::JSGlobalObject*, const String& title)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::stopProfiling(*worker, title);
}

void WorkerConsoleClient::takeHeapSnapshot(JSC::JSGlobalObject*, const String& title)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::takeHeapSnapshot(*worker, title);
}

void WorkerConsoleClient::timeStamp(JSC::JSGlobalObject*, Ref<ScriptArguments>&& arguments)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::consoleTimeStamp(*worker, WTFMove(arguments));
}

// FIXME: <https://webkit.org/b/243362> Web Inspector: support starting/stopping recordings from the console in a Worker
void WorkerConsoleClient::record(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) { }
void WorkerConsoleClient::recordEnd(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) { }

// FIXME: <https://webkit.org/b/243361> Web Inspector: support console screenshots in a Worker
void WorkerConsoleClient::screenshot(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) { }

} // namespace WebCore
