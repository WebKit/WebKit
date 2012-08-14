/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "V8DOMFormData.h"

#include "DOMFormData.h"
#include "V8Binding.h"
#include "V8Blob.h"
#include "V8HTMLFormElement.h"
#include "V8Proxy.h"
#include "V8Utilities.h"

namespace WebCore {

v8::Handle<v8::Value> V8DOMFormData::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.FormData.Constructor");

    if (!args.IsConstructCall())
        return throwTypeError("DOM object constructor cannot be called as a function.", args.GetIsolate());

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

    HTMLFormElement* form = 0;
    if (args.Length() > 0 && V8HTMLFormElement::HasInstance(args[0]))
        form = V8HTMLFormElement::toNative(args[0]->ToObject());
    RefPtr<DOMFormData> domFormData = DOMFormData::create(form);

    v8::Handle<v8::Object> wrapper = args.Holder();
    V8DOMWrapper::setDOMWrapper(wrapper, &info, domFormData.get());
    V8DOMWrapper::setJSWrapperForDOMObject(domFormData.release(), wrapper);
    return wrapper;
}

v8::Handle<v8::Value> V8DOMFormData::appendCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.FormData.append()");
    
    if (args.Length() < 2)
        return throwError(SyntaxError, "Not enough arguments", args.GetIsolate());

    DOMFormData* domFormData = V8DOMFormData::toNative(args.Holder());

    String name = toWebCoreStringWithNullCheck(args[0]);

    v8::Handle<v8::Value> arg = args[1];
    if (V8Blob::HasInstance(arg)) {
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);
        Blob* blob = V8Blob::toNative(object);
        ASSERT(blob);

        String filename;
        if (args.Length() >= 3 && !args[2]->IsUndefined())
            filename = toWebCoreStringWithNullCheck(args[2]);

        domFormData->append(name, blob, filename);
    } else
        domFormData->append(name, toWebCoreStringWithNullCheck(arg));

    return v8::Undefined();
}

} // namespace WebCore
