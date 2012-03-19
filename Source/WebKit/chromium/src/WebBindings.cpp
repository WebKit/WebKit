/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebBindings.h"

#include "npruntime_impl.h"
#include "npruntime_priv.h"

#if USE(V8)
#include <wtf/ArrayBufferView.h>
#include "DOMWindow.h"
#include "NPV8Object.h"  // for PrivateIdentifier
#include "Range.h"
#include "V8ArrayBuffer.h"
#include "V8ArrayBufferView.h"
#include "V8BindingState.h"
#include "V8DOMWrapper.h"
#include "V8Element.h"
#include "V8NPUtils.h"
#include "V8Proxy.h"
#include "V8Range.h"
#elif USE(JSC)
#include "bridge/c/c_utility.h"
#endif
#include "WebArrayBuffer.h"
#include "platform/WebArrayBufferView.h"
#include "WebElement.h"
#include "WebRange.h"

using namespace WebCore;

namespace WebKit {

bool WebBindings::construct(NPP npp, NPObject* object, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    return _NPN_Construct(npp, object, args, argCount, result);
}

NPObject* WebBindings::createObject(NPP npp, NPClass* npClass)
{
    return _NPN_CreateObject(npp, npClass);
}

bool WebBindings::enumerate(NPP npp, NPObject* object, NPIdentifier** identifier, uint32_t* identifierCount)
{
    return _NPN_Enumerate(npp, object, identifier, identifierCount);
}

bool WebBindings::evaluate(NPP npp, NPObject* object, NPString* script, NPVariant* result)
{
    return _NPN_Evaluate(npp, object, script, result);
}

bool WebBindings::evaluateHelper(NPP npp, bool popupsAllowed, NPObject* object, NPString* script, NPVariant* result)
{
    return _NPN_EvaluateHelper(npp, popupsAllowed, object, script, result);
}

NPIdentifier WebBindings::getIntIdentifier(int32_t number)
{
    return _NPN_GetIntIdentifier(number);
}

bool WebBindings::getProperty(NPP npp, NPObject* object, NPIdentifier property, NPVariant* result)
{
    return _NPN_GetProperty(npp, object, property, result);
}

NPIdentifier WebBindings::getStringIdentifier(const NPUTF8* string)
{
    return _NPN_GetStringIdentifier(string);
}

void WebBindings::getStringIdentifiers(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers)
{
    _NPN_GetStringIdentifiers(names, nameCount, identifiers);
}

bool WebBindings::hasMethod(NPP npp, NPObject* object, NPIdentifier method)
{
    return _NPN_HasMethod(npp, object, method);
}

bool WebBindings::hasProperty(NPP npp, NPObject* object, NPIdentifier property)
{
    return _NPN_HasProperty(npp, object, property);
}

bool WebBindings::identifierIsString(NPIdentifier identifier)
{
    return _NPN_IdentifierIsString(identifier);
}

int32_t WebBindings::intFromIdentifier(NPIdentifier identifier)
{
    return _NPN_IntFromIdentifier(identifier);
}

void WebBindings::initializeVariantWithStringCopy(NPVariant* variant, const NPString* value)
{
#if USE(V8)
    _NPN_InitializeVariantWithStringCopy(variant, value);
#else
    NPN_InitializeVariantWithStringCopy(variant, value);
#endif
}

bool WebBindings::invoke(NPP npp, NPObject* object, NPIdentifier method, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    return _NPN_Invoke(npp, object, method, args, argCount, result);
}

bool WebBindings::invokeDefault(NPP npp, NPObject* object, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    return _NPN_InvokeDefault(npp, object, args, argCount, result);
}

void WebBindings::releaseObject(NPObject* object)
{
    return _NPN_ReleaseObject(object);
}

void WebBindings::releaseVariantValue(NPVariant* variant)
{
    _NPN_ReleaseVariantValue(variant);
}

bool WebBindings::removeProperty(NPP npp, NPObject* object, NPIdentifier identifier)
{
    return _NPN_RemoveProperty(npp, object, identifier);
}

NPObject* WebBindings::retainObject(NPObject* object)
{
    return _NPN_RetainObject(object);
}

void WebBindings::setException(NPObject* object, const NPUTF8* message)
{
    _NPN_SetException(object, message);
}

bool WebBindings::setProperty(NPP npp, NPObject* object, NPIdentifier identifier, const NPVariant* value)
{
    return _NPN_SetProperty(npp, object, identifier, value);
}

void WebBindings::unregisterObject(NPObject* object)
{
#if USE(V8)
    _NPN_UnregisterObject(object);
#endif
}

NPUTF8* WebBindings::utf8FromIdentifier(NPIdentifier identifier)
{
    return _NPN_UTF8FromIdentifier(identifier);
}

void WebBindings::extractIdentifierData(const NPIdentifier& identifier, const NPUTF8*& string, int32_t& number, bool& isString)
{
    PrivateIdentifier* data = static_cast<PrivateIdentifier*>(identifier);
    if (!data) {
        isString = false;
        number = 0;
        return;
    }

    isString = data->isString;
    if (isString)
        string = data->value.string;
    else
        number = data->value.number;
}

#if USE(V8)

static bool getRangeImpl(NPObject* object, WebRange* webRange)
{
    if (!object || (object->_class != npScriptObjectClass))
        return false;

    V8NPObject* v8NPObject = reinterpret_cast<V8NPObject*>(object);
    v8::Handle<v8::Object> v8Object(v8NPObject->v8Object);
    if (!V8Range::info.equals(V8DOMWrapper::domWrapperType(v8Object)))
        return false;

    Range* native = V8Range::HasInstance(v8Object) ? V8Range::toNative(v8Object) : 0;
    if (!native)
        return false;

    *webRange = WebRange(native);
    return true;
}

static bool getElementImpl(NPObject* object, WebElement* webElement)
{
    if (!object || (object->_class != npScriptObjectClass))
        return false;

    V8NPObject* v8NPObject = reinterpret_cast<V8NPObject*>(object);
    v8::Handle<v8::Object> v8Object(v8NPObject->v8Object);
    Element* native = V8Element::HasInstance(v8Object) ? V8Element::toNative(v8Object) : 0;
    if (!native)
        return false;

    *webElement = WebElement(native);
    return true;
}

static bool getArrayBufferImpl(NPObject* object, WebArrayBuffer* arrayBuffer)
{
    if (!object || (object->_class != npScriptObjectClass))
        return false;

    V8NPObject* v8NPObject = reinterpret_cast<V8NPObject*>(object);
    v8::Handle<v8::Object> v8Object(v8NPObject->v8Object);
    ArrayBuffer* native = V8ArrayBuffer::HasInstance(v8Object) ? V8ArrayBuffer::toNative(v8Object) : 0;
    if (!native)
        return false;

    *arrayBuffer = WebArrayBuffer(native);
    return true;
}

static bool getArrayBufferViewImpl(NPObject* object, WebArrayBufferView* arrayBufferView)
{
    if (!object || (object->_class != npScriptObjectClass))
        return false;

    V8NPObject* v8NPObject = reinterpret_cast<V8NPObject*>(object);
    v8::Handle<v8::Object> v8Object(v8NPObject->v8Object);
    ArrayBufferView* native = V8ArrayBufferView::HasInstance(v8Object) ? V8ArrayBufferView::toNative(v8Object) : 0;
    if (!native)
        return false;

    *arrayBufferView = WebArrayBufferView(native);
    return true;
}

static NPObject* makeIntArrayImpl(const WebVector<int>& data)
{
    v8::HandleScope handleScope;
    v8::Handle<v8::Array> result = v8::Array::New(data.size());
    for (size_t i = 0; i < data.size(); ++i)
        result->Set(i, v8::Number::New(data[i]));

    DOMWindow* window = V8Proxy::retrieveWindow(V8Proxy::currentContext());
    return npCreateV8ScriptObject(0, result, window);
}

static NPObject* makeStringArrayImpl(const WebVector<WebString>& data)
{
    v8::HandleScope handleScope;
    v8::Handle<v8::Array> result = v8::Array::New(data.size());
    for (size_t i = 0; i < data.size(); ++i)
        result->Set(i, data[i].data() ? v8::String::New(reinterpret_cast<const uint16_t*>((data[i].data())), data[i].length()) : v8::String::New(""));

    DOMWindow* window = V8Proxy::retrieveWindow(V8Proxy::currentContext());
    return npCreateV8ScriptObject(0, result, window);
}

#endif

bool WebBindings::getRange(NPObject* range, WebRange* webRange)
{
#if USE(V8)
    return getRangeImpl(range, webRange);
#else
    // Not supported on other ports (JSC, etc).
    return false;
#endif
}

bool WebBindings::getArrayBuffer(NPObject* arrayBuffer, WebArrayBuffer* webArrayBuffer)
{
#if USE(V8)
    return getArrayBufferImpl(arrayBuffer, webArrayBuffer);
#else
    // Not supported on other ports (JSC, etc).
    return false;
#endif
}

bool WebBindings::getArrayBufferView(NPObject* arrayBufferView, WebArrayBufferView* webArrayBufferView)
{
#if USE(V8)
    return getArrayBufferViewImpl(arrayBufferView, webArrayBufferView);
#else
    // Not supported on other ports (JSC, etc).
    return false;
#endif
}

bool WebBindings::getElement(NPObject* element, WebElement* webElement)
{
#if USE(V8)
    return getElementImpl(element, webElement);
#else
    // Not supported on other ports (JSC, etc.).
    return false;
#endif
}

NPObject* WebBindings::makeIntArray(const WebVector<int>& data)
{
#if USE(V8)
    return makeIntArrayImpl(data);
#else
    // Not supported on other ports (JSC, etc.).
    return 0;
#endif
}

NPObject* WebBindings::makeStringArray(const WebVector<WebString>& data)
{
#if USE(V8)
    return makeStringArrayImpl(data);
#else
    // Not supported on other ports (JSC, etc.).
    return 0;
#endif
}

void WebBindings::pushExceptionHandler(ExceptionHandler handler, void* data)
{
    WebCore::pushExceptionHandler(handler, data);
}

void WebBindings::popExceptionHandler()
{
    WebCore::popExceptionHandler();
}

#if WEBKIT_USING_V8
void WebBindings::toNPVariant(v8::Local<v8::Value> object, NPObject* root, NPVariant* result)
{
    WebCore::convertV8ObjectToNPVariant(object, root, result);
}

v8::Handle<v8::Value> WebBindings::toV8Value(const NPVariant* variant)
{
    if (variant->type == NPVariantType_Object) {
        NPObject* object = NPVARIANT_TO_OBJECT(*variant);
        if (object->_class != npScriptObjectClass)
            return v8::Undefined();
        V8NPObject* v8Object = reinterpret_cast<V8NPObject*>(object);
        return convertNPVariantToV8Object(variant, v8Object->rootObject->frame()->script()->windowScriptNPObject());
    }
    // Safe to pass 0 since we have checked the script object class to make sure the
    // argument is a primitive v8 type.
    return convertNPVariantToV8Object(variant, 0);
}
#endif

} // namespace WebKit
