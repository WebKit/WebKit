/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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
#include "JSDOMWindowProxy.h"

#include "CommonVM.h"
#include "Frame.h"
#include "GCController.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowProperties.h"
#include "JSEventTarget.h"
#include "ScriptController.h"
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/StrongInlines.h>

namespace WebCore {

const ClassInfo JSDOMWindowProxy::s_info = { "JSDOMWindowProxy", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDOMWindowProxy) };

inline JSDOMWindowProxy::JSDOMWindowProxy(VM& vm, Structure& structure, DOMWrapperWorld& world)
    : Base(vm, &structure)
    , m_world(world)
{
}

void JSDOMWindowProxy::finishCreation(VM& vm, DOMWindow& window)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    setWindow(window);
}

JSDOMWindowProxy& JSDOMWindowProxy::create(JSC::VM& vm, DOMWindow& window, DOMWrapperWorld& world)
{
    auto& structure = *JSC::Structure::create(vm, 0, jsNull(), JSC::TypeInfo(JSC::PureForwardingProxyType, StructureFlags), info());
    auto& proxy = *new (NotNull, JSC::allocateCell<JSDOMWindowProxy>(vm.heap)) JSDOMWindowProxy(vm, structure, world);
    proxy.finishCreation(vm, window);
    return proxy;
}

void JSDOMWindowProxy::destroy(JSCell* cell)
{
    static_cast<JSDOMWindowProxy*>(cell)->JSDOMWindowProxy::~JSDOMWindowProxy();
}

void JSDOMWindowProxy::setWindow(VM& vm, JSDOMWindow& window)
{
    setTarget(vm, &window);
    structure()->setGlobalObject(vm, &window);
    GCController::singleton().garbageCollectSoon();
}

void JSDOMWindowProxy::setWindow(DOMWindow& domWindow)
{
    // Replacing JSDOMWindow via telling JSDOMWindowProxy to use the same DOMWindow it already uses makes no sense,
    // so we'd better never try to.
    ASSERT(!window() || &domWindow != &window()->wrapped());

    VM& vm = commonVM();
    auto& prototypeStructure = *JSDOMWindowPrototype::createStructure(vm, nullptr, jsNull());

    // Explicitly protect the prototype so it isn't collected when we allocate the global object.
    // (Once the global object is fully constructed, it will mark its own prototype.)
    // FIXME: Why do we need to protect this when there's a pointer to it on the stack?
    // Perhaps the issue is that structure objects aren't seen when scanning the stack?
    Strong<JSDOMWindowPrototype> prototype(vm, JSDOMWindowPrototype::create(vm, nullptr, &prototypeStructure));

    auto& windowStructure = *JSDOMWindow::createStructure(vm, nullptr, prototype.get());
    auto& window = *JSDOMWindow::create(vm, &windowStructure, domWindow, this);
    prototype->structure()->setGlobalObject(vm, &window);

    auto& propertiesStructure = *JSDOMWindowProperties::createStructure(vm, &window, JSEventTarget::prototype(vm, window));
    auto& properties = *JSDOMWindowProperties::create(&propertiesStructure, window);
    prototype->structure()->setPrototypeWithoutTransition(vm, &properties);

    setWindow(vm, window);

    ASSERT(window.globalObject() == &window);
    ASSERT(prototype->globalObject() == &window);
}

DOMWindow& JSDOMWindowProxy::wrapped() const
{
    return window()->wrapped();
}

DOMWindow* JSDOMWindowProxy::toWrapped(VM& vm, JSObject* value)
{
    auto* wrapper = jsDynamicCast<JSDOMWindowProxy*>(vm, value);
    return wrapper ? &wrapper->window()->wrapped() : nullptr;
}

JSValue toJS(ExecState* state, Frame& frame)
{
    return &frame.windowProxyController().windowProxy(currentWorld(*state));
}

JSDOMWindowProxy& toJSDOMWindowProxy(Frame& frame, DOMWrapperWorld& world)
{
    return frame.windowProxyController().windowProxy(world);
}

} // namespace WebCore
