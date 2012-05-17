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
#include "V8XMLHttpRequest.h"

#include "Frame.h"
#include "OriginAccessEntry.h"
#include "SecurityOrigin.h"
#include "V8Binding.h"
#include "V8IsolatedContext.h"
#include "V8Proxy.h"
#include "V8Utilities.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8XMLHttpRequest::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.XMLHttpRequest.Constructor");

    if (!args.IsConstructCall())
        return V8Proxy::throwTypeError("DOM object constructor cannot be called as a function.");

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

    // Expect no parameters.
    // Allocate a XMLHttpRequest object as its internal field.
    ScriptExecutionContext* context = getScriptExecutionContext();
    if (!context)
        return V8Proxy::throwError(V8Proxy::ReferenceError, "XMLHttpRequest constructor's associated context is not available");

    RefPtr<SecurityOrigin> securityOrigin;
    if (V8IsolatedContext* isolatedContext = V8IsolatedContext::getEntered())
        securityOrigin = isolatedContext->securityOrigin();
    RefPtr<XMLHttpRequest> xmlHttpRequest = XMLHttpRequest::create(context, securityOrigin);
    V8DOMWrapper::setDOMWrapper(args.Holder(), &info, xmlHttpRequest.get());
    V8DOMWrapper::setJSWrapperForActiveDOMObject(xmlHttpRequest.release(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}

} // namespace WebCore
