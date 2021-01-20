/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#include "InspectorFrontendAPIDispatcher.h"

#include "Frame.h"
#include "InspectorController.h"
#include "Page.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ScriptSourceCode.h"
#include "ScriptState.h"
#include <JavaScriptCore/FrameTracers.h>
#include <wtf/RunLoop.h>


namespace WebCore {

using EvaluationError = InspectorFrontendAPIDispatcher::EvaluationError;
using EvaluationResultHandler = CompletionHandler<Expected<JSC::JSValue, EvaluationError>>;

InspectorFrontendAPIDispatcher::InspectorFrontendAPIDispatcher(Page& frontendPage)
    : m_frontendPage(makeWeakPtr(frontendPage))
{
}

InspectorFrontendAPIDispatcher::~InspectorFrontendAPIDispatcher()
{
    invalidateQueuedExpressions();
}

void InspectorFrontendAPIDispatcher::reset()
{
    m_frontendLoaded = false;
    m_suspended = false;

    invalidateQueuedExpressions();
}

void InspectorFrontendAPIDispatcher::frontendLoaded()
{
    ASSERT(m_frontendPage);
    m_frontendLoaded = true;

    // In some convoluted WebKitLegacy-only scenarios, the backend may try to dispatch events to the frontend
    // underneath InspectorFrontendHost::loaded() when it is unsafe to execute script, causing suspend() to
    // be called before the frontend has fully loaded. See <https://bugs.webkit.org/show_bug.cgi?id=218840>.
    if (!m_suspended)
        evaluateQueuedExpressions();
}

void InspectorFrontendAPIDispatcher::suspend(UnsuspendSoon unsuspendSoon)
{
    if (m_suspended)
        return;

    m_suspended = true;

    if (unsuspendSoon == UnsuspendSoon::Yes) {
        RunLoop::main().dispatch([protectedThis = makeRef(*this)] {
            // If the frontend page has been deallocated, there's nothing to do.
            if (!protectedThis->m_frontendPage)
                return;

            protectedThis->unsuspend();
        });
    }
}

void InspectorFrontendAPIDispatcher::unsuspend()
{
    if (!m_suspended)
        return;

    m_suspended = false;

    if (m_frontendLoaded)
        evaluateQueuedExpressions();
}

JSC::JSGlobalObject* InspectorFrontendAPIDispatcher::frontendGlobalObject()
{
    if (!m_frontendPage)
        return nullptr;
    
    return m_frontendPage->mainFrame().script().globalObject(mainThreadNormalWorld());
}

static String expressionForEvaluatingCommand(const String& command, Vector<Ref<JSON::Value>>&& arguments)
{
    StringBuilder expression;
    expression.appendLiteral("InspectorFrontendAPI.dispatch([\"");
    expression.append(command);
    expression.append('"');

    for (auto& argument : arguments) {
        expression.appendLiteral(", ");
        argument->writeJSON(expression);
    }

    expression.append(']');
    expression.append(')');
    return expression.toString();
}

InspectorFrontendAPIDispatcher::EvaluationResult InspectorFrontendAPIDispatcher::dispatchCommandWithResultSync(const String& command, Vector<Ref<JSON::Value>>&& arguments)
{
    if (m_suspended)
        return makeUnexpected(EvaluationError::ExecutionSuspended);

    return evaluateExpression(expressionForEvaluatingCommand(command, WTFMove(arguments)));
}

void InspectorFrontendAPIDispatcher::dispatchCommandWithResultAsync(const String& command, Vector<Ref<JSON::Value>>&& arguments, EvaluationResultHandler&& resultHandler)
{
    evaluateOrQueueExpression(expressionForEvaluatingCommand(command, WTFMove(arguments)), WTFMove(resultHandler));
}

void InspectorFrontendAPIDispatcher::dispatchMessageAsync(const String& message)
{
    evaluateOrQueueExpression(makeString("InspectorFrontendAPI.dispatchMessageAsync(", message, ")"));
}

void InspectorFrontendAPIDispatcher::evaluateOrQueueExpression(const String& expression, EvaluationResultHandler&& optionalResultHandler)
{
    // If the frontend page has been deallocated, then there is nothing to do.
    if (!m_frontendPage) {
        if (optionalResultHandler)
            optionalResultHandler(makeUnexpected(EvaluationError::ContextDestroyed));

        return;
    }

    // Sometimes we get here by sending messages for events triggered by DOM mutations earlier in the call stack.
    // If this is the case, then it's not safe to evaluate script synchronously, so do it later. This only affects
    // WebKit1 and some layout tests that use a single web process for both the inspector and inspected page.
    if (!ScriptDisallowedScope::InMainThread::isScriptAllowed())
        suspend(UnsuspendSoon::Yes);

    if (!m_frontendLoaded || m_suspended) {
        m_queuedEvaluations.append(std::make_pair(expression, WTFMove(optionalResultHandler)));
        return;
    }

    ValueOrException result = evaluateExpression(expression);
    if (optionalResultHandler)
        optionalResultHandler(result);
}

void InspectorFrontendAPIDispatcher::invalidateQueuedExpressions()
{
    auto queuedEvaluations = std::exchange(m_queuedEvaluations, { });
    for (auto& pair : queuedEvaluations) {
        auto resultHandler = WTFMove(pair.second);
        if (resultHandler)
            resultHandler(makeUnexpected(EvaluationError::ContextDestroyed));
    }
}

void InspectorFrontendAPIDispatcher::evaluateQueuedExpressions()
{
    // If the frontend page has been deallocated, then there is nothing to do.
    if (!m_frontendPage)
        return;

    if (m_queuedEvaluations.isEmpty())
        return;

    auto queuedEvaluations = std::exchange(m_queuedEvaluations, { });
    for (auto& pair : queuedEvaluations) {
        auto result = evaluateExpression(pair.first);
        if (auto resultHandler = WTFMove(pair.second))
            resultHandler(result);
    }
}

ValueOrException InspectorFrontendAPIDispatcher::evaluateExpression(const String& expression)
{
    ASSERT(m_frontendPage);
    ASSERT(!m_suspended);
    ASSERT(m_queuedEvaluations.isEmpty());

    JSC::SuspendExceptionScope scope(&m_frontendPage->inspectorController().vm());
    return m_frontendPage->mainFrame().script().evaluateInWorld(ScriptSourceCode(expression), mainThreadNormalWorld());
}

void InspectorFrontendAPIDispatcher::evaluateExpressionForTesting(const String& expression)
{
    evaluateOrQueueExpression(expression);
}

} // namespace WebKit
