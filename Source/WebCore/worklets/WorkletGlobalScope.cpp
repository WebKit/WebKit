/*
 * Copyright (C) 2018-2020 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "WorkletGlobalScope.h"

#include "Frame.h"
#include "InspectorInstrumentation.h"
#include "JSWorkletGlobalScope.h"
#include "PageConsoleClient.h"
#include "SecurityOriginPolicy.h"
#include "Settings.h"
#include "WorkerMessagePortChannelProvider.h"
#include "WorkerOrWorkletThread.h"
#include "WorkerScriptLoader.h"
#include "WorkletParameters.h"
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
using namespace Inspector;

WTF_MAKE_ISO_ALLOCATED_IMPL(WorkletGlobalScope);

static std::atomic<unsigned> gNumberOfWorkletGlobalScopes { 0 };

WorkletGlobalScope::WorkletGlobalScope(WorkerOrWorkletThread& thread, Ref<JSC::VM>&& vm, const WorkletParameters& parameters)
    : WorkerOrWorkletGlobalScope(WorkerThreadType::Worklet, WTFMove(vm), &thread)
    , m_topOrigin(SecurityOrigin::createUnique())
    , m_url(parameters.windowURL)
    , m_jsRuntimeFlags(parameters.jsRuntimeFlags)
    , m_settingsValues(parameters.settingsValues)
{
    ++gNumberOfWorkletGlobalScopes;

    setSecurityOriginPolicy(SecurityOriginPolicy::create(SecurityOrigin::create(this->url())));
    setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { this->url() }, *this));
}

WorkletGlobalScope::WorkletGlobalScope(Document& document, Ref<JSC::VM>&& vm, ScriptSourceCode&& code)
    : WorkerOrWorkletGlobalScope(WorkerThreadType::Worklet, WTFMove(vm), nullptr)
    , m_document(document)
    , m_topOrigin(SecurityOrigin::createUnique())
    , m_url(code.url())
    , m_jsRuntimeFlags(document.settings().javaScriptRuntimeFlags())
    , m_code(WTFMove(code))
    , m_settingsValues(document.settingsValues().isolatedCopy())
{
    ++gNumberOfWorkletGlobalScopes;

    ASSERT(document.page());

    setSecurityOriginPolicy(SecurityOriginPolicy::create(SecurityOrigin::create(this->url())));
    setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { this->url() }, *this));
}

WorkletGlobalScope::~WorkletGlobalScope()
{
    ASSERT(!script());
    removeFromContextsMap();
    ASSERT(gNumberOfWorkletGlobalScopes);
    --gNumberOfWorkletGlobalScopes;
}

unsigned WorkletGlobalScope::numberOfWorkletGlobalScopes()
{
    return gNumberOfWorkletGlobalScopes;
}

void WorkletGlobalScope::prepareForDestruction()
{
    WorkerOrWorkletGlobalScope::prepareForDestruction();

    if (script()) {
        script()->vm().notifyNeedTermination();
        clearScript();
    }
}

String WorkletGlobalScope::userAgent(const URL& url) const
{
    if (!m_document)
        return "";
    return m_document->userAgent(url);
}

void WorkletGlobalScope::evaluate()
{
    if (m_code)
        script()->evaluate(*m_code);
}

URL WorkletGlobalScope::completeURL(const String& url, ForceUTF8) const
{
    if (url.isNull())
        return URL();
    return URL(this->url(), url);
}

void WorkletGlobalScope::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<ScriptCallStack>&& stack)
{
    if (!m_document || isJSExecutionForbidden())
        return;
    m_document->logExceptionToConsole(errorMessage, sourceURL, lineNumber, columnNumber, WTFMove(stack));
}

void WorkletGlobalScope::addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&& message)
{
    if (!m_document || isJSExecutionForbidden() || !message)
        return;
    m_document->addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(message->source(), message->type(), message->level(), message->message(), 0));
}

void WorkletGlobalScope::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    if (!m_document || isJSExecutionForbidden())
        return;
    m_document->addConsoleMessage(source, level, message, requestIdentifier);
}

void WorkletGlobalScope::addMessage(MessageSource source, MessageLevel level, const String& messageText, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::JSGlobalObject*, unsigned long requestIdentifier)
{
    if (!m_document || isJSExecutionForbidden())
        return;
    m_document->addMessage(source, level, messageText, sourceURL, lineNumber, columnNumber, WTFMove(callStack), nullptr, requestIdentifier);
}

ReferrerPolicy WorkletGlobalScope::referrerPolicy() const
{
    return ReferrerPolicy::NoReferrer;
}

void WorkletGlobalScope::fetchAndInvokeScript(const URL& moduleURL, FetchRequestCredentials credentials, CompletionHandler<void(std::optional<Exception>&&)>&& completionHandler)
{
    ASSERT(!isMainThread());
    script()->loadAndEvaluateModule(moduleURL, credentials, WTFMove(completionHandler));
}

MessagePortChannelProvider& WorkletGlobalScope::messagePortChannelProvider()
{
    if (!m_messagePortChannelProvider)
        m_messagePortChannelProvider = makeUnique<WorkerMessagePortChannelProvider>(*this);
    return *m_messagePortChannelProvider;
}

} // namespace WebCore
