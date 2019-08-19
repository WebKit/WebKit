/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *  Copyright (c) 2015 Canon Inc. All rights reserved.
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
#include "JSDOMWindowBase.h"

#include "ActiveDOMCallbackMicrotask.h"
#include "Chrome.h"
#include "CommonVM.h"
#include "DOMWindow.h"
#include "Document.h"
#include "FetchResponse.h"
#include "Frame.h"
#include "InspectorController.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMGlobalObjectTask.h"
#include "JSDOMWindowCustom.h"
#include "JSFetchResponse.h"
#include "JSMicrotaskCallback.h"
#include "JSNode.h"
#include "Logging.h"
#include "Page.h"
#include "RejectedPromiseTracker.h"
#include "RuntimeApplicationChecks.h"
#include "ScriptController.h"
#include "ScriptModuleLoader.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/JSInternalPromiseDeferred.h>
#include <JavaScriptCore/JSWebAssembly.h>
#include <JavaScriptCore/Microtask.h>
#include <JavaScriptCore/PromiseDeferredTimer.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/Language.h>
#include <wtf/MainThread.h>

#if PLATFORM(IOS_FAMILY)
#include "ChromeClient.h"
#endif


namespace WebCore {
using namespace JSC;

const ClassInfo JSDOMWindowBase::s_info = { "Window", &JSDOMGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDOMWindowBase) };

const GlobalObjectMethodTable JSDOMWindowBase::s_globalObjectMethodTable = {
    &supportsRichSourceInfo,
    &shouldInterruptScript,
    &javaScriptRuntimeFlags,
    &queueTaskToEventLoop,
    &shouldInterruptScriptBeforeTimeout,
    &moduleLoaderImportModule,
    &moduleLoaderResolve,
    &moduleLoaderFetch,
    &moduleLoaderCreateImportMetaProperties,
    &moduleLoaderEvaluate,
    &promiseRejectionTracker,
    &defaultLanguage,
#if ENABLE(WEBASSEMBLY)
    &compileStreaming,
    &instantiateStreaming,
#else
    nullptr,
    nullptr,
#endif
};

JSDOMWindowBase::JSDOMWindowBase(VM& vm, Structure* structure, RefPtr<DOMWindow>&& window, JSWindowProxy* proxy)
    : JSDOMGlobalObject(vm, structure, proxy->world(), &s_globalObjectMethodTable)
    , m_windowCloseWatchpoints((window && window->frame()) ? IsWatched : IsInvalidated)
    , m_wrapped(WTFMove(window))
    , m_proxy(proxy)
{
}

void JSDOMWindowBase::finishCreation(VM& vm, JSWindowProxy* proxy)
{
    Base::finishCreation(vm, proxy);
    ASSERT(inherits(vm, info()));

    auto& builtinNames = static_cast<JSVMClientData*>(vm.clientData)->builtinNames();

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(builtinNames.documentPublicName(), jsNull(), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(builtinNames.windowPublicName(), m_proxy, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
    };

    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));

    if (m_wrapped && m_wrapped->frame() && m_wrapped->frame()->settings().needsSiteSpecificQuirks())
        setNeedsSiteSpecificQuirks(true);
}

void JSDOMWindowBase::destroy(JSCell* cell)
{
    static_cast<JSDOMWindowBase*>(cell)->JSDOMWindowBase::~JSDOMWindowBase();
}

void JSDOMWindowBase::updateDocument()
{
    // Since "document" property is defined as { configurable: false, writable: false, enumerable: true },
    // users cannot change its attributes further.
    // Reaching here, the attributes of "document" property should be never changed.
    ASSERT(m_wrapped->document());
    ExecState* exec = globalExec();
    bool shouldThrowReadOnlyError = false;
    bool ignoreReadOnlyErrors = true;
    bool putResult = false;
    symbolTablePutTouchWatchpointSet(this, exec, static_cast<JSVMClientData*>(exec->vm().clientData)->builtinNames().documentPublicName(), toJS(exec, this, m_wrapped->document()), shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
}

ScriptExecutionContext* JSDOMWindowBase::scriptExecutionContext() const
{
    return m_wrapped->document();
}

void JSDOMWindowBase::printErrorMessage(const String& message) const
{
    printErrorMessageForFrame(wrapped().frame(), message);
}

bool JSDOMWindowBase::supportsRichSourceInfo(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    Frame* frame = thisObject->wrapped().frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    bool enabled = page->inspectorController().enabled();
    ASSERT(enabled || !thisObject->debugger());
    return enabled;
}

static inline bool shouldInterruptScriptToPreventInfiniteRecursionWhenClosingPage(Page* page)
{
    // See <rdar://problem/5479443>. We don't think that page can ever be NULL
    // in this case, but if it is, we've gotten into a state where we may have
    // hung the UI, with no way to ask the client whether to cancel execution.
    // For now, our solution is just to cancel execution no matter what,
    // ensuring that we never hang. We might want to consider other solutions
    // if we discover problems with this one.
    ASSERT(page);
    return !page;
}

bool JSDOMWindowBase::shouldInterruptScript(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    ASSERT(thisObject->wrapped().frame());
    Page* page = thisObject->wrapped().frame()->page();
    return shouldInterruptScriptToPreventInfiniteRecursionWhenClosingPage(page);
}

bool JSDOMWindowBase::shouldInterruptScriptBeforeTimeout(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    ASSERT(thisObject->wrapped().frame());
    Page* page = thisObject->wrapped().frame()->page();

    if (shouldInterruptScriptToPreventInfiniteRecursionWhenClosingPage(page))
        return true;

#if PLATFORM(IOS_FAMILY)
    if (page->chrome().client().isStopping())
        return true;
#endif

    return JSGlobalObject::shouldInterruptScriptBeforeTimeout(object);
}

RuntimeFlags JSDOMWindowBase::javaScriptRuntimeFlags(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    Frame* frame = thisObject->wrapped().frame();
    if (!frame)
        return RuntimeFlags();
    return frame->settings().javaScriptRuntimeFlags();
}

void JSDOMWindowBase::queueTaskToEventLoop(JSGlobalObject& object, Ref<JSC::Microtask>&& task)
{
    JSDOMWindowBase& thisObject = static_cast<JSDOMWindowBase&>(object);

    auto callback = JSMicrotaskCallback::create(thisObject, WTFMove(task));
    auto microtask = makeUnique<ActiveDOMCallbackMicrotask>(MicrotaskQueue::mainThreadQueue(), *thisObject.scriptExecutionContext(), [callback = WTFMove(callback)]() mutable {
        callback->call();
    });

    MicrotaskQueue::mainThreadQueue().append(WTFMove(microtask));
}

void JSDOMWindowBase::willRemoveFromWindowProxy()
{
    setCurrentEvent(0);
}

JSWindowProxy* JSDOMWindowBase::proxy() const
{
    return m_proxy;
}

JSValue toJS(ExecState* state, DOMWindow& domWindow)
{
    auto* frame = domWindow.frame();
    if (!frame)
        return jsNull();
    return toJS(state, frame->windowProxy());
}

JSDOMWindow* toJSDOMWindow(Frame& frame, DOMWrapperWorld& world)
{
    return frame.script().globalObject(world);
}

JSDOMWindow* toJSDOMWindow(JSC::VM& vm, JSValue value)
{
    if (!value.isObject())
        return nullptr;

    while (!value.isNull()) {
        JSObject* object = asObject(value);
        const ClassInfo* classInfo = object->classInfo(vm);
        if (classInfo == JSDOMWindow::info())
            return jsCast<JSDOMWindow*>(object);
        if (classInfo == JSWindowProxy::info())
            return jsDynamicCast<JSDOMWindow*>(vm, jsCast<JSWindowProxy*>(object)->window());
        value = object->getPrototypeDirect(vm);
    }
    return nullptr;
}

DOMWindow& incumbentDOMWindow(ExecState& state)
{
    return asJSDOMWindow(&callerGlobalObject(state))->wrapped();
}

DOMWindow& activeDOMWindow(ExecState& state)
{
    return asJSDOMWindow(state.lexicalGlobalObject())->wrapped();
}

DOMWindow& firstDOMWindow(ExecState& state)
{
    VM& vm = state.vm();
    return asJSDOMWindow(vm.vmEntryGlobalObject(&state))->wrapped();
}

Document* responsibleDocument(ExecState& state)
{
    CallerFunctor functor;
    state.iterate(functor);
    auto* callerFrame = functor.callerFrame();
    if (!callerFrame)
        return nullptr;
    return asJSDOMWindow(callerFrame->lexicalGlobalObject())->wrapped().document();
}

void JSDOMWindowBase::fireFrameClearedWatchpointsForWindow(DOMWindow* window)
{
    JSC::VM& vm = commonVM();
    JSVMClientData* clientData = static_cast<JSVMClientData*>(vm.clientData);
    Vector<Ref<DOMWrapperWorld>> wrapperWorlds;
    clientData->getAllWorlds(wrapperWorlds);
    for (unsigned i = 0; i < wrapperWorlds.size(); ++i) {
        auto& wrappers = wrapperWorlds[i]->wrappers();
        auto result = wrappers.find(window);
        if (result == wrappers.end())
            continue;
        JSC::JSObject* wrapper = result->value.get();
        if (!wrapper)
            continue;
        JSDOMWindowBase* jsWindow = JSC::jsCast<JSDOMWindowBase*>(wrapper);
        jsWindow->m_windowCloseWatchpoints.fireAll(vm, "Frame cleared");
    }
}

JSC::Identifier JSDOMWindowBase::moduleLoaderResolve(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleName, JSC::JSValue importerModuleKey, JSC::JSValue scriptFetcher)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader().resolve(globalObject, exec, moduleLoader, moduleName, importerModuleKey, scriptFetcher);
    return { };
}

JSC::JSInternalPromise* JSDOMWindowBase::moduleLoaderFetch(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue parameters, JSC::JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        RELEASE_AND_RETURN(scope, document->moduleLoader().fetch(globalObject, exec, moduleLoader, moduleKey, parameters, scriptFetcher));
    JSC::JSInternalPromiseDeferred* deferred = JSC::JSInternalPromiseDeferred::tryCreate(exec, globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, deferred->reject(exec, jsUndefined()));
}

JSC::JSValue JSDOMWindowBase::moduleLoaderEvaluate(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue moduleRecord, JSC::JSValue scriptFetcher)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader().evaluate(globalObject, exec, moduleLoader, moduleKey, moduleRecord, scriptFetcher);
    return JSC::jsUndefined();
}

JSC::JSInternalPromise* JSDOMWindowBase::moduleLoaderImportModule(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader* moduleLoader, JSC::JSString* moduleName, JSC::JSValue parameters, const JSC::SourceOrigin& sourceOrigin)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        RELEASE_AND_RETURN(scope, document->moduleLoader().importModule(globalObject, exec, moduleLoader, moduleName, parameters, sourceOrigin));
    JSC::JSInternalPromiseDeferred* deferred = JSC::JSInternalPromiseDeferred::tryCreate(exec, globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    RELEASE_AND_RETURN(scope, deferred->reject(exec, jsUndefined()));
}

JSC::JSObject* JSDOMWindowBase::moduleLoaderCreateImportMetaProperties(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSModuleRecord* moduleRecord, JSC::JSValue scriptFetcher)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader().createImportMetaProperties(globalObject, exec, moduleLoader, moduleKey, moduleRecord, scriptFetcher);
    return constructEmptyObject(exec, globalObject->nullPrototypeObjectStructure());
}

#if ENABLE(WEBASSEMBLY)
static Optional<Vector<uint8_t>> tryAllocate(JSC::ExecState* exec, JSC::JSPromiseDeferred* promise, const char* data, size_t byteSize)
{
    Vector<uint8_t> arrayBuffer;
    if (!arrayBuffer.tryReserveCapacity(byteSize)) {
        promise->reject(exec, createOutOfMemoryError(exec));
        return WTF::nullopt;
    }

    arrayBuffer.grow(byteSize);
    memcpy(arrayBuffer.data(), data, byteSize);

    return arrayBuffer;
}

static bool isResponseCorrect(JSC::ExecState* exec, FetchResponse* inputResponse, JSC::JSPromiseDeferred* promise)
{
    bool isResponseCorsSameOrigin = inputResponse->type() == ResourceResponse::Type::Basic || inputResponse->type() == ResourceResponse::Type::Cors || inputResponse->type() == ResourceResponse::Type::Default;

    if (!isResponseCorsSameOrigin) {
        promise->reject(exec, createTypeError(exec, "Response is not CORS-same-origin"_s));
        return false;
    }

    if (!inputResponse->ok()) {
        promise->reject(exec, createTypeError(exec, "Response has not returned OK status"_s));
        return false;
    }

    auto contentType = inputResponse->headers().fastGet(HTTPHeaderName::ContentType);
    if (!equalLettersIgnoringASCIICase(contentType, "application/wasm")) {
        promise->reject(exec, createTypeError(exec, "Unexpected response MIME type. Expected 'application/wasm'"_s));
        return false;
    }

    return true;
}

static void handleResponseOnStreamingAction(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, FetchResponse* inputResponse, JSC::JSPromiseDeferred* promise, Function<void(JSC::ExecState* exec, const char* data, size_t byteSize)>&& actionCallback)
{
    if (!isResponseCorrect(exec, inputResponse, promise))
        return;

    if (inputResponse->isBodyReceivedByChunk()) {
        inputResponse->consumeBodyReceivedByChunk([promise, callback = WTFMove(actionCallback), globalObject, data = SharedBuffer::create()] (auto&& result) mutable {
            ExecState* exec = globalObject->globalExec();
            if (result.hasException()) {
                promise->reject(exec, createTypeError(exec, result.exception().message()));
                return;
            }

            if (auto chunk = result.returnValue())
                data->append(reinterpret_cast<const char*>(chunk->data), chunk->size);
            else {
                VM& vm = exec->vm();
                JSLockHolder lock(vm);

                callback(exec, data->data(), data->size());
            }
        });
        return;
    }

    auto body = inputResponse->consumeBody();
    WTF::switchOn(body, [&] (Ref<FormData>& formData) {
        if (auto buffer = formData->asSharedBuffer()) {
            VM& vm = exec->vm();
            JSLockHolder lock(vm);

            actionCallback(exec, buffer->data(), buffer->size());
            return;
        }
        // FIXME: http://webkit.org/b/184886> Implement loading for the Blob type
        promise->reject(exec, createTypeError(exec, "Unexpected Response's Content-type"_s));
    }, [&] (Ref<SharedBuffer>& buffer) {
        VM& vm = exec->vm();
        JSLockHolder lock(vm);

        actionCallback(exec, buffer->data(), buffer->size());
    }, [&] (std::nullptr_t&) {
        promise->reject(exec, createTypeError(exec, "Unexpected Response's Content-type"_s));
    });
}

void JSDOMWindowBase::compileStreaming(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSPromiseDeferred* promise, JSC::JSValue source)
{
    ASSERT(source);

    VM& vm = exec->vm();

    ASSERT(vm.promiseDeferredTimer->hasPendingPromise(promise));
    ASSERT(vm.promiseDeferredTimer->hasDependancyInPendingPromise(promise, globalObject));

    if (auto inputResponse = JSFetchResponse::toWrapped(vm, source)) {
        handleResponseOnStreamingAction(globalObject, exec, inputResponse, promise, [promise] (JSC::ExecState* exec, const char* data, size_t byteSize) mutable {
            if (auto arrayBuffer = tryAllocate(exec, promise, data, byteSize))
                JSC::JSWebAssembly::webAssemblyModuleValidateAsync(exec, promise, WTFMove(*arrayBuffer));
        });
    } else
        promise->reject(exec, createTypeError(exec, "first argument must be an Response or Promise for Response"_s));
}

void JSDOMWindowBase::instantiateStreaming(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSPromiseDeferred* promise, JSC::JSValue source, JSC::JSObject* importedObject)
{
    ASSERT(source);

    VM& vm = exec->vm();

    ASSERT(vm.promiseDeferredTimer->hasPendingPromise(promise));
    ASSERT(vm.promiseDeferredTimer->hasDependancyInPendingPromise(promise, globalObject));
    ASSERT(vm.promiseDeferredTimer->hasDependancyInPendingPromise(promise, importedObject));

    if (auto inputResponse = JSFetchResponse::toWrapped(vm, source)) {
        handleResponseOnStreamingAction(globalObject, exec, inputResponse, promise, [promise, importedObject] (JSC::ExecState* exec, const char* data, size_t byteSize) mutable {
            if (auto arrayBuffer = tryAllocate(exec, promise, data, byteSize))
                JSC::JSWebAssembly::webAssemblyModuleInstantinateAsync(exec, promise, WTFMove(*arrayBuffer), importedObject);
        });
    } else
        promise->reject(exec, createTypeError(exec, "first argument must be an Response or Promise for Response"_s));
}
#endif

} // namespace WebCore
