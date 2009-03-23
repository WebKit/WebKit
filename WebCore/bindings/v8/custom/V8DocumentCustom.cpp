/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#include "Document.h"

#include "ExceptionCode.h"
#include "JSXPathNSResolver.h"
#include "Node.h"
#include "XPathNSResolver.h"
#include "XPathResult.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Node.h"
#include "V8Proxy.h"
#include "V8XPathNSResolver.h"
#include "V8XPathResult.h"

#include <wtf/RefPtr.h>

namespace WebCore {

CALLBACK_FUNC_DECL(DocumentEvaluate)
{
    INC_STATS("DOM.Document.evaluate()");

    Document* document = V8Proxy::DOMWrapperToNode<Document>(args.Holder());
    ExceptionCode ec = 0;
    String expression = toWebCoreString(args[0]);
    Node* contextNode = 0;
    if (V8Node::HasInstance(args[1]))
        contextNode = V8Proxy::DOMWrapperToNode<Node>(args[1]);

    XPathNSResolver* resolver = 0;
    if (V8XPathNSResolver::HasInstance(args[2]))
        resolver = V8Proxy::ToNativeObject<XPathNSResolver>(V8ClassIndex::XPATHNSRESOLVER, args[2]);
    else if (args[2]->IsObject())
        resolver = new JSXPathNSResolver(args[2]->ToObject());
    else if (!args[2]->IsNull() && !args[2]->IsUndefined())
        return throwError(TYPE_MISMATCH_ERR);

    int type = toInt32(args[3]);
    XPathResult* inResult = 0;
    if (V8XPathResult::HasInstance(args[4]))
        inResult = V8Proxy::ToNativeObject<XPathResult>(V8ClassIndex::XPATHRESULT, args[4]);

    v8::TryCatch exceptionCatcher;
    RefPtr<XPathResult> result = document->evaluate(expression, contextNode, resolver, type, inResult, ec);
    if (exceptionCatcher.HasCaught())
        return throwError(exceptionCatcher.Exception());

    if (ec)
        return throwError(ec);

    return V8Proxy::ToV8Object(V8ClassIndex::XPATHRESULT, result.get());
}

CALLBACK_FUNC_DECL(DocumentGetCSSCanvasContext)
{
    INC_STATS("DOM.Document.getCSSCanvasContext");
    v8::Handle<v8::Value> holder = args.Holder();
    Document* imp = V8Proxy::DOMWrapperToNode<Document>(holder);
    String contextId = toWebCoreString(args[0]);
    String name = toWebCoreString(args[1]);
    int width = toInt32(args[2]);
    int height = toInt32(args[3]);
    CanvasRenderingContext2D* result = imp->getCSSCanvasContext(contextId, name, width, height);
    return V8Proxy::ToV8Object(V8ClassIndex::CANVASRENDERINGCONTEXT2D, result);
}

} // namespace WebCore
