/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "InternalReadableStream.h"

#include "JSDOMConvertObject.h"
#include "JSDOMConvertSequences.h"
#include "JSDOMException.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSObjectInlines.h>

namespace WebCore {

static ExceptionOr<JSC::JSValue> invokeReadableStreamFunction(JSC::JSGlobalObject& globalObject, const JSC::Identifier& identifier, const JSC::MarkedArgumentBuffer& arguments)
{
    JSC::VM& vm = globalObject.vm();
    JSC::JSLockHolder lock(vm);

    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    auto function = globalObject.get(&globalObject, identifier);
    ASSERT(!!scope.exception() || function.isCallable());
    scope.assertNoExceptionExceptTermination();
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    auto callData = JSC::getCallData(function);

    auto result = call(&globalObject, function, callData, JSC::jsUndefined(), arguments);
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    return result;
}

ExceptionOr<Ref<InternalReadableStream>> InternalReadableStream::createFromUnderlyingSource(JSDOMGlobalObject& globalObject, JSC::JSValue underlyingSource, JSC::JSValue strategy, std::optional<double> highWaterMark)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().createInternalReadableStreamFromUnderlyingSourcePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(underlyingSource);
    arguments.append(strategy);
    if (highWaterMark)
        arguments.append(JSC::jsNumber(*highWaterMark));
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException()) [[unlikely]]
        return result.releaseException();

    ASSERT(result.returnValue().isObject());
    return adoptRef(*new InternalReadableStream(globalObject, *result.returnValue().toObject(&globalObject)));
}

Ref<InternalReadableStream> InternalReadableStream::fromObject(JSDOMGlobalObject& globalObject, JSC::JSObject& object)
{
    return adoptRef(*new InternalReadableStream(globalObject, object));
}

bool InternalReadableStream::isLocked() const
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return false;

    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().isReadableStreamLockedPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(*globalObject, privateName, arguments);
    TRY_CLEAR_EXCEPTION(scope, false);

    return result.hasException() ? false : result.returnValue().isTrue();
}

bool InternalReadableStream::isDisturbed() const
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return false;

    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().isReadableStreamDisturbedPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(*globalObject, privateName, arguments);
    TRY_CLEAR_EXCEPTION(scope, false);

    return result.hasException() ? false : result.returnValue().isTrue();
}

InternalReadableStream::State InternalReadableStream::state() const
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return State::Errored;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamStatePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return State::Errored;

    // Values must match @streamReadable, @streamClosed, @streamErrored in JSDOMGlobalObject.cpp.
    double state = result.returnValue().toNumber(globalObject);
    if (state == 1)
        return State::Closed;
    if (state == 4)
        return State::Readable;

    ASSERT(state == 3);
    return State::Errored;
}

JSC::JSValue InternalReadableStream::storedError(JSDOMGlobalObject& globalObject) const
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamStoredErrorPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(globalObject, privateName, arguments);
    return result.hasException() ? JSC::jsUndefined() : result.releaseReturnValue();
}

void InternalReadableStream::cancel(Exception&& exception)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    JSC::JSLockHolder lock(globalObject->vm());
    cancel(*globalObject, toJSNewlyCreated(globalObject, JSC::jsCast<JSDOMGlobalObject*>(globalObject), DOMException::create(WTF::move(exception))));
    TRY_CLEAR_EXCEPTION(scope, void());
}

void InternalReadableStream::lock()
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().acquireReadableStreamDefaultReaderPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    invokeReadableStreamFunction(*globalObject, privateName, arguments);
    TRY_CLEAR_EXCEPTION(scope, void());
}

ExceptionOr<std::pair<Ref<InternalReadableStream>, Ref<InternalReadableStream>>> InternalReadableStream::tee(bool shouldClone)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return Exception { ExceptionCode::InvalidStateError };

    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto result = tee(*globalObject, shouldClone);
    if (scope.exception()) [[unlikely]]
        return Exception { ExceptionCode::ExistingExceptionError };

    auto resultsConversionResult = convert<IDLSequence<IDLObject>>(*globalObject, result);
    if (resultsConversionResult.hasException(scope)) [[unlikely]]
        return Exception { ExceptionCode::ExistingExceptionError };

    auto results = resultsConversionResult.releaseReturnValue();
    ASSERT(results.size() == 2);

    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    return std::make_pair(InternalReadableStream::fromObject(jsDOMGlobalObject, *results[0].get()), InternalReadableStream::fromObject(jsDOMGlobalObject, *results[1].get()));
}

JSC::JSValue InternalReadableStream::cancel(JSC::JSGlobalObject& globalObject, JSC::JSValue reason)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamCancelPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(reason);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalReadableStream::tee(JSC::JSGlobalObject& globalObject, bool shouldClone)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamTeePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(shouldClone ? JSC::JSValue(JSC::JSValue::JSTrue) : JSC::JSValue(JSC::JSValue::JSFalse));
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

}
