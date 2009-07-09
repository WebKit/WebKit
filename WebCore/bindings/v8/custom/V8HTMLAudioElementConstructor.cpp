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
#include "HTMLAudioElement.h"

#include "Document.h"
#include "Frame.h"
#include "HTMLNames.h"

#include "V8Binding.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {

CALLBACK_FUNC_DECL(HTMLAudioElementConstructor)
{
    INC_STATS("DOM.HTMLAudioElement.Contructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.");

    Document* document = V8Proxy::retrieveFrame()->document();
    if (!document)
        return throwError("Audio constructor associated document is unavailable", V8Proxy::ReferenceError);

    // Make sure the document is added to the DOM Node map. Otherwise, the HTMLAudioElement instance
    // may end up being the only node in the map and get garbage-ccollected prematurely.
    V8DOMWrapper::convertNodeToV8Object(document);

    RefPtr<HTMLAudioElement> audio = new HTMLAudioElement(HTMLNames::audioTag, V8Proxy::retrieveFrame()->document());
    if (args.Length() > 0)
        audio->setSrc(toWebCoreString(args[0]));

    V8DOMWrapper::setDOMWrapper(args.Holder(), V8ClassIndex::ToInt(V8ClassIndex::NODE), audio.get());
    audio->ref();
    V8DOMWrapper::setJSWrapperForDOMNode(audio.get(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}

} // namespace WebCore
