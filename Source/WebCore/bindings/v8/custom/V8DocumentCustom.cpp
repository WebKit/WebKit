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
#include "Frame.h"
#include "Node.h"
#include "TouchList.h"
#include "XPathNSResolver.h"
#include "XPathResult.h"

#include "V8Binding.h"
#include "V8CanvasRenderingContext2D.h"
#include "V8CustomXPathNSResolver.h"
#include "V8DOMImplementation.h"
#include "V8DOMWindowShell.h"
#include "V8DOMWrapper.h"
#include "V8HTMLDocument.h"
#include "V8Node.h"
#include "V8Touch.h"
#include "V8TouchList.h"
#if ENABLE(WEBGL)
#include "V8WebGLRenderingContext.h"
#endif
#include "V8XPathNSResolver.h"
#include "V8XPathResult.h"

#if ENABLE(SVG)
#include "V8SVGDocument.h"
#endif

#include <wtf/RefPtr.h>

namespace WebCore {

v8::Handle<v8::Value> V8Document::evaluateMethodCustom(const v8::Arguments& args)
{
    RefPtr<Document> document = V8Document::toNative(args.Holder());
    ExceptionCode ec = 0;
    String expression = toWebCoreString(args[0]);
    RefPtr<Node> contextNode;
    if (V8Node::HasInstance(args[1], args.GetIsolate()))
        contextNode = V8Node::toNative(v8::Handle<v8::Object>::Cast(args[1]));

    RefPtr<XPathNSResolver> resolver = toXPathNSResolver(args[2], args.GetIsolate());
    if (!resolver && !args[2]->IsNull() && !args[2]->IsUndefined())
        return setDOMException(TYPE_MISMATCH_ERR, args.GetIsolate());

    int type = toInt32(args[3]);
    RefPtr<XPathResult> inResult;
    if (V8XPathResult::HasInstance(args[4], args.GetIsolate()))
        inResult = V8XPathResult::toNative(v8::Handle<v8::Object>::Cast(args[4]));

    V8TRYCATCH(RefPtr<XPathResult>, result, document->evaluate(expression, contextNode.get(), resolver.get(), type, inResult.get(), ec));
    if (ec)
        return setDOMException(ec, args.GetIsolate());

    return toV8Fast(result.release(), args, document.get());
}

v8::Handle<v8::Object> wrap(Document* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    if (impl->isHTMLDocument())
        return wrap(static_cast<HTMLDocument*>(impl), creationContext, isolate);
#if ENABLE(SVG)
    if (impl->isSVGDocument())
        return wrap(static_cast<SVGDocument*>(impl), creationContext, isolate);
#endif
    v8::Handle<v8::Object> wrapper = V8Document::createWrapper(impl, creationContext, isolate);
    if (wrapper.IsEmpty())
        return wrapper;
    if (!worldForEnteredContext()) {
        if (Frame* frame = impl->frame())
            frame->script()->windowShell(mainThreadNormalWorld())->updateDocumentWrapper(wrapper);
    }
    return wrapper;
}

#if ENABLE(TOUCH_EVENTS)
v8::Handle<v8::Value> V8Document::createTouchListMethodCustom(const v8::Arguments& args)
{
    RefPtr<TouchList> touchList = TouchList::create();

    for (int i = 0; i < args.Length(); i++) {
        Touch* touch = V8DOMWrapper::isWrapperOfType(args[i], &V8Touch::info) ? V8Touch::toNative(args[i]->ToObject()) : 0;
        touchList->append(touch);
    }

    return toV8(touchList.release(), args.Holder(), args.GetIsolate());
}
#endif

} // namespace WebCore
