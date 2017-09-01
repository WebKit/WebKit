/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "JSDOMConvertSequences.h"
#include "JSReadableStreamSink.h"
#include "JSReadableStreamSource.h"
#include "WebCoreJSClientData.h"

using namespace JSC;

namespace WebCore {

Ref<ReadableStream> ReadableStream::create(JSC::ExecState& execState, RefPtr<ReadableStreamSource>&& source)
{
    VM& vm = execState.vm();
    auto& clientData = *static_cast<JSVMClientData*>(vm.clientData);
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(execState.lexicalGlobalObject());

    auto* constructor = JSC::asObject(globalObject.get(&execState, clientData.builtinNames().ReadableStreamPrivateName()));

    ConstructData constructData;
    ConstructType constructType = constructor->methodTable(vm)->getConstructData(constructor, constructData);
    ASSERT(constructType != ConstructType::None);

    MarkedArgumentBuffer args;
    args.append(source ? toJSNewlyCreated(&execState, &globalObject, source.releaseNonNull()) : JSC::jsUndefined());

    auto newReadableStream = jsDynamicDowncast<JSReadableStream*>(vm, JSC::construct(&execState, constructor, constructType, constructData, args));

    return create(globalObject, *newReadableStream);
}

void ReadableStream::pipeTo(ReadableStreamSink& sink)
{
    auto& state = *m_globalObject->globalExec();
    JSVMClientData* clientData = static_cast<JSVMClientData*>(state.vm().clientData);
    const Identifier& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamPipeToPrivateName();

    auto readableStreamPipeTo = m_globalObject->get(&state, privateName);
    ASSERT(readableStreamPipeTo.isFunction());

    CallData callData;
    CallType callType = getCallData(readableStreamPipeTo, callData);
    ASSERT(callType != JSC::CallType::None);
    MarkedArgumentBuffer arguments;
    arguments.append(readableStream());
    arguments.append(toJS(&state, m_globalObject.get(), sink));
    JSC::call(&state, readableStreamPipeTo, callType, callData, JSC::jsUndefined(), arguments);
}

std::pair<Ref<ReadableStream>, Ref<ReadableStream>> ReadableStream::tee()
{
    auto& state = *m_globalObject->globalExec();
    JSVMClientData* clientData = static_cast<JSVMClientData*>(state.vm().clientData);
    const Identifier& privateName = clientData->builtinFunctions().readableStreamInternalsBuiltins().readableStreamTeePrivateName();

    auto readableStreamTee = m_globalObject->get(&state, privateName);
    ASSERT(readableStreamTee.isFunction());

    CallData callData;
    CallType callType = getCallData(readableStreamTee, callData);
    ASSERT(callType != JSC::CallType::None);
    MarkedArgumentBuffer arguments;
    arguments.append(readableStream());
    arguments.append(JSC::jsBoolean(true));
    JSValue returnedValue = JSC::call(&state, readableStreamTee, callType, callData, JSC::jsUndefined(), arguments);

    auto results = Detail::SequenceConverter<IDLInterface<ReadableStream>>::convert(state, returnedValue);

    ASSERT(results.size() == 2);
    return std::make_pair(results[0].releaseNonNull(), results[1].releaseNonNull());
}

}
