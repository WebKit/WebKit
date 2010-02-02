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
    String key = toWebCoreString(name);

    // Length property cannot be overridden.
    DEFINE_STATIC_LOCAL(const AtomicString, length, ("length"));
    if (key == length)
        return v8::Integer::New(list->length());

    RefPtr<Node> result = list->itemWithName(key);
    if (!result)
        return notHandledByInterceptor();

    return toV8(result.release());
}

// Need to support call so that list(0) works.
v8::Handle<v8::Value> V8NodeList::callAsFunctionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.NodeList.callAsFunction()");
    if (args.Length() < 1)
        return v8::Undefined();

    NodeList* list = V8NodeList::toNative(args.Holder());

    // The first argument must be a number.
    v8::Local<v8::Uint32> index = args[0]->ToArrayIndex();
    if (index.IsEmpty())
        return v8::Undefined();

    RefPtr<Node> result = list->item(index->Uint32Value());
    return toV8(result.release());
}

} // namespace WebCore
