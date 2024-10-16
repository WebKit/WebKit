/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "JSReadableStream.h"
#include "WebCoreJSClientData.h"

namespace WebCore {

static ExceptionOr<JSC::JSValue> invokeReadableStreamDefaultReaderFunction(JSC::JSGlobalObject& globalObject, const JSC::Identifier& identifier, const JSC::MarkedArgumentBuffer& arguments)
{
    Ref vm = globalObject.vm();
    JSC::JSLockHolder lock(vm.get());

    auto scope = DECLARE_CATCH_SCOPE(vm.get());

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
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().createInternalReadableStreamDefaultReaderPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(stream);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamDefaultReaderFunction(globalObject, privateName, arguments);
    if (UNLIKELY(result.hasException()))
        return result.releaseException();

    ASSERT(result.returnValue().isObject());
    return adoptRef(*new InternalReadableStreamDefaultReader(globalObject, *result.returnValue().toObject(&globalObject)));
}

ExceptionOr<void> InternalReadableStreamDefaultReader::releaseLock()
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return Exception { ExceptionCode::InvalidStateError };

    auto* clientData = static_cast<JSVMClientData*>(globalObject->vm().clientData);
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
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
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
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
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
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
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

}
