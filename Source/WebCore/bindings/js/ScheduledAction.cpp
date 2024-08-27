/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *  Copyright (C) 2009 Google Inc. All rights reseved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "ScheduledAction.h"

#include "ContentSecurityPolicy.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWindow.h"
#include "JSExecState.h"
#include "JSExecStateInstrumentation.h"
#include "JSWorkerGlobalScope.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include "ScriptSourceCode.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/SourceProvider.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScheduledAction);

std::unique_ptr<ScheduledAction> ScheduledAction::create(DOMWrapperWorld& isolatedWorld, Strong<JSObject>&& function)
{
    return std::unique_ptr<ScheduledAction>(new ScheduledAction(isolatedWorld, WTFMove(function)));
}

std::unique_ptr<ScheduledAction> ScheduledAction::create(DOMWrapperWorld& isolatedWorld, String&& code)
{
    return std::unique_ptr<ScheduledAction>(new ScheduledAction(isolatedWorld, WTFMove(code)));
}

ScheduledAction::ScheduledAction(DOMWrapperWorld& isolatedWorld, Strong<JSObject>&& function)
    : m_isolatedWorld(isolatedWorld)
    , m_function(WTFMove(function))
    , m_sourceTaintedOrigin(JSC::SourceTaintedOrigin::Untainted)
{
}

ScheduledAction::ScheduledAction(DOMWrapperWorld& isolatedWorld, String&& code)
    : m_isolatedWorld(isolatedWorld)
    , m_function(isolatedWorld.vm())
    , m_code(WTFMove(code))
    , m_sourceTaintedOrigin(JSC::computeNewSourceTaintedOriginFromStack(isolatedWorld.vm(), isolatedWorld.vm().topCallFrame))
{
}

ScheduledAction::~ScheduledAction() = default;

void ScheduledAction::addArguments(FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    m_arguments = WTFMove(arguments);
}

auto ScheduledAction::type() const -> Type
{
    return m_function ? Type::Function : Type::Code;
}

void ScheduledAction::execute(ScriptExecutionContext& context)
{
    if (auto* document = dynamicDowncast<Document>(context))
        execute(*document);
    else
        execute(downcast<WorkerGlobalScope>(context));
}

void ScheduledAction::executeFunctionInContext(JSGlobalObject* globalObject, JSValue thisValue, ScriptExecutionContext& context)
{
    ASSERT(m_function);
    VM& vm = context.vm();
    JSLockHolder lock(vm);
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    JSObject* jsFunction = m_function.get();
    auto callData = JSC::getCallData(jsFunction);
    if (callData.type == CallData::Type::None)
        return;

    auto* jsFunctionGlobalObject = jsFunction->globalObject();

    JSGlobalObject* lexicalGlobalObject = globalObject;

    MarkedArgumentBuffer arguments;
    arguments.ensureCapacity(m_arguments.size());
    for (auto& argument : m_arguments)
        arguments.append(argument.get());
    if (UNLIKELY(arguments.hasOverflowed())) {
        reportException(jsFunctionGlobalObject, JSC::Exception::create(vm, createOutOfMemoryError(lexicalGlobalObject)));
        return;
    }

    JSExecState::instrumentFunction(&context, callData);

    NakedPtr<JSC::Exception> exception;
    JSExecState::profiledCall(lexicalGlobalObject, ProfilingReason::Other, jsFunction, callData, thisValue, arguments, exception);
    catchScope.assertNoExceptionExceptTermination();
    
    InspectorInstrumentation::didCallFunction(&context);

    if (exception)
        reportException(jsFunctionGlobalObject, exception);
}

void ScheduledAction::execute(Document& document)
{
    auto* window = toJSDOMWindow(document.frame(), m_isolatedWorld);
    if (!window)
        return;

    RefPtr frame = dynamicDowncast<LocalFrame>(window->wrapped().frame());
    if (!frame || !frame->script().canExecuteScripts(ReasonForCallingCanExecuteScripts::AboutToExecuteScript))
        return;

    if (m_function)
        executeFunctionInContext(window, &window->proxy(), document);
    else
        frame->script().executeScriptInWorldIgnoringException(m_isolatedWorld, m_code, m_sourceTaintedOrigin);
}

void ScheduledAction::execute(WorkerGlobalScope& workerGlobalScope)
{
    // In a Worker, the execution should always happen on a worker thread.
    ASSERT(workerGlobalScope.thread().thread() == &Thread::current());

    auto* scriptController = workerGlobalScope.script();

    if (m_function) {
        auto* contextWrapper = scriptController->globalScopeWrapper();
        executeFunctionInContext(contextWrapper, contextWrapper, workerGlobalScope);
    } else {
        ScriptSourceCode code(m_code, m_sourceTaintedOrigin, URL(workerGlobalScope.url()));
        scriptController->evaluate(code);
    }
}

} // namespace WebCore
