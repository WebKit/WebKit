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

#if ENABLE(3D_CANVAS) || ENABLE(BLOB)

#include "ArrayBuffer.h"

#include "ExceptionCode.h"
#include "V8Binding.h"
#include "V8ArrayBuffer.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8ArrayBuffer::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.ArrayBuffer.Constructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.", V8Proxy::SyntaxError);

    // If we return a previously constructed ArrayBuffer,
    // e.g. from the call to ArrayBufferView.buffer, this code is called
    // with a zero-length argument list. The V8DOMWrapper will then
    // set the internal pointer in the newly-created object.
    // Unfortunately it doesn't look like it's possible to distinguish
    // between this case and that where the user calls "new
    // ArrayBuffer()" from JavaScript. To guard against problems,
    // we always create at least a zero-length ArrayBuffer, even
    // if it is immediately overwritten by the V8DOMWrapper.

    // Supported constructors:
    // ArrayBuffer(n) where n is an integer:
    //   -- create an empty buffer of n bytes

    int argLength = args.Length();
    int length = 0;
    if (argLength > 0)
        length = toInt32(args[0]); // NaN/+inf/-inf returns 0, this is intended by WebIDL
    RefPtr<ArrayBuffer> buffer;
    if (length >= 0)
        buffer = ArrayBuffer::create(static_cast<unsigned>(length), 1);
    if (!buffer.get())
        return throwError("ArrayBuffer size is not a small enough positive integer.", V8Proxy::RangeError);
    // Transform the holder into a wrapper object for the array.
    V8DOMWrapper::setDOMWrapper(args.Holder(), &info, buffer.get());
    return toV8(buffer.release(), args.Holder());
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS) || ENABLE(BLOB)
