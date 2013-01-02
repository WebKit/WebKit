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
#include "V8HTMLFormControlsCollection.h"

#include "HTMLCollection.h"
#include "RadioNodeList.h"
#include "V8Binding.h"
#include "V8Node.h"
#include "V8RadioNodeList.h"

namespace WebCore {

static v8::Handle<v8::Value> getNamedItems(HTMLFormControlsCollection* collection, const AtomicString& name, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    Vector<RefPtr<Node> > namedItems;
    collection->namedItems(name, namedItems);

    if (!namedItems.size())
        return v8Undefined();

    if (namedItems.size() == 1)
        return toV8(namedItems.at(0).release(), creationContext, isolate);

    return toV8(collection->ownerNode()->radioNodeList(name).get(), creationContext, isolate);
}

v8::Handle<v8::Value> V8HTMLFormControlsCollection::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{

    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())
        return v8Undefined();
    if (info.Holder()->HasRealNamedCallbackProperty(name))
        return v8Undefined();

    HTMLFormControlsCollection* imp = V8HTMLFormControlsCollection::toNative(info.Holder());
    return getNamedItems(imp, toWebCoreAtomicString(name), info.Holder(), info.GetIsolate());
}

v8::Handle<v8::Value> V8HTMLFormControlsCollection::namedItemCallback(const v8::Arguments& args)
{
    HTMLFormControlsCollection* imp = V8HTMLFormControlsCollection::toNative(args.Holder());
    v8::Handle<v8::Value> result = getNamedItems(imp, toWebCoreString(args[0]), args.Holder(), args.GetIsolate());

    if (result.IsEmpty())
        return v8::Undefined(args.GetIsolate());

    return result;
}

} // namespace WebCore
