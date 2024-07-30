/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WasmStreamingCompiler.h"

#include "JSBigInt.h"
#include "JSWebAssembly.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "StrongInlines.h"
#include "WasmLLIntPlan.h"
#include "WasmStreamingPlan.h"
#include "WasmWorklist.h"

#if ENABLE(WEBASSEMBLY)

namespace JSC { namespace Wasm {

StreamingCompiler::StreamingCompiler(VM& vm, CompilerMode compilerMode, JSGlobalObject* globalObject, JSPromise* promise, JSObject* importObject)
    : m_vm(vm)
    , m_compilerMode(compilerMode)
    , m_info(Wasm::ModuleInformation::create())
    , m_parser(m_info.get(), *this)
{
    Vector<Weak<JSCell>> dependencies;
    dependencies.append(Weak<JSCell>(globalObject));
    if (importObject)
        dependencies.append(Weak<JSCell>(importObject));
    m_ticket = vm.deferredWorkTimer->addPendingWork(DeferredWorkTimer::WorkType::AtSomePoint, vm, promise, WTFMove(dependencies));
    ASSERT(vm.deferredWorkTimer->hasPendingWork(m_ticket));
    ASSERT(vm.deferredWorkTimer->hasDependencyInPendingWork(m_ticket, globalObject));
    ASSERT(!importObject || vm.deferredWorkTimer->hasDependencyInPendingWork(m_ticket, importObject));
}

StreamingCompiler::~StreamingCompiler()
{
    if (m_ticket) {
        auto ticket = std::exchange(m_ticket, nullptr);
        m_vm.deferredWorkTimer->scheduleWorkSoon(ticket, [](DeferredWorkTimer::Ticket) { });
    }
}

Ref<StreamingCompiler> StreamingCompiler::create(VM& vm, CompilerMode compilerMode, JSGlobalObject* globalObject, JSPromise* promise, JSObject* importObject)
{
    return adoptRef(*new StreamingCompiler(vm, compilerMode, globalObject, promise, importObject));
}

bool StreamingCompiler::didReceiveFunctionData(unsigned functionIndex, const Wasm::FunctionData&)
{
    if (!m_plan) {
        m_plan = adoptRef(*new LLIntPlan(m_vm, m_info.copyRef(), m_compilerMode, Plan::dontFinalize()));
        // Plan already failed in preparation. We do not start threaded compilation.
        // Keep Plan failed, and "finalize" will reject promise with that failure.
        if (!m_plan->failed()) {
            m_remainingCompilationRequests = m_info->functions.size();
            m_threadedCompilationStarted = true;
        }
    }

    if (m_threadedCompilationStarted) {
        Ref<Plan> plan = adoptRef(*new StreamingPlan(m_vm, m_info.copyRef(), *m_plan, functionIndex, createSharedTask<Plan::CallbackType>([compiler = Ref { *this }](Plan& plan) {
            compiler->didCompileFunction(static_cast<StreamingPlan&>(plan));
        })));
        ensureWorklist().enqueue(WTFMove(plan));
    }
    return true;
}

void StreamingCompiler::didCompileFunction(StreamingPlan& plan)
{
    Locker locker { m_lock };
    ASSERT(m_threadedCompilationStarted);
    if (plan.failed())
        m_plan->didFailInStreaming(plan.errorMessage());
    m_remainingCompilationRequests--;
    if (!m_remainingCompilationRequests)
        m_plan->didCompileFunctionInStreaming();
    completeIfNecessary();
}

void StreamingCompiler::didFinishParsing()
{
    if (!m_plan) {
        // Reaching here means that this WebAssembly module has no functions.
        ASSERT(!m_info->functions.size());
        ASSERT(!m_remainingCompilationRequests);
        m_plan = adoptRef(*new LLIntPlan(m_vm, m_info.copyRef(), m_compilerMode, Plan::dontFinalize()));
        // If plan is already failed in preparation, we will reject promise with plan's failure soon in finalize.
    }
}

void StreamingCompiler::completeIfNecessary()
{
    if (m_eagerFailed)
        return;

    if (!m_remainingCompilationRequests && m_finalized) {
        m_plan->completeInStreaming();
        didComplete();
    }
}

void StreamingCompiler::didComplete()
{

    auto makeValidationResult = [](JSC::Wasm::LLIntPlan& plan) -> Module::ValidationResult {
        ASSERT(!plan.hasWork());
        if (plan.failed())
            return Unexpected<String>(plan.errorMessage());
        return JSC::Wasm::Module::ValidationResult(Module::create(plan));
    };

    auto result = makeValidationResult(*m_plan);
    auto ticket = std::exchange(m_ticket, nullptr);
    switch (m_compilerMode) {
    case CompilerMode::Validation: {
        m_vm.deferredWorkTimer->scheduleWorkSoon(ticket, [result = WTFMove(result)](DeferredWorkTimer::Ticket ticket) mutable {
            JSPromise* promise = jsCast<JSPromise*>(ticket->target());
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(ticket->dependencies()[0].get());
            VM& vm = globalObject->vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            if (UNLIKELY(!result.has_value())) {
                throwException(globalObject, scope, createJSWebAssemblyCompileError(globalObject, vm, result.error()));
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }

            JSWebAssemblyModule* module = JSWebAssemblyModule::create(vm, globalObject->webAssemblyModuleStructure(), WTFMove(result.value()));

            scope.release();
            promise->resolve(globalObject, module);
        });
        return;
    }

    case CompilerMode::FullCompile: {
        m_vm.deferredWorkTimer->scheduleWorkSoon(ticket, [result = WTFMove(result)](DeferredWorkTimer::Ticket ticket) mutable {
            JSPromise* promise = jsCast<JSPromise*>(ticket->target());
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(ticket->dependencies()[0].get());
            JSObject* importObject = jsCast<JSObject*>(ticket->dependencies()[1].get());
            VM& vm = globalObject->vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            if (UNLIKELY(!result.has_value())) {
                throwException(globalObject, scope, createJSWebAssemblyCompileError(globalObject, vm, result.error()));
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }

            JSWebAssemblyModule* module = JSWebAssemblyModule::create(vm, globalObject->webAssemblyModuleStructure(), WTFMove(result.value()));
            JSWebAssembly::instantiateForStreaming(vm, globalObject, promise, module, importObject);
            if (UNLIKELY(scope.exception())) {
                promise->rejectWithCaughtException(globalObject, scope);
                return;
            }
        });
        return;
    }
    }
}

void StreamingCompiler::finalize(JSGlobalObject* globalObject)
{
    auto state = m_parser.finalize();
    if (state != StreamingParser::State::Finished) {
        fail(globalObject, createJSWebAssemblyCompileError(globalObject, globalObject->vm(), m_parser.errorMessage()));
        return;
    }
    {
        Locker locker { m_lock };
        m_finalized = true;
        completeIfNecessary();
    }
}

void StreamingCompiler::fail(JSGlobalObject* globalObject, JSValue error)
{
    {
        Locker locker { m_lock };
        ASSERT(!m_finalized);
        if (m_eagerFailed)
            return;
        m_eagerFailed = true;
    }
    auto ticket = std::exchange(m_ticket, nullptr);
    JSPromise* promise = jsCast<JSPromise*>(ticket->target());
    // The pending work TicketData was keeping the promise alive. We need to
    // make sure it is reachable from the stack before we remove it from the
    // pending work list. Note: m_ticket stores it as a PackedPtr, which is not
    // scannable by the GC.
    WTF::compilerFence();
    m_vm.deferredWorkTimer->cancelPendingWork(ticket);
    promise->reject(globalObject, error);
}

void StreamingCompiler::cancel()
{
    {
        Locker locker { m_lock };
        ASSERT(!m_finalized);
        if (m_eagerFailed)
            return;
        m_eagerFailed = true;
    }
    auto ticket = std::exchange(m_ticket, nullptr);
    m_vm.deferredWorkTimer->cancelPendingWork(ticket);
}


} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
