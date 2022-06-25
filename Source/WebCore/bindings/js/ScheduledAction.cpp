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
#include "DOMWindow.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "Frame.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWindow.h"
#include "JSExecState.h"
#include "JSExecStateInstrumentation.h"
#include "JSWorkerGlobalScope.h"
#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include "ScriptSourceCode.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <JavaScriptCore/JSLock.h>

namespace WebCore {
using namespace JSC;

std::unique_ptr<ScheduledAction> ScheduledAction::create(DOMWrapperWorld& isolatedWorld, JSC::Strong<JSC::Unknown>&& function)
{
    return std::unique_ptr<ScheduledAction>(new ScheduledAction(isolatedWorld, WTFMove(function)));
}

std::unique_ptr<ScheduledAction> ScheduledAction::create(DOMWrapperWorld& isolatedWorld, String&& code)
{
    return std::unique_ptr<ScheduledAction>(new ScheduledAction(isolatedWorld, WTFMove(code)));
}

ScheduledAction::ScheduledAction(DOMWrapperWorld& isolatedWorld, JSC::Strong<JSC::Unknown>&& function)
    : m_isolatedWorld(isolatedWorld)
    , m_function(WTFMove(function))
{
}

ScheduledAction::ScheduledAction(DOMWrapperWorld& isolatedWorld, String&& code)
    : m_isolatedWorld(isolatedWorld)
    , m_function(isolatedWorld.vm())
    , m_code(WTFMove(code))
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
    if (is<Document>(context))
        execute(downcast<Document>(context));
    else
        execute(downcast<WorkerGlobalScope>(context));
}

void ScheduledAction::executeFunctionInContext(JSGlobalObject* globalObject, JSValue thisValue, ScriptExecutionContext& context)
{
    ASSERT(m_function);
    VM& vm = context.vm();
    JSLockHolder lock(vm);
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    auto callData = JSC::getCallData(m_function.get());
    if (callData.type == CallData::Type::None)
        return;

    JSGlobalObject* lexicalGlobalObject = globalObject;

    MarkedArgumentBuffer arguments;
    for (auto& argument : m_arguments)
        arguments.append(argument.get());
    if (UNLIKELY(arguments.hasOverflowed())) {
        {
            auto throwScope = DECLARE_THROW_SCOPE(vm);
            throwOutOfMemoryError(lexicalGlobalObject, throwScope);
        }
        NakedPtr<JSC::Exception> exception = catchScope.exception();
        catchScope.clearException();
        reportException(lexicalGlobalObject, exception);
        return;
    }

    JSExecState::instrumentFunction(&context, callData);

    NakedPtr<JSC::Exception> exception;
    JSExecState::profiledCall(lexicalGlobalObject, JSC::ProfilingReason::Other, m_function.get(), callData, thisValue, arguments, exception);
    EXCEPTION_ASSERT(!catchScope.exception());
    
    InspectorInstrumentation::didCallFunction(&context);

    if (exception)
        reportException(lexicalGlobalObject, exception);
}

void ScheduledAction::execute(Document& document)
{
    JSDOMWindow* window = toJSDOMWindow(document.frame(), m_isolatedWorld);
    if (!window)
        return;

    RefPtr<Frame> frame = window->wrapped().frame();
    if (!frame || !frame->script().canExecuteScripts(AboutToExecuteScript))
        return;

    if (m_function)
        executeFunctionInContext(window, &window->proxy(), document);
    else
        frame->script().executeScriptInWorldIgnoringException(m_isolatedWorld, m_code);
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
        ScriptSourceCode code(m_code, URL(workerGlobalScope.url()));
        scriptController->evaluate(code);
    }
}

} // namespace WebCore
