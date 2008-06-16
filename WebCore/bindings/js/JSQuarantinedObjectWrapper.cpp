/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 */

#include "config.h"
#include "JSQuarantinedObjectWrapper.h"

#include <kjs/JSGlobalObject.h>

using namespace KJS;

namespace WebCore {

const ClassInfo JSQuarantinedObjectWrapper::s_info = { "JSQuarantinedObjectWrapper", 0, 0, 0 };

JSQuarantinedObjectWrapper* JSQuarantinedObjectWrapper::asWrapper(JSValue* value)
{
    if (!value->isObject())
        return 0;

    JSObject* object = static_cast<JSObject*>(value);

    if (!object->inherits(&JSQuarantinedObjectWrapper::s_info))
        return 0;

    return static_cast<JSQuarantinedObjectWrapper*>(object);
}

JSValue* JSQuarantinedObjectWrapper::cachedValueGetter(ExecState*, const Identifier&, const PropertySlot& slot)
{
    JSValue* v = slot.slotBase();
    ASSERT(v);
    return v;
}

JSQuarantinedObjectWrapper::JSQuarantinedObjectWrapper(ExecState* unwrappedExec, JSObject* unwrappedObject, JSValue* wrappedPrototype)
    : JSObject(wrappedPrototype)
    , m_unwrappedGlobalObject(unwrappedExec->dynamicGlobalObject())
    , m_unwrappedObject(unwrappedObject)
{
    ASSERT_ARG(unwrappedExec, unwrappedExec);
    ASSERT_ARG(unwrappedObject, unwrappedObject);
    ASSERT_ARG(wrappedPrototype, wrappedPrototype);
    ASSERT_ARG(wrappedPrototype, !wrappedPrototype->isObject() || asWrapper(wrappedPrototype));
}

JSQuarantinedObjectWrapper::~JSQuarantinedObjectWrapper()
{
}

bool JSQuarantinedObjectWrapper::allowsUnwrappedAccessFrom(const ExecState* exec) const
{
    return m_unwrappedGlobalObject->pageGroupIdentifier() == exec->dynamicGlobalObject()->pageGroupIdentifier();
}

ExecState* JSQuarantinedObjectWrapper::unwrappedExecState() const
{
    return m_unwrappedGlobalObject->globalExec();
}

void JSQuarantinedObjectWrapper::transferExceptionToExecState(ExecState* exec) const
{
    ASSERT(exec != unwrappedExecState());

    if (!unwrappedExecState()->hadException())
        return;

    exec->setException(wrapOutgoingValue(unwrappedExecState(), unwrappedExecState()->exception()));
    unwrappedExecState()->clearException();
}

void JSQuarantinedObjectWrapper::mark()
{
    JSObject::mark();

    if (!m_unwrappedObject->marked())
        m_unwrappedObject->mark();
    if (!m_unwrappedGlobalObject->marked())
        m_unwrappedGlobalObject->mark();
}

bool JSQuarantinedObjectWrapper::getOwnPropertySlot(ExecState* exec, const Identifier& identifier, PropertySlot& slot)
{
    if (!allowsGetProperty()) {
        slot.setUndefined();
        return true;
    }

    PropertySlot unwrappedSlot(m_unwrappedObject);
    bool result = m_unwrappedObject->getOwnPropertySlot(unwrappedExecState(), identifier, unwrappedSlot);
    if (result) {
        JSValue* unwrappedValue = unwrappedSlot.getValue(unwrappedExecState(), identifier);
        slot.setCustom(static_cast<JSObject*>(wrapOutgoingValue(unwrappedExecState(), unwrappedValue)), cachedValueGetter);
    }

    transferExceptionToExecState(exec);

    return result;
}

bool JSQuarantinedObjectWrapper::getOwnPropertySlot(ExecState* exec, unsigned identifier, PropertySlot& slot)
{
    if (!allowsGetProperty()) {
        slot.setUndefined();
        return true;
    }

    PropertySlot unwrappedSlot(m_unwrappedObject);
    bool result = m_unwrappedObject->getOwnPropertySlot(unwrappedExecState(), identifier, unwrappedSlot);
    if (result) {
        JSValue* unwrappedValue = unwrappedSlot.getValue(unwrappedExecState(), identifier);
        slot.setCustom(static_cast<JSObject*>(wrapOutgoingValue(unwrappedExecState(), unwrappedValue)), cachedValueGetter);
    }

    transferExceptionToExecState(exec);

    return result;
}

void JSQuarantinedObjectWrapper::put(ExecState* exec, const Identifier& identifier, JSValue* value)
{
    if (!allowsSetProperty())
        return;

    JSValue* preparedValue = prepareIncomingValue(exec, value);
    if (!preparedValue)
        return;

    m_unwrappedObject->put(unwrappedExecState(), identifier, preparedValue);

    transferExceptionToExecState(exec);
}

void JSQuarantinedObjectWrapper::put(ExecState* exec, unsigned identifier, JSValue* value)
{
    if (!allowsSetProperty())
        return;

    JSValue* preparedValue = prepareIncomingValue(exec, value);
    if (!preparedValue)
        return;

    m_unwrappedObject->put(unwrappedExecState(), identifier, preparedValue);

    transferExceptionToExecState(exec);
}

bool JSQuarantinedObjectWrapper::deleteProperty(ExecState* exec, const Identifier& identifier)
{
    if (!allowsDeleteProperty())
        return false;

    bool result = m_unwrappedObject->deleteProperty(unwrappedExecState(), identifier);

    transferExceptionToExecState(exec);

    return result;
}

bool JSQuarantinedObjectWrapper::deleteProperty(ExecState* exec, unsigned identifier)
{
    if (!allowsDeleteProperty())
        return false;

    bool result = m_unwrappedObject->deleteProperty(unwrappedExecState(), identifier);

    transferExceptionToExecState(exec);

    return result;
}

KJS::ConstructType JSQuarantinedObjectWrapper::getConstructData(KJS::ConstructData& data)
{
    return m_unwrappedObject->getConstructData(data);
}

JSObject* JSQuarantinedObjectWrapper::construct(ExecState* exec, const ArgList& args)
{
    if (!allowsConstruct())
        return 0;

    ArgList argsCopy;
    for (size_t i = 0; i < args.size(); ++i) {
        JSValue* preparedValue = prepareIncomingValue(exec, args[i]);
        if (!preparedValue)
            return 0;
        argsCopy.append(preparedValue);
    }

    JSValue* resultValue = wrapOutgoingValue(unwrappedExecState(), m_unwrappedObject->construct(unwrappedExecState(), argsCopy));
    ASSERT(resultValue->isObject());
    JSObject* result = resultValue->toObject(exec);

    transferExceptionToExecState(exec);

    return result;
}

bool JSQuarantinedObjectWrapper::implementsHasInstance() const
{
    return m_unwrappedObject->implementsHasInstance();
}

bool JSQuarantinedObjectWrapper::hasInstance(ExecState* exec, JSValue* value)
{
    if (!allowsHasInstance())
        return false;

    JSValue* preparedValue = prepareIncomingValue(exec, value);
    if (!preparedValue)
        return false;

    bool result = m_unwrappedObject->hasInstance(unwrappedExecState(), preparedValue);

    transferExceptionToExecState(exec);

    return result;
}

CallType JSQuarantinedObjectWrapper::getCallData(CallData&)
{
    CallData temp;
    return m_unwrappedObject->getCallData(temp) != CallTypeNone ? CallTypeNative : CallTypeNone;
}

JSValue* JSQuarantinedObjectWrapper::callAsFunction(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    if (!allowsCallAsFunction())
        return jsUndefined();

    JSObject* preparedThisObj = static_cast<JSObject*>(prepareIncomingValue(exec, thisObj));
    if (!preparedThisObj)
        return jsUndefined();

    ArgList preparedArgs;
    for (size_t i = 0; i < args.size(); ++i) {
        JSValue* preparedValue = prepareIncomingValue(exec, args[i]);
        if (!preparedValue)
            return jsUndefined();
        preparedArgs.append(preparedValue);
    }

    JSValue* result = wrapOutgoingValue(unwrappedExecState(), m_unwrappedObject->callAsFunction(unwrappedExecState(), preparedThisObj, preparedArgs));

    transferExceptionToExecState(exec);

    return result;
}

void JSQuarantinedObjectWrapper::getPropertyNames(ExecState* exec, PropertyNameArray& array)
{
    if (!allowsGetPropertyNames())
        return;

    m_unwrappedObject->getPropertyNames(unwrappedExecState(), array);
}

} // namespace WebCore
