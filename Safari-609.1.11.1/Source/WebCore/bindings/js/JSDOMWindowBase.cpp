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

#include "Chrome.h"
#include "CommonVM.h"
#include "DOMWindow.h"
#include "Document.h"
#include "EventLoop.h"
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
#include <JavaScriptCore/JSWebAssembly.h>
#include <JavaScriptCore/Microtask.h>
#include <JavaScriptCore/PromiseTimer.h>
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
    &queueMicrotaskToEventLoop,
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
    JSGlobalObject* lexicalGlobalObject = this;
    bool shouldThrowReadOnlyError = false;
    bool ignoreReadOnlyErrors = true;
    bool putResult = false;
    symbolTablePutTouchWatchpointSet(this, lexicalGlobalObject, static_cast<JSVMClientData*>(lexicalGlobalObject->vm().clientData)->builtinNames().documentPublicName(), toJS(lexicalGlobalObject, this, m_wrapped->document()), shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
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
    // in this case, but if it is, we've gotten into a lexicalGlobalObject where we may have
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

void JSDOMWindowBase::queueMicrotaskToEventLoop(JSGlobalObject& object, Ref<JSC::Microtask>&& task)
{
    JSDOMWindowBase& thisObject = static_cast<JSDOMWindowBase&>(object);

    auto callback = JSMicrotaskCallback::create(thisObject, WTFMove(task));
    auto& eventLoop = thisObject.scriptExecutionContext()->eventLoop();
    eventLoop.queueMicrotask([callback = WTFMove(callback)]() mutable {
        callback->call();
    });
}

void JSDOMWindowBase::willRemoveFromWindowProxy()
{
    setCurrentEvent(0);
}

JSWindowProxy* JSDOMWindowBase::proxy() const
{
    return m_proxy;
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, DOMWindow& domWindow)
{
    auto* frame = domWindow.frame();
    if (!frame)
        return jsNull();
    return toJS(lexicalGlobalObject, frame->windowProxy());
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

DOMWindow& incumbentDOMWindow(JSGlobalObject& lexicalGlobalObject, CallFrame& callFrame)
{
    return asJSDOMWindow(&callerGlobalObject(lexicalGlobalObject, callFrame))->wrapped();
}

DOMWindow& activeDOMWindow(JSGlobalObject& lexicalGlobalObject)
{
    return asJSDOMWindow(&lexicalGlobalObject)->wrapped();
}

DOMWindow& firstDOMWindow(JSGlobalObject& lexicalGlobalObject)
{
    VM& vm = lexicalGlobalObject.vm();
    return asJSDOMWindow(vm.deprecatedVMEntryGlobalObject(&lexicalGlobalObject))->wrapped();
}

Document* responsibleDocument(VM& vm, CallFrame& callFrame)
{
    CallerFunctor functor;
    callFrame.iterate(vm, functor);
    auto* callerFrame = functor.callerFrame();
    if (!callerFrame)
        return nullptr;
    return asJSDOMWindow(callerFrame->lexicalGlobalObject(vm))->wrapped().document();
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

JSC::Identifier JSDOMWindowBase::moduleLoaderResolve(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleName, JSC::JSValue importerModuleKey, JSC::JSValue scriptFetcher)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader().resolve(globalObject, moduleLoader, moduleName, importerModuleKey, scriptFetcher);
    return { };
}

JSC::JSInternalPromise* JSDOMWindowBase::moduleLoaderFetch(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue parameters, JSC::JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        RELEASE_AND_RETURN(scope, document->moduleLoader().fetch(globalObject, moduleLoader, moduleKey, parameters, scriptFetcher));
    JSC::JSInternalPromise* promise = JSC::JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    scope.release();
    promise->reject(globalObject, jsUndefined());
    return promise;
}

JSC::JSValue JSDOMWindowBase::moduleLoaderEvaluate(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue moduleRecord, JSC::JSValue scriptFetcher)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader().evaluate(globalObject, moduleLoader, moduleKey, moduleRecord, scriptFetcher);
    return JSC::jsUndefined();
}

JSC::JSInternalPromise* JSDOMWindowBase::moduleLoaderImportModule(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSString* moduleName, JSC::JSValue parameters, const JSC::SourceOrigin& sourceOrigin)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        RELEASE_AND_RETURN(scope, document->moduleLoader().importModule(globalObject, moduleLoader, moduleName, parameters, sourceOrigin));
    JSC::JSInternalPromise* promise = JSC::JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    scope.release();
    promise->reject(globalObject, jsUndefined());
    return promise;
}

JSC::JSObject* JSDOMWindowBase::moduleLoaderCreateImportMetaProperties(JSC::JSGlobalObject* globalObject, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSModuleRecord* moduleRecord, JSC::JSValue scriptFetcher)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader().createImportMetaProperties(globalObject, moduleLoader, moduleKey, moduleRecord, scriptFetcher);
    return constructEmptyObject(globalObject->vm(), globalObject->nullPrototypeObjectStructure());
}

#if ENABLE(WEBASSEMBLY)
static Optional<Vector<uint8_t>> tryAllocate(JSC::JSGlobalObject* lexicalGlobalObject, JSC::JSPromise* promise, const char* data, size_t byteSize)
{
    Vector<uint8_t> arrayBuffer;
    if (!arrayBuffer.tryReserveCapacity(byteSize)) {
        promise->reject(lexicalGlobalObject, createOutOfMemoryError(lexicalGlobalObject));
        return WTF::nullopt;
    }

    arrayBuffer.grow(byteSize);
    memcpy(arrayBuffer.data(), data, byteSize);

    return arrayBuffer;
}

static bool isResponseCorrect(JSC::JSGlobalObject* lexicalGlobalObject, FetchResponse* inputResponse, JSC::JSPromise* promise)
{
    bool isResponseCorsSameOrigin = inputResponse->type() == ResourceResponse::Type::Basic || inputResponse->type() == ResourceResponse::Type::Cors || inputResponse->type() == ResourceResponse::Type::Default;

    if (!isResponseCorsSameOrigin) {
        promise->reject(lexicalGlobalObject, createTypeError(lexicalGlobalObject, "Response is not CORS-same-origin"_s));
        return false;
    }

    if (!inputResponse->ok()) {
        promise->reject(lexicalGlobalObject, createTypeError(lexicalGlobalObject, "Response has not returned OK status"_s));
        return false;
    }

    auto contentType = inputResponse->headers().fastGet(HTTPHeaderName::ContentType);
    if (!equalLettersIgnoringASCIICase(contentType, "application/wasm")) {
        promise->reject(lexicalGlobalObject, createTypeError(lexicalGlobalObject, "Unexpected response MIME type. Expected 'application/wasm'"_s));
        return false;
    }

    return true;
}

static void handleResponseOnStreamingAction(JSC::JSGlobalObject* globalObject, FetchResponse* inputResponse, JSC::JSPromise* promise, Function<void(JSC::JSGlobalObject* lexicalGlobalObject, const char* data, size_t byteSize)>&& actionCallback)
{
    if (!isResponseCorrect(globalObject, inputResponse, promise))
        return;

    if (inputResponse->isBodyReceivedByChunk()) {
        inputResponse->consumeBodyReceivedByChunk([promise, callback = WTFMove(actionCallback), globalObject, data = SharedBuffer::create()] (auto&& result) mutable {
            if (result.hasException()) {
                promise->reject(globalObject, createTypeError(globalObject, result.exception().message()));
                return;
            }

            if (auto chunk = result.returnValue())
                data->append(reinterpret_cast<const char*>(chunk->data), chunk->size);
            else {
                VM& vm = globalObject->vm();
                JSLockHolder lock(vm);

                callback(globalObject, data->data(), data->size());
            }
        });
        return;
    }

    auto body = inputResponse->consumeBody();
    WTF::switchOn(body, [&] (Ref<FormData>& formData) {
        if (auto buffer = formData->asSharedBuffer()) {
            VM& vm = globalObject->vm();
            JSLockHolder lock(vm);

            actionCallback(globalObject, buffer->data(), buffer->size());
            return;
        }
        // FIXME: http://webkit.org/b/184886> Implement loading for the Blob type
        promise->reject(globalObject, createTypeError(globalObject, "Unexpected Response's Content-type"_s));
    }, [&] (Ref<SharedBuffer>& buffer) {
        VM& vm = globalObject->vm();
        JSLockHolder lock(vm);

        actionCallback(globalObject, buffer->data(), buffer->size());
    }, [&] (std::nullptr_t&) {
        promise->reject(globalObject, createTypeError(globalObject, "Unexpected Response's Content-type"_s));
    });
}

void JSDOMWindowBase::compileStreaming(JSC::JSGlobalObject* globalObject, JSC::JSPromise* promise, JSC::JSValue source)
{
    ASSERT(source);

    VM& vm = globalObject->vm();

    ASSERT(vm.promiseTimer->hasPendingPromise(promise));
    ASSERT(vm.promiseTimer->hasDependancyInPendingPromise(promise, globalObject));

    if (auto inputResponse = JSFetchResponse::toWrapped(vm, source)) {
        handleResponseOnStreamingAction(globalObject, inputResponse, promise, [promise] (JSC::JSGlobalObject* lexicalGlobalObject, const char* data, size_t byteSize) mutable {
            if (auto arrayBuffer = tryAllocate(lexicalGlobalObject, promise, data, byteSize))
                JSC::JSWebAssembly::webAssemblyModuleValidateAsync(lexicalGlobalObject, promise, WTFMove(*arrayBuffer));
        });
    } else
        promise->reject(globalObject, createTypeError(globalObject, "first argument must be an Response or Promise for Response"_s));
}

void JSDOMWindowBase::instantiateStreaming(JSC::JSGlobalObject* globalObject, JSC::JSPromise* promise, JSC::JSValue source, JSC::JSObject* importedObject)
{
    ASSERT(source);

    VM& vm = globalObject->vm();

    ASSERT(vm.promiseTimer->hasPendingPromise(promise));
    ASSERT(vm.promiseTimer->hasDependancyInPendingPromise(promise, globalObject));
    ASSERT(vm.promiseTimer->hasDependancyInPendingPromise(promise, importedObject));

    if (auto inputResponse = JSFetchResponse::toWrapped(vm, source)) {
        handleResponseOnStreamingAction(globalObject, inputResponse, promise, [promise, importedObject] (JSC::JSGlobalObject* lexicalGlobalObject, const char* data, size_t byteSize) mutable {
            if (auto arrayBuffer = tryAllocate(lexicalGlobalObject, promise, data, byteSize))
                JSC::JSWebAssembly::webAssemblyModuleInstantinateAsync(lexicalGlobalObject, promise, WTFMove(*arrayBuffer), importedObject);
        });
    } else
        promise->reject(globalObject, createTypeError(globalObject, "first argument must be an Response or Promise for Response"_s));
}
#endif

} // namespace WebCore
