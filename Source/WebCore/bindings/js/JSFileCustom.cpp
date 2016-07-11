/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSFile.h"

#include "JSDOMBinding.h"
#include "JSDictionary.h"
#include "WebKitBlobBuilder.h"
#include <runtime/Error.h>
#include <runtime/JSArray.h>
#include <runtime/JSArrayBuffer.h>
#include <runtime/JSArrayBufferView.h>
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>

using namespace JSC;

namespace WebCore {

EncodedJSValue JSC_HOST_CALL constructJSFile(ExecState& exec)
{
    auto* constructor = jsCast<DOMConstructorObject*>(exec.callee());
    ScriptExecutionContext* context = constructor->scriptExecutionContext();
    if (!context)
        return throwVMError(&exec, createReferenceError(&exec, "File constructor associated document is unavailable"));

    JSValue arg = exec.argument(0);
    if (arg.isUndefinedOrNull())
        return throwVMTypeError(&exec, ASCIILiteral("First argument to File constructor must be a valid sequence, was undefined or null"));

    unsigned blobPartsLength = 0;
    JSObject* blobParts = toJSSequence(exec, arg, blobPartsLength);
    if (exec.hadException())
        return JSValue::encode(jsUndefined());
    ASSERT(blobParts);

    arg = exec.argument(1);
    if (arg.isUndefined())
        return throwVMTypeError(&exec, ASCIILiteral("Second argument to File constructor must be a valid string, was undefined"));

    String filename = arg.toWTFString(&exec).replace('/', ':');
    if (exec.hadException())
        return JSValue::encode(jsUndefined());

    String normalizedType;
    Optional<int64_t> lastModified;

    arg = exec.argument(2);
    if (!arg.isUndefinedOrNull()) {
        JSObject* filePropertyBagObject = arg.getObject();
        if (!filePropertyBagObject)
            return throwVMTypeError(&exec, ASCIILiteral("Third argument of the constructor is not of type Object"));

        // Create the dictionary wrapper from the initializer object.
        JSDictionary dictionary(&exec, filePropertyBagObject);

        // Attempt to get the type property.
        String type;
        dictionary.get("type", type);
        if (exec.hadException())
            return JSValue::encode(jsUndefined());

        normalizedType = Blob::normalizedContentType(type);

        // Only try to parse the lastModified date if there was not an invalid type argument.
        if (type.isEmpty() ||  !normalizedType.isEmpty()) {
            dictionary.get("lastModified", lastModified);
            if (exec.hadException())
                return JSValue::encode(jsUndefined());
        }
    }

    if (!lastModified)
        lastModified = currentTimeMS();

    BlobBuilder blobBuilder;

    for (unsigned i = 0; i < blobPartsLength; ++i) {
        JSValue item = blobParts->get(&exec, i);
        if (exec.hadException())
            return JSValue::encode(jsUndefined());

        if (ArrayBuffer* arrayBuffer = toArrayBuffer(item))
            blobBuilder.append(arrayBuffer);
        else if (RefPtr<ArrayBufferView> arrayBufferView = toArrayBufferView(item))
            blobBuilder.append(WTFMove(arrayBufferView));
        else if (Blob* blob = JSBlob::toWrapped(item))
            blobBuilder.append(blob);
        else {
            String string = item.toWTFString(&exec);
            if (exec.hadException())
                return JSValue::encode(jsUndefined());
            blobBuilder.append(string, ASCIILiteral("transparent"));
        }
    }

    auto file = File::create(blobBuilder.finalize(), filename, normalizedType, lastModified.value());
    return JSValue::encode(CREATE_DOM_WRAPPER(constructor->globalObject(), File, WTFMove(file)));
}

} // namespace WebCore

