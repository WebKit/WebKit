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
#include "JSReadableStreamSink.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSObjectInlines.h>

namespace WebCore {

static ExceptionOr<JSC::JSValue> invokeReadableStreamFunction(JSC::JSGlobalObject& globalObject, const JSC::Identifier& identifier, const JSC::MarkedArgumentBuffer& arguments)
{
    JSC::VM& vm = globalObject.vm();
    JSC::JSLockHolder lock(vm);

    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto function = globalObject.get(&globalObject, identifier);
    ASSERT(!!scope.exception() || function.isCallable());
    scope.assertNoExceptionExceptTermination();
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    auto callData = JSC::getCallData(function);

    auto result = call(&globalObject, function, callData, JSC::jsUndefined(), arguments);
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    return result;
}

ExceptionOr<Ref<InternalReadableStream>> InternalReadableStream::createFromUnderlyingSource(JSDOMGlobalObject& globalObject, JSC::JSValue underlyingSource, JSC::JSValue strategy)
{
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().createInternalReadableStreamFromUnderlyingSourcePrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(underlyingSource);
    arguments.append(strategy);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(globalObject, privateName, arguments);
    if (UNLIKELY(result.hasException()))
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

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());

    auto* clientData = static_cast<JSVMClientData*>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().isReadableStreamLockedPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(*globalObject, privateName, arguments);
    if (scope.exception())
        scope.clearException();

    return result.hasException() ? false : result.returnValue().isTrue();
}

bool InternalReadableStream::isDisturbed() const
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return false;

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());

    auto* clientData = static_cast<JSVMClientData*>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().isReadableStreamDisturbedPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(*globalObject, privateName, arguments);
    if (scope.exception())
        scope.clearException();

    return result.hasException() ? false : result.returnValue().isTrue();
}

void InternalReadableStream::cancel(Exception&& exception)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSC::JSLockHolder lock(globalObject->vm());
    cancel(*globalObject, toJSNewlyCreated(globalObject, JSC::jsCast<JSDOMGlobalObject*>(globalObject), DOMException::create(WTFMove(exception))), Use::Private);
    if (UNLIKELY(scope.exception()))
        scope.clearException();
}

void InternalReadableStream::lock()
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());

    auto* clientData = static_cast<JSVMClientData*>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().acquireReadableStreamDefaultReaderPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    ASSERT(!arguments.hasOverflowed());

    invokeReadableStreamFunction(*globalObject, privateName, arguments);
    if (UNLIKELY(scope.exception()))
        scope.clearException();
}

void InternalReadableStream::pipeTo(ReadableStreamSink& sink)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    JSC::JSLockHolder lock(globalObject->vm());

    auto* clientData = static_cast<JSVMClientData*>(globalObject->vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamPipeToPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(toJS(globalObject, globalObject, sink));
    ASSERT(!arguments.hasOverflowed());

    invokeReadableStreamFunction(*globalObject, privateName, arguments);
    if (UNLIKELY(scope.exception()))
        scope.clearException();
}

ExceptionOr<std::pair<Ref<InternalReadableStream>, Ref<InternalReadableStream>>> InternalReadableStream::tee(bool shouldClone)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return Exception { ExceptionCode::InvalidStateError };

    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto result = tee(*globalObject, shouldClone);
    if (UNLIKELY(scope.exception()))
        return Exception { ExceptionCode::ExistingExceptionError };

    auto results = Detail::SequenceConverter<IDLObject>::convert(*globalObject, result);
    ASSERT(results.size() == 2);

    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(globalObject);
    return std::make_pair(InternalReadableStream::fromObject(jsDOMGlobalObject, *results[0].get()), InternalReadableStream::fromObject(jsDOMGlobalObject, *results[1].get()));
}

JSC::JSValue InternalReadableStream::cancel(JSC::JSGlobalObject& globalObject, JSC::JSValue reason, Use use)
{
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
    auto& names = clientData->builtinFunctions().readableStreamInternalsBuiltins();
    auto& privateName = use == Use::Bindings ? names.readableStreamCancelForBindingsPrivateName() : names.readableStreamCancelPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(reason);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalReadableStream::getReader(JSC::JSGlobalObject& globalObject, JSC::JSValue options)
{
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamGetReaderForBindingsPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(options);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalReadableStream::pipeTo(JSC::JSGlobalObject& globalObject, JSC::JSValue streams, JSC::JSValue options)
{
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamPipeToForBindingsPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(streams);
    arguments.append(options);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalReadableStream::pipeThrough(JSC::JSGlobalObject& globalObject, JSC::JSValue dest, JSC::JSValue options)
{
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamPipeThroughForBindingsPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(guardedObject());
    arguments.append(dest);
    arguments.append(options);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeReadableStreamFunction(globalObject, privateName, arguments);
    if (result.hasException())
        return { };

    return result.returnValue();
}

JSC::JSValue InternalReadableStream::tee(JSC::JSGlobalObject& globalObject, bool shouldClone)
{
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
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
