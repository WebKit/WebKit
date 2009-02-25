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
#include "HTMLDocument.h"

#include "Frame.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {

NAMED_PROPERTY_DELETER(HTMLDocument)
{
    // Only handle document.all.  Insert the marker object into the
    // shadow internal field to signal that document.all is no longer
    // shadowed.
    String key = toWebCoreString(name);
    if (key != "all")
        return deletionNotHandledByInterceptor();

    ASSERT(info.Holder()->InternalFieldCount() == kHTMLDocumentInternalFieldCount);
    v8::Local<v8::Value> marker = info.Holder()->GetInternalField(kHTMLDocumentMarkerIndex);
    info.Holder()->SetInternalField(kHTMLDocumentShadowIndex, marker);
    return v8::True();
}

NAMED_PROPERTY_SETTER(HTMLDocument)
{
    INC_STATS("DOM.HTMLDocument.NamedPropertySetter");
    // Only handle document.all.  We insert the value into the shadow
    // internal field from which the getter will retrieve it.
    String key = toWebCoreString(name);
    if (key == "all") {
        ASSERT(info.Holder()->InternalFieldCount() == kHTMLDocumentInternalFieldCount);
        info.Holder()->SetInternalField(kHTMLDocumentShadowIndex, value);
    }
    return notHandledByInterceptor();
}

NAMED_PROPERTY_GETTER(HTMLDocument)
{
    INC_STATS("DOM.HTMLDocument.NamedPropertyGetter");
    AtomicString key = toWebCoreString(name);

    // Special case for document.all.  If the value in the shadow
    // internal field is not the marker object, then document.all has
    // been temporarily shadowed and we return the value.
    if (key == "all") {
        ASSERT(info.Holder()->InternalFieldCount() == kHTMLDocumentInternalFieldCount);
        v8::Local<v8::Value> marker = info.Holder()->GetInternalField(kHTMLDocumentMarkerIndex);
        v8::Local<v8::Value> value = info.Holder()->GetInternalField(kHTMLDocumentShadowIndex);
        if (marker != value)
            return value;
    }

    HTMLDocument* imp = V8Proxy::DOMWrapperToNode<HTMLDocument>(info.Holder());

    // Fast case for named elements that are not there.
    if (!imp->hasNamedItem(key.impl()) && !imp->hasExtraNamedItem(key.impl()))
        return v8::Handle<v8::Value>();

    RefPtr<HTMLCollection> items = imp->documentNamedItems(key);
    if (!items->length())
        return notHandledByInterceptor();

    if (items->length() == 1) {
        Node* node = items->firstItem();
        Frame* frame = 0;
        if (node->hasTagName(HTMLNames::iframeTag) && (frame = static_cast<HTMLIFrameElement*>(node)->contentFrame()))
            return V8Proxy::ToV8Object(V8ClassIndex::DOMWINDOW, frame->domWindow());

        return V8Proxy::NodeToV8Object(node);
    }

    return V8Proxy::ToV8Object(V8ClassIndex::HTMLCOLLECTION, items.get());
}

} // namespace WebCore
