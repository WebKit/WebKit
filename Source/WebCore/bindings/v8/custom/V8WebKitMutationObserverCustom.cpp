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

#if ENABLE(MUTATION_OBSERVERS)

#include "V8WebKitMutationObserver.h"

#include "Dictionary.h"
#include "ExceptionCode.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"
#include "V8DOMWrapper.h"
#include "V8MutationCallback.h"
#include "V8Node.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WebKitMutationObserver.h"

#include <wtf/HashSet.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

v8::Handle<v8::Value> V8WebKitMutationObserver::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebKitMutationObserver.Constructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.", V8Proxy::TypeError);

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

    if (args.Length() < 1)
        return V8Proxy::throwNotEnoughArgumentsError();

    v8::Local<v8::Value> arg = args[0];
    if (!arg->IsObject())
        return throwError(TYPE_MISMATCH_ERR);

    ScriptExecutionContext* context = getScriptExecutionContext();
    if (!context)
        return throwError("WebKitMutationObserver constructor's associated frame unavailable", V8Proxy::ReferenceError);

    RefPtr<MutationCallback> callback = V8MutationCallback::create(arg, context);
    RefPtr<WebKitMutationObserver> observer = WebKitMutationObserver::create(callback.release());

    V8DOMWrapper::setDOMWrapper(args.Holder(), &info, observer.get());
    V8DOMWrapper::setJSWrapperForDOMObject(observer.release(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}

v8::Handle<v8::Value> V8WebKitMutationObserver::observeCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebKitMutationObserver.observe");
    if (args.Length() < 2)
        return V8Proxy::throwNotEnoughArgumentsError();
    WebKitMutationObserver* imp = V8WebKitMutationObserver::toNative(args.Holder());
    EXCEPTION_BLOCK(Node*, target, V8Node::HasInstance(args[0]) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0);

    if (!args[1]->IsObject())
        return throwError(TYPE_MISMATCH_ERR);

    Dictionary optionsObject(args[1]);
    unsigned options = 0;
    HashSet<AtomicString> attributeFilter;
    bool option;
    if (optionsObject.get("childList", option) && option)
        options |= WebKitMutationObserver::ChildList;
    if (optionsObject.get("attributes", option) && option)
        options |= WebKitMutationObserver::Attributes;
    if (optionsObject.get("attributeFilter", attributeFilter))
        options |= WebKitMutationObserver::AttributeFilter;
    if (optionsObject.get("characterData", option) && option)
        options |= WebKitMutationObserver::CharacterData;
    if (optionsObject.get("subtree", option) && option)
        options |= WebKitMutationObserver::Subtree;
    if (optionsObject.get("attributeOldValue", option) && option)
        options |= WebKitMutationObserver::AttributeOldValue;
    if (optionsObject.get("characterDataOldValue", option) && option)
        options |= WebKitMutationObserver::CharacterDataOldValue;

    ExceptionCode ec = 0;
    imp->observe(target, options, attributeFilter, ec);
    if (ec)
        V8Proxy::setDOMException(ec, args.GetIsolate());
    return v8::Handle<v8::Value>();
}

} // namespace WebCore

#endif // ENABLE(MUTATION_OBSERVERS)
