/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "WasmGlobal.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyRuntimeError.h"
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

JSValue Global::get(JSGlobalObject* globalObject) const
{
    switch (m_type.kind) {
    case TypeKind::I32:
        return jsNumber(bitwise_cast<int32_t>(static_cast<uint32_t>(m_value.m_primitive)));
    case TypeKind::I64:
        return JSBigInt::makeHeapBigIntOrBigInt32(globalObject, static_cast<int64_t>(m_value.m_primitive));
    case TypeKind::F32:
        return jsNumber(purifyNaN(static_cast<double>(bitwise_cast<float>(static_cast<uint32_t>(m_value.m_primitive)))));
    case TypeKind::F64:
        return jsNumber(purifyNaN(bitwise_cast<double>(m_value.m_primitive)));
    case TypeKind::Externref:
    case TypeKind::Funcref:
    case TypeKind::TypeIdx:
        return m_value.m_externref.get();
    default:
        return jsUndefined();
    }
}

void Global::set(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    ASSERT(m_mutability != Wasm::GlobalInformation::Immutable);
    switch (m_type.kind) {
    case TypeKind::I32: {
        int32_t value = argument.toInt32(globalObject);
        RETURN_IF_EXCEPTION(throwScope, void());
        m_value.m_primitive = static_cast<uint64_t>(static_cast<uint32_t>(value));
        break;
    }
    case TypeKind::I64: {
        int64_t value = argument.toBigInt64(globalObject);
        RETURN_IF_EXCEPTION(throwScope, void());
        m_value.m_primitive = static_cast<uint64_t>(value);
        break;
    }
    case TypeKind::F32: {
        float value = argument.toFloat(globalObject);
        RETURN_IF_EXCEPTION(throwScope, void());
        m_value.m_primitive = static_cast<uint64_t>(bitwise_cast<uint32_t>(value));
        break;
    }
    case TypeKind::F64: {
        double value = argument.toNumber(globalObject);
        RETURN_IF_EXCEPTION(throwScope, void());
        m_value.m_primitive = bitwise_cast<uint64_t>(value);
        break;
    }
    case TypeKind::Externref: {
        RELEASE_ASSERT(m_owner);
        if (!m_type.isNullable() && argument.isNull()) {
            throwException(globalObject, throwScope, createJSWebAssemblyRuntimeError(globalObject, vm, "Non-null Externref cannot be null"));
            return;
        }
        m_value.m_externref.set(m_owner->vm(), m_owner, argument);
        break;
    }
    case TypeKind::TypeIdx:
    case TypeKind::Funcref: {
        RELEASE_ASSERT(m_owner);
        bool isNullable = m_type.isNullable();
        WebAssemblyFunction* wasmFunction = nullptr;
        WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
        if (!isWebAssemblyHostFunction(vm, argument, wasmFunction, wasmWrapperFunction) && (!isNullable || !argument.isNull())) {
            throwException(globalObject, throwScope, createJSWebAssemblyRuntimeError(globalObject, vm, "Funcref must be an exported wasm function"));
            return;
        }
        if (m_type.kind == TypeKind::TypeIdx && (wasmFunction || wasmWrapperFunction)) {
            Wasm::SignatureIndex paramIndex = m_type.index;
            Wasm::SignatureIndex argIndex;
            if (wasmFunction)
                argIndex = wasmFunction->signatureIndex();
            else
                argIndex = wasmWrapperFunction->signatureIndex();
            if (paramIndex != argIndex) {
                throwException(globalObject, throwScope, createJSWebAssemblyRuntimeError(globalObject, vm, "Argument function did not match the reference type"));
                return;
            }
        }
        m_value.m_externref.set(m_owner->vm(), m_owner, argument);
        break;
    }
    default:
        break;
    }
}

template<typename Visitor>
void Global::visitAggregateImpl(Visitor& visitor)
{
    switch (m_type.kind) {
    case TypeKind::Externref:
    case TypeKind::Funcref: {
        RELEASE_ASSERT(m_owner);
        visitor.append(m_value.m_externref);
        break;
    }
    default:
        break;
    }
}

DEFINE_VISIT_AGGREGATE(Global);

} } // namespace JSC::Global

#endif // ENABLE(WEBASSEMBLY)
