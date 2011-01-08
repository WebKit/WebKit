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
#include "V8HTMLOptionsCollection.h"

#include "HTMLOptionsCollection.h"
#include "HTMLOptionElement.h"
#include "ExceptionCode.h"

#include "V8Binding.h"
#include "V8Collection.h"
#include "V8HTMLOptionElement.h"
#include "V8HTMLSelectElementCustom.h"
#include "V8Node.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8HTMLOptionsCollection::removeCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLOptionsCollection.remove()");
    HTMLOptionsCollection* imp = V8HTMLOptionsCollection::toNative(args.Holder());
    HTMLSelectElement* base = static_cast<HTMLSelectElement*>(imp->base());
    return removeElement(base, args);
}

v8::Handle<v8::Value> V8HTMLOptionsCollection::addCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLOptionsCollection.add()");
    if (!V8HTMLOptionElement::HasInstance(args[0])) {
        V8Proxy::setDOMException(TYPE_MISMATCH_ERR);
        return v8::Undefined();
    }
    HTMLOptionsCollection* imp = V8HTMLOptionsCollection::toNative(args.Holder());
    HTMLOptionElement* option = V8HTMLOptionElement::toNative(v8::Handle<v8::Object>(v8::Handle<v8::Object>::Cast(args[0])));

    ExceptionCode ec = 0;
    if (args.Length() < 2)
        imp->add(option, ec);
    else {
        bool ok;
        v8::TryCatch try_catch;
        int index = toInt32(args[1], ok);

        if (try_catch.HasCaught())
            return v8::Undefined();

        if (!ok)
            ec = TYPE_MISMATCH_ERR;
        else
            imp->add(option, index, ec);
    }

    if (ec != 0)
        V8Proxy::setDOMException(ec);

    return v8::Undefined();
}

v8::Handle<v8::Value> V8HTMLOptionsCollection::lengthAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLOptionsCollection.length._get");
    HTMLOptionsCollection* imp = V8HTMLOptionsCollection::toNative(info.Holder());
    int v = imp->length();
    return v8::Integer::New(v);
}

void V8HTMLOptionsCollection::lengthAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLOptionsCollection.length._set");
    HTMLOptionsCollection* imp = V8HTMLOptionsCollection::toNative(info.Holder());
    double v = value->NumberValue();
    unsigned newLength = 0;
    ExceptionCode ec = 0;
    if (!isnan(v) && !isinf(v)) {
        if (v < 0.0)
            ec = INDEX_SIZE_ERR;
        else if (v > static_cast<double>(UINT_MAX))
            newLength = UINT_MAX;
        else
            newLength = static_cast<unsigned>(v);
    }
    if (!ec)
        imp->setLength(value->Uint32Value(), ec);

    V8Proxy::setDOMException(ec);
}

v8::Handle<v8::Value> V8HTMLOptionsCollection::indexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLOptionsCollection.IndexedPropertyGetter");
    HTMLOptionsCollection* collection = V8HTMLOptionsCollection::toNative(info.Holder());

    RefPtr<Node> result = collection->item(index);
    if (!result)
        return notHandledByInterceptor();

    return toV8(result.release());
}

v8::Handle<v8::Value> V8HTMLOptionsCollection::indexedPropertySetter(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLOptionsCollection.IndexedPropertySetter");
    HTMLOptionsCollection* collection = V8HTMLOptionsCollection::toNative(info.Holder());
    HTMLSelectElement* base = static_cast<HTMLSelectElement*>(collection->base());
    return toOptionsCollectionSetter(index, value, base);
}

} // namespace WebCore
