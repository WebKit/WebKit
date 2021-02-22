/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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
#include "JSDOMGlobalObject.h"

#include "DOMWindow.h"
#include "Document.h"
#include "FetchResponse.h"
#include "JSAbortAlgorithm.h"
#include "JSAbortSignal.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMWindow.h"
#include "JSEventListener.h"
#include "JSFetchResponse.h"
#include "JSIDBSerializationGlobalObject.h"
#include "JSMediaStream.h"
#include "JSMediaStreamTrack.h"
#include "JSRTCIceCandidate.h"
#include "JSRTCSessionDescription.h"
#include "JSReadableStream.h"
#include "JSRemoteDOMWindow.h"
#include "JSWorkerGlobalScope.h"
#include "JSWorkletGlobalScope.h"
#include "RejectedPromiseTracker.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptModuleLoader.h"
#include "StructuredClone.h"
#include "WebCoreJSClientData.h"
#include "WorkerGlobalScope.h"
#include "WorkletGlobalScope.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/StructureInlines.h>
#include <JavaScriptCore/WasmStreamingCompiler.h>

namespace WebCore {
using namespace JSC;

JSC_DECLARE_HOST_FUNCTION(makeThisTypeErrorForBuiltins);
JSC_DECLARE_HOST_FUNCTION(makeGetterTypeErrorForBuiltins);
JSC_DECLARE_HOST_FUNCTION(makeDOMExceptionForBuiltins);
JSC_DECLARE_HOST_FUNCTION(isReadableByteStreamAPIEnabled);
JSC_DECLARE_HOST_FUNCTION(isWritableStreamAPIEnabled);
JSC_DECLARE_HOST_FUNCTION(whenSignalAborted);

const ClassInfo JSDOMGlobalObject::s_info = { "DOMGlobalObject", &JSGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDOMGlobalObject) };

JSDOMGlobalObject::JSDOMGlobalObject(VM& vm, Structure* structure, Ref<DOMWrapperWorld>&& world, const GlobalObjectMethodTable* globalObjectMethodTable)
    : JSGlobalObject(vm, structure, globalObjectMethodTable)
    , m_world(WTFMove(world))
    , m_worldIsNormal(m_world->isNormal())
    , m_builtinInternalFunctions(vm)
{
}

JSDOMGlobalObject::~JSDOMGlobalObject() = default;

void JSDOMGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSDOMGlobalObject*>(cell)->JSDOMGlobalObject::~JSDOMGlobalObject();
}

JSC_DEFINE_HOST_FUNCTION(makeThisTypeErrorForBuiltins, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto interfaceName = callFrame->uncheckedArgument(0).getString(globalObject);
    scope.assertNoException();
    auto functionName = callFrame->uncheckedArgument(1).getString(globalObject);
    scope.assertNoException();
    return JSValue::encode(createTypeError(globalObject, makeThisTypeErrorMessage(interfaceName.utf8().data(), functionName.utf8().data())));
}

JSC_DEFINE_HOST_FUNCTION(makeGetterTypeErrorForBuiltins, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto interfaceName = callFrame->uncheckedArgument(0).getString(globalObject);
    scope.assertNoException();
    auto attributeName = callFrame->uncheckedArgument(1).getString(globalObject);
    scope.assertNoException();

    auto error = static_cast<ErrorInstance*>(createTypeError(globalObject, makeGetterTypeErrorMessage(interfaceName.utf8().data(), attributeName.utf8().data())));
    error->setNativeGetterTypeError();
    return JSValue::encode(error);
}

JSC_DEFINE_HOST_FUNCTION(makeDOMExceptionForBuiltins, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);

    auto& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto codeValue = callFrame->uncheckedArgument(0).getString(globalObject);
    scope.assertNoException();

    auto message = callFrame->uncheckedArgument(1).getString(globalObject);
    scope.assertNoException();

    ExceptionCode code { TypeError };
    if (codeValue == "AbortError")
        code = AbortError;
    auto value = createDOMException(globalObject, code, message);

    EXCEPTION_ASSERT(!scope.exception() || isTerminatedExecutionException(vm, scope.exception()));

    return JSValue::encode(value);
}

JSC_DEFINE_HOST_FUNCTION(isReadableByteStreamAPIEnabled, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsBoolean(RuntimeEnabledFeatures::sharedFeatures().readableByteStreamAPIEnabled()));
}

JSC_DEFINE_HOST_FUNCTION(isWritableStreamAPIEnabled, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsBoolean(RuntimeEnabledFeatures::sharedFeatures().writableStreamAPIEnabled()));
}

JSC_DEFINE_HOST_FUNCTION(whenSignalAborted, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame);
    ASSERT(callFrame->argumentCount() == 2);

    auto& vm = globalObject->vm();
    auto* abortSignal = jsDynamicCast<JSAbortSignal*>(vm, callFrame->uncheckedArgument(0));
    if (UNLIKELY(!abortSignal))
        return JSValue::encode(JSValue(JSC::JSValue::JSFalse));

    auto* jsDOMGlobalObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    Ref<AbortAlgorithm> abortAlgorithm = JSAbortAlgorithm::create(callFrame->uncheckedArgument(1).getObject(), jsDOMGlobalObject);

    bool result = AbortSignal::whenSignalAborted(abortSignal->wrapped(), WTFMove(abortAlgorithm));
    return JSValue::encode(result ? JSValue(JSC::JSValue::JSTrue) : JSValue(JSC::JSValue::JSFalse));
}

void JSDOMGlobalObject::addBuiltinGlobals(VM& vm)
{
    m_builtinInternalFunctions.initialize(*this);

    JSVMClientData& clientData = *static_cast<JSVMClientData*>(vm.clientData);
    JSDOMGlobalObject::GlobalPropertyInfo staticGlobals[] = {
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().makeThisTypeErrorPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeThisTypeErrorForBuiltins), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().makeGetterTypeErrorPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeGetterTypeErrorForBuiltins), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().makeDOMExceptionPrivateName(),
            JSFunction::create(vm, this, 2, String(), makeDOMExceptionForBuiltins), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().whenSignalAbortedPrivateName(),
            JSFunction::create(vm, this, 2, String(), whenSignalAborted), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().cloneArrayBufferPrivateName(),
            JSFunction::create(vm, this, 3, String(), cloneArrayBuffer), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().structuredCloneArrayBufferPrivateName(),
            JSFunction::create(vm, this, 1, String(), structuredCloneArrayBuffer), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().structuredCloneArrayBufferViewPrivateName(),
            JSFunction::create(vm, this, 1, String(), structuredCloneArrayBufferView), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(vm.propertyNames->builtinNames().ArrayBufferPrivateName(), arrayBufferConstructor(), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamClosedPrivateName(), jsNumber(1), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamClosingPrivateName(), jsNumber(2), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamErroredPrivateName(), jsNumber(3), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamReadablePrivateName(), jsNumber(4), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamWaitingPrivateName(), jsNumber(5), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().streamWritablePrivateName(), jsNumber(6), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().readableByteStreamAPIEnabledPrivateName(), JSFunction::create(vm, this, 0, String(), isReadableByteStreamAPIEnabled), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        JSDOMGlobalObject::GlobalPropertyInfo(clientData.builtinNames().writableStreamAPIEnabledPrivateName(), JSFunction::create(vm, this, 0, String(), isWritableStreamAPIEnabled), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
    };
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
}

void JSDOMGlobalObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    addBuiltinGlobals(vm);

    RELEASE_ASSERT(classInfo(vm));
}

void JSDOMGlobalObject::finishCreation(VM& vm, JSObject* thisValue)
{
    Base::finishCreation(vm, thisValue);
    ASSERT(inherits(vm, info()));

    addBuiltinGlobals(vm);

    RELEASE_ASSERT(classInfo(vm));
}

ScriptExecutionContext* JSDOMGlobalObject::scriptExecutionContext() const
{
    if (inherits<JSDOMWindowBase>(vm()))
        return jsCast<const JSDOMWindowBase*>(this)->scriptExecutionContext();
    if (inherits<JSRemoteDOMWindowBase>(vm()))
        return nullptr;
    if (inherits<JSWorkerGlobalScopeBase>(vm()))
        return jsCast<const JSWorkerGlobalScopeBase*>(this)->scriptExecutionContext();
    if (inherits<JSWorkletGlobalScopeBase>(vm()))
        return jsCast<const JSWorkletGlobalScopeBase*>(this)->scriptExecutionContext();
#if ENABLE(INDEXED_DATABASE)
    if (inherits<JSIDBSerializationGlobalObject>(vm()))
        return jsCast<const JSIDBSerializationGlobalObject*>(this)->scriptExecutionContext();
#endif
    dataLog("Unexpected global object: ", JSValue(this), "\n");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

template<typename Visitor>
void JSDOMGlobalObject::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSDOMGlobalObject* thisObject = jsCast<JSDOMGlobalObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    {
        auto locker = holdLock(thisObject->m_gcLock);

        for (auto& structure : thisObject->structures(locker).values())
            visitor.append(structure);

        for (auto& constructor : thisObject->constructors(locker).values())
            visitor.append(constructor);

        for (auto& guarded : thisObject->guardedObjects(locker))
            guarded->visitAggregate(visitor);
    }

    thisObject->m_builtinInternalFunctions.visit(visitor);
}

DEFINE_VISIT_CHILDREN(JSDOMGlobalObject);

void JSDOMGlobalObject::promiseRejectionTracker(JSGlobalObject* jsGlobalObject, JSPromise* promise, JSPromiseRejectionOperation operation)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(jsGlobalObject);
    auto* context = globalObject.scriptExecutionContext();
    if (!context)
        return;

    // FIXME: If script has muted errors (cross origin), terminate these steps.
    // <https://webkit.org/b/171415> Implement the `muted-errors` property of Scripts to avoid onerror/onunhandledrejection for cross-origin scripts

    switch (operation) {
    case JSPromiseRejectionOperation::Reject:
        context->ensureRejectedPromiseTracker().promiseRejected(globalObject, *promise);
        break;
    case JSPromiseRejectionOperation::Handle:
        context->ensureRejectedPromiseTracker().promiseHandled(globalObject, *promise);
        break;
    }
}

void JSDOMGlobalObject::reportUncaughtExceptionAtEventLoop(JSGlobalObject* jsGlobalObject, JSC::Exception* exception)
{
    reportException(jsGlobalObject, exception);
}

void JSDOMGlobalObject::clearDOMGuardedObjects()
{
    auto guardedObjectsCopy = m_guardedObjects;
    for (auto& guarded : guardedObjectsCopy)
        guarded->clear();
}

#if ENABLE(WEBASSEMBLY)
// https://webassembly.github.io/spec/web-api/index.html#compile-a-potential-webassembly-response
static JSC::JSPromise* handleResponseOnStreamingAction(JSC::JSGlobalObject* globalObject, JSC::JSValue source, JSC::Wasm::CompilerMode compilerMode, JSC::JSObject* importObject)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);

    auto deferred = DeferredPromise::create(*jsCast<JSDOMGlobalObject*>(globalObject), DeferredPromise::Mode::RetainPromiseOnResolve);

    auto inputResponse = JSFetchResponse::toWrapped(vm, source);
    if (!inputResponse) {
        deferred->reject(TypeError, "first argument must be an Response or Promise for Response"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    if (auto exception = inputResponse->loadingException()) {
        deferred->reject(*exception);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // 4. If response is not CORS-same-origin, reject returnValue with a TypeError and abort these substeps.
    // If response is opaque, content-type becomes "".
    if (!inputResponse->isCORSSameOrigin()) {
        deferred->reject(TypeError, "Response is not CORS-same-origin"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // 3. If mimeType is not `application/wasm`, reject returnValue with a TypeError and abort these substeps.
    if (!inputResponse->hasWasmMIMEType()) {
        deferred->reject(TypeError, "Unexpected response MIME type. Expected 'application/wasm'"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // 5. If response’s status is not an ok status, reject returnValue with a TypeError and abort these substeps.
    if (!inputResponse->ok()) {
        deferred->reject(TypeError, "Response has not returned OK status"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // https://fetch.spec.whatwg.org/#concept-body-consume-body
    if (inputResponse->isDisturbedOrLocked()) {
        deferred->reject(TypeError, "Response is disturbed or locked"_s);
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    // FIXME: for efficiency, we should load blobs directly instead of going through the readableStream path.
    if (inputResponse->isBlobBody()) {
        auto streamOrException = inputResponse->readableStream(*globalObject);
        if (UNLIKELY(streamOrException.hasException())) {
            deferred->reject(streamOrException.releaseException());
            return jsCast<JSC::JSPromise*>(deferred->promise());
        }
    }

    auto compiler = JSC::Wasm::StreamingCompiler::create(vm, compilerMode, globalObject, jsCast<JSC::JSPromise*>(deferred->promise()), importObject);

    if (inputResponse->isBodyReceivedByChunk()) {
        inputResponse->consumeBodyReceivedByChunk([globalObject, compiler = WTFMove(compiler)](auto&& result) mutable {
            VM& vm = globalObject->vm();
            JSLockHolder lock(vm);

            if (result.hasException()) {
                auto exception = result.exception();
                if (exception.code() == ExistingExceptionError) {
                    auto scope = DECLARE_CATCH_SCOPE(vm);

                    EXCEPTION_ASSERT(scope.exception());

                    auto error = scope.exception()->value();
                    scope.clearException();

                    compiler->fail(globalObject, error);
                    return;
                }

                auto scope = DECLARE_THROW_SCOPE(vm);
                auto error = createDOMException(*globalObject, WTFMove(exception));
                if (UNLIKELY(scope.exception())) {
                    ASSERT(isTerminatedExecutionException(vm, scope.exception()));
                    compiler->cancel();
                    return;
                }

                compiler->fail(globalObject, error);
                return;
            }

            if (auto chunk = result.returnValue())
                compiler->addBytes(chunk->data, chunk->size);
            else
                compiler->finalize(globalObject);
        });
        return jsCast<JSC::JSPromise*>(deferred->promise());
    }

    auto body = inputResponse->consumeBody();
    WTF::switchOn(body, [&](Ref<FormData>& formData) {
        if (auto buffer = formData->asSharedBuffer()) {
            compiler->addBytes(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
            compiler->finalize(globalObject);
            return;
        }
        // FIXME: Support FormData loading.
        // https://bugs.webkit.org/show_bug.cgi?id=221248
        compiler->fail(globalObject, createDOMException(*globalObject, Exception { NotSupportedError, "Not implemented"_s  }));
    }, [&](Ref<SharedBuffer>& buffer) {
        compiler->addBytes(reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size());
        compiler->finalize(globalObject);
    }, [&](std::nullptr_t&) {
        compiler->finalize(globalObject);
    });

    return jsCast<JSC::JSPromise*>(deferred->promise());
}

JSC::JSPromise* JSDOMGlobalObject::compileStreaming(JSC::JSGlobalObject* globalObject, JSC::JSValue source)
{
    ASSERT(source);
    return handleResponseOnStreamingAction(globalObject, source, JSC::Wasm::CompilerMode::Validation, nullptr);
}

JSC::JSPromise* JSDOMGlobalObject::instantiateStreaming(JSC::JSGlobalObject* globalObject, JSC::JSValue source, JSC::JSObject* importObject)
{
    ASSERT(source);
    return handleResponseOnStreamingAction(globalObject, source, JSC::Wasm::CompilerMode::FullCompile, importObject);
}
#endif

static ScriptModuleLoader* scriptModuleLoader(JSDOMGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    if (globalObject->inherits<JSDOMWindowBase>(vm)) {
        if (auto document = jsCast<const JSDOMWindowBase*>(globalObject)->wrapped().document())
            return &document->moduleLoader();
        return nullptr;
    }
    if (globalObject->inherits<JSRemoteDOMWindowBase>(vm))
        return nullptr;
    if (globalObject->inherits<JSWorkerGlobalScopeBase>(vm))
        return &jsCast<const JSWorkerGlobalScopeBase*>(globalObject)->wrapped().moduleLoader();
    if (globalObject->inherits<JSWorkletGlobalScopeBase>(vm))
        return &jsCast<const JSWorkletGlobalScopeBase*>(globalObject)->wrapped().moduleLoader();
#if ENABLE(INDEXED_DATABASE)
    if (globalObject->inherits<JSIDBSerializationGlobalObject>(vm))
        return nullptr;
#endif
    dataLog("Unexpected global object: ", JSValue(globalObject), "\n");
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

JSC::Identifier JSDOMGlobalObject::moduleLoaderResolve(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleName, JSC::JSValue importerModuleKey, JSC::JSValue scriptFetcher)
{
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        return loader->resolve(globalObject, moduleLoader, moduleName, importerModuleKey, scriptFetcher);
    return { };
}

JSC::JSInternalPromise* JSDOMGlobalObject::moduleLoaderFetch(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue parameters, JSC::JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        RELEASE_AND_RETURN(scope, loader->fetch(globalObject, moduleLoader, moduleKey, parameters, scriptFetcher));
    JSC::JSInternalPromise* promise = JSC::JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    scope.release();
    promise->reject(globalObject, jsUndefined());
    return promise;
}

JSC::JSValue JSDOMGlobalObject::moduleLoaderEvaluate(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue moduleRecord, JSC::JSValue scriptFetcher, JSC::JSValue awaitedValue, JSC::JSValue resumeMode)
{
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        return loader->evaluate(globalObject, moduleLoader, moduleKey, moduleRecord, scriptFetcher, awaitedValue, resumeMode);
    return JSC::jsUndefined();
}

JSC::JSInternalPromise* JSDOMGlobalObject::moduleLoaderImportModule(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSString* moduleName, JSC::JSValue parameters, const JSC::SourceOrigin& sourceOrigin)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        RELEASE_AND_RETURN(scope, loader->importModule(globalObject, moduleLoader, moduleName, parameters, sourceOrigin));
    JSC::JSInternalPromise* promise = JSC::JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    scope.release();
    promise->reject(globalObject, jsUndefined());
    return promise;
}

JSC::JSObject* JSDOMGlobalObject::moduleLoaderCreateImportMetaProperties(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSModuleRecord* moduleRecord, JSC::JSValue scriptFetcher)
{
    JSDOMGlobalObject* thisObject = JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    if (auto* loader = scriptModuleLoader(thisObject))
        return loader->createImportMetaProperties(globalObject, moduleLoader, moduleKey, moduleRecord, scriptFetcher);
    return constructEmptyObject(globalObject->vm(), globalObject->nullPrototypeObjectStructure());
}

JSDOMGlobalObject& callerGlobalObject(JSGlobalObject& lexicalGlobalObject, CallFrame& callFrame)
{
    class GetCallerGlobalObjectFunctor {
    public:
        GetCallerGlobalObjectFunctor() = default;

        StackVisitor::Status operator()(StackVisitor& visitor) const
        {
            if (!m_hasSkippedFirstFrame) {
                m_hasSkippedFirstFrame = true;
                return StackVisitor::Continue;
            }

            if (auto* codeBlock = visitor->codeBlock())
                m_globalObject = codeBlock->globalObject();
            else {
                ASSERT(visitor->callee().rawPtr());
                // FIXME: Callee is not an object if the caller is Web Assembly.
                // Figure out what to do here. We can probably get the global object
                // from the top-most Wasm Instance. https://bugs.webkit.org/show_bug.cgi?id=165721
                if (visitor->callee().isCell() && visitor->callee().asCell()->isObject())
                    m_globalObject = jsCast<JSObject*>(visitor->callee().asCell())->globalObject();
            }
            return StackVisitor::Done;
        }

        JSGlobalObject* globalObject() const { return m_globalObject; }

    private:
        mutable bool m_hasSkippedFirstFrame { false };
        mutable JSGlobalObject* m_globalObject { nullptr };
    };

    GetCallerGlobalObjectFunctor iter;
    callFrame.iterate(lexicalGlobalObject.vm(), iter);
    if (iter.globalObject())
        return *jsCast<JSDOMGlobalObject*>(iter.globalObject());

    // If we cannot find JSGlobalObject in caller frames, we just return the current lexicalGlobalObject.
    return *jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject);
}

JSDOMGlobalObject* toJSDOMGlobalObject(ScriptExecutionContext& context, DOMWrapperWorld& world)
{
    if (is<Document>(context))
        return toJSDOMWindow(downcast<Document>(context).frame(), world);

    if (is<WorkerOrWorkletGlobalScope>(context))
        return downcast<WorkerOrWorkletGlobalScope>(context).script()->globalScopeWrapper();

    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebCore
