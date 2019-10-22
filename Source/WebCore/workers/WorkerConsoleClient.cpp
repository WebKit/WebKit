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
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>

namespace WebCore {
using namespace Inspector;

WorkerConsoleClient::WorkerConsoleClient(WorkerGlobalScope& workerGlobalScope)
    : m_workerGlobalScope(workerGlobalScope)
{
}

WorkerConsoleClient::~WorkerConsoleClient() = default;

void WorkerConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::JSGlobalObject* exec, Ref<Inspector::ScriptArguments>&& arguments)
{
    String messageText;
    arguments->getFirstArgumentAsString(messageText);
    auto message = makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, type, level, messageText, WTFMove(arguments), exec);
    m_workerGlobalScope.addConsoleMessage(WTFMove(message));
}

void WorkerConsoleClient::count(JSC::JSGlobalObject* exec, const String& label)
{
    InspectorInstrumentation::consoleCount(m_workerGlobalScope, exec, label);
}

void WorkerConsoleClient::countReset(JSC::JSGlobalObject* exec, const String& label)
{
    InspectorInstrumentation::consoleCountReset(m_workerGlobalScope, exec, label);
}

void WorkerConsoleClient::time(JSC::JSGlobalObject* exec, const String& label)
{
    InspectorInstrumentation::startConsoleTiming(m_workerGlobalScope, exec, label);
}

void WorkerConsoleClient::timeLog(JSC::JSGlobalObject* exec, const String& label, Ref<ScriptArguments>&& arguments)
{
    InspectorInstrumentation::logConsoleTiming(m_workerGlobalScope, exec, label, WTFMove(arguments));
}

void WorkerConsoleClient::timeEnd(JSC::JSGlobalObject* exec, const String& label)
{
    InspectorInstrumentation::stopConsoleTiming(m_workerGlobalScope, exec, label);
}

// FIXME: <https://webkit.org/b/153499> Web Inspector: console.profile should use the new Sampling Profiler
void WorkerConsoleClient::profile(JSC::JSGlobalObject*, const String&) { }
void WorkerConsoleClient::profileEnd(JSC::JSGlobalObject*, const String&) { }

// FIXME: <https://webkit.org/b/127634> Web Inspector: support debugging web workers
void WorkerConsoleClient::takeHeapSnapshot(JSC::JSGlobalObject*, const String&) { }
void WorkerConsoleClient::timeStamp(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) { }

void WorkerConsoleClient::record(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) { }
void WorkerConsoleClient::recordEnd(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) { }

void WorkerConsoleClient::screenshot(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) { }

} // namespace WebCore
