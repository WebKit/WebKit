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
#include "V8Document.h"

#include "CanvasRenderingContext.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Node.h"
#include "XPathNSResolver.h"
#include "XPathResult.h"

#include "V8Binding.h"
#include "V8CanvasRenderingContext2D.h"
#include "V8CustomXPathNSResolver.h"
#include "V8DOMImplementation.h"
#include "V8HTMLDocument.h"
#include "V8IsolatedContext.h"
#include "V8Node.h"
#include "V8Proxy.h"
#include "V8SVGDocument.h"
#include "V8WebGLRenderingContext.h"
#include "V8XPathNSResolver.h"
#include "V8XPathResult.h"

#include <wtf/RefPtr.h>

namespace WebCore {

#if ENABLE(XPATH)
v8::Handle<v8::Value> V8Document::evaluateCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Document.evaluate()");

    RefPtr<Document> document = V8Document::toNative(args.Holder());
    ExceptionCode ec = 0;
    String expression = toWebCoreString(args[0]);
    RefPtr<Node> contextNode;
    if (V8Node::HasInstance(args[1]))
        contextNode = V8Node::toNative(v8::Handle<v8::Object>::Cast(args[1]));

    RefPtr<XPathNSResolver> resolver = V8DOMWrapper::getXPathNSResolver(args[2], V8Proxy::retrieve(V8Proxy::retrieveFrameForCallingContext()));
    if (!resolver && !args[2]->IsNull() && !args[2]->IsUndefined())
        return throwError(TYPE_MISMATCH_ERR);

    int type = toInt32(args[3]);
    RefPtr<XPathResult> inResult;
    if (V8XPathResult::HasInstance(args[4]))
        inResult = V8XPathResult::toNative(v8::Handle<v8::Object>::Cast(args[4]));

    v8::TryCatch exceptionCatcher;
    RefPtr<XPathResult> result = document->evaluate(expression, contextNode.get(), resolver.get(), type, inResult.get(), ec);
    if (exceptionCatcher.HasCaught())
        return throwError(exceptionCatcher.Exception());

    if (ec)
        return throwError(ec);

    return toV8(result.release());
}
#endif

v8::Handle<v8::Value> V8Document::getCSSCanvasContextCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Document.getCSSCanvasContext");
    v8::Handle<v8::Object> holder = args.Holder();
    Document* imp = V8Document::toNative(holder);
    String contextId = toWebCoreString(args[0]);
    String name = toWebCoreString(args[1]);
    int width = toInt32(args[2]);
    int height = toInt32(args[3]);
    CanvasRenderingContext* result = imp->getCSSCanvasContext(contextId, name, width, height);
    if (!result)
        return v8::Undefined();
    if (result->is2d())
        return toV8(static_cast<CanvasRenderingContext2D*>(result));
#if ENABLE(3D_CANVAS)
    else if (result->is3d())
        return toV8(static_cast<WebGLRenderingContext*>(result));
#endif // ENABLE(3D_CANVAS)
    ASSERT_NOT_REACHED();
    return v8::Undefined();
}


// DOMImplementation is a singleton in WebCore. If we use our normal
// mapping from DOM objects to V8 wrappers, the same wrapper will be
// shared for all frames in the same process. This is a major
// security problem. Therefore, we generate a DOMImplementation
// wrapper per document and store it in an internal field of the
// document. Since the DOMImplementation object is a singleton, we do
// not have to do anything to keep the DOMImplementation object alive
// for the lifetime of the wrapper.
v8::Handle<v8::Value> V8Document::implementationAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    ASSERT(info.Holder()->InternalFieldCount() >= internalFieldCount);

    // Check if the internal field already contains a wrapper.
    v8::Local<v8::Value> implementation = info.Holder()->GetInternalField(V8Document::implementationIndex);
    if (!implementation->IsUndefined())
        return implementation;

    // Generate a wrapper.
    Document* document = V8Document::toNative(info.Holder());
    v8::Handle<v8::Value> wrapper = toV8(document->implementation());

    // Store the wrapper in the internal field.
    info.Holder()->SetInternalField(implementationIndex, wrapper);

    return wrapper;
}

v8::Handle<v8::Value> toV8(Document* impl, bool forceNewObject)
{
    if (!impl)
        return v8::Null();
    if (impl->isHTMLDocument())
        return toV8(static_cast<HTMLDocument*>(impl), forceNewObject);
#if ENABLE(SVG)
    if (impl->isSVGDocument())
        return toV8(static_cast<SVGDocument*>(impl), forceNewObject);
#endif
    v8::Handle<v8::Object> wrapper = V8Document::wrap(impl, forceNewObject);
    if (wrapper.IsEmpty())
        return wrapper;
    if (!V8IsolatedContext::getEntered()) {
        if (V8Proxy* proxy = V8Proxy::retrieve(impl->frame()))
            proxy->windowShell()->updateDocumentWrapper(wrapper);
    }
    return wrapper;
}

} // namespace WebCore
