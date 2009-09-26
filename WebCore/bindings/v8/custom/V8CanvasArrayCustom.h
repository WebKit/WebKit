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

#include "CanvasArrayBuffer.h"

#include "V8Binding.h"
#include "V8CanvasArrayBuffer.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

namespace WebCore {

    // Template function used by the CanvasArray*Constructor callbacks.
    template<class ArrayClass>
    v8::Handle<v8::Value> constructCanvasArray(const v8::Arguments& args,
                                               int classIndex)
    {
        if (!args.IsConstructCall())
            return throwError("DOM object constructor cannot be called as a function.");

        int argLen = args.Length();
        // Supported constructors:
        // Canvas<T>Array(n) where n is an integer:
        //   -- create an empty array of n elements
        // Canvas<T>Array(arr) where arr is an array:
        //   -- create a Canvas<T>Array containing the contents of "arr"
        // Canvas<T>Array(buf, offset, length)
        //   -- create a Canvas<T>Array pointing to the CanvasArrayBuffer
        //      "buf", starting at the specified offset, for the given
        //      length

        if (argLen == 0)
            return throwError("No arguments specified to constructor");

        // See whether the first argument is a CanvasArrayBuffer.
        if (V8CanvasArrayBuffer::HasInstance(args[0])) {
            if (argLen > 3)
                return throwError("Wrong number of arguments to new Canvas<T>Array(CanvasArrayBuffer, int, int)");

            CanvasArrayBuffer* buf =
                V8DOMWrapper::convertToNativeObject<CanvasArrayBuffer>(V8ClassIndex::CANVASARRAYBUFFER,
                                                                       args[0]->ToObject());
            if (buf == NULL)
                return throwError("Could not convert argument 0 to a CanvasArrayBuffer");
            bool ok;
            int offset = 0;
            if (argLen > 1) {
                offset = toInt32(args[1], ok);
                if (!ok)
                    return throwError("Could not convert argument 1 to an integer");
            }
            int length = buf->byteLength() - offset;
            if (argLen > 2) {
                length = toInt32(args[2], ok);
                if (!ok)
                    return throwError("Could not convert argument 2 to an integer");
            }
            if (length < 0)
                return throwError("Length / offset out of range");

            RefPtr<ArrayClass> array = ArrayClass::create(buf, offset, length);
            if (array == NULL)
                return throwError("Invalid arguments to new Canvas<T>Array(CanvasArrayBuffer, int, int)");
            // Transform the holder into a wrapper object for the array.
            V8DOMWrapper::setDOMWrapper(args.Holder(), classIndex, array.get());
            return toV8(array.release(), args.Holder());
        }

        int len = 0;
        v8::Handle<v8::Array> srcArray;
        if (argLen != 1)
            return throwError("Wrong number of arguments to new Canvas<T>Array(int / array)");

        if (args[0]->IsInt32()) {
            len = toInt32(args[0]);
        } else if (args[0]->IsArray()) {
            srcArray = v8::Local<v8::Array>::Cast(args[0]);
            if (srcArray.IsEmpty())
                return throwError("Could not convert argument 0 to an array");
            len = srcArray->Length();
        } else
            return throwError("Could not convert argument 0 to either an int32 or an array");

        RefPtr<ArrayClass> array = ArrayClass::create(len);
        if (!srcArray.IsEmpty()) {
            // Need to copy the incoming array into the newly created CanvasArray.
            for (int i = 0; i < len; i++) {
                v8::Local<v8::Value> val = srcArray->Get(v8::Integer::New(i));
                if (!val->IsNumber()) {
                    char buf[256];
                    sprintf(buf, "Could not convert array element %d to a number", i);
                    return throwError(buf);
                }
                array->set(i, val->NumberValue());
            }
        }

        // Transform the holder into a wrapper object for the array.
        V8DOMWrapper::setDOMWrapper(args.Holder(), classIndex, array.get());
        return toV8(array.release(), args.Holder());
    }

}

#endif // ENABLE(3D_CANVAS)
