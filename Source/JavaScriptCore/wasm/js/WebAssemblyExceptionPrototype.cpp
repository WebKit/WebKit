/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WebAssemblyExceptionPrototype.h"

#if ENABLE(WEBASSEMBLY)

#include "AuxiliaryBarrierInlines.h"
#include "JSCInlines.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyTag.h"

namespace JSC {
static JSC_DECLARE_HOST_FUNCTION(webAssemblyExceptionProtoFuncGetArg);
static JSC_DECLARE_HOST_FUNCTION(webAssemblyExceptionProtoFuncIs);
}

#include "WebAssemblyExceptionPrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyExceptionPrototype::s_info = { "WebAssembly.Exception"_s, &Base::s_info, &prototypeTableWebAssemblyException, nullptr, CREATE_METHOD_TABLE(WebAssemblyExceptionPrototype) };

/* Source for WebAssemblyExceptionPrototype.lut.h
 @begin prototypeTableWebAssemblyException
 getArg   webAssemblyExceptionProtoFuncGetArg   Function 2
 is       webAssemblyExceptionProtoFuncIs       Function 1
 @end
 */

WebAssemblyExceptionPrototype* WebAssemblyExceptionPrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyExceptionPrototype>(vm)) WebAssemblyExceptionPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* WebAssemblyExceptionPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyExceptionPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

WebAssemblyExceptionPrototype::WebAssemblyExceptionPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

ALWAYS_INLINE static JSWebAssemblyException* getException(JSGlobalObject* globalObject, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!thisValue.isCell())) {
        throwVMError(globalObject, scope, createNotAnObjectError(globalObject, thisValue));
        return nullptr;
    }
    auto* tag = jsDynamicCast<JSWebAssemblyException*>(thisValue.asCell());
    if (LIKELY(tag))
        return tag;
    throwTypeError(globalObject, scope, "WebAssembly.Exception operation called on non-Exception object"_s);
    return nullptr;
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyExceptionProtoFuncGetArg, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    const auto& formatMessage = [&](const auto& message) {
        return makeString("WebAssembly.Exception.getArg(): ", message);
    };

    JSWebAssemblyException* jsException = getException(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, { });

    if (UNLIKELY(callFrame->argumentCount() < 2))
        return JSValue::encode(throwException(globalObject, throwScope, createNotEnoughArgumentsError(globalObject)));

    JSWebAssemblyTag* tag = jsDynamicCast<JSWebAssemblyTag*>(callFrame->argument(0));
    if (UNLIKELY(!tag))
        return throwVMTypeError(globalObject, throwScope, formatMessage("First argument must be a WebAssembly.Tag"_s));

    uint32_t index = toNonWrappingUint32(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(throwScope, { });

    if (UNLIKELY(jsException->tag() != tag->tag()))
        return throwVMTypeError(globalObject, throwScope, formatMessage("First argument does not match the exception tag"_s));

    if (UNLIKELY(index >= tag->tag().parameterCount()))
        return throwVMRangeError(globalObject, throwScope, formatMessage("Index out of range"_s));

    RELEASE_AND_RETURN(throwScope, JSValue::encode(jsException->getArg(globalObject, index)));
}

JSC_DEFINE_HOST_FUNCTION(webAssemblyExceptionProtoFuncIs, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSWebAssemblyException* jsException = getException(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(throwScope, { });

    if (UNLIKELY(callFrame->argumentCount() < 1))
        return JSValue::encode(throwException(globalObject, throwScope, createNotEnoughArgumentsError(globalObject)));

    JSWebAssemblyTag* tag = jsDynamicCast<JSWebAssemblyTag*>(callFrame->argument(0));
    if (!tag)
        return throwVMTypeError(globalObject, throwScope, "WebAssembly.Exception.is(): First argument must be a WebAssembly.Tag"_s);

    RELEASE_AND_RETURN(throwScope, JSValue::encode(jsBoolean(jsException->tag() == tag->tag())));
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
