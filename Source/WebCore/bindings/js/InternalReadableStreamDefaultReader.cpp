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
 * THIS SOFTWARE IS PROVIDED BY CANON INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CANON INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InternalReadableStreamDefaultReader.h"

#include "JSDOMPromise.h"
#include "JSReadableStream.h"
#include "WebCoreJSClientData.h"

namespace WebCore {

static ExceptionOr<JSC::JSValue> invokeReadableStreamDefaultReaderFunction(JSC::JSGlobalObject& globalObject, const JSC::Identifier& identifier, const JSC::MarkedArgumentBuffer& arguments)
{
    Ref vm = globalObject.vm();
    JSC::JSLockHolder lock(vm.get());

    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm.get());

    auto function = globalObject.get(&globalObject, identifier);
    ASSERT(!!scope.exception() || function.isCallable());
    scope.assertNoExceptionExceptTermination();
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    auto callData = JSC::getCallData(function);

    auto result = call(&globalObject, function, callData, JSC::jsUndefined(), arguments);
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    return result;
}

ExceptionOr<Ref<InternalReadableStreamDefaultReader>> InternalReadableStreamDefaultReader::create(JSDOMGlobalObject& globalObject, InternalReadableStream& stream)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().createInternalReadableStreamDefaultReaderPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(stream);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamDefaultReaderFunction(globalObject, privateName, arguments);
    if (result.hasException())  [[unlikely]]
        return result.releaseException();

    ASSERT(result.returnValue().isObject());
    return adoptRef(*new InternalReadableStreamDefaultReader(globalObject, *result.returnValue().toObject(&globalObject)));
}

ExceptionOr<void> InternalReadableStreamDefaultReader::releaseLock()
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return Exception { ExceptionCode::InvalidStateError };

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamDefaultReaderReleaseLockForBindingsPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());

    auto result = invokeReadableStreamDefaultReaderFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return Exception { ExceptionCode::ExistingExceptionError };

    return { };
}

JSC::JSValue InternalReadableStreamDefaultReader::readForBindings(JSC::JSGlobalObject& globalObject)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamDefaultReaderReadForBindingsPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());

    auto result = invokeReadableStreamDefaultReaderFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalReadableStreamDefaultReader::closedForBindings(JSC::JSGlobalObject& globalObject)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamDefaultReaderClosedForBindingsPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());

    auto result = invokeReadableStreamDefaultReaderFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalReadableStreamDefaultReader::cancelForBindings(JSC::JSGlobalObject& globalObject, JSC::JSValue reason)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& names = clientData->builtinFunctions().readableStreamInternalsBuiltins();
    auto& privateName = names.readableStreamDefaultReaderCancelForBindingsPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(reason);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamDefaultReaderFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

void InternalReadableStreamDefaultReader::onClosedPromiseRejection(Function<void(JSDOMGlobalObject&, JSC::JSValue)>&& callback)
{
    // FIXME: We should register only one settlement handler for rejection and resolution.
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& names = clientData->builtinFunctions().readableStreamInternalsBuiltins();
    auto& privateName = names.readableStreamDefaultReaderClosedPromisePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());

    auto result = invokeReadableStreamDefaultReaderFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return;

    auto* promise = jsCast<JSC::JSPromise*>(result.returnValue());
    if (!promise)
        return;

    Ref domPromise = DOMPromise::create(*globalObject, *promise);
    domPromise->whenSettledWithResult([callback = WTF::move(callback)](auto* globalObject, bool isFulfilled, auto result) {
        if (isFulfilled || !globalObject)
            return;
        callback(*globalObject, result);
    });
}

void InternalReadableStreamDefaultReader::onClosedPromiseResolution(Function<void()>&& callback)
{
    // FIXME: We should register only one settlement handler for rejection and resolution.
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& names = clientData->builtinFunctions().readableStreamInternalsBuiltins();
    auto& privateName = names.readableStreamDefaultReaderClosedPromisePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());

    auto result = invokeReadableStreamDefaultReaderFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return;

    auto* promise = jsCast<JSC::JSPromise*>(result.returnValue());
    if (!promise)
        return;

    Ref domPromise = DOMPromise::create(*globalObject, *promise);
    domPromise->whenSettledWithResult([callback = WTF::move(callback)](auto*, bool isFulfilled, auto) {
        if (isFulfilled)
            callback();
    });
}

}
