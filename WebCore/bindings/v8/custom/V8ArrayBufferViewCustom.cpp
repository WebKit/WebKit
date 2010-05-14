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

#if ENABLE(3D_CANVAS)
#include "V8ArrayBufferView.h"

#include "V8Binding.h"
#include "V8Proxy.h"
#include "V8Int8Array.h"
#include "V8FloatArray.h"
#include "V8Int32Array.h"
#include "V8Int16Array.h"
#include "V8Uint8Array.h"
#include "V8Uint32Array.h"
#include "V8Uint16Array.h"

namespace WebCore {

v8::Handle<v8::Value> toV8(ArrayBufferView* impl)
{
    if (!impl)
        return v8::Null();
    if (impl->isByteArray())
        return toV8(static_cast<Int8Array*>(impl));
    if (impl->isFloatArray())
        return toV8(static_cast<FloatArray*>(impl));
    if (impl->isIntArray())
        return toV8(static_cast<Int32Array*>(impl));
    if (impl->isShortArray())
        return toV8(static_cast<Int16Array*>(impl));
    if (impl->isUnsignedByteArray())
        return toV8(static_cast<Uint8Array*>(impl));
    if (impl->isUnsignedIntArray())
        return toV8(static_cast<Uint32Array*>(impl));
    if (impl->isUnsignedShortArray())
        return toV8(static_cast<Uint16Array*>(impl));
    return v8::Handle<v8::Value>();
}

v8::Handle<v8::Value> V8ArrayBufferView::sliceCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.ArrayBufferView.slice");
    // Forms:
    // * slice(long start, long end);

    ArrayBufferView* imp = V8ArrayBufferView::toNative(args.Holder());
    int start, end;
    switch (args.Length()) {
    case 0:
        start = 0;
        end = imp->length();
        break;
    case 1:
        start = toInt32(args[0]);
        end = imp->length();
        break;
    default:
        start = toInt32(args[0]);
        end = toInt32(args[1]);
    }
    return toV8(imp->slice(start, end));
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
