/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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

#if ENABLE(CSS_PAINTING_API)

#include "Document.h"
#include "Frame.h"
#include "InspectorInstrumentation.h"
#include "JSWorkletGlobalScope.h"
#include "PageConsoleClient.h"
#include "SecurityOriginPolicy.h"
#include "Settings.h"
#include "WorkletScriptController.h"

#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/ScriptCallStack.h>

namespace WebCore {
using namespace Inspector;

WorkletGlobalScope::WorkletGlobalScope(Document& document, ScriptSourceCode&& code)
    : m_document(makeWeakPtr(document))
    , m_sessionID(m_document->sessionID())
    , m_script(makeUniqueRef<WorkletScriptController>(this))
    , m_topOrigin(SecurityOrigin::createUnique())
    , m_identifier(makeString("WorkletGlobalScope:"_s, Inspector::IdentifiersFactory::createIdentifier()))
    , m_eventQueue(*this)
    , m_code(WTFMove(code))
{
    auto* frame = document.frame();
    m_jsRuntimeFlags = frame ? frame->settings().javaScriptRuntimeFlags() : JSC::RuntimeFlags();
    ASSERT(document.page());

    setSecurityOriginPolicy(SecurityOriginPolicy::create(m_topOrigin.copyRef()));
    setContentSecurityPolicy(std::make_unique<ContentSecurityPolicy>(URL { code.url() }, *this));
}

WorkletGlobalScope::~WorkletGlobalScope()
{
    removeFromContextsMap();
    m_script->workletGlobalScopeWrapper()->setConsoleClient(nullptr);
}

String WorkletGlobalScope::origin() const
{
    return m_topOrigin->toString();
}

String WorkletGlobalScope::userAgent(const URL& url) const
{
    if (!m_document)
        return "";
    return m_document->userAgent(url);
}

void WorkletGlobalScope::evaluate()
{
    m_script->evaluate(m_code);
}

bool WorkletGlobalScope::isJSExecutionForbidden() const
{
    return m_script->isExecutionForbidden();
}

void WorkletGlobalScope::disableEval(const String& errorMessage)
{
    m_script->disableEval(errorMessage);
}

void WorkletGlobalScope::disableWebAssembly(const String& errorMessage)
{
    m_script->disableWebAssembly(errorMessage);
}

URL WorkletGlobalScope::completeURL(const String& url) const
{
    if (url.isNull())
        return URL();
    return URL(m_code.url(), url);
}

void WorkletGlobalScope::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<ScriptCallStack>&& stack)
{
    if (!m_document)
        return;
    m_document->logExceptionToConsole(errorMessage, sourceURL, lineNumber, columnNumber, WTFMove(stack));
}

void WorkletGlobalScope::addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&& message)
{
    if (!m_document)
        return;
    m_document->addConsoleMessage(WTFMove(message));
}

void WorkletGlobalScope::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    if (!m_document)
        return;
    m_document->addConsoleMessage(source, level, message, requestIdentifier);
}

void WorkletGlobalScope::addMessage(MessageSource source, MessageLevel level, const String& messageText, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::ExecState* state, unsigned long requestIdentifier)
{
    if (!m_document)
        return;
    m_document->addMessage(source, level, messageText, sourceURL, lineNumber, columnNumber, WTFMove(callStack), state, requestIdentifier);
}

} // namespace WebCore
#endif
