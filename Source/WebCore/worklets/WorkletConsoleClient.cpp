/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WorkletConsoleClient.h"

#if ENABLE(CSS_PAINTING_API)

#include "InspectorInstrumentation.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>

namespace WebCore {
using namespace Inspector;

WorkletConsoleClient::WorkletConsoleClient(WorkletGlobalScope& workletGlobalScope)
    : m_workletGlobalScope(workletGlobalScope)
{
}

WorkletConsoleClient::~WorkletConsoleClient() = default;

void WorkletConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::ExecState* exec, Ref<Inspector::ScriptArguments>&& arguments)
{
    String messageText;
    arguments->getFirstArgumentAsString(messageText);
    auto message = makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, type, level, messageText, WTFMove(arguments), exec);
    m_workletGlobalScope.addConsoleMessage(WTFMove(message));
}

void WorkletConsoleClient::count(JSC::ExecState*, const String&) { }
void WorkletConsoleClient::countReset(JSC::ExecState*, const String&) { }

void WorkletConsoleClient::time(JSC::ExecState*, const String&) { }
void WorkletConsoleClient::timeLog(JSC::ExecState*, const String&, Ref<ScriptArguments>&&) { }
void WorkletConsoleClient::timeEnd(JSC::ExecState*, const String&) { }

void WorkletConsoleClient::profile(JSC::ExecState*, const String&) { }
void WorkletConsoleClient::profileEnd(JSC::ExecState*, const String&) { }

void WorkletConsoleClient::takeHeapSnapshot(JSC::ExecState*, const String&) { }
void WorkletConsoleClient::timeStamp(JSC::ExecState*, Ref<ScriptArguments>&&) { }

void WorkletConsoleClient::record(JSC::ExecState*, Ref<ScriptArguments>&&) { }
void WorkletConsoleClient::recordEnd(JSC::ExecState*, Ref<ScriptArguments>&&) { }

void WorkletConsoleClient::screenshot(JSC::ExecState*, Ref<ScriptArguments>&&) { }

} // namespace WebCore
#endif
