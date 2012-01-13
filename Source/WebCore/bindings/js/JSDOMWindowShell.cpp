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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "JSDOMWindowShell.h"

#include "Frame.h"
#include "JSDOMWindow.h"
#include "DOMWindow.h"
#include "ScriptController.h"
#include <heap/StrongInlines.h>
#include <runtime/JSObject.h>

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSDOMWindowShell);

const ClassInfo JSDOMWindowShell::s_info = { "JSDOMWindowShell", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSDOMWindowShell) };

JSDOMWindowShell::JSDOMWindowShell(Structure* structure, DOMWrapperWorld* world)
    : Base(*world->globalData(), structure)
    , m_world(world)
{
}

void JSDOMWindowShell::finishCreation(JSGlobalData& globalData, PassRefPtr<DOMWindow> window)
{
    Base::finishCreation(globalData);
    ASSERT(inherits(&s_info));
    setWindow(window);
}

void JSDOMWindowShell::destroy(JSCell* cell)
{
    jsCast<JSDOMWindowShell*>(cell)->JSDOMWindowShell::~JSDOMWindowShell();
}

void JSDOMWindowShell::setWindow(PassRefPtr<DOMWindow> domWindow)
{
    // Replacing JSDOMWindow via telling JSDOMWindowShell to use the same DOMWindow it already uses makes no sense,
    // so we'd better never try to.
    ASSERT(!window() || domWindow.get() != window()->impl());
    // Explicitly protect the global object's prototype so it isn't collected
    // when we allocate the global object. (Once the global object is fully
    // constructed, it can mark its own prototype.)
    Structure* prototypeStructure = JSDOMWindowPrototype::createStructure(*JSDOMWindow::commonJSGlobalData(), 0, jsNull());
    Strong<JSDOMWindowPrototype> prototype(*JSDOMWindow::commonJSGlobalData(), JSDOMWindowPrototype::create(*JSDOMWindow::commonJSGlobalData(), 0, prototypeStructure));

    Structure* structure = JSDOMWindow::createStructure(*JSDOMWindow::commonJSGlobalData(), 0, prototype.get());
    JSDOMWindow* jsDOMWindow = JSDOMWindow::create(*JSDOMWindow::commonJSGlobalData(), structure, domWindow, this);
    prototype->structure()->setGlobalObject(*JSDOMWindow::commonJSGlobalData(), jsDOMWindow);
    setWindow(*JSDOMWindow::commonJSGlobalData(), jsDOMWindow);
    ASSERT(jsDOMWindow->globalObject() == jsDOMWindow);
    ASSERT(prototype->globalObject() == jsDOMWindow);
}

// ----
// JSObject methods
// ----

UString JSDOMWindowShell::className(const JSObject* object)
{
    const JSDOMWindowShell* thisObject = jsCast<const JSDOMWindowShell*>(object);
    return thisObject->window()->methodTable()->className(thisObject->window());
}

bool JSDOMWindowShell::getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(cell);
    return thisObject->window()->methodTable()->getOwnPropertySlot(thisObject->window(), exec, propertyName, slot);
}

bool JSDOMWindowShell::getOwnPropertyDescriptor(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(object);
    return thisObject->window()->methodTable()->getOwnPropertyDescriptor(thisObject->window(), exec, propertyName, descriptor);
}

void JSDOMWindowShell::put(JSCell* cell, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(cell);
    thisObject->window()->methodTable()->put(thisObject->window(), exec, propertyName, value, slot);
}

void JSDOMWindowShell::putDirectVirtual(JSObject* object, ExecState* exec, const Identifier& propertyName, JSValue value, unsigned attributes)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(object);
    thisObject->window()->putDirectVirtual(thisObject->window(), exec, propertyName, value, attributes);
}

bool JSDOMWindowShell::defineOwnProperty(JSC::JSObject* object, JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertyDescriptor& descriptor, bool shouldThrow)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(object);
    return thisObject->window()->methodTable()->defineOwnProperty(thisObject->window(), exec, propertyName, descriptor, shouldThrow);
}

bool JSDOMWindowShell::deleteProperty(JSCell* cell, ExecState* exec, const Identifier& propertyName)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(cell);
    return thisObject->window()->methodTable()->deleteProperty(thisObject->window(), exec, propertyName);
}

void JSDOMWindowShell::getPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(object);
    thisObject->window()->methodTable()->getPropertyNames(thisObject->window(), exec, propertyNames, mode);
}

void JSDOMWindowShell::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(object);
    thisObject->window()->methodTable()->getOwnPropertyNames(thisObject->window(), exec, propertyNames, mode);
}

void JSDOMWindowShell::defineGetter(JSObject* object, ExecState* exec, const Identifier& propertyName, JSObject* getterFunction, unsigned attributes)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(object);
    thisObject->window()->methodTable()->defineGetter(thisObject->window(), exec, propertyName, getterFunction, attributes);
}

void JSDOMWindowShell::defineSetter(JSObject* object, ExecState* exec, const Identifier& propertyName, JSObject* setterFunction, unsigned attributes)
{
    JSDOMWindowShell* thisObject = jsCast<JSDOMWindowShell*>(object);
    thisObject->window()->methodTable()->defineSetter(thisObject->window(), exec, propertyName, setterFunction, attributes);
}


// ----
// JSDOMWindow methods
// ----

DOMWindow* JSDOMWindowShell::impl() const
{
    return window()->impl();
}

void* JSDOMWindowShell::operator new(size_t size)
{
    Heap& heap = JSDOMWindow::commonJSGlobalData()->heap;
#if ENABLE(GC_VALIDATION)
    ASSERT(!heap.globalData()->isInitializingObject());
    heap.globalData()->setInitializingObject(true);
#endif
    return heap.allocate(size);
}

// ----
// Conversion methods
// ----

JSValue toJS(ExecState* exec, Frame* frame)
{
    if (!frame)
        return jsNull();
    return frame->script()->windowShell(currentWorld(exec));
}

JSDOMWindowShell* toJSDOMWindowShell(Frame* frame, DOMWrapperWorld* isolatedWorld)
{
    if (!frame)
        return 0;
    return frame->script()->windowShell(isolatedWorld);
}

} // namespace WebCore
