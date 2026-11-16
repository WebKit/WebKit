/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSWindowProxy.h"

#include "CommonVM.h"
#include "Document.h"
#include "Frame.h"
#include "GarbageCollectionController.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowProperties.h"
#include "JSEventTarget.h"
#include "ScriptController.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/StructureInlines.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

using namespace JSC;

const ClassInfo JSWindowProxy::s_info = { "JSWindowProxy"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWindowProxy) };

inline JSWindowProxy::JSWindowProxy(VM& vm, Structure& structure, DOMWrapperWorld& world)
    : Base(vm, &structure, nullptr)
    , m_world(world)
{
}

JSWindowProxy::~JSWindowProxy() = default;

void JSWindowProxy::finishCreation(VM& vm, DOMWindow& window)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    setWindow(window);
}

JSWindowProxy& JSWindowProxy::create(VM& vm, DOMWindow& window, DOMWrapperWorld& world)
{
    auto& structure = *Structure::create(vm, 0, jsNull(), TypeInfo(GlobalProxyType, StructureFlags), info());
    auto& proxy = *new (NotNull, JSC::allocateCell<JSWindowProxy>(vm)) JSWindowProxy(vm, structure, world);
    proxy.finishCreation(vm, window);
    return proxy;
}

void JSWindowProxy::destroy(JSCell* cell)
{
    // We cannot rely on jsCast() during JSObject destruction.
    SUPPRESS_MEMORY_UNSAFE_CAST static_cast<JSWindowProxy*>(cell)->JSWindowProxy::~JSWindowProxy();
}

void JSWindowProxy::setWindow(VM& vm, JSDOMGlobalObject& window)
{
    ASSERT(window.classInfo() == JSDOMWindow::info());
    setTarget(vm, &window);
    GarbageCollectionController::singleton().garbageCollectSoon();
}

void JSWindowProxy::setWindow(DOMWindow& domWindow)
{
    // Replacing JSDOMWindow via telling JSWindowProxy to use the same LocalDOMWindow it already uses makes no sense,
    // so we'd better never try to.
    ASSERT(!window() || &domWindow != &wrapped());

    auto* localWindow = dynamicDowncast<LocalDOMWindow>(domWindow);

    VM& vm = commonVM();
    auto& prototypeStructure = *JSDOMWindowPrototype::createStructure(vm, nullptr, jsNull());

    // Explicitly protect the prototype so it isn't collected when we allocate the global object.
    // (Once the global object is fully constructed, it will mark its own prototype.)
    JSNonFinalObject* prototype = static_cast<JSNonFinalObject*>(JSDOMWindowPrototype::create(vm, nullptr, &prototypeStructure));
    JSC::EnsureStillAliveScope protectedPrototype(prototype);

    JSDOMGlobalObject* window = nullptr;
    auto& windowStructure = *JSDOMWindow::createStructure(vm, nullptr, prototype);
    window = JSDOMWindow::create(vm, &windowStructure, domWindow, this);
    if (localWindow) {
        bool linkedWithNewSDK = true;
#if PLATFORM(COCOA)
        linkedWithNewSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DOMWindowReuseRestriction);
#endif
        if (!localWindow->document()->haveInitializedSecurityOrigin() && linkedWithNewSDK)
            localWindow->setAsWrappedWithoutInitializedSecurityOrigin();
    }

    prototype->structure()->setRealm(vm, window);

    auto& propertiesStructure = *JSDOMWindowProperties::createStructure(vm, window, JSEventTarget::prototype(vm, *window));
    auto& properties = *JSDOMWindowProperties::create(&propertiesStructure, *window);
    properties.didBecomePrototype(vm);
    prototype->structure()->setPrototypeWithoutTransition(vm, &properties);

    setWindow(vm, *window);

    ASSERT(window->realm() == window);
    ASSERT(prototype->realm() == window);
}

WindowProxy* JSWindowProxy::windowProxy() const
{
    auto& window = wrapped();
    return window.frame() ? &window.frame()->windowProxy() : nullptr;
}

void JSWindowProxy::attachDebugger(JSC::Debugger* debugger)
{
    auto* globalObject = window();
    JSLockHolder lock(globalObject->vm());

    if (debugger)
        debugger->attach(globalObject);
    else if (auto* currentDebugger = globalObject->debugger())
        currentDebugger->detach(globalObject, JSC::Debugger::TerminatingDebuggingSession);
}

DOMWindow& JSWindowProxy::wrapped() const
{
    auto* window = this->window();
    return downcast<JSDOMWindowBase>(window)->wrapped();
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, WindowProxy& windowProxy)
{
    auto* jsWindowProxy = windowProxy.jsWindowProxy(protect(currentWorld(*lexicalGlobalObject)));
    return jsWindowProxy ? JSValue(jsWindowProxy) : jsNull();
}

JSWindowProxy* toJSWindowProxy(WindowProxy& windowProxy, DOMWrapperWorld& world)
{
    return windowProxy.jsWindowProxy(world);
}

WindowProxy* JSWindowProxy::toWrapped(VM&, JSValue value)
{
    if (!value.isObject())
        return nullptr;
    JSObject* object = asObject(value);
    if (auto* windowProxy = dynamicDowncast<JSWindowProxy>(*object))
        return windowProxy->windowProxy();
    return nullptr;
}

JSC::GCClient::IsoSubspace* JSWindowProxy::subspaceForImpl(JSC::VM& vm)
{
    return &downcast<JSVMClientData>(vm.clientData)->windowProxySpace();
}

} // namespace WebCore
