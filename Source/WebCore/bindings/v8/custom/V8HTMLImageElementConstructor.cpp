/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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
#include "V8HTMLImageElementConstructor.h"

#include "BindingState.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "V8Binding.h"
#include "V8Document.h"
#include "V8HTMLImageElement.h"
#include <wtf/RefPtr.h>

namespace WebCore {

WrapperTypeInfo V8HTMLImageElementConstructor::info = { V8HTMLImageElementConstructor::GetTemplate, V8HTMLImageElement::derefObject, 0, V8HTMLImageElement::toEventTarget, 0, V8HTMLImageElement::installPerContextPrototypeProperties, 0, WrapperTypeObjectPrototype };

static v8::Handle<v8::Value> v8HTMLImageElementConstructorMethodCustom(const v8::Arguments& args)
{
    if (!args.IsConstructCall())
        return throwTypeError("DOM object constructor cannot be called as a function.", args.GetIsolate());

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

    Document* document = currentDocument(BindingState::instance());

    // Make sure the document is added to the DOM Node map. Otherwise, the HTMLImageElement instance
    // may end up being the only node in the map and get garbage-collected prematurely.
    // FIXME: The correct way to do this would be to make HTMLImageElement derive from
    // ActiveDOMObject and use its interface to keep its wrapper alive. Then we would
    // remove this code and the special case in isObservableThroughDOM.
    toV8(document, args.Holder(), args.GetIsolate());

    int width;
    int height;
    int* optionalWidth = 0;
    int* optionalHeight = 0;
    if (args.Length() > 0) {
        width = toInt32(args[0]);
        optionalWidth = &width;
    }
    if (args.Length() > 1) {
        height = toInt32(args[1]);
        optionalHeight = &height;
    }

    RefPtr<HTMLImageElement> image = HTMLImageElement::createForJSConstructor(document, optionalWidth, optionalHeight);
    v8::Handle<v8::Object> wrapper = args.Holder();
    V8DOMWrapper::associateObjectWithWrapper(image.release(), &V8HTMLImageElementConstructor::info, wrapper, args.GetIsolate(), WrapperConfiguration::Dependent);
    return wrapper;
}

v8::Persistent<v8::FunctionTemplate> V8HTMLImageElementConstructor::GetTemplate(v8::Isolate* isolate)
{
    static v8::Persistent<v8::FunctionTemplate> cachedTemplate;
    if (!cachedTemplate.IsEmpty())
        return cachedTemplate;

    v8::HandleScope scope;
    v8::Local<v8::FunctionTemplate> result = v8::FunctionTemplate::New(v8HTMLImageElementConstructorMethodCustom);

    v8::Local<v8::ObjectTemplate> instance = result->InstanceTemplate();
    instance->SetInternalFieldCount(V8HTMLImageElement::internalFieldCount);
    result->SetClassName(v8::String::NewSymbol("HTMLImageElement"));
    result->Inherit(V8HTMLImageElement::GetTemplate(isolate));

    cachedTemplate = v8::Persistent<v8::FunctionTemplate>::New(isolate, result);
    return cachedTemplate;
}

} // namespace WebCore
