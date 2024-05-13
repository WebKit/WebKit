/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011, 2012 Google Inc. All Rights Reserved.
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
#include "WorkerOrWorkletScriptController.h"

#include "CommonVM.h"
#include "DedicatedWorkerGlobalScope.h"
#include "EventLoop.h"
#include "JSAudioWorkletGlobalScope.h"
#include "JSDOMBinding.h"
#include "JSDedicatedWorkerGlobalScope.h"
#include "JSEventTarget.h"
#include "JSExecState.h"
#include "JSPaintWorkletGlobalScope.h"
#include "JSServiceWorkerGlobalScope.h"
#include "JSSharedWorkerGlobalScope.h"
#include "ModuleFetchFailureKind.h"
#include "ModuleFetchParameters.h"
#include "ScriptSourceCode.h"
#include "WebCoreJSClientData.h"
#include "WorkerConsoleClient.h"
#include "WorkerModuleScriptLoader.h"
#include "WorkerOrWorkletThread.h"
#include "WorkerRunLoop.h"
#include "WorkerScriptFetcher.h"
#include <JavaScriptCore/AbstractModuleRecord.h>
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/DeferTermination.h>
#include <JavaScriptCore/DeferredWorkTimer.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/GCActivityCallback.h>
#include <JavaScriptCore/JSGlobalProxyInlines.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSNativeStdFunction.h>
#include <JavaScriptCore/JSScriptFetchParameters.h>
#include <JavaScriptCore/JSScriptFetcher.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/VMTrapsInlines.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerOrWorkletScriptController);

using namespace JSC;

WorkerOrWorkletScriptController::WorkerOrWorkletScriptController(WorkerThreadType type, Ref<VM>&& vm, WorkerOrWorkletGlobalScope* globalScope)
    : m_vm(WTFMove(vm))
    , m_globalScope(globalScope)
    , m_globalScopeWrapper(*m_vm)
{
    if (!isMainThread() || m_vm != &commonVM()) {
        m_vm->heap.acquireAccess(); // It's not clear that we have good discipline for heap access, so turn it on permanently.
        {
            JSLockHolder lock(m_vm.get());
            m_vm->ensureTerminationException();
            m_vm->forbidExecutionOnTermination();
        }

        JSVMClientData::initNormalWorld(m_vm.get(), type);
    }
}

WorkerOrWorkletScriptController::~WorkerOrWorkletScriptController()
{
    JSLockHolder lock(vm());
    if (m_globalScopeWrapper) {
        m_globalScopeWrapper->clearDOMGuardedObjects();
        m_globalScopeWrapper->setConsoleClient(nullptr);

    }
    m_globalScopeWrapper.clear();
    m_vm = nullptr;
}

void WorkerOrWorkletScriptController::attachDebugger(JSC::Debugger* debugger)
{
    initScriptIfNeeded();
    debugger->attach(m_globalScopeWrapper.get());
}

void WorkerOrWorkletScriptController::detachDebugger(JSC::Debugger* debugger)
{
    debugger->detach(m_globalScopeWrapper.get(), JSC::Debugger::TerminatingDebuggingSession);
}

void WorkerOrWorkletScriptController::forbidExecution()
{
    ASSERT(m_globalScope->isContextThread());
    m_vm->setExecutionForbidden();
}

bool WorkerOrWorkletScriptController::isExecutionForbidden() const
{
    ASSERT(m_globalScope->isContextThread());
    return m_vm->executionForbidden();
}

void WorkerOrWorkletScriptController::scheduleExecutionTermination()
{
    {
        // The mutex provides a memory barrier to ensure that once
        // termination is scheduled, isTerminatingExecution() will
        // accurately reflect that lexicalGlobalObject when called from another thread.
        Locker locker { m_scheduledTerminationLock };
        if (m_isTerminatingExecution)
            return;

        m_isTerminatingExecution = true;
    }
    if (m_vm != &commonVM())
        m_vm->notifyNeedTermination();
}

bool WorkerOrWorkletScriptController::isTerminatingExecution() const
{
    // See comments in scheduleExecutionTermination regarding mutex usage.
    Locker locker { m_scheduledTerminationLock };
    return m_isTerminatingExecution;
}

void WorkerOrWorkletScriptController::releaseHeapAccess()
{
    m_vm->heap.releaseAccess();
}

void WorkerOrWorkletScriptController::acquireHeapAccess()
{
    m_vm->heap.acquireAccess();
}

void WorkerOrWorkletScriptController::addTimerSetNotification(JSC::JSRunLoopTimer::TimerNotificationCallback callback)
{
    auto processTimer = [&] (JSRunLoopTimer* timer) {
        if (!timer)
            return;
        timer->addTimerSetNotification(callback);
    };

    processTimer(m_vm->heap.fullActivityCallback());
    processTimer(m_vm->heap.edenActivityCallback());
    processTimer(m_vm->deferredWorkTimer.ptr());
}

void WorkerOrWorkletScriptController::removeTimerSetNotification(JSC::JSRunLoopTimer::TimerNotificationCallback callback)
{
    auto processTimer = [&] (JSRunLoopTimer* timer) {
        if (!timer)
            return;
        timer->removeTimerSetNotification(callback);
    };

    processTimer(m_vm->heap.fullActivityCallback());
    processTimer(m_vm->heap.edenActivityCallback());
    processTimer(m_vm->deferredWorkTimer.ptr());
}

void WorkerOrWorkletScriptController::setException(JSC::Exception* exception)
{
    JSC::JSGlobalObject* lexicalGlobalObject = m_globalScopeWrapper.get();
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwException(lexicalGlobalObject, scope, exception);
}

void WorkerOrWorkletScriptController::disableEval(const String& errorMessage)
{
    initScriptIfNeeded();
    JSLockHolder lock { vm() };

    m_globalScopeWrapper->setEvalEnabled(false, errorMessage);
}

void WorkerOrWorkletScriptController::disableWebAssembly(const String& errorMessage)
{
    initScriptIfNeeded();
    JSLockHolder lock { vm() };

    m_globalScopeWrapper->setWebAssemblyEnabled(false, errorMessage);
}

void WorkerOrWorkletScriptController::setRequiresTrustedTypes(bool required)
{
    initScriptIfNeeded();
    JSLockHolder lock { vm() };

    m_globalScopeWrapper->setRequiresTrustedTypes(required);
}

void WorkerOrWorkletScriptController::evaluateAndReportException(const ScriptSourceCode& sourceCode)
{
    VM& vm = this->vm();
    NakedPtr<JSC::Exception> uncaughtException;
    evaluate(sourceCode, uncaughtException);
    if (uncaughtException) {
        JSLockHolder lock(vm);
        reportException(m_globalScopeWrapper.get(), uncaughtException);
    }
}

void WorkerOrWorkletScriptController::evaluate(const ScriptSourceCode& sourceCode, NakedPtr<JSC::Exception>& returnedException)
{
    if (isExecutionForbidden())
        return;

    initScriptIfNeeded();

    auto& globalObject = *m_globalScopeWrapper.get();
    VM& vm = globalObject.vm();
    JSLockHolder lock { vm };

    JSExecState::profiledEvaluate(&globalObject, JSC::ProfilingReason::Other, sourceCode.jsSourceCode(), m_globalScopeWrapper->globalThis(), returnedException);

    if ((returnedException && vm.isTerminationException(returnedException)) || isTerminatingExecution()) {
        forbidExecution();
        return;
    }
}

JSC::JSValue WorkerOrWorkletScriptController::evaluateModule(JSC::AbstractModuleRecord& moduleRecord, JSC::JSValue awaitedValue, JSC::JSValue resumeMode)
{
    auto& globalObject = *m_globalScopeWrapper.get();
    VM& vm = globalObject.vm();
    JSLockHolder lock { vm };
    return moduleRecord.evaluate(&globalObject, awaitedValue, resumeMode);
}

void WorkerOrWorkletScriptController::loadModuleAndEvaluate(WorkerScriptFetcher& scriptFetcher, const ScriptSourceCode& sourceCode, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (isExecutionForbidden()) {
        completionHandler(nullString());
        return;
    }

    initScriptIfNeeded();

    auto& globalObject = *m_globalScopeWrapper.get();
    VM& vm = globalObject.vm();
    JSLockHolder lock { vm };
    auto scope = DECLARE_THROW_SCOPE(vm);

    Ref protector { scriptFetcher };
    {
        auto jsScriptFetcher = JSScriptFetcher::create(vm, { protector.ptr() });

        Identifier moduleName = JSModuleLoader::createIdentifierForEntryPointModule(vm);
        auto& loadModulePromise = JSExecState::loadModule(globalObject, moduleName, sourceCode.jsSourceCode(), jsScriptFetcher);
        scope.assertNoExceptionExceptTermination();
        RETURN_IF_EXCEPTION(scope, void());

        auto task = createSharedTask<void(const String&)>([completionHandler = WTFMove(completionHandler)](const String& message) mutable {
            completionHandler(message);
        });

        auto& loadModuleFullfillHandler = *JSNativeStdFunction::create(vm, &globalObject, 1, String(), [task, protector, moduleName, jsScriptFetcher](JSGlobalObject* globalObject, CallFrame*) {
            VM& vm = globalObject->vm();
            JSLockHolder lock { vm };
            protector->setHasFinishedFetchingModules();

            auto& evaluateModulePromise = JSExecState::evaluateModule(*globalObject, moduleName, jsScriptFetcher);
            auto& evaluateModuleFullfillHandler = *JSNativeStdFunction::create(vm, globalObject, 0, String(), [task](JSGlobalObject* globalObject, CallFrame*) -> EncodedJSValue {
                VM& vm = globalObject->vm();
                JSLockHolder lock { vm };
                task->run(nullString());
                return JSValue::encode(jsUndefined());
            });

            auto& evaluateModuleRejectHandler = *JSNativeStdFunction::create(vm, globalObject, 1, String(), [task](JSGlobalObject* globalObject, CallFrame* callFrame) -> EncodedJSValue {
                VM& vm = globalObject->vm();
                JSLockHolder lock { vm };

                JSValue errorValue = callFrame->argument(0);
                reportException(globalObject, errorValue);

                auto catchScope = DECLARE_CATCH_SCOPE(vm);
                String message = retrieveErrorMessage(*globalObject, vm, errorValue, catchScope);

                task->run(message);
                return JSValue::encode(jsUndefined());
            });

            return JSValue::encode(evaluateModulePromise.then(globalObject, &evaluateModuleFullfillHandler, &evaluateModuleRejectHandler));
        });

        auto& loadModuleRejectHandler = *JSNativeStdFunction::create(vm, &globalObject, 1, String(), [globalScope = m_globalScope, task, protector](JSGlobalObject* globalObject, CallFrame* callFrame) {
            VM& vm = globalObject->vm();
            JSLockHolder lock { vm };
            protector->setHasFinishedFetchingModules();

            String message;

            JSValue errorValue = callFrame->argument(0);
            if (auto* object = errorValue.getObject()) {
                if (JSValue failureKindValue = object->getDirect(vm, vm.propertyNames->builtinNames().failureKindPrivateName())) {
                    if (static_cast<ModuleFetchFailureKind>(failureKindValue.asInt32()) == ModuleFetchFailureKind::WasPropagatedError)
                        message = "Importing a module script failed."_s;
                }
            }

            if (!message) {
                auto catchScope = DECLARE_CATCH_SCOPE(vm);
                message = retrieveErrorMessage(*globalObject, vm, errorValue, catchScope);
            }

            downcast<WorkerGlobalScope>(*globalScope).reportErrorToWorkerObject(message);
            task->run(message);

            return JSValue::encode(jsUndefined());
        });

        loadModulePromise.then(&globalObject, &loadModuleFullfillHandler, &loadModuleRejectHandler);
    }
    m_globalScope->eventLoop().performMicrotaskCheckpoint();

    // Drive RunLoop until we get either of "Worker is terminated", "Loading is done", or "Loading is failed".
    WorkerRunLoop& runLoop = m_globalScope->workerOrWorkletThread()->runLoop();

    // We do not want to receive messages that are not related to asynchronous resource loading.
    // Otherwise, a worker discards some messages from the main thread here in a racy way.
    // For example, the main thread can postMessage just after creating a Worker. In that case, postMessage's
    // task is queued in WorkerRunLoop before start running module scripts. This task should not be discarded
    // in the following driving of the RunLoop which mainly attempt to collect initial load of module scripts.
    String taskMode = WorkerModuleScriptLoader::taskMode();

    // Allow tasks scheduled from the WorkerEventLoop.
    constexpr bool allowEventLoopTasks = true;

    bool success = true;
    while (!protector->hasFinishedFetchingModules() && success) {
        success = runLoop.runInMode(m_globalScope, taskMode, allowEventLoopTasks);
        if (success)
            m_globalScope->eventLoop().performMicrotaskCheckpoint();
    }
}

// https://html.spec.whatwg.org/multipage/worklets.html#dom-worklet-addmodule
// fetchModule() and evaluateModule() are called separately because the algorithm (see step 5.4.4)
// swallows the exception raised during module evaluation
void WorkerOrWorkletScriptController::fetchWorkletModuleAndEvaluate(const URL& moduleURL, FetchOptions::Credentials credentials, CompletionHandler<void(std::optional<Exception>&&)>&& completionHandler)
{
    if (isExecutionForbidden()) {
        completionHandler(Exception { ExceptionCode::NotAllowedError });
        return;
    }

    initScriptIfNeeded();

    auto& globalObject = *m_globalScopeWrapper.get();
    VM& vm = globalObject.vm();
    JSLockHolder lock { vm };

    auto parameters = ModuleFetchParameters::create(JSC::ScriptFetchParameters::Type::JavaScript, emptyString(), /* isTopLevelModule */ true);
    auto scriptFetcher = WorkerScriptFetcher::create(WTFMove(parameters), credentials, globalScope()->destination(), globalScope()->referrerPolicy());
    {
        Identifier moduleName = Identifier::fromString(vm, moduleURL.string());
        auto jsScriptFetcher = JSScriptFetcher::create(vm, { scriptFetcher.ptr() });
        auto& fetchModulePromise = JSExecState::fetchModule(globalObject, moduleName, JSScriptFetchParameters::create(vm, scriptFetcher->parameters()), jsScriptFetcher);

        auto task = createSharedTask<void(std::optional<Exception>&&)>([completionHandler = WTFMove(completionHandler)](std::optional<Exception>&& exception) mutable {
            completionHandler(WTFMove(exception));
        });

        auto& fetchModuleFullfillHandler = *JSNativeStdFunction::create(vm, &globalObject, 1, String(), [moduleName, jsScriptFetcher, task](JSGlobalObject* globalObject, CallFrame*) -> EncodedJSValue {
            VM& vm = globalObject->vm();
            JSLockHolder lock { vm };

            auto& evaluateModulePromise = JSExecState::evaluateModule(*globalObject, moduleName, jsScriptFetcher);
            auto& evaluateModuleSettledHandler = *JSNativeStdFunction::create(vm, globalObject, 1, String(), [task](JSGlobalObject* globalObject, CallFrame*) -> EncodedJSValue {
                VM& vm = globalObject->vm();
                JSLockHolder lock { vm };

                // FIXME: Once console logging from Worklet is fixed, report an exception here if necessary.
                // https://bugs.webkit.org/show_bug.cgi?id=220039

                task->run(std::nullopt);
                return JSValue::encode(jsUndefined());
            });

            return JSValue::encode(evaluateModulePromise.then(globalObject, &evaluateModuleSettledHandler, &evaluateModuleSettledHandler));
        });

        auto& fetchModuleRejectHandler = *JSNativeStdFunction::create(vm, &globalObject, 1, String(), [task](JSGlobalObject* globalObject, CallFrame* callFrame) {
            VM& vm = globalObject->vm();
            JSLockHolder lock { vm };

            ExceptionCode code = ExceptionCode::AbortError;

            JSValue errorValue = callFrame->argument(0);
            if (auto* object = errorValue.getObject()) {
                if (JSValue failureKindValue = object->getDirect(vm, vm.propertyNames->builtinNames().failureKindPrivateName())) {
                    switch (static_cast<ModuleFetchFailureKind>(failureKindValue.asInt32())) {
                    case ModuleFetchFailureKind::WasPropagatedError:
                    case ModuleFetchFailureKind::WasCanceled:
                        code = ExceptionCode::AbortError;
                        break;
                    case ModuleFetchFailureKind::WasFetchError:
                    case ModuleFetchFailureKind::WasResolveError:
                        code = ExceptionCode::TypeError;
                        break;
                    }
                } else if (object->inherits<ErrorInstance>()) {
                    auto* error = jsCast<ErrorInstance*>(object);
                    if (error->errorType() == ErrorType::TypeError)
                        code = ExceptionCode::TypeError;
                    else if (error->errorType() == ErrorType::SyntaxError)
                        code = ExceptionCode::JSSyntaxError;
                }
            }

            auto catchScope = DECLARE_CATCH_SCOPE(vm);
            String message = retrieveErrorMessageWithoutName(*globalObject, vm, errorValue, catchScope);
            task->run(Exception { code, message });
            return JSValue::encode(jsUndefined());
        });

        fetchModulePromise.then(&globalObject, &fetchModuleFullfillHandler, &fetchModuleRejectHandler);
    }
    m_globalScope->eventLoop().performMicrotaskCheckpoint();
}

template<typename JSGlobalScopePrototype, typename JSGlobalScope, typename GlobalScope>
void WorkerOrWorkletScriptController::initScriptWithSubclass()
{
    ASSERT(!m_globalScopeWrapper);

    JSLockHolder lock { vm() };

    // Explicitly protect the global object's prototype so it isn't collected
    // when we allocate the global object. (Once the global object is fully
    // constructed, it can mark its own prototype.)
    Structure* contextPrototypeStructure = JSGlobalScopePrototype::createStructure(*m_vm, nullptr, jsNull());
    auto* contextPrototype = JSGlobalScopePrototype::create(*m_vm, nullptr, contextPrototypeStructure);
    Structure* structure = JSGlobalScope::createStructure(*m_vm, nullptr, contextPrototype);
    auto* proxyStructure = JSGlobalProxy::createStructure(*m_vm, nullptr, jsNull());
    auto* proxy = JSGlobalProxy::create(*m_vm, proxyStructure);

    m_globalScopeWrapper.set(*m_vm, JSGlobalScope::create(*m_vm, structure, static_cast<GlobalScope&>(*m_globalScope), proxy));
    contextPrototypeStructure->setGlobalObject(*m_vm, m_globalScopeWrapper.get());
    ASSERT(structure->globalObject() == m_globalScopeWrapper);
    ASSERT(m_globalScopeWrapper->structure()->globalObject() == m_globalScopeWrapper);
    contextPrototype->structure()->setGlobalObject(*m_vm, m_globalScopeWrapper.get());
    auto* globalScopePrototype = JSGlobalScope::prototype(*m_vm, *m_globalScopeWrapper.get());
    globalScopePrototype->didBecomePrototype(*m_vm);
    contextPrototype->structure()->setPrototypeWithoutTransition(*m_vm, globalScopePrototype);

    proxy->setTarget(*m_vm, m_globalScopeWrapper.get());

    ASSERT(m_globalScopeWrapper->globalObject() == m_globalScopeWrapper);
    ASSERT(asObject(m_globalScopeWrapper->getPrototypeDirect())->globalObject() == m_globalScopeWrapper);

    m_consoleClient = makeUnique<WorkerConsoleClient>(*m_globalScope);
    m_globalScopeWrapper->setConsoleClient(*m_consoleClient);
}

void WorkerOrWorkletScriptController::initScript()
{
    ASSERT(m_vm.get());
    JSC::DeferTermination deferTermination(*m_vm.get());

    if (is<DedicatedWorkerGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSDedicatedWorkerGlobalScopePrototype, JSDedicatedWorkerGlobalScope, DedicatedWorkerGlobalScope>();
        return;
    }

    if (is<SharedWorkerGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSSharedWorkerGlobalScopePrototype, JSSharedWorkerGlobalScope, SharedWorkerGlobalScope>();
        return;
    }

    if (is<ServiceWorkerGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSServiceWorkerGlobalScopePrototype, JSServiceWorkerGlobalScope, ServiceWorkerGlobalScope>();
        return;
    }

    if (is<PaintWorkletGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSPaintWorkletGlobalScopePrototype, JSPaintWorkletGlobalScope, PaintWorkletGlobalScope>();
        return;
    }

#if ENABLE(WEB_AUDIO)
    if (is<AudioWorkletGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSAudioWorkletGlobalScopePrototype, JSAudioWorkletGlobalScope, AudioWorkletGlobalScope>();
        return;
    }
#endif

    ASSERT_NOT_REACHED();
}

} // namespace WebCore
