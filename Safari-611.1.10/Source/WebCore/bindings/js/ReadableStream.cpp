/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "ReadableStream.h"

#include "Exception.h"
#include "ExceptionCode.h"
#include "JSDOMConvertSequences.h"
#include "JSReadableStreamSink.h"
#include "JSReadableStreamSource.h"
#include "WebCoreJSClientData.h"


namespace WebCore {
using namespace JSC;

ExceptionOr<Ref<ReadableStream>> ReadableStream::create(JSC::JSGlobalObject& lexicalGlobalObject, RefPtr<ReadableStreamSource>&& source)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto& clientData = *static_cast<JSVMClientData*>(vm.clientData);
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject);

    auto* constructor = JSC::asObject(globalObject.get(&lexicalGlobalObject, clientData.builtinNames().ReadableStreamPrivateName()));

    auto constructData = getConstructData(vm, constructor);
    ASSERT(constructData.type != CallData::Type::None);

    MarkedArgumentBuffer args;
    args.append(source ? toJSNewlyCreated(&lexicalGlobalObject, &globalObject, source.releaseNonNull()) : JSC::jsUndefined());
    ASSERT(!args.hasOverflowed());

    JSObject* object = JSC::construct(&lexicalGlobalObject, constructor, constructData, args);
    ASSERT(!!scope.exception() == !object);
    RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

    return create(globalObject, *jsCast<JSReadableStream*>(object));
}

namespace ReadableStreamInternal {
static inline JSC::JSValue callFunction(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue jsFunction, JSC::JSValue thisValue, const JSC::ArgList& arguments)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    auto callData = JSC::getCallData(vm, jsFunction);
    ASSERT(callData.type != JSC::CallData::Type::None);
    auto result = call(&lexicalGlobalObject, jsFunction, callData, thisValue, arguments);
    scope.assertNoException();
    return result;
}
}

void ReadableStream::pipeTo(ReadableStreamSink& sink)
{
    auto& lexicalGlobalObject = *m_globalObject;
    JSVMClientData* clientData = static_cast<JSVMClientData*>(lexicalGlobalObject.vm().clientData);
    const Identifier& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamPipeToPrivateName();

    auto readableStreamPipeTo = m_globalObject->get(&lexicalGlobalObject, privateName);
    ASSERT(readableStreamPipeTo.isCallable(lexicalGlobalObject.vm()));

    MarkedArgumentBuffer arguments;
    arguments.append(readableStream());
    arguments.append(toJS(&lexicalGlobalObject, m_globalObject.get(), sink));
    ASSERT(!arguments.hasOverflowed());
    ReadableStreamInternal::callFunction(lexicalGlobalObject, readableStreamPipeTo, JSC::jsUndefined(), arguments);
}

std::pair<Ref<ReadableStream>, Ref<ReadableStream>> ReadableStream::tee()
{
    auto& lexicalGlobalObject = *m_globalObject;
    JSVMClientData* clientData = static_cast<JSVMClientData*>(lexicalGlobalObject.vm().clientData);
    const Identifier& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamTeePrivateName();

    auto readableStreamTee = m_globalObject->get(&lexicalGlobalObject, privateName);
    ASSERT(readableStreamTee.isCallable(lexicalGlobalObject.vm()));

    MarkedArgumentBuffer arguments;
    arguments.append(readableStream());
    arguments.append(JSC::jsBoolean(true));
    ASSERT(!arguments.hasOverflowed());
    auto returnedValue = ReadableStreamInternal::callFunction(lexicalGlobalObject, readableStreamTee, JSC::jsUndefined(), arguments);

    auto results = Detail::SequenceConverter<IDLInterface<ReadableStream>>::convert(lexicalGlobalObject, returnedValue);

    ASSERT(results.size() == 2);
    return std::make_pair(results[0].releaseNonNull(), results[1].releaseNonNull());
}

void ReadableStream::lock()
{
    auto& lexicalGlobalObject = *m_globalObject;
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto& clientData = *static_cast<JSVMClientData*>(vm.clientData);

    auto* constructor = JSC::asObject(m_globalObject->get(&lexicalGlobalObject, clientData.builtinNames().ReadableStreamDefaultReaderPrivateName()));

    auto constructData = getConstructData(vm, constructor);
    ASSERT(constructData.type != CallData::Type::None);

    MarkedArgumentBuffer args;
    args.append(readableStream());
    ASSERT(!args.hasOverflowed());

    JSC::construct(&lexicalGlobalObject, constructor, constructData, args);
    scope.assertNoException();
}

static inline bool checkReadableStream(JSDOMGlobalObject& globalObject, JSReadableStream* readableStream, JSC::JSValue function)
{
    auto& lexicalGlobalObject = globalObject;

    ASSERT(function);
    JSC::MarkedArgumentBuffer arguments;
    arguments.append(readableStream);
    ASSERT(!arguments.hasOverflowed());
    return ReadableStreamInternal::callFunction(lexicalGlobalObject, function, JSC::jsUndefined(), arguments).isTrue();
}

bool ReadableStream::isLocked() const
{
    return checkReadableStream(*globalObject(), readableStream(), globalObject()->builtinInternalFunctions().readableStreamInternals().m_isReadableStreamLockedFunction.get());
}

bool ReadableStream::isDisturbed() const
{
    return checkReadableStream(*globalObject(), readableStream(), globalObject()->builtinInternalFunctions().readableStreamInternals().m_isReadableStreamDisturbedFunction.get());
}

bool ReadableStream::isDisturbed(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    auto& vm = lexicalGlobalObject.vm();
    auto& globalObject = *jsDynamicCast<JSDOMGlobalObject*>(vm, &lexicalGlobalObject);
    auto* readableStream = jsDynamicCast<JSReadableStream*>(vm, value);

    return checkReadableStream(globalObject, readableStream, globalObject.builtinInternalFunctions().readableStreamInternals().m_isReadableStreamDisturbedFunction.get());
}

}
