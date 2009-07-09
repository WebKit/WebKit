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
#include "V8Proxy.h"

// FIXME: The name of this file will change once refactoring is complete
#include "v8_npobject.h"

namespace WebCore {

NAMED_PROPERTY_GETTER(HTMLPlugInElement)
{
    INC_STATS("DOM.HTMLPlugInElement.NamedPropertyGetter");
    HTMLPlugInElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLPlugInElement>(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return notHandledByInterceptor();

    v8::Local<v8::Object> instance = v8::Local<v8::Object>::New(scriptInstance->instance());
    if (instance.IsEmpty())
        return notHandledByInterceptor();

    return NPObjectGetNamedProperty(instance, name);
}

NAMED_PROPERTY_SETTER(HTMLPlugInElement)
{
    INC_STATS("DOM.HTMLPlugInElement.NamedPropertySetter");
    HTMLPlugInElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLPlugInElement>(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return notHandledByInterceptor();

    v8::Local<v8::Object> instance = v8::Local<v8::Object>::New(scriptInstance->instance());
    if (instance.IsEmpty())
        return notHandledByInterceptor();

    return NPObjectSetNamedProperty(instance, name, value);
}

CALLBACK_FUNC_DECL(HTMLPlugInElement)
{
    INC_STATS("DOM.HTMLPluginElement()");
    return NPObjectInvokeDefaultHandler(args);
}

INDEXED_PROPERTY_GETTER(HTMLPlugInElement)
{
    INC_STATS("DOM.HTMLPlugInElement.IndexedPropertyGetter");
    HTMLPlugInElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLPlugInElement>(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return notHandledByInterceptor();

    v8::Local<v8::Object> instance = v8::Local<v8::Object>::New(scriptInstance->instance());
    if (instance.IsEmpty())
        return notHandledByInterceptor();

    return NPObjectGetIndexedProperty(instance, index);
}

INDEXED_PROPERTY_SETTER(HTMLPlugInElement)
{
    INC_STATS("DOM.HTMLPlugInElement.IndexedPropertySetter");
    HTMLPlugInElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLPlugInElement>(info.Holder());
    ScriptInstance scriptInstance = imp->getInstance();
    if (!scriptInstance)
        return notHandledByInterceptor();

    v8::Local<v8::Object> instance = v8::Local<v8::Object>::New(scriptInstance->instance());
    if (instance.IsEmpty())
        return notHandledByInterceptor();

    return NPObjectSetIndexedProperty(instance, index, value);
}

} // namespace WebCore
