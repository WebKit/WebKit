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
#include "V8NamedNodeMap.h"

#include "BindingState.h"
#include "NamedNodeMap.h"
#include "V8Attr.h"
#include "V8Binding.h"
#include "V8Element.h"
#include "V8Node.h"

#include <wtf/RefPtr.h>

namespace WebCore {

v8::Handle<v8::Value> V8NamedNodeMap::indexedPropertyGetter(uint32_t index, const v8::AccessorInfo& info)
{
    NamedNodeMap* imp = V8NamedNodeMap::toNative(info.Holder());
    RefPtr<Node> result = imp->item(index);
    if (!result)
        return v8Undefined();

    return toV8(result.release(), info.Holder(), info.GetIsolate());
}

v8::Handle<v8::Value> V8NamedNodeMap::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())
        return v8Undefined();
    if (info.Holder()->HasRealNamedCallbackProperty(name))
        return v8Undefined();

    NamedNodeMap* imp = V8NamedNodeMap::toNative(info.Holder());
    RefPtr<Node> result = imp->getNamedItem(toWebCoreString(name));
    if (!result)
        return v8Undefined();

    return toV8(result.release(), info.Holder(), info.GetIsolate());
}

} // namespace WebCore
