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

#include <inspector/ConsoleMessage.h>
#include <inspector/ScriptArguments.h>

using namespace Inspector;

namespace WebCore {

WorkerConsoleClient::WorkerConsoleClient(WorkerGlobalScope& workerGlobalScope)
    : m_workerGlobalScope(workerGlobalScope)
{
}

WorkerConsoleClient::~WorkerConsoleClient()
{
}

void WorkerConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::ExecState* exec, RefPtr<Inspector::ScriptArguments>&& arguments)
{
    String messageText;
    arguments->getFirstArgumentAsString(messageText);
    auto message = std::make_unique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, type, level, messageText, WTFMove(arguments), exec);
    m_workerGlobalScope.addConsoleMessage(WTFMove(message));
}

// FIXME: <https://webkit.org/b/153499> Web Inspector: console.profile should use the new Sampling Profiler
void WorkerConsoleClient::profile(JSC::ExecState*, const String&) { }
void WorkerConsoleClient::profileEnd(JSC::ExecState*, const String&) { }

// FIXME: <https://webkit.org/b/127634> Web Inspector: support debugging web workers
void WorkerConsoleClient::count(JSC::ExecState*, RefPtr<ScriptArguments>&&) { }
void WorkerConsoleClient::takeHeapSnapshot(JSC::ExecState*, const String&) { }
void WorkerConsoleClient::time(JSC::ExecState*, const String&) { }
void WorkerConsoleClient::timeEnd(JSC::ExecState*, const String&) { }
void WorkerConsoleClient::timeStamp(JSC::ExecState*, RefPtr<ScriptArguments>&&) { }

} // namespace WebCore
