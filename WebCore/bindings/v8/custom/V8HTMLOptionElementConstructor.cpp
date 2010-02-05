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
#include "V8HTMLOptionElementConstructor.h"

#include "HTMLOptionElement.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "Text.h"
#include "V8Binding.h"
#include "V8HTMLOptionElement.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {

static v8::Handle<v8::Value> v8HTMLOptionElementConstructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLOptionElement.Contructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.");

    Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
    if (!frame)
        return throwError("Option constructor associated frame is unavailable", V8Proxy::ReferenceError);

    Document* document = frame->document();
    if (!document)
        return throwError("Option constructor associated document is unavailable", V8Proxy::ReferenceError);

    RefPtr<HTMLOptionElement> option = new HTMLOptionElement(HTMLNames::optionTag, document);

    ExceptionCode ec = 0;
    RefPtr<Text> text = document->createTextNode("");
    if (args.Length() > 0) {
        if (!args[0]->IsUndefined()) {
            text->setData(toWebCoreString(args[0]), ec);
            if (ec)
                throwError(ec);
        }

        option->appendChild(text.release(), ec);
        if (ec)
            throwError(ec);

        if (args.Length() > 1) {
            if (!args[1]->IsUndefined())
                option->setValue(toWebCoreString(args[1]));

            if (args.Length() > 2) {
                option->setDefaultSelected(args[2]->BooleanValue());
                if (args.Length() > 3)
                    option->setSelected(args[3]->BooleanValue());
            }
        }
    }

    V8DOMWrapper::setDOMWrapper(args.Holder(), V8ClassIndex::ToInt(V8ClassIndex::OPTION), option.get());
    option->ref();
    V8DOMWrapper::setJSWrapperForDOMNode(option.get(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}

v8::Persistent<v8::FunctionTemplate> V8HTMLOptionElementConstructor::GetTemplate()
{
    static v8::Persistent<v8::FunctionTemplate> cachedTemplate;
    if (!cachedTemplate.IsEmpty())
        return cachedTemplate;

    v8::HandleScope scope;
    v8::Local<v8::FunctionTemplate> result = v8::FunctionTemplate::New(v8HTMLOptionElementConstructorCallback);

    v8::Local<v8::ObjectTemplate> instance = result->InstanceTemplate();
    instance->SetInternalFieldCount(V8HTMLOptionElement::internalFieldCount);
    result->SetClassName(v8::String::New("HTMLOptionElement"));
    result->Inherit(V8HTMLOptionElement::GetTemplate());

    cachedTemplate = v8::Persistent<v8::FunctionTemplate>::New(result);
    return cachedTemplate;
}

} // namespace WebCore
