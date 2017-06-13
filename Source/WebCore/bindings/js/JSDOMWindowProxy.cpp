/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
#include <heap/StrongInlines.h>
#include <runtime/JSObject.h>

using namespace JSC;

namespace WebCore {

const ClassInfo JSDOMWindowProxy::s_info = { "JSDOMWindowProxy", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDOMWindowProxy) };

JSDOMWindowProxy::JSDOMWindowProxy(VM& vm, Structure* structure, DOMWrapperWorld& world)
    : Base(vm, structure)
    , m_world(world)
{
}

void JSDOMWindowProxy::finishCreation(VM& vm, RefPtr<DOMWindow>&& window)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    setWindow(WTFMove(window));
}

void JSDOMWindowProxy::destroy(JSCell* cell)
{
    static_cast<JSDOMWindowProxy*>(cell)->JSDOMWindowProxy::~JSDOMWindowProxy();
}

void JSDOMWindowProxy::setWindow(VM& vm, JSDOMWindow* window)
{
    ASSERT_ARG(window, window);
    setTarget(vm, window);
    structure()->setGlobalObject(vm, window);
    GCController::singleton().garbageCollectSoon();
}

void JSDOMWindowProxy::setWindow(RefPtr<DOMWindow>&& domWindow)
{
    // Replacing JSDOMWindow via telling JSDOMWindowProxy to use the same DOMWindow it already uses makes no sense,
    // so we'd better never try to.
    ASSERT(!window() || domWindow.get() != &window()->wrapped());
    // Explicitly protect the global object's prototype so it isn't collected
    // when we allocate the global object. (Once the global object is fully
    // constructed, it can mark its own prototype.)
    
    VM& vm = commonVM();
    Structure* prototypeStructure = JSDOMWindowPrototype::createStructure(vm, nullptr, jsNull());
    Strong<JSDOMWindowPrototype> prototype(vm, JSDOMWindowPrototype::create(vm, nullptr, prototypeStructure));

    Structure* structure = JSDOMWindow::createStructure(vm, nullptr, prototype.get());
    JSDOMWindow* jsDOMWindow = JSDOMWindow::create(vm, structure, *domWindow, this);
    prototype->structure()->setGlobalObject(vm, jsDOMWindow);

    Structure* windowPropertiesStructure = JSDOMWindowProperties::createStructure(vm, jsDOMWindow, JSEventTarget::prototype(vm, *jsDOMWindow));
    JSDOMWindowProperties* windowProperties = JSDOMWindowProperties::create(windowPropertiesStructure, *jsDOMWindow);

    prototype->structure()->setPrototypeWithoutTransition(vm, windowProperties);
    setWindow(vm, jsDOMWindow);
    ASSERT(jsDOMWindow->globalObject() == jsDOMWindow);
    ASSERT(prototype->globalObject() == jsDOMWindow);
}

// ----
// JSDOMWindow methods
// ----

DOMWindow& JSDOMWindowProxy::wrapped() const
{
    return window()->wrapped();
}

DOMWindow* JSDOMWindowProxy::toWrapped(VM& vm, JSObject* value)
{
    auto* wrapper = jsDynamicDowncast<JSDOMWindowProxy*>(vm, value);
    if (!wrapper)
        return nullptr;
    return &wrapper->window()->wrapped();
}

// ----
// Conversion methods
// ----

JSValue toJS(ExecState* exec, Frame& frame)
{
    return frame.script().windowProxy(currentWorld(exec));
}

JSDOMWindowProxy* toJSDOMWindowProxy(Frame& frame, DOMWrapperWorld& world)
{
    return frame.script().windowProxy(world);
}

} // namespace WebCore
