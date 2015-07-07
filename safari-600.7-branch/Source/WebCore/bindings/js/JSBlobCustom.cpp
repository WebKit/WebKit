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
#include "JSBlob.h"

#include "Blob.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "JSDOMBinding.h"
#include "JSDictionary.h"
#include "JSFile.h"
#include "WebKitBlobBuilder.h"
#include <runtime/Error.h>
#include <runtime/JSArray.h>
#include <runtime/JSArrayBuffer.h>
#include <runtime/JSArrayBufferView.h>
#include <wtf/Assertions.h>

using namespace JSC;

namespace WebCore {

JSValue toJS(ExecState*, JSDOMGlobalObject* globalObject, Blob* blob)
{
    if (!blob)
        return jsNull();

    if (blob->isFile())
        return wrap<JSFile>(globalObject, static_cast<File*>(blob));

    return wrap<JSBlob>(globalObject, blob);
}

EncodedJSValue JSC_HOST_CALL constructJSBlob(ExecState* exec)
{
    DOMConstructorObject* jsConstructor = jsCast<DOMConstructorObject*>(exec->callee());
    ScriptExecutionContext* context = jsConstructor->scriptExecutionContext();
    if (!context)
        return throwVMError(exec, createReferenceError(exec, "Blob constructor associated document is unavailable"));

    if (!exec->argumentCount()) {
        RefPtr<Blob> blob = Blob::create();
        return JSValue::encode(CREATE_DOM_WRAPPER(jsConstructor->globalObject(), Blob, blob.get()));
    }

    unsigned blobPartsLength = 0;
    JSObject* blobParts = toJSSequence(exec, exec->argument(0), blobPartsLength);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    ASSERT(blobParts);

    String type;
    String endings = ASCIILiteral("transparent");

    if (exec->argumentCount() > 1) {
        JSValue blobPropertyBagValue = exec->argument(1);

        if (!blobPropertyBagValue.isObject())
            return throwVMError(exec, createTypeError(exec, "Second argument of the constructor is not of type Object"));

        // Given the above test, this will always yield an object.
        JSObject* blobPropertyBagObject = blobPropertyBagValue.toObject(exec);

        // Create the dictionary wrapper from the initializer object.
        JSDictionary dictionary(exec, blobPropertyBagObject);

        // Attempt to get the endings property and validate it.
        bool containsEndings = dictionary.get("endings", endings);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        if (containsEndings) {
            if (endings != "transparent" && endings != "native")
                return throwVMError(exec, createTypeError(exec, "The endings property must be either \"transparent\" or \"native\""));
        }

        // Attempt to get the type property.
        dictionary.get("type", type);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    ASSERT(endings == "transparent" || endings == "native");

    BlobBuilder blobBuilder;

    for (unsigned i = 0; i < blobPartsLength; ++i) {
        JSValue item = blobParts->get(exec, i);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        if (ArrayBuffer* arrayBuffer = toArrayBuffer(item))
            blobBuilder.append(arrayBuffer);
        else if (RefPtr<ArrayBufferView> arrayBufferView = toArrayBufferView(item))
            blobBuilder.append(arrayBufferView.release());
        else if (Blob* blob = toBlob(item))
            blobBuilder.append(blob);
        else {
            String string = item.toString(exec)->value(exec);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
            blobBuilder.append(string, endings);
        }
    }

    RefPtr<Blob> blob = Blob::create(blobBuilder.finalize(), Blob::normalizedContentType(type));

    return JSValue::encode(CREATE_DOM_WRAPPER(jsConstructor->globalObject(), Blob, blob.get()));
}

} // namespace WebCore
