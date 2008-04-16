/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "JSDOMWindowWrapper.h"

#include "Frame.h"
#include "JSDOMWindow.h"
#include "DOMWindow.h"
#include "kjs_proxy.h"
#include <kjs/object.h>

using namespace KJS;

namespace WebCore {

const ClassInfo JSDOMWindowWrapper::s_info = { "JSDOMWindowWrapper", 0, 0 };

JSDOMWindowWrapper::JSDOMWindowWrapper(DOMWindow* domWindow)
    : Base(jsNull())
    , m_window(0)
{
    m_window = new JSDOMWindow(domWindow, this);
    setPrototype(m_window->prototype());
}

JSDOMWindowWrapper::~JSDOMWindowWrapper()
{
}

// ----
// JSObject methods
// ----

void JSDOMWindowWrapper::mark()
{
    Base::mark();
    if (m_window && !m_window->marked())
        m_window->mark();
}

UString JSDOMWindowWrapper::className() const
{
    return m_window->className();
}

bool JSDOMWindowWrapper::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return m_window->getOwnPropertySlot(exec, propertyName, slot);
}

void JSDOMWindowWrapper::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    m_window->put(exec, propertyName, value);
}

void JSDOMWindowWrapper::putWithAttributes(ExecState* exec, const Identifier& propertyName, JSValue* value, unsigned attributes)
{
    m_window->putWithAttributes(exec, propertyName, value, attributes);
}

bool JSDOMWindowWrapper::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    return m_window->deleteProperty(exec, propertyName);
}

void JSDOMWindowWrapper::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    m_window->getPropertyNames(exec, propertyNames);
}

bool JSDOMWindowWrapper::getPropertyAttributes(const Identifier& propertyName, unsigned& attributes) const
{
    return m_window->getPropertyAttributes(propertyName, attributes);
}

void JSDOMWindowWrapper::defineGetter(ExecState* exec, const Identifier& propertyName, JSObject* getterFunction)
{
    m_window->defineGetter(exec, propertyName, getterFunction);
}

void JSDOMWindowWrapper::defineSetter(ExecState* exec, const Identifier& propertyName, JSObject* setterFunction)
{
    m_window->defineSetter(exec, propertyName, setterFunction);
}

JSValue* JSDOMWindowWrapper::lookupGetter(ExecState* exec, const Identifier& propertyName)
{
    return m_window->lookupGetter(exec, propertyName);
}

JSValue* JSDOMWindowWrapper::lookupSetter(ExecState* exec, const Identifier& propertyName)
{
    return m_window->lookupSetter(exec, propertyName);
}

JSGlobalObject* JSDOMWindowWrapper::toGlobalObject(ExecState*) const
{
    return m_window;
}

// ----
// JSDOMWindow methods
// ----

DOMWindow* JSDOMWindowWrapper::impl() const
{
    return m_window->impl();
}

void JSDOMWindowWrapper::disconnectFrame()
{
    m_window->disconnectFrame();
}

void JSDOMWindowWrapper::clear()
{
    m_window->clear();
}

// ----
// Conversion methods
// ----

JSValue* toJS(ExecState*, Frame* frame)
{
    if (!frame)
        return jsNull();
    return frame->scriptProxy()->windowWrapper();
}

JSDOMWindowWrapper* toJSDOMWindowWrapper(Frame* frame)
{
    if (!frame)
        return 0;
    return frame->scriptProxy()->windowWrapper();
}

} // namespace WebCore
