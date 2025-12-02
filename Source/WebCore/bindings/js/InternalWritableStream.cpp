/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "InternalWritableStream.h"

#include "Exception.h"
#include "JSDOMExceptionHandling.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSArrayBufferViewInlines.h>
#include <JavaScriptCore/JSObjectInlines.h>

namespace WebCore {

static ExceptionOr<JSC::JSValue> invokeWritableStreamFunction(JSC::JSGlobalObject& globalObject, const JSC::Identifier& identifier, const JSC::MarkedArgumentBuffer& arguments)
{
    JSC::VM& vm = globalObject.vm();
    JSC::JSLockHolder lock(vm);

    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto function = globalObject.get(&globalObject, identifier);
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
    ASSERT(function.isCallable());

    auto callData = JSC::getCallData(function);

    auto result = call(&globalObject, function, callData, JSC::jsUndefined(), arguments);
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    return result;
}

ExceptionOr<JSC::JSValue> InternalWritableStream::writeChunkForBingings(JSC::JSGlobalObject& globalObject, JSC::JSValue chunk)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());
    auto& writerPrivateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().acquireWritableStreamDefaultWriterPrivateName();
    auto writerResult = invokeWritableStreamFunction(globalObject, writerPrivateName, arguments);
    if (writerResult.hasException()) [[unlikely]]
        return writerResult.releaseException();

    arguments.clear();
    arguments.append(writerResult.returnValue());
    arguments.append(chunk);
    ASSERT(!arguments.hasOverflowed());
    auto& writePrivateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultWriterWritePrivateName();
    auto writeResult = invokeWritableStreamFunction(globalObject, writePrivateName, arguments);
    if (writeResult.hasException()) [[unlikely]]
        return writeResult.releaseException();

    arguments.clear();
    arguments.append(writerResult.returnValue());
    ASSERT(!arguments.hasOverflowed());
    auto& releasePrivateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultWriterReleasePrivateName();
    auto releaseResult = invokeWritableStreamFunction(globalObject, releasePrivateName, arguments);
    if (releaseResult.hasException()) [[unlikely]]
        return releaseResult.releaseException();

    return writeResult;
}

ExceptionOr<Ref<InternalWritableStream>> InternalWritableStream::createFromUnderlyingSink(JSDOMGlobalObject& globalObject, JSC::JSValue underlyingSink, JSC::JSValue strategy)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().createInternalWritableStreamFromUnderlyingSinkPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(underlyingSink);
    arguments.append(strategy);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeWritableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException()) [[unlikely]]
        return result.releaseException();

    ASSERT(result.returnValue().isObject());
    return adoptRef(*new InternalWritableStream(globalObject, *result.returnValue().toObject(&globalObject)));
}

Ref<InternalWritableStream> InternalWritableStream::fromObject(JSDOMGlobalObject& globalObject, JSC::JSObject& object)
{
    return adoptRef(*new InternalWritableStream(globalObject, object));
}

bool InternalWritableStream::locked() const
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return false;

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().isWritableStreamLockedPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeWritableStreamFunction(*globalObject, privateName, arguments);
    if (scope.exception())
        scope.clearException();

    return result.hasException() ? false : result.returnValue().isTrue();
}

void InternalWritableStream::lock()
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().acquireWritableStreamDefaultWriterPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    invokeWritableStreamFunction(*globalObject, privateName, arguments);
    if (scope.exception()) [[unlikely]]
        scope.clearException();
}

JSC::JSValue InternalWritableStream::abortForBindings(JSC::JSGlobalObject& globalObject, JSC::JSValue reason)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamAbortForBindingsPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(reason);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeWritableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalWritableStream::abort(JSC::JSGlobalObject& globalObject, JSC::JSValue reason)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamAbortPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(reason);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeWritableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalWritableStream::closeForBindings(JSC::JSGlobalObject& globalObject)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamCloseForBindingsPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeWritableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

void InternalWritableStream::closeIfPossible()
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamCloseIfPossiblePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    invokeWritableStreamFunction(*globalObject, privateName, arguments);
    if (scope.exception()) [[unlikely]]
        scope.clearException();
}

void InternalWritableStream::errorIfPossible(Exception&& exception)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    Ref vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto* clientData = downcast<JSVMClientData>(vm->clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamErrorIfPossiblePrivateName();

    auto reason = createDOMException(*globalObject, WTFMove(exception));

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(reason);
    ASSERT(!arguments.hasOverflowed());

    invokeWritableStreamFunction(*globalObject, privateName, arguments);
    if (scope.exception()) [[unlikely]]
        scope.clearException();
}

JSC::JSValue InternalWritableStream::errorIfPossible(JSC::JSGlobalObject& globalObject, JSC::JSValue reason)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamErrorIfPossiblePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(reason);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeWritableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalWritableStream::getWriter(JSC::JSGlobalObject& globalObject)
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().acquireWritableStreamDefaultWriterPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeWritableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

// FIXME: Remove globalObject parameter.
String InternalWritableStream::state(JSC::JSGlobalObject& globalObject) const
{
    auto* clientData = downcast<JSVMClientData>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamStatePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeWritableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue().toWTFString(&globalObject);
}

ExceptionOr<JSC::JSValue> InternalWritableStream::storedError() const
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return Exception { ExceptionCode::InvalidStateError };

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamStoredErrorPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    return invokeWritableStreamFunction(*globalObject, privateName, arguments);
}

bool InternalWritableStream::closeQueuedOrInFlight()
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return true;

    auto* clientData = downcast<JSVMClientData>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamCloseQueuedOrInFlightPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeWritableStreamFunction(*globalObject, privateName, arguments);
    if (result.hasException())
        return true;

    return result.returnValue().toBoolean(globalObject);
}

}
