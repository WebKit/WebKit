/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "InternalWritableStreamWriter.h"

#include "InternalWritableStream.h"
#include "JSDOMPromise.h"
#include "WebCoreJSClientData.h"
#include "WritableStream.h"

namespace WebCore {

static ExceptionOr<JSC::JSValue> invokeWritableStreamWriterFunction(JSC::JSGlobalObject& globalObject, const JSC::Identifier& identifier, const JSC::MarkedArgumentBuffer& arguments)
{
    JSC::VM& vm = globalObject.vm();
    JSC::JSLockHolder lock(vm);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto function = globalObject.get(&globalObject, identifier);
    ASSERT(!!scope.exception() || function.isCallable());
    scope.assertNoExceptionExceptTermination();
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    auto callData = JSC::getCallData(function);

    auto result = call(&globalObject, function, callData, JSC::jsUndefined(), arguments);
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    return result;
}

ExceptionOr<Ref<InternalWritableStreamWriter>> acquireWritableStreamDefaultWriter(JSDOMGlobalObject& globalObject, WritableStream& destination)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().acquireWritableStreamDefaultWriterPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(destination.internalWritableStream());

    auto result = invokeWritableStreamWriterFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return Exception { ExceptionCode::ExistingExceptionError };

    ASSERT(result.returnValue().isObject());
    return InternalWritableStreamWriter::create(globalObject, *result.returnValue().toObject(&globalObject));
}

RefPtr<DOMPromise> writableStreamDefaultWriterCloseWithErrorPropagation(InternalWritableStreamWriter& writer)
{
    auto* globalObject = writer.globalObject();
    if (!globalObject)
        return nullptr;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultWriterCloseWithErrorPropagationPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(writer.guardedObject());

    auto result = invokeWritableStreamWriterFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return nullptr;

    auto* promise = jsCast<JSC::JSPromise*>(result.returnValue());
    if (!promise)
        return nullptr;

    return DOMPromise::create(*globalObject, *promise);
}

void writableStreamDefaultWriterRelease(InternalWritableStreamWriter& writer)
{
    auto* globalObject = writer.globalObject();
    if (!globalObject || !writer.guardedObject())
        return;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultWriterReleasePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(writer.guardedObject());

    invokeWritableStreamWriterFunction(*globalObject, privateName, arguments);
}

RefPtr<DOMPromise> writableStreamDefaultWriterWrite(InternalWritableStreamWriter& writer, JSC::JSValue value)
{
    auto* globalObject = writer.globalObject();
    if (!globalObject)
        return nullptr;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultWriterWritePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(writer.guardedObject());
    arguments.append(value);

    auto result = invokeWritableStreamWriterFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return nullptr;

    auto* promise = jsCast<JSC::JSPromise*>(result.returnValue());
    if (!promise)
        return nullptr;

    return DOMPromise::create(*globalObject, *promise);
}

void InternalWritableStreamWriter::onClosedPromiseRejection(Function<void(JSDOMGlobalObject&, JSC::JSValue)>&& callback)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultWriterClosedPromisePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());

    auto result = invokeWritableStreamWriterFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return;

    auto* promise = jsCast<JSC::JSPromise*>(result.returnValue());
    if (!promise)
        return;

    Ref domPromise = DOMPromise::create(*globalObject, *promise);
    domPromise->whenSettledWithResult([callback = WTF::move(callback)](auto* globalObject, bool isFulfilled, auto result) mutable {
        if (isFulfilled || !globalObject)
            return;
        callback(*globalObject, result);
    });
}

void InternalWritableStreamWriter::onClosedPromiseResolution(Function<void()>&& callback)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultWriterClosedPromisePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());

    auto result = invokeWritableStreamWriterFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return;

    auto* promise = jsCast<JSC::JSPromise*>(result.returnValue());
    if (!promise)
        return;

    Ref domPromise = DOMPromise::create(*globalObject, *promise);
    domPromise->whenSettledWithResult([callback = WTF::move(callback)](auto*, bool isFulfilled, auto) mutable {
        if (!isFulfilled)
            return;
        callback();
    });
}

static int writableStreamDefaultWriterGetDesiredSize(InternalWritableStreamWriter& writer)
{
    auto* globalObject = writer.globalObject();
    if (!globalObject)
        return 0;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultWriterGetDesiredSizePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(writer.guardedObject());

    auto result = invokeWritableStreamWriterFunction(*globalObject, privateName, arguments);
    return result.returnValue().toNumber(globalObject);
}

void InternalWritableStreamWriter::whenReady(Function<void (bool)>&& callback)
{
    if (writableStreamDefaultWriterGetDesiredSize(*this) > 0) {
        callback(true);
        return;
    }

    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultWriterReadyPromisePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());

    auto result = invokeWritableStreamWriterFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return;

    auto* promise = jsCast<JSC::JSPromise*>(result.returnValue());
    if (!promise)
        return;

    Ref domPromise = DOMPromise::create(*globalObject, *promise);
    domPromise->whenSettledWithResult([callback = WTF::move(callback)](auto*, bool isFulfilled, auto) mutable {
        callback(isFulfilled);
    });
}

}
