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
#include "V8HTMLAllCollection.h"

#include "HTMLAllCollection.h"

#include "V8Binding.h"
#include "V8NamedNodesCollection.h"
#include "V8Node.h"
#include "V8NodeList.h"
#include "V8Proxy.h"

namespace WebCore {

static v8::Handle<v8::Value> getNamedItems(HTMLAllCollection* collection, AtomicString name, v8::Isolate* isolate)
{
    Vector<RefPtr<Node> > namedItems;
    collection->namedItems(name, namedItems);

    if (!namedItems.size())
        return v8Undefined();

    if (namedItems.size() == 1)
        return toV8(namedItems.at(0).release(), isolate);

    return toV8(V8NamedNodesCollection::create(namedItems), isolate);
}

static v8::Handle<v8::Value> getItem(HTMLAllCollection* collection, v8::Handle<v8::Value> argument, v8::Isolate* isolate)
{
    v8::Local<v8::Uint32> index = argument->ToArrayIndex();
    if (index.IsEmpty()) {
        v8::Handle<v8::Value> result = getNamedItems(collection, toWebCoreString(argument->ToString()), isolate);

        if (result.IsEmpty())
            return v8::Undefined();

        return result;
    }

    RefPtr<Node> result = collection->item(index->Uint32Value());
    return toV8(result.release(), isolate);
}

v8::Handle<v8::Value> V8HTMLAllCollection::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLAllCollection.NamedPropertyGetter");

    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())
        return v8Undefined();
    if (info.Holder()->HasRealNamedCallbackProperty(name))
        return v8Undefined();

    HTMLAllCollection* imp = V8HTMLAllCollection::toNative(info.Holder());
    return getNamedItems(imp, toWebCoreAtomicString(name), info.GetIsolate());
}

v8::Handle<v8::Value> V8HTMLAllCollection::itemCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLAllCollection.item()");
    HTMLAllCollection* imp = V8HTMLAllCollection::toNative(args.Holder());
    return getItem(imp, args[0], args.GetIsolate());
}

v8::Handle<v8::Value> V8HTMLAllCollection::namedItemCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLAllCollection.namedItem()");
    HTMLAllCollection* imp = V8HTMLAllCollection::toNative(args.Holder());
    v8::Handle<v8::Value> result = getNamedItems(imp, toWebCoreString(args[0]), args.GetIsolate());

    if (result.IsEmpty())
        return v8::Undefined();

    return result;
}

v8::Handle<v8::Value> V8HTMLAllCollection::callAsFunctionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLAllCollection.callAsFunction()");
    if (args.Length() < 1)
        return v8::Undefined();

    HTMLAllCollection* imp = V8HTMLAllCollection::toNative(args.Holder());

    if (args.Length() == 1)
        return getItem(imp, args[0], args.GetIsolate());

    // If there is a second argument it is the index of the item we want.
    String name = toWebCoreString(args[0]);
    v8::Local<v8::Uint32> index = args[1]->ToArrayIndex();
    if (index.IsEmpty())
        return v8::Undefined();

    if (Node* node = imp->namedItemWithIndex(name, index->Uint32Value()))
        return toV8(node, args.GetIsolate());

    return v8::Undefined();
}

} // namespace WebCore
