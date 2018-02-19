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
#include "Frame.h"
#include "InspectorController.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMGlobalObjectTask.h"
#include "JSDOMWindowCustom.h"
#include "JSMainThreadExecState.h"
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
#include <bytecode/CodeBlock.h>
#include <heap/StrongInlines.h>
#include <runtime/JSInternalPromise.h>
#include <runtime/JSInternalPromiseDeferred.h>
#include <runtime/Microtask.h>
#include <wtf/Language.h>
#include <wtf/MainThread.h>

#if PLATFORM(IOS)
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
    &defaultLanguage
};

JSDOMWindowBase::JSDOMWindowBase(VM& vm, Structure* structure, RefPtr<DOMWindow>&& window, JSDOMWindowProxy* proxy)
    : JSDOMGlobalObject(vm, structure, proxy->world(), &s_globalObjectMethodTable, window ? &window->document()->threadLocalCache() : nullptr)
    , m_windowCloseWatchpoints((window && window->frame()) ? IsWatched : IsInvalidated)
    , m_wrapped(WTFMove(window))
    , m_proxy(proxy)
{
}

void JSDOMWindowBase::finishCreation(VM& vm, JSDOMWindowProxy* proxy)
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

#if PLATFORM(IOS)
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

class JSDOMWindowMicrotaskCallback : public RefCounted<JSDOMWindowMicrotaskCallback> {
public:
    static Ref<JSDOMWindowMicrotaskCallback> create(JSDOMWindowBase& globalObject, Ref<JSC::Microtask>&& task)
    {
        return adoptRef(*new JSDOMWindowMicrotaskCallback(globalObject, WTFMove(task)));
    }

    void call()
    {
        Ref<JSDOMWindowMicrotaskCallback> protectedThis(*this);
        VM& vm = m_globalObject->vm();
        JSLockHolder lock(vm);
        auto scope = DECLARE_THROW_SCOPE(vm);

        ExecState* exec = m_globalObject->globalExec();

        JSMainThreadExecState::runTask(exec, m_task);

        scope.assertNoException();
    }

private:
    JSDOMWindowMicrotaskCallback(JSDOMWindowBase& globalObject, Ref<JSC::Microtask>&& task)
        : m_globalObject { globalObject.vm(), &globalObject }
        , m_task { WTFMove(task) }
    {
    }

    Strong<JSDOMWindowBase> m_globalObject;
    Ref<JSC::Microtask> m_task;
};

void JSDOMWindowBase::queueTaskToEventLoop(JSGlobalObject& object, Ref<JSC::Microtask>&& task)
{
    JSDOMWindowBase& thisObject = static_cast<JSDOMWindowBase&>(object);

    RefPtr<JSDOMWindowMicrotaskCallback> callback = JSDOMWindowMicrotaskCallback::create(thisObject, WTFMove(task));
    auto microtask = std::make_unique<ActiveDOMCallbackMicrotask>(MicrotaskQueue::mainThreadQueue(), *thisObject.scriptExecutionContext(), [callback]() mutable {
        callback->call();
    });

    MicrotaskQueue::mainThreadQueue().append(WTFMove(microtask));
}

void JSDOMWindowBase::willRemoveFromWindowProxy()
{
    setCurrentEvent(0);
}

JSDOMWindowProxy* JSDOMWindowBase::proxy() const
{
    return m_proxy;
}

// JSDOMGlobalObject* is ignored, accessing a window in any context will
// use that DOMWindow's prototype chain.
JSValue toJS(ExecState* state, JSDOMGlobalObject*, DOMWindow& domWindow)
{
    return toJS(state, domWindow);
}

JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject*, Frame& frame)
{
    return toJS(state, frame);
}

JSValue toJS(ExecState* state, DOMWindow& domWindow)
{
    return toJS(state, domWindow.frame());
}

JSDOMWindow* toJSDOMWindow(Frame& frame, DOMWrapperWorld& world)
{
    return frame.script().windowProxy(world)->window();
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
        if (classInfo == JSDOMWindowProxy::info())
            return jsCast<JSDOMWindowProxy*>(object)->window();
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
    return asJSDOMWindow(state.vmEntryGlobalObject())->wrapped();
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
        return document->moduleLoader()->resolve(globalObject, exec, moduleLoader, moduleName, importerModuleKey, scriptFetcher);
    return { };
}

JSC::JSInternalPromise* JSDOMWindowBase::moduleLoaderFetch(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue parameters, JSC::JSValue scriptFetcher)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader()->fetch(globalObject, exec, moduleLoader, moduleKey, parameters, scriptFetcher);
    JSC::JSInternalPromiseDeferred* deferred = JSC::JSInternalPromiseDeferred::create(exec, globalObject);
    return deferred->reject(exec, jsUndefined());
}

JSC::JSValue JSDOMWindowBase::moduleLoaderEvaluate(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSValue moduleRecord, JSC::JSValue scriptFetcher)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader()->evaluate(globalObject, exec, moduleLoader, moduleKey, moduleRecord, scriptFetcher);
    return JSC::jsUndefined();
}

JSC::JSInternalPromise* JSDOMWindowBase::moduleLoaderImportModule(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader* moduleLoader, JSC::JSString* moduleName, JSC::JSValue parameters, const JSC::SourceOrigin& sourceOrigin)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader()->importModule(globalObject, exec, moduleLoader, moduleName, parameters, sourceOrigin);
    JSC::JSInternalPromiseDeferred* deferred = JSC::JSInternalPromiseDeferred::create(exec, globalObject);
    return deferred->reject(exec, jsUndefined());
}

JSC::JSObject* JSDOMWindowBase::moduleLoaderCreateImportMetaProperties(JSC::JSGlobalObject* globalObject, JSC::ExecState* exec, JSC::JSModuleLoader* moduleLoader, JSC::JSValue moduleKey, JSC::JSModuleRecord* moduleRecord, JSC::JSValue scriptFetcher)
{
    JSDOMWindowBase* thisObject = JSC::jsCast<JSDOMWindowBase*>(globalObject);
    if (RefPtr<Document> document = thisObject->wrapped().document())
        return document->moduleLoader()->createImportMetaProperties(globalObject, exec, moduleLoader, moduleKey, moduleRecord, scriptFetcher);
    return constructEmptyObject(exec, globalObject->nullPrototypeObjectStructure());
}

void JSDOMWindowBase::promiseRejectionTracker(JSGlobalObject* jsGlobalObject, ExecState* exec, JSPromise* promise, JSPromiseRejectionOperation operation)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    VM& vm = exec->vm();
    auto& globalObject = *JSC::jsCast<JSDOMWindowBase*>(jsGlobalObject);
    auto* context = globalObject.scriptExecutionContext();
    if (!context)
        return;

    // InternalPromises should not be exposed to user scripts.
    if (JSC::jsDynamicCast<JSC::JSInternalPromise*>(vm, promise))
        return;

    // FIXME: If script has muted errors (cross origin), terminate these steps.
    // <https://webkit.org/b/171415> Implement the `muted-errors` property of Scripts to avoid onerror/onunhandledrejection for cross-origin scripts

    switch (operation) {
    case JSPromiseRejectionOperation::Reject:
        context->ensureRejectedPromiseTracker().promiseRejected(*exec, globalObject, *promise);
        break;
    case JSPromiseRejectionOperation::Handle:
        context->ensureRejectedPromiseTracker().promiseHandled(*exec, globalObject, *promise);
        break;
    }
}

} // namespace WebCore
