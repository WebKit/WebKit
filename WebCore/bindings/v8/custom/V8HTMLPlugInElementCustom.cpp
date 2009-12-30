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
#include "HTMLPlugInElement.h"

#include "ScriptInstance.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8HTMLAppletElement.h"
#include "V8HTMLEmbedElement.h"
#include "V8HTMLObjectElement.h"
#include "V8NPObject.h"
#include "V8Proxy.h"

namespace WebCore {

// FIXME: Consider moving getter/setter helpers to V8NPObject and renaming this file to V8PluginElementFunctions
// to match JSC bindings naming convention.

static v8::Handle<v8::Value> npObjectNamedGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    HTMLPlugInElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLPlugInElement>(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return notHandledByInterceptor();

    v8::Local<v8::Object> instance = v8::Local<v8::Object>::New(scriptInstance->instance());
    if (instance.IsEmpty())
        return notHandledByInterceptor();

    return npObjectGetNamedProperty(instance, name);
}

static v8::Handle<v8::Value> npObjectNamedSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    HTMLPlugInElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLPlugInElement>(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return notHandledByInterceptor();

    v8::Local<v8::Object> instance = v8::Local<v8::Object>::New(scriptInstance->instance());
    if (instance.IsEmpty())
        return notHandledByInterceptor();

    return npObjectSetNamedProperty(instance, name, value);
}

NAMED_PROPERTY_GETTER(HTMLAppletElement)
{
    INC_STATS("DOM.HTMLAppletElement.NamedPropertyGetter");
    return npObjectNamedGetter(name, info);
}

NAMED_PROPERTY_GETTER(HTMLEmbedElement)
{
    INC_STATS("DOM.HTMLEmbedElement.NamedPropertyGetter");
    return npObjectNamedGetter(name, info);
}

NAMED_PROPERTY_GETTER(HTMLObjectElement)
{
    INC_STATS("DOM.HTMLObjectElement.NamedPropertyGetter");
    return npObjectNamedGetter(name, info);
}

NAMED_PROPERTY_SETTER(HTMLAppletElement)
{
    INC_STATS("DOM.HTMLAppletElement.NamedPropertySetter");
    return npObjectNamedSetter(name, value, info);
}

NAMED_PROPERTY_SETTER(HTMLEmbedElement)
{
    INC_STATS("DOM.HTMLEmbedElement.NamedPropertySetter");
    return npObjectNamedSetter(name, value, info);
}

NAMED_PROPERTY_SETTER(HTMLObjectElement)
{
    INC_STATS("DOM.HTMLObjectElement.NamedPropertySetter");
    return npObjectNamedSetter(name, value, info);
}

v8::Handle<v8::Value> V8HTMLAppletElement::callAsFunctionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLAppletElement()");
    return npObjectInvokeDefaultHandler(args);
}

v8::Handle<v8::Value> V8HTMLEmbedElement::callAsFunctionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLEmbedElement()");
    return npObjectInvokeDefaultHandler(args);
}

v8::Handle<v8::Value> V8HTMLObjectElement::callAsFunctionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLObjectElement()");
    return npObjectInvokeDefaultHandler(args);
}

v8::Handle<v8::Value> npObjectIndexedGetter(uint32_t index, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLPlugInElement.IndexedPropertyGetter");
    HTMLPlugInElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLPlugInElement>(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return notHandledByInterceptor();

    v8::Local<v8::Object> instance = v8::Local<v8::Object>::New(scriptInstance->instance());
    if (instance.IsEmpty())
        return notHandledByInterceptor();

    return npObjectGetIndexedProperty(instance, index);
}

v8::Handle<v8::Value> npObjectIndexedSetter(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLPlugInElement.IndexedPropertySetter");
    HTMLPlugInElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLPlugInElement>(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return notHandledByInterceptor();

    v8::Local<v8::Object> instance = v8::Local<v8::Object>::New(scriptInstance->instance());
    if (instance.IsEmpty())
        return notHandledByInterceptor();

    return npObjectSetIndexedProperty(instance, index, value);
}

INDEXED_PROPERTY_GETTER(HTMLAppletElement)
{
    INC_STATS("DOM.HTMLAppletElement.IndexedPropertyGetter");
    return npObjectIndexedGetter(index, info);
}

INDEXED_PROPERTY_GETTER(HTMLEmbedElement)
{
    INC_STATS("DOM.HTMLEmbedElement.IndexedPropertyGetter");
    return npObjectIndexedGetter(index, info);
}

INDEXED_PROPERTY_GETTER(HTMLObjectElement)
{
    INC_STATS("DOM.HTMLObjectElement.IndexedPropertyGetter");
    return npObjectIndexedGetter(index, info);
}

INDEXED_PROPERTY_SETTER(HTMLAppletElement)
{
    INC_STATS("DOM.HTMLAppletElement.IndexedPropertySetter");
    return npObjectIndexedSetter(index, value, info);
}

INDEXED_PROPERTY_SETTER(HTMLEmbedElement)
{
    INC_STATS("DOM.HTMLEmbedElement.IndexedPropertySetter");
    return npObjectIndexedSetter(index, value, info);
}

INDEXED_PROPERTY_SETTER(HTMLObjectElement)
{
    INC_STATS("DOM.HTMLObjectElement.IndexedPropertySetter");
    return npObjectIndexedSetter(index, value, info);
}

} // namespace WebCore
