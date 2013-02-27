/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#if ENABLE(INDEXED_DATABASE)
#include "V8IDBRequest.h"

#include "ExceptionCode.h"
#include "V8Binding.h"
#include "V8DOMWrapper.h"
#include "V8IDBAny.h"

namespace WebCore {

// FIXME: Remove this custom binding implementation after gathering data for: http://wkbug.com/105363
v8::Handle<v8::Value> V8IDBRequest::resultAttrGetterCustom(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    IDBRequest* imp = V8IDBRequest::toNative(info.Holder());
    ExceptionCode ec = 0;
    RefPtr<IDBAny> v = imp->result(ec);
    if (UNLIKELY(ec))
        return setDOMException(ec, info.GetIsolate());
    RefPtr<IDBAny> result = imp->result(ec);
    v8::Handle<v8::Value> wrapper = result.get() ? v8::Handle<v8::Value>(DOMDataStore::getWrapper(result.get(), info.GetIsolate())) : v8Undefined();
    if (wrapper.IsEmpty()) {
        wrapper = toV8(result.get(), info.Holder(), info.GetIsolate());
        if (!wrapper.IsEmpty()) {
            // A flaky crash has been seen in the setNamedHiddenReference call but with no reliable repro.
            // Introduce an extra call to determine if the issue lies with the holder or with the wrapper.
            // http://wkbug.com/105363
            V8HiddenPropertyName::setNamedHiddenReference(info.Holder(), "dummyAttrForDebugging", v8::String::New("foo"));
            // Also verify that the string is sane.
            if (wrapper->IsString())
                ASSERT(wrapper->ToString()->Utf8Length() >= 0);
            V8HiddenPropertyName::setNamedHiddenReference(info.Holder(), "result", wrapper);
        }
    }
    return wrapper;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
