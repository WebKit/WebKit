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
#include "HTMLFormElement.h"

#include "HTMLCollection.h"
#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8NamedNodesCollection.h"
#include "V8Proxy.h"

namespace WebCore {

INDEXED_PROPERTY_GETTER(HTMLFormElement)
{
    INC_STATS("DOM.HTMLFormElement.IndexedPropertyGetter");
    HTMLFormElement* form = V8DOMWrapper::convertDOMWrapperToNode<HTMLFormElement>(info.Holder());
    
    RefPtr<Node> formElement = form->elements()->item(index);
    if (!formElement)
        return notHandledByInterceptor();
    return V8DOMWrapper::convertNodeToV8Object(formElement.get());
}


NAMED_PROPERTY_GETTER(HTMLFormElement)
{
    INC_STATS("DOM.HTMLFormElement.NamedPropertyGetter");
    HTMLFormElement* imp = V8DOMWrapper::convertDOMWrapperToNode<HTMLFormElement>(info.Holder());
    AtomicString v = v8StringToAtomicWebCoreString(name);

    // Call getNamedElements twice, first time check if it has a value
    // and let HTMLFormElement update its cache.
    // See issue: 867404
    {
        Vector<RefPtr<Node> > elements;
        imp->getNamedElements(v, elements);
        if (elements.isEmpty())
            return notHandledByInterceptor();
    }

    // Second call may return different results from the first call,
    // but if the first the size cannot be zero.
    Vector<RefPtr<Node> > elements;
    imp->getNamedElements(v, elements);
    ASSERT(!elements.isEmpty());

    if (elements.size() == 1)
        return V8DOMWrapper::convertNodeToV8Object(elements.at(0).get());

    NodeList* collection = new V8NamedNodesCollection(elements);
    return V8DOMWrapper::convertToV8Object(V8ClassIndex::NODELIST, collection);
}
    
CALLBACK_FUNC_DECL(HTMLFormElementSubmit) {
    INC_STATS("DOM.HTMLFormElement.submit()");
    HTMLFormElement* form = V8DOMWrapper::convertDOMWrapperToNative<HTMLFormElement>(args.Holder());
    form->submit(0, false, false);
    return v8::Undefined();
}

} // namespace WebCore
