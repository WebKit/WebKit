/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NPObjectWrapper.h"

namespace WebCore {

struct NPProxyObject {
    NPObject object;
    NPObjectWrapper* wrapper;
};

NPClass NPObjectWrapper::m_npClassWrapper = {
    NP_CLASS_STRUCT_VERSION,
    NPObjectWrapper::NPAllocate,
    NPObjectWrapper::NPDeallocate,
    NPObjectWrapper::NPPInvalidate,
    NPObjectWrapper::NPHasMethod,
    NPObjectWrapper::NPInvoke,
    NPObjectWrapper::NPInvokeDefault,
    NPObjectWrapper::NPHasProperty,
    NPObjectWrapper::NPGetProperty,
    NPObjectWrapper::NPSetProperty,
    NPObjectWrapper::NPRemoveProperty,
    NPObjectWrapper::NPNEnumerate,
    NPObjectWrapper::NPNConstruct
};

NPObjectWrapper::NPObjectWrapper(NPObject* obj)
    : m_wrappedNPObject(obj)
{
}

NPObject* NPObjectWrapper::create(NPObject* object)
{
    ASSERT(object);
    NPProxyObject* proxyObject = reinterpret_cast<NPProxyObject*>(_NPN_CreateObject(0, &m_npClassWrapper));
    proxyObject->wrapper = new NPObjectWrapper(object);
    return reinterpret_cast<NPObject*>(proxyObject);
}

void NPObjectWrapper::clear()
{
    m_wrappedNPObject = 0;   
}

NPObjectWrapper* NPObjectWrapper::getWrapper(NPObject* obj)
{
    if (&m_npClassWrapper == obj->_class) {
        NPProxyObject* proxyObject = reinterpret_cast<NPProxyObject*>(obj);
        return proxyObject->wrapper;
    }
    return 0;
}

NPObject* NPObjectWrapper::getUnderlyingNPObject(NPObject* obj)
{
    NPObjectWrapper* wrapper = getWrapper(obj);
    return wrapper ? wrapper->m_wrappedNPObject : 0;
}

NPObject* NPObjectWrapper::getObjectForCall(NPObject* obj)
{
    NPObject* actualObject = getUnderlyingNPObject(obj);
    return actualObject ? actualObject : 0;
}

NPObject* NPObjectWrapper::NPAllocate(NPP, NPClass*)
{
    return reinterpret_cast<NPObject*>(new NPProxyObject);
}

void NPObjectWrapper::NPDeallocate(NPObject* obj)
{
    NPProxyObject* proxyObject = reinterpret_cast<NPProxyObject*>(obj);
    delete proxyObject->wrapper;
    delete proxyObject;
}

void NPObjectWrapper::NPPInvalidate(NPObject* obj)
{
    NPObject* actualObject = getObjectForCall(obj);
    if (actualObject && actualObject->_class->invalidate)
        actualObject->_class->invalidate(actualObject);
}

bool NPObjectWrapper::NPHasMethod(NPObject* obj, NPIdentifier name)
{
    NPObject* actualObject = getObjectForCall(obj);
    return actualObject ? _NPN_HasMethod(0, actualObject, name) : false;
}

bool NPObjectWrapper::NPInvoke(NPObject* obj, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    NPObject* actualObject = getObjectForCall(obj);
    return actualObject ? _NPN_Invoke(0, actualObject, name, args, argCount, result) : false;
}

bool NPObjectWrapper::NPInvokeDefault(NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    NPObject* actualObject = getObjectForCall(obj);
    return actualObject ? _NPN_InvokeDefault(0, actualObject, args, argCount, result) : false;
}
 
bool NPObjectWrapper::NPHasProperty(NPObject* obj, NPIdentifier name)
{
    NPObject* actualObject = getObjectForCall(obj);
    return actualObject ? _NPN_HasProperty(0, actualObject, name) : false;
}

bool NPObjectWrapper::NPGetProperty(NPObject* obj, NPIdentifier name, NPVariant* result)
{
    NPObject* actualObject = getObjectForCall(obj);
    return actualObject ? _NPN_GetProperty(0, actualObject, name, result) : false;
}

bool NPObjectWrapper::NPSetProperty(NPObject* obj, NPIdentifier name, const NPVariant* value)
{
    NPObject* actualObject = getObjectForCall(obj);
    return actualObject ? _NPN_SetProperty(0, actualObject, name, value) : false;
}

bool NPObjectWrapper::NPRemoveProperty(NPObject* obj, NPIdentifier name) {
    NPObject* actualObject = getObjectForCall(obj);
    return actualObject ? _NPN_RemoveProperty(0, actualObject, name) : false;
}

bool NPObjectWrapper::NPNEnumerate(NPObject* obj, NPIdentifier** value, uint32_t* count)
{
    NPObject* actualObject = getObjectForCall(obj);
    return actualObject ? _NPN_Enumerate(0, actualObject, value, count) : false;
}

bool NPObjectWrapper::NPNConstruct(NPObject* obj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    NPObject* actualObject = getObjectForCall(obj);
    return actualObject ? _NPN_Construct(0, actualObject, args, argCount, result) : false;
}

bool NPObjectWrapper::NPInvokePrivate(NPP npp, NPObject* obj, bool isDefault, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    NPObject* actualObject = getObjectForCall(obj);
    if (!actualObject)
        return false;

    if (isDefault) {
        return _NPN_InvokeDefault(0, actualObject, args, argCount, result);
    } else {
        return _NPN_Invoke(0, actualObject, name, args, argCount, result);
    }
}

} // namespace WebCore
