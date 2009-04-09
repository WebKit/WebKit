/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#include "Frame.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8ObjectEventListener.h"
#include "V8Proxy.h"
#include "XMLHttpRequest.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(XMLHttpRequestConstructor)
{
    INC_STATS("DOM.XMLHttpRequest.Constructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.", V8Proxy::TYPE_ERROR);

    // Expect no parameters.
    // Allocate a XMLHttpRequest object as its internal field.
    ScriptExecutionContext* context = 0;
#if ENABLE(WORKERS)
    WorkerContextExecutionProxy* proxy = WorkerContextExecutionProxy::retrieve();
    if (proxy)
        context = proxy->workerContext();
    else
#endif
        context = V8Proxy::retrieveFrame()->document();
    RefPtr<XMLHttpRequest> xmlHttpRequest = XMLHttpRequest::create(context);
    V8Proxy::SetDOMWrapper(args.Holder(), V8ClassIndex::ToInt(V8ClassIndex::XMLHTTPREQUEST), xmlHttpRequest.get());

    // Add object to the wrapper map.
    xmlHttpRequest->ref();
    V8Proxy::SetJSWrapperForActiveDOMObject(xmlHttpRequest.get(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}

} // namespace WebCore
