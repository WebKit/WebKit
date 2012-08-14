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
#include "V8NodeList.h" 

#include "DynamicNodeList.h"
#include "NodeList.h"
#include "V8Binding.h"
#include "V8Node.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

v8::Handle<v8::Value> V8NodeList::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.NodeList.NamedPropertyGetter");
    NodeList* list = V8NodeList::toNative(info.Holder());
    AtomicString key = toWebCoreAtomicString(name);

    // Length property cannot be overridden.
    DEFINE_STATIC_LOCAL(const AtomicString, length, ("length"));
    if (key == length)
        return v8Integer(list->length(), info.GetIsolate());

    RefPtr<Node> result = list->itemWithName(key);
    if (!result)
        return v8Undefined();

    return toV8(result.release(), info.GetIsolate());
}

void V8NodeList::visitDOMWrapper(DOMDataStore* store, void* object, v8::Persistent<v8::Object> wrapper)
{
    NodeList* impl = static_cast<NodeList*>(object);
    if (impl->isDynamicNodeList()) {
        Node* owner = static_cast<DynamicNodeList*>(impl)->ownerNode();
        if (owner) {
            v8::Persistent<v8::Object> ownerWrapper = store->domNodeMap().get(owner);
            if (!ownerWrapper.IsEmpty()) {
                v8::Persistent<v8::Value> value = wrapper;
                v8::V8::AddImplicitReferences(ownerWrapper, &value, 1);
            }
        }
    }
}

} // namespace WebCore
