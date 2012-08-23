/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "Blob.h"

#include "Dictionary.h"
#include "V8ArrayBuffer.h"
#include "V8ArrayBufferView.h"
#include "V8Binding.h"
#include "V8Blob.h"
#include "V8File.h"
#include "V8Utilities.h"
#include "WebKitBlobBuilder.h"
#include <wtf/RefPtr.h>

namespace WebCore {

v8::Handle<v8::Value> toV8(Blob* impl, v8::Isolate* isolate)
{
    if (!impl)
        return v8NullWithCheck(isolate);

    if (impl->isFile())
        return toV8(toFile(impl), isolate);

    return V8Blob::wrap(impl, isolate);
}

v8::Handle<v8::Value> V8Blob::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Blob.Constructor");

    if (!args.IsConstructCall())
        return throwTypeError("DOM object constructor cannot be called as a function.", args.GetIsolate());

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

    // Get the script execution context.
    ScriptExecutionContext* context = getScriptExecutionContext();
    if (!context)
        return throwError(ReferenceError, "Blob constructor associated document is unavailable", args.GetIsolate());

    if (!args.Length()) {
        RefPtr<Blob> blob = Blob::create();
        return toV8(blob.get(), args.GetIsolate());
    }

    v8::Local<v8::Value> firstArg = args[0];
    if (!firstArg->IsArray())
        return throwTypeError("First argument of the constructor is not of type Array", args.GetIsolate());

    String type;
    String endings = "transparent";

    if (args.Length() > 1) {
        if (!args[1]->IsObject())
            return throwTypeError("Second argument of the constructor is not of type Object", args.GetIsolate());

        EXCEPTION_BLOCK(Dictionary, dictionary, Dictionary(args[1], args.GetIsolate()));

        v8::TryCatch tryCatchEndings;
        bool containsEndings = dictionary.get("endings", endings);
        if (tryCatchEndings.HasCaught())
            return throwError(tryCatchEndings.Exception(), args.GetIsolate());

        if (containsEndings) {
            if (endings != "transparent" && endings != "native")
                return throwTypeError("The endings property must be either \"transparent\" or \"native\"", args.GetIsolate());
        }

        v8::TryCatch tryCatchType;
        dictionary.get("type", type);
        if (tryCatchType.HasCaught())
            return throwError(tryCatchType.Exception(), args.GetIsolate());
        if (!type.containsOnlyASCII())
            return throwError(SyntaxError, "type must consist of ASCII characters", args.GetIsolate());
        type.makeLower();
    }

    ASSERT(endings == "transparent" || endings == "native");

    RefPtr<WebKitBlobBuilder> blobBuilder = WebKitBlobBuilder::create();

    EXCEPTION_BLOCK(v8::Local<v8::Array>, blobParts, v8::Local<v8::Array>::Cast(firstArg));
    uint32_t length = blobParts->Length();

    for (uint32_t i = 0; i < length; ++i) {
        v8::Local<v8::Value> item = blobParts->Get(v8::Uint32::New(i));
        ASSERT(!item.IsEmpty());
#if ENABLE(BLOB)
        if (V8ArrayBuffer::HasInstance(item)) {
            ArrayBuffer* arrayBuffer = V8ArrayBuffer::toNative(v8::Handle<v8::Object>::Cast(item));
            ASSERT(arrayBuffer);
            blobBuilder->append(context, arrayBuffer);
        } else if (V8ArrayBufferView::HasInstance(item)) {
            ArrayBufferView* arrayBufferView = V8ArrayBufferView::toNative(v8::Handle<v8::Object>::Cast(item));
            ASSERT(arrayBufferView);
            blobBuilder->append(arrayBufferView);
        } else
#endif
        if (V8Blob::HasInstance(item)) {
            Blob* blob = V8Blob::toNative(v8::Handle<v8::Object>::Cast(item));
            ASSERT(blob);
            blobBuilder->append(blob);
        } else {
            EXCEPTION_BLOCK(String, stringValue, toWebCoreString(item));
            blobBuilder->append(stringValue, endings, ASSERT_NO_EXCEPTION);
        }
    }

    RefPtr<Blob> blob = blobBuilder->getBlob(type, BlobConstructedByConstructor);
    return toV8(blob.get(), args.GetIsolate());
}

} // namespace WebCore
