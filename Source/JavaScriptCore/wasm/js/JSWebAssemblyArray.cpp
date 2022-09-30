/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
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
#include "JSWebAssemblyArray.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "WasmFormat.h"

namespace JSC {

const ClassInfo JSWebAssemblyArray::s_info = { "WebAssembly.Array"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyArray) };

JSWebAssemblyArray::JSWebAssemblyArray(VM& vm, Structure* structure, Wasm::Type elementType, size_t size, FixedVector<uint32_t>&& payload)
    : Base(vm, structure)
    , m_elementType(elementType)
    , m_size(size)
    , m_payload32(WTFMove(payload))
{
}

JSWebAssemblyArray::JSWebAssemblyArray(VM& vm, Structure* structure, Wasm::Type elementType, size_t size, FixedVector<uint64_t>&& payload)
    : Base(vm, structure)
    , m_elementType(elementType)
    , m_size(size)
    , m_payload64(WTFMove(payload))
{
}

JSWebAssemblyArray::~JSWebAssemblyArray()
{
    switch (m_elementType.kind) {
    case Wasm::TypeKind::I32:
    case Wasm::TypeKind::F32:
        m_payload32.~FixedVector<uint32_t>();
        break;
    default:
        m_payload64.~FixedVector<uint64_t>();
        break;
    }
}

void JSWebAssemblyArray::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    // FIXME: When the JS API is further defined, the precise semantics of how
    // the array should be exposed to JS may changed. For now, we seal it to
    // ensure that nobody can accidentally depend on being able to extend arrays.
    seal(vm);
}

void JSWebAssemblyArray::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyArray*>(cell)->JSWebAssemblyArray::~JSWebAssemblyArray();
}

template<typename Visitor>
void JSWebAssemblyArray::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSWebAssemblyArray* thisObject = jsCast<JSWebAssemblyArray*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    if (isRefType(thisObject->elementType())) {
        for (unsigned i = 0; i < thisObject->size(); ++i)
            visitor.append(bitwise_cast<WriteBarrier<Unknown>>(thisObject->get(i)));
    }
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyArray);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
