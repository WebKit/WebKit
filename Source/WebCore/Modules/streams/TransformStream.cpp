/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TransformStream.h"

#include "JSDOMConvertObject.h"
#include "JSDOMConvertSequences.h"
#include "JSReadableStream.h"
#include "JSTransformStream.h"
#include "JSWritableStream.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSObjectInlines.h>

namespace WebCore {

struct CreateInternalTransformStreamResult {
    JSC::JSValue transform;
    Ref<ReadableStream> readable;
    Ref<WritableStream> writable;
};

static ExceptionOr<CreateInternalTransformStreamResult> createInternalTransformStream(JSDOMGlobalObject&, JSC::JSValue transformer, JSC::JSValue writableStrategy, JSC::JSValue readableStrategy);

ExceptionOr<Ref<TransformStream>> TransformStream::create(JSC::JSGlobalObject& globalObject, std::optional<JSC::Strong<JSC::JSObject>>&& transformer, std::optional<JSC::Strong<JSC::JSObject>>&& writableStrategy, std::optional<JSC::Strong<JSC::JSObject>>&& readableStrategy)
{
    JSC::JSValue transformerValue = JSC::jsUndefined();
    if (transformer)
        transformerValue = transformer->get();

    JSC::JSValue writableStrategyValue = JSC::jsUndefined();
    if (writableStrategy)
        writableStrategyValue = writableStrategy->get();

    JSC::JSValue readableStrategyValue = JSC::jsUndefined();
    if (readableStrategy)
        readableStrategyValue = readableStrategy->get();

    auto result = createInternalTransformStream(*JSC::jsCast<JSDOMGlobalObject*>(&globalObject), transformerValue, writableStrategyValue, readableStrategyValue);
    if (result.hasException())
        return result.releaseException();

    auto transformResult = result.releaseReturnValue();
    return adoptRef(*new TransformStream(transformResult.transform, WTFMove(transformResult.readable), WTFMove(transformResult.writable)));
}

TransformStream::TransformStream(JSC::JSValue internalTransformStream, Ref<ReadableStream>&& readable, Ref<WritableStream>&& writable)
    : m_internalTransformStream(internalTransformStream)
    , m_readable(WTFMove(readable))
    , m_writable(WTFMove(writable))
{
}

TransformStream::~TransformStream()
{
}

static ExceptionOr<JSC::JSValue> invokeTransformStreamFunction(JSC::JSGlobalObject& globalObject, const JSC::Identifier& identifier, const JSC::MarkedArgumentBuffer& arguments)
{
    JSC::VM& vm = globalObject.vm();
    JSC::JSLockHolder lock(vm);

    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto function = globalObject.get(&globalObject, identifier);
    ASSERT(function.isCallable());
    scope.assertNoExceptionExceptTermination();

    auto callData = JSC::getCallData(function);

    auto result = call(&globalObject, function, callData, JSC::jsUndefined(), arguments);
    RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });

    return result;
}

ExceptionOr<CreateInternalTransformStreamResult> createInternalTransformStream(JSDOMGlobalObject& globalObject, JSC::JSValue transformer, JSC::JSValue writableStrategy, JSC::JSValue readableStrategy)
{
    auto* clientData = static_cast<JSVMClientData*>(globalObject.vm().clientData);
    auto& privateName = clientData->builtinFunctions().transformStreamInternalsBuiltins().createInternalTransformStreamFromTransformerPrivateName();

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(transformer);
    arguments.append(writableStrategy);
    arguments.append(readableStrategy);
    ASSERT(!arguments.hasOverflowed());

    auto result = invokeTransformStreamFunction(globalObject, privateName, arguments);
    if (UNLIKELY(result.hasException()))
        return result.releaseException();

    auto results = Detail::SequenceConverter<IDLObject>::convert(globalObject, result.returnValue());
    ASSERT(results.size() == 3);

    return CreateInternalTransformStreamResult { results[0].get(), JSC::jsDynamicCast<JSReadableStream*>(results[1].get())->wrapped(), JSC::jsDynamicCast<JSWritableStream*>(results[2].get())->wrapped() };
}

template<typename Visitor>
void JSTransformStream::visitAdditionalChildren(Visitor& visitor)
{
    wrapped().internalTransformStream().visit(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSTransformStream);

} // namespace WebCore
