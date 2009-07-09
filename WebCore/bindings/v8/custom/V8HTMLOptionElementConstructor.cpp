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
#include "HTMLOptionElement.h"

#include "Document.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "Text.h"

#include "V8Binding.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {

CALLBACK_FUNC_DECL(HTMLOptionElementConstructor)
{
    INC_STATS("DOM.HTMLOptionElement.Contructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.");

    Document* document = V8Proxy::retrieveFrame()->document();
    if (!document)
        return throwError("Option constructor associated document is unavailable", V8Proxy::ReferenceError);

    RefPtr<HTMLOptionElement> option = new HTMLOptionElement(HTMLNames::optionTag, V8Proxy::retrieveFrame()->document());

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

    V8DOMWrapper::setDOMWrapper(args.Holder(), V8ClassIndex::ToInt(V8ClassIndex::NODE), option.get());
    option->ref();
    V8DOMWrapper::setJSWrapperForDOMNode(option.get(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}

} // namespace WebCore
