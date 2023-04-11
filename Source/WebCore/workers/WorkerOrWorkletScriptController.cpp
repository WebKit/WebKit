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
#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/DeferTermination.h>
#include <JavaScriptCore/DeferredWorkTimer.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/GCActivityCallback.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSNativeStdFunction.h>
#include <JavaScriptCore/JSScriptFetchParameters.h>
#include <JavaScriptCore/JSScriptFetcher.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/VMTrapsInlines.h>

namespace WebCore {

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

void WorkerOrWorkletScriptController::evaluate(const ScriptSourceCode& sourceCode, String* returnedExceptionMessage)
{
    if (isExecutionForbidden())
        return;

    VM& vm = this->vm();
    NakedPtr<JSC::Exception> uncaughtException;
    evaluate(sourceCode, uncaughtException, returnedExceptionMessage);
    if ((uncaughtException && vm.isTerminationException(uncaughtException)) || isTerminatingExecution()) {
        forbidExecution();
        return;
    }
    if (uncaughtException) {
        JSLockHolder lock(vm);
        reportException(m_globalScopeWrapper.get(), uncaughtException);
    }
}

void WorkerOrWorkletScriptController::evaluate(const ScriptSourceCode& sourceCode, NakedPtr<JSC::Exception>& returnedException, String* returnedExceptionMessage)
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

    if (returnedException) {
        if (m_globalScope->canIncludeErrorDetails(sourceCode.cachedScript(), sourceCode.url().string())) {
            // FIXME: It's not great that this can run arbitrary code to string-ify the value of the exception.
            // Do we need to do anything to handle that properly, if it, say, raises another exception?
            if (returnedExceptionMessage)
                *returnedExceptionMessage = returnedException->value().toWTFString(&globalObject);
        } else {
            // Overwrite the detailed error with a generic error.
            String genericErrorMessage { "Script error."_s };
            if (returnedExceptionMessage)
                *returnedExceptionMessage = genericErrorMessage;
            returnedException = JSC::Exception::create(vm, createError(&globalObject, genericErrorMessage));
        }
    }
}

static Identifier jsValueToModuleKey(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    if (value.isSymbol())
        return Identifier::fromUid(jsCast<Symbol*>(value)->privateName());
    ASSERT(value.isString());
    return asString(value)->toIdentifier(lexicalGlobalObject);
}

JSC::JSValue WorkerOrWorkletScriptController::evaluateModule(JSC::AbstractModuleRecord& moduleRecord, JSC::JSValue awaitedValue, JSC::JSValue resumeMode)
{
    auto& globalObject = *m_globalScopeWrapper.get();
    VM& vm = globalObject.vm();
    JSLockHolder lock { vm };
    return moduleRecord.evaluate(&globalObject, awaitedValue, resumeMode);
}

bool WorkerOrWorkletScriptController::loadModuleSynchronously(WorkerScriptFetcher& scriptFetcher, const ScriptSourceCode& sourceCode)
{
    if (isExecutionForbidden())
        return false;

    initScriptIfNeeded();

    auto& globalObject = *m_globalScopeWrapper.get();
    VM& vm = globalObject.vm();
    JSLockHolder lock { vm };
    auto scope = DECLARE_THROW_SCOPE(vm);

    Ref protector { scriptFetcher };
    {
        auto& promise = JSExecState::loadModule(globalObject, sourceCode.jsSourceCode(), JSC::JSScriptFetcher::create(vm, { &scriptFetcher }));
        scope.assertNoExceptionExceptTermination();
        RETURN_IF_EXCEPTION(scope, false);

        auto& fulfillHandler = *JSNativeStdFunction::create(vm, &globalObject, 1, String(), [protector](JSGlobalObject* globalObject, CallFrame* callFrame) -> JSC::EncodedJSValue {
            VM& vm = globalObject->vm();
            JSLockHolder lock { vm };
            auto scope = DECLARE_THROW_SCOPE(vm);
            Identifier moduleKey = jsValueToModuleKey(globalObject, callFrame->argument(0));
            RETURN_IF_EXCEPTION(scope, { });
            protector->notifyLoadCompleted(*moduleKey.impl());
            return JSValue::encode(jsUndefined());
        });

        auto& rejectHandler = *JSNativeStdFunction::create(vm, &globalObject, 1, String(), [protector](JSGlobalObject* globalObject, CallFrame* callFrame) {
            VM& vm = globalObject->vm();
            JSLockHolder lock { vm };
            JSValue errorValue = callFrame->argument(0);
            auto scope = DECLARE_CATCH_SCOPE(vm);
            if (errorValue.isObject()) {
                auto* object = JSC::asObject(errorValue);
                if (JSValue failureKindValue = object->getDirect(vm, builtinNames(vm).failureKindPrivateName())) {
                    // This is host propagated error in the module loader pipeline.
                    switch (static_cast<ModuleFetchFailureKind>(failureKindValue.asInt32())) {
                    case ModuleFetchFailureKind::WasPropagatedError:
                        protector->notifyLoadFailed(LoadableScript::Error {
                            LoadableScript::ErrorType::Fetch,
                            { },
                            { }
                        });
                        break;
                    // For a fetch error that was not propagated from further in the
                    // pipeline, we include the console error message but do not
                    // include an error value as it should not be reported.
                    case ModuleFetchFailureKind::WasFetchError:
                        protector->notifyLoadFailed(LoadableScript::Error {
                            LoadableScript::ErrorType::Fetch,
                            LoadableScript::ConsoleMessage {
                                MessageSource::JS,
                                MessageLevel::Error,
                                retrieveErrorMessage(*globalObject, vm, errorValue, scope),
                            },
                            { }
                        });
                        break;
                    case ModuleFetchFailureKind::WasResolveError:
                        protector->notifyLoadFailed(LoadableScript::Error {
                            LoadableScript::ErrorType::Resolve,
                            LoadableScript::ConsoleMessage {
                                MessageSource::JS,
                                MessageLevel::Error,
                                retrieveErrorMessage(*globalObject, vm, errorValue, scope),
                            },
                            // The error value may need to be propagated here as it is in
                            // ScriptController in the future.
                            { }
                        });
                        break;
                    case ModuleFetchFailureKind::WasCanceled:
                        protector->notifyLoadWasCanceled();
                        break;
                    }
                    return JSValue::encode(jsUndefined());
                }
            }

            protector->notifyLoadFailed(LoadableScript::Error {
                LoadableScript::ErrorType::Script,
                LoadableScript::ConsoleMessage {
                    MessageSource::JS,
                    MessageLevel::Error,
                    retrieveErrorMessage(*globalObject, vm, errorValue, scope),
                },
                // The error value may need to be propagated here as it is in
                // ScriptController in the future.
                { }
            });
            return JSValue::encode(jsUndefined());
        });

        promise.then(&globalObject, &fulfillHandler, &rejectHandler);
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
    while ((!protector->isLoaded() && !protector->wasCanceled()) && success) {
        success = runLoop.runInMode(m_globalScope, taskMode, allowEventLoopTasks);
        if (success)
            m_globalScope->eventLoop().performMicrotaskCheckpoint();
    }

    return success;
}

void WorkerOrWorkletScriptController::linkAndEvaluateModule(WorkerScriptFetcher& scriptFetcher, const ScriptSourceCode& sourceCode, String* returnedExceptionMessage)
{
    if (isExecutionForbidden())
        return;

    initScriptIfNeeded();

    auto& globalObject = *m_globalScopeWrapper.get();
    VM& vm = globalObject.vm();
    JSLockHolder lock { vm };

    NakedPtr<JSC::Exception> returnedException;
    JSExecState::linkAndEvaluateModule(globalObject, Identifier::fromUid(vm, scriptFetcher.moduleKey()), jsUndefined(), returnedException);
    if ((returnedException && vm.isTerminationException(returnedException)) || isTerminatingExecution()) {
        forbidExecution();
        return;
    }

    if (returnedException) {
        if (m_globalScope->canIncludeErrorDetails(sourceCode.cachedScript(), sourceCode.url().string())) {
            // FIXME: It's not great that this can run arbitrary code to string-ify the value of the exception.
            // Do we need to do anything to handle that properly, if it, say, raises another exception?
            if (returnedExceptionMessage)
                *returnedExceptionMessage = returnedException->value().toWTFString(&globalObject);
        } else {
            String genericErrorMessage { "Script error."_s };
            if (returnedExceptionMessage)
                *returnedExceptionMessage = genericErrorMessage;
        }

        JSLockHolder lock(vm);
        reportException(m_globalScopeWrapper.get(), returnedException);
    }
}

void WorkerOrWorkletScriptController::loadAndEvaluateModule(const URL& moduleURL, FetchOptions::Credentials credentials, CompletionHandler<void(std::optional<Exception>&&)>&& completionHandler)
{
    if (isExecutionForbidden()) {
        completionHandler(Exception { NotAllowedError });
        return;
    }

    initScriptIfNeeded();

    auto& globalObject = *m_globalScopeWrapper.get();
    VM& vm = globalObject.vm();
    JSLockHolder lock { vm };

    auto parameters = ModuleFetchParameters::create(JSC::ScriptFetchParameters::Type::JavaScript, emptyString(), /* isTopLevelModule */ true);
    auto scriptFetcher = WorkerScriptFetcher::create(WTFMove(parameters), credentials, globalScope()->destination(), globalScope()->referrerPolicy());
    {
        auto& promise = JSExecState::loadModule(globalObject, moduleURL, JSC::JSScriptFetchParameters::create(vm, scriptFetcher->parameters()), JSC::JSScriptFetcher::create(vm, { scriptFetcher.ptr() }));

        auto task = createSharedTask<void(std::optional<Exception>&&)>([completionHandler = WTFMove(completionHandler)](std::optional<Exception>&& exception) mutable {
            completionHandler(WTFMove(exception));
        });

        auto& fulfillHandler = *JSNativeStdFunction::create(vm, &globalObject, 1, String(), [task, scriptFetcher](JSGlobalObject* globalObject, CallFrame* callFrame) -> JSC::EncodedJSValue {
            // task->run(std::nullopt);
            VM& vm = globalObject->vm();
            JSLockHolder lock { vm };
            auto scope = DECLARE_THROW_SCOPE(vm);

            Identifier moduleKey = jsValueToModuleKey(globalObject, callFrame->argument(0));
            RETURN_IF_EXCEPTION(scope, { });
            scriptFetcher->notifyLoadCompleted(*moduleKey.impl());

            auto* context = downcast<WorkerOrWorkletGlobalScope>(jsCast<JSDOMGlobalObject*>(globalObject)->scriptExecutionContext());
            if (!context || !context->script()) {
                task->run(std::nullopt);
                return JSValue::encode(jsUndefined());
            }

            NakedPtr<JSC::Exception> returnedException;
            JSExecState::linkAndEvaluateModule(*globalObject, moduleKey, jsUndefined(), returnedException);
            if ((returnedException && vm.isTerminationException(returnedException)) || context->script()->isTerminatingExecution()) {
                if (context->script())
                    context->script()->forbidExecution();
                task->run(std::nullopt);
                return JSValue::encode(jsUndefined());
            }

            if (returnedException) {
                String message;
                if (context->canIncludeErrorDetails(nullptr, moduleKey.string())) {
                    // FIXME: It's not great that this can run arbitrary code to string-ify the value of the exception.
                    // Do we need to do anything to handle that properly, if it, say, raises another exception?
                    message = returnedException->value().toWTFString(globalObject);
                } else
                    message = "Script error."_s;
                context->reportException(message, { }, { }, { }, { }, { });
            }

            task->run(std::nullopt);
            return JSValue::encode(jsUndefined());
        });

        auto& rejectHandler = *JSNativeStdFunction::create(vm, &globalObject, 1, String(), [task](JSGlobalObject* globalObject, CallFrame* callFrame) {
            VM& vm = globalObject->vm();
            JSLockHolder lock { vm };
            JSValue errorValue = callFrame->argument(0);
            if (errorValue.isObject()) {
                auto* object = JSC::asObject(errorValue);
                if (JSValue failureKindValue = object->getDirect(vm, builtinNames(vm).failureKindPrivateName())) {
                    auto catchScope = DECLARE_CATCH_SCOPE(vm);
                    String message = retrieveErrorMessageWithoutName(*globalObject, vm, object, catchScope);
                    switch (static_cast<ModuleFetchFailureKind>(failureKindValue.asInt32())) {
                    case ModuleFetchFailureKind::WasFetchError:
                    case ModuleFetchFailureKind::WasResolveError:
                        task->run(Exception { TypeError, message });
                        break;
                    case ModuleFetchFailureKind::WasPropagatedError:
                    case ModuleFetchFailureKind::WasCanceled:
                        task->run(Exception { AbortError, message });
                        break;
                    }
                    return JSValue::encode(jsUndefined());
                }
                if (object->inherits<ErrorInstance>()) {
                    auto* error = jsCast<ErrorInstance*>(object);
                    switch (error->errorType()) {
                    case ErrorType::TypeError: {
                        auto catchScope = DECLARE_CATCH_SCOPE(vm);
                        String message = retrieveErrorMessageWithoutName(*globalObject, vm, error, catchScope);
                        task->run(Exception { TypeError, message });
                        return JSValue::encode(jsUndefined());
                    }
                    case ErrorType::SyntaxError: {
                        auto catchScope = DECLARE_CATCH_SCOPE(vm);
                        String message = retrieveErrorMessageWithoutName(*globalObject, vm, error, catchScope);
                        task->run(Exception { JSSyntaxError, message });
                        return JSValue::encode(jsUndefined());
                    }
                    default:
                        break;
                    }
                }
            }

            auto catchScope = DECLARE_CATCH_SCOPE(vm);
            String message = retrieveErrorMessageWithoutName(*globalObject, vm, errorValue, catchScope);
            task->run(Exception { AbortError, message });
            return JSValue::encode(jsUndefined());
        });

        promise.then(&globalObject, &fulfillHandler, &rejectHandler);
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
    auto* proxyStructure = JSProxy::createStructure(*m_vm, nullptr, jsNull());
    auto* proxy = JSProxy::create(*m_vm, proxyStructure);

    m_globalScopeWrapper.set(*m_vm, JSGlobalScope::create(*m_vm, structure, static_cast<GlobalScope&>(*m_globalScope), proxy));
    contextPrototypeStructure->setGlobalObject(*m_vm, m_globalScopeWrapper.get());
    ASSERT(structure->globalObject() == m_globalScopeWrapper);
    ASSERT(m_globalScopeWrapper->structure()->globalObject() == m_globalScopeWrapper);
    contextPrototype->structure()->setGlobalObject(*m_vm, m_globalScopeWrapper.get());
    auto* globalScopePrototype = JSGlobalScope::prototype(*m_vm, *m_globalScopeWrapper.get());
    globalScopePrototype->didBecomePrototype();
    contextPrototype->structure()->setPrototypeWithoutTransition(*m_vm, globalScopePrototype);

    proxy->setTarget(*m_vm, m_globalScopeWrapper.get());
    proxy->structure()->setGlobalObject(*m_vm, m_globalScopeWrapper.get());

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

#if ENABLE(SERVICE_WORKER)
    if (is<ServiceWorkerGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSServiceWorkerGlobalScopePrototype, JSServiceWorkerGlobalScope, ServiceWorkerGlobalScope>();
        return;
    }
#endif

#if ENABLE(CSS_PAINTING_API)
    if (is<PaintWorkletGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSPaintWorkletGlobalScopePrototype, JSPaintWorkletGlobalScope, PaintWorkletGlobalScope>();
        return;
    }
#endif

#if ENABLE(WEB_AUDIO)
    if (is<AudioWorkletGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSAudioWorkletGlobalScopePrototype, JSAudioWorkletGlobalScope, AudioWorkletGlobalScope>();
        return;
    }
#endif

    ASSERT_NOT_REACHED();
}

} // namespace WebCore
