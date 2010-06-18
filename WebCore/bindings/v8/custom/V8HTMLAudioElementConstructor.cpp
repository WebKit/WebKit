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
#include "V8HTMLAudioElementConstructor.h"

#include "HTMLAudioElement.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "V8Binding.h"
#include "V8Document.h"
#include "V8HTMLAudioElement.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {

WrapperTypeInfo V8HTMLAudioElementConstructor::info = { V8HTMLAudioElementConstructor::GetTemplate, 0, 0 };

static v8::Handle<v8::Value> v8HTMLAudioElementConstructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLAudioElement.Contructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.");

    Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
    if (!frame)
        return throwError("Audio constructor associated frame is unavailable", V8Proxy::ReferenceError);

    Document* document = frame->document();
    if (!document)
        return throwError("Audio constructor associated document is unavailable", V8Proxy::ReferenceError);

    // Make sure the document is added to the DOM Node map. Otherwise, the HTMLAudioElement instance
    // may end up being the only node in the map and get garbage-collected prematurely.
    toV8(document);


    String src;
    if (args.Length() > 0)
        src = toWebCoreString(args[0]);
    RefPtr<HTMLAudioElement> audio = HTMLAudioElement::createForJSConstructor(document, src);

    V8DOMWrapper::setDOMWrapper(args.Holder(), &V8HTMLAudioElementConstructor::info, audio.get());
    audio->ref();
    V8DOMWrapper::setJSWrapperForDOMNode(audio.get(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}

v8::Persistent<v8::FunctionTemplate> V8HTMLAudioElementConstructor::GetTemplate()
{
    static v8::Persistent<v8::FunctionTemplate> cachedTemplate;
    if (!cachedTemplate.IsEmpty())
        return cachedTemplate;

    v8::HandleScope scope;
    v8::Local<v8::FunctionTemplate> result = v8::FunctionTemplate::New(v8HTMLAudioElementConstructorCallback);

    v8::Local<v8::ObjectTemplate> instance = result->InstanceTemplate();
    instance->SetInternalFieldCount(V8HTMLAudioElement::internalFieldCount);
    result->SetClassName(v8::String::New("HTMLAudioElement"));
    result->Inherit(V8HTMLAudioElement::GetTemplate());

    cachedTemplate = v8::Persistent<v8::FunctionTemplate>::New(result);
    return cachedTemplate;
}

} // namespace WebCore
