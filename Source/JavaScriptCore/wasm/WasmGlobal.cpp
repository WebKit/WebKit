/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

JSValue Global::get() const
{
    switch (m_type) {
    case Wasm::Type::I32:
        return jsNumber(bitwise_cast<int32_t>(static_cast<uint32_t>(m_value.m_primitive)));
    case Wasm::Type::F32:
        return jsNumber(purifyNaN(static_cast<double>(bitwise_cast<float>(static_cast<uint32_t>(m_value.m_primitive)))));
    case Wasm::Type::F64:
        return jsNumber(purifyNaN(bitwise_cast<double>(m_value.m_primitive)));
    case Wasm::Anyref:
    case Wasm::Funcref:
        return m_value.m_anyref.get();
    default:
        return jsUndefined();
    }
}

void Global::set(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    ASSERT(m_mutability != Wasm::GlobalInformation::Immutable);
    switch (m_type) {
    case Wasm::Type::I32: {
        int32_t value = argument.toInt32(globalObject);
        RETURN_IF_EXCEPTION(throwScope, void());
        m_value.m_primitive = static_cast<uint64_t>(static_cast<uint32_t>(value));
        break;
    }
    case Wasm::Type::F32: {
        float value = argument.toFloat(globalObject);
        RETURN_IF_EXCEPTION(throwScope, void());
        m_value.m_primitive = static_cast<uint64_t>(bitwise_cast<uint32_t>(value));
        break;
    }
    case Wasm::Type::F64: {
        double value = argument.toNumber(globalObject);
        RETURN_IF_EXCEPTION(throwScope, void());
        m_value.m_primitive = bitwise_cast<uint64_t>(value);
        break;
    }
    case Wasm::Anyref: {
        RELEASE_ASSERT(m_owner);
        m_value.m_anyref.set(m_owner->vm(), m_owner, argument);
        break;
    }
    case Wasm::Funcref: {
        RELEASE_ASSERT(m_owner);
        if (!isWebAssemblyHostFunction(vm, argument) && !argument.isNull()) {
            throwException(globalObject, throwScope, createJSWebAssemblyRuntimeError(globalObject, vm, "Funcref must be an exported wasm function"));
            return;
        }
        m_value.m_anyref.set(m_owner->vm(), m_owner, argument);
        break;
    }
    default:
        break;
    }
}

void Global::visitAggregate(SlotVisitor& visitor)
{
    switch (m_type) {
    case Wasm::Type::Anyref:
    case Wasm::Type::Funcref: {
        RELEASE_ASSERT(m_owner);
        visitor.append(m_value.m_anyref);
        break;
    }
    default:
        break;
    }
}

} } // namespace JSC::Global

#endif // ENABLE(WEBASSEMBLY)
