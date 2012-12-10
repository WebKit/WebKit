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
#include "V8HTMLCollection.h"

#include "HTMLCollection.h"
#include "HTMLPropertiesCollection.h"
#include "PropertyNodeList.h"
#include "V8Binding.h"
#include "V8HTMLAllCollection.h"
#include "V8HTMLFormControlsCollection.h"
#include "V8HTMLOptionsCollection.h"
#include "V8NamedNodesCollection.h"
#include "V8Node.h"

namespace WebCore {

v8::Handle<v8::Value> V8HTMLCollection::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.HTMLCollection.NamedPropertyGetter");

    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())
        return v8Undefined();
    if (info.Holder()->HasRealNamedCallbackProperty(name))
        return v8Undefined();

    HTMLCollection* imp = V8HTMLCollection::toNative(info.Holder());
#if ENABLE(MICRODATA)
    if (imp->type() == ItemProperties) {
        PropertyNodeList* item = static_cast<HTMLPropertiesCollection*>(imp)->propertyNodeList(toWebCoreAtomicString(name));
        if (!item)
            return v8Undefined();
        return toV8(item, info.Holder(), info.GetIsolate());
    }
#endif
    Node* item = imp->namedItem(toWebCoreAtomicString(name));
    if (!item)
        return v8Undefined();
    return toV8(item, info.Holder(), info.GetIsolate());
}

v8::Handle<v8::Object> wrap(HTMLCollection* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    switch (impl->type()) {
    case FormControls:
        return wrap(static_cast<HTMLFormControlsCollection*>(impl), creationContext, isolate);
    case SelectOptions:
        return wrap(static_cast<HTMLOptionsCollection*>(impl), creationContext, isolate);
    case DocAll:
        return wrap(static_cast<HTMLAllCollection*>(impl), creationContext, isolate);
    default:
        break;
    }

    return V8HTMLCollection::createWrapper(impl, creationContext, isolate);
}

} // namespace WebCore
