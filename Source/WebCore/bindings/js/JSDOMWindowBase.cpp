/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reseved.
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
#include "JSDocument.h"
#include "JSFetchResponse.h"
#include "JSMicrotaskCallback.h"
#include "JSNode.h"
#include "Logging.h"
#include "Page.h"
#include "RejectedPromiseTracker.h"
#include "RuntimeApplicationChecks.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptController.h"
#include "ScriptModuleLoader.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/DeferredWorkTimer.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/JSWebAssembly.h>
#include <JavaScriptCore/Microtask.h>
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
    &reportUncaughtExceptionAtEventLoop,
    &currentScriptExecutionOwner,
    &scriptExecutionStatus,
    [] { return defaultLanguage(); },
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
    , m_wrapped(WTFMove(window))
{
    m_proxy.set(vm, this, proxy);
}

SUPPRESS_ASAN inline void JSDOMWindowBase::initStaticGlobals(JSC::VM& vm)
{
    auto& builtinNames = static_cast<JSVMClientData*>(vm.clientData)->builtinNames();

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(builtinNames.documentPublicName(), jsNull(), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(builtinNames.windowPublicName(), m_proxy.get(), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
    };

    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
}

void JSDOMWindowBase::finishCreation(VM& vm, JSWindowProxy* proxy)
{
    Base::finishCreation(vm, proxy);
    ASSERT(inherits(vm, info()));

    initStaticGlobals(vm);

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
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    bool shouldThrowReadOnlyError = false;
    bool ignoreReadOnlyErrors = true;
    bool putResult = false;
    symbolTablePutTouchWatchpointSet(this, lexicalGlobalObject, static_cast<JSVMClientData*>(vm.clientData)->builtinNames().documentPublicName(), toJS(lexicalGlobalObject, this, m_wrapped->document()), shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception());
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
    // Propagating media only user gesture for Fetch API's promise chain.
    auto userGestureToken = UserGestureIndicator::currentUserGesture();
    if (userGestureToken && (!userGestureToken->isPropagatedFromFetch() || !RuntimeEnabledFeatures::sharedFeatures().userGesturePromisePropagationEnabled()))
        userGestureToken = nullptr;
    eventLoop.queueMicrotask([callback = WTFMove(callback), userGestureToken = WTFMove(userGestureToken)]() mutable {
        if (!userGestureToken) {
            callback->call();
            return;
        }

        UserGestureIndicator gestureIndicator(userGestureToken, UserGestureToken::GestureScope::MediaOnly, UserGestureToken::IsPropagatedFromFetch::Yes);
        callback->call();
    });
}

JSC::JSObject* JSDOMWindowBase::currentScriptExecutionOwner(JSGlobalObject* object)
{
    JSDOMWindowBase* thisObject = static_cast<JSDOMWindowBase*>(object);
    return jsCast<JSObject*>(toJS(thisObject, thisObject, thisObject->wrapped().document()));
}

JSC::ScriptExecutionStatus JSDOMWindowBase::scriptExecutionStatus(JSC::JSGlobalObject*, JSC::JSObject* owner)
{
    return jsCast<JSDocument*>(owner)->wrapped().jscScriptExecutionStatus();
}

void JSDOMWindowBase::willRemoveFromWindowProxy()
{
    setCurrentEvent(0);
}

void JSDOMWindowBase::setCurrentEvent(Event* currentEvent)
{
    m_currentEvent = currentEvent;
}

Event* JSDOMWindowBase::currentEvent() const
{
    return m_currentEvent.get();
}

JSWindowProxy& JSDOMWindowBase::proxy() const
{
    return *jsCast<JSWindowProxy*>(&JSDOMGlobalObject::proxy());
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

DOMWindow& incumbentDOMWindow(JSGlobalObject& fallbackGlobalObject, CallFrame& callFrame)
{
    if (auto* globalObject = CallFrame::globalObjectOfClosestCodeBlock(fallbackGlobalObject.vm(), &callFrame))
        return asJSDOMWindow(globalObject)->wrapped();
    return asJSDOMWindow(&fallbackGlobalObject)->wrapped();
}

DOMWindow& incumbentDOMWindow(JSGlobalObject& fallbackGlobalObject)
{
    VM& vm = fallbackGlobalObject.vm();
    if (auto* globalObject = CallFrame::globalObjectOfClosestCodeBlock(vm, vm.topCallFrame))
        return asJSDOMWindow(globalObject)->wrapped();
    return asJSDOMWindow(&fallbackGlobalObject)->wrapped();
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
        jsWindow->m_windowCloseWatchpoints->fireAll(vm, "Frame cleared");
    }
}

} // namespace WebCore
