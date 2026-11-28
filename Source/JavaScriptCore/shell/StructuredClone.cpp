/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "StructuredClone.h"

#include "IteratorOperations.h"
#include "JSArrayBuffer.h"
#include "JSCInlines.h"

namespace JSCShell {

using namespace JSC;

EncodedJSValue throwSerializationError(JSGlobalObject* globalObject, ThrowScope& scope, SerializationReturnCode code)
{
    switch (code) {
    case SerializationReturnCode::SuccessfullyCompleted:
        RELEASE_ASSERT_NOT_REACHED();
    case SerializationReturnCode::StackOverflowError:
        return throwVMException(globalObject, scope, createStackOverflowError(globalObject));
    case SerializationReturnCode::ValidationError:
    case SerializationReturnCode::DataCloneError:
        return throwVMTypeError(globalObject, scope, "Value could not be cloned."_s);
    case SerializationReturnCode::ExistingExceptionError:
        ASSERT(scope.exception());
        return encodedJSValue();
    case SerializationReturnCode::InterruptedExecutionError:
    case SerializationReturnCode::UnspecifiedError:
        break;
    }
    return throwVMException(globalObject, scope, createError(globalObject, "Value could not be cloned."_s));
}

RefPtr<Message> serializeMessage(JSGlobalObject* globalObject, JSValue value, JSValue transfer, ASCIILiteral apiName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    MarkedArgumentBuffer transferredObjects;
    Vector<Ref<ArrayBuffer>> transferredBuffers;

    if (!transfer.isUndefined()) {
        HashSet<ArrayBuffer*> visited;
        forEachInIterable(globalObject, transfer, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            RefPtr arrayBuffer = toPossiblySharedArrayBuffer(vm, value);
            if (!arrayBuffer) {
                throwTypeError(globalObject, scope, makeString(apiName, " transfer list can only contain ArrayBuffers"_s));
                return;
            }
            if (!visited.add(arrayBuffer.get()).isNewEntry) {
                throwTypeError(globalObject, scope, "Duplicate transferable for structured clone"_s);
                return;
            }
            transferredObjects.appendWithCrashOnOverflow(value);
            transferredBuffers.append(arrayBuffer.releaseNonNull());
        });
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    Vector<uint8_t> data;
    auto [code, sideChannels] = JSCCloneSerializer::serialize(globalObject, value, data, ArgList(transferredObjects));
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (code != SerializationReturnCode::SuccessfullyCompleted) {
        throwSerializationError(globalObject, scope, code);
        return nullptr;
    }

    for (auto& arrayBuffer : transferredBuffers) {
        if (arrayBuffer->isDetached() || arrayBuffer->isShared() || !arrayBuffer->isDetachable()) {
            throwSerializationError(globalObject, scope, SerializationReturnCode::DataCloneError);
            return nullptr;
        }
    }

    ArrayBufferContentsArray transferredContents(transferredBuffers.size());
    for (size_t i = 0; i < transferredBuffers.size(); ++i) {
        if (!protect(transferredBuffers[i])->transferTo(vm, transferredContents[i])) {
            throwSerializationError(globalObject, scope, SerializationReturnCode::DataCloneError);
            return nullptr;
        }
    }

    return Message::create(WTF::move(data), WTF::move(sideChannels), 0, WTF::move(transferredContents));
}

} // namespace JSCShell
